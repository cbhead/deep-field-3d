using System.Linq;
using DeepField.Sim.Content;

namespace DeepField.Sim;

/// <summary>One exclusive status slot per channel. Strongest wins, re-hit
/// refreshes, never stacks; Source survives for DoT kill attribution.</summary>
public struct StatusSlot
{
    public string? StatusId;
    public float TimeLeft;
    public string Source;

    public readonly bool Active => StatusId is not null && TimeLeft > 0f;
}

public sealed class Enemy
{
    public int Id;
    public string DefId = "";
    public float Hp;
    public float MaxHp;
    public int RouteIndex;        // which map route this enemy walks
    public int Leg;               // index into route legs
    public float LegProgress;     // meters along current leg
    public float TotalTraveled;   // meters along whole route — the "first" targeting metric
    public Vec3 Pos;              // spine position + lateral offset applied
    public Vec3 Facing;           // normalized travel direction (Aegis front arc)
    public float LateralOffset;   // scatter across the path width (seeded at spawn)
    public int Bounty;
    public int LeakDamage;
    public int WaveIndex;
    public bool Dead;

    /// <summary>Indexed by (int)Channel — fixed size, no allocation per status.</summary>
    public StatusSlot[] Statuses = new StatusSlot[8];

    // M2 state.
    public float Shield;
    public float ShieldTimer;     // counts down after damage; regen when expired
    public float CcResist;        // 0..1 gauge; full = immune to hard control
    public bool Burrowed;         // Mole: untargetable while underground

    /// <summary>Stopped to hit a structure. Not a status: it is a fact about
    /// what the enemy is doing this tick, recomputed every tick from whether a
    /// structure is in reach, so it cannot get stuck on.</summary>
    public bool Sieging;
}

public sealed class Tower
{
    public int Id;
    public string DefId = "";
    public string SocketId = "";
    public Vec3 Pos;
    public float Cooldown;
    public int Spent;             // money sunk (placement + upgrades) for sell refunds
    public int Kills;
    public float DamageDealt;

    /// <summary>Current structure health. TowerDef has carried StructureHp
    /// since M2 and nothing read it, because the thing that attacks structures
    /// is the Ram — so the Barricade's 300 hp was a number in a table. Towers
    /// with StructureHp 0 are indestructible and Hp stays at 0 for them, which
    /// keeps the common case free of bookkeeping.</summary>
    public float Hp;

    /// <summary>Level per upgrade path, parallel to TowerDef.UpgradePaths (0 = unbought).</summary>
    public int[] PathLevels = System.Array.Empty<int>();

    // Beam ramp: which target it is holding and for how long. Serialized,
    // because a resumed world with a half-charged Filament must continue
    // identically — the determinism gate compares event logs across a save.
    public int RampTargetId = -1;
    public float RampSeconds;

    /// <summary>Which enemy this tower last shot at. Only used to notice that
    /// it is acquiring something new, which is when Night's delay applies —
    /// weather taxes finding a target, not keeping one.</summary>
    public int LastTargetId = -1;

    /// <summary>Forge Overdrive: fire-rate factor and remaining time.</summary>
    public float BuffTimer;
    public float BuffFactor = 1f;
}

/// <summary>A vehicle someone can sit in. The sim keeps two things about it and
/// deliberately not a third: who is in which seat (authoritative — a seat is a
/// thing two players can fight over) and where the driver last said it is (not
/// authoritative — movement has never entered this sim and this is not the
/// place to start). Nothing here is integrated; Pos and YawDegrees only ever
/// change because a VehicleSync said so.</summary>
public sealed class Vehicle
{
    public string Id = "";
    public string DefId = "";
    public Vec3 Pos;
    public float YawDegrees;

    /// <summary>Occupant player id per seat, 0 for empty. Seat 0 drives, which
    /// is why it is an index and not a set.</summary>
    public int[] Seats = System.Array.Empty<int>();

    public int DriverId => Seats.Length > 0 ? Seats[0] : 0;

    public int SeatOf(int playerId) => System.Array.IndexOf(Seats, playerId);
}

/// <summary>Path-floor trap: charge-based, rearming. Placed via the same
/// command as towers; lives on a trap socket.</summary>
public sealed class Trap
{
    public int Id;
    public string DefId = "";
    public string SocketId = "";
    public Vec3 Pos;
    public int ChargesLeft;
    public float RearmTimer;
}

public sealed class Projectile
{
    public int Id;
    public int FiredBy;
    public int TargetId;
    public Vec3 Pos;
    public float Speed;
    public float Damage;
    public float SplashRadius;
    public float SplashFalloff;
    public bool Dead;
}

/// <summary>Players are command sources with authoritative vitals — never
/// entities inside the tick's targeting/movement systems.</summary>
public sealed class PlayerState
{
    public int Id;
    public string Name = "";
    public string FactionId = "";
    public int FactionLevel = 1;

    /// <summary>XP earned this match (kills, reactions, revives); the client
    /// banks it into the local profile at match end.</summary>
    public int MatchXp;

    // Match contribution. Tracked per player so the end-of-match screen can
    // say who did what — the sim already attributes damage and kills to a
    // player id, this just keeps the running totals.
    public int Kills;
    public float DamageDealt;
    public int TowersBuilt;
    public int Revives;
    public Vec3 Pos;
    public float Hp = Balance.PlayerMaxHp;
    public bool Downed;
    public float BleedoutTimer;
    public float ReviveProgress;
    public float RespawnTimer;
    public float RegenDelay;
    public float WeaponCooldown;

    /// <summary>Rounds left in the magazine, per weapon. The reserve is
    /// unlimited — a player is never disarmed — but the magazine is not, so
    /// firing has a rhythm instead of being a held button. Absent means full.</summary>
    public Dictionary<string, int> Magazine = new();

    /// <summary>Seconds left on a reload. Firing is refused while this runs,
    /// which is the cost that makes the magazine mean something.</summary>
    public float ReloadTimer;
    public string ReloadingWeapon = "";

    public int RoundsIn(string weaponId) =>
        Magazine.TryGetValue(weaponId, out int n) ? n : Weapons.All[weaponId].MagazineSize;
    public float AbilityCooldown;
    public string WeaponId = "sidearm";
    public HashSet<string> OwnedWeapons = new() { "sidearm" };
    public Dictionary<ScrapType, int> Scrap = new();
    public bool Connected = true;

    /// <summary>Melee: everyone carries the wrench from spawn and it is never
    /// taken away, so this is never empty and a player is never unarmed.</summary>
    public string MeleeId = "wrench";
    public float MeleeCooldown;
    public HashSet<string> OwnedMelee = new() { "wrench" };
    public Dictionary<string, MeleeBuild> MeleeBuilds = new();

    public MeleeBuild MeleeBuildFor(string meleeId)
    {
        if (!MeleeBuilds.TryGetValue(meleeId, out var build))
        {
            build = new MeleeBuild();
            MeleeBuilds[meleeId] = build;
        }
        return build;
    }

    /// <summary>Gunsmith state: build per owned weapon, ammo crafted once each.</summary>
    public Dictionary<string, WeaponBuild> Builds = new();
    public HashSet<string> CraftedAmmo = new() { "standard" };

    public WeaponBuild BuildFor(string weaponId)
    {
        if (!Builds.TryGetValue(weaponId, out var build))
        {
            build = new WeaponBuild();
            Builds[weaponId] = build;
        }
        return build;
    }

    public bool Alive => !Downed && RespawnTimer <= 0f;
}

/// <summary>Scrap lying on the floor where something died. The team's half of
/// a drop is banked the instant the kill lands — tower money is everyone's
/// problem and nobody should have to walk for it — but the personal half is a
/// physical thing you go and get, which is what makes leaving the perch cost
/// something and pays you for taking the risk.
///
/// Uncollected scrap is not destroyed: when it expires it goes to the team
/// pool. The economy is conserved either way, so a wave nobody could reach
/// still funds the defence; you just don't get to spend it on your own gun.</summary>
public sealed class ScrapPickup
{
    public int Id;
    public ScrapType Type;
    public int Amount;
    public Vec3 Pos;
    /// <summary>Seconds before it banks to the team pool.</summary>
    public float Life;
    /// <summary>Where it comes to rest. A Skiff dies fifteen metres up and its
    /// scrap used to hang there, visible and unreachable — the one drop the
    /// player who earned it could never collect. It falls to the walkable
    /// surface under the kill instead.</summary>
    public float GroundY;
    public bool Dead;
}

public enum MatchPhase
{
    Intermission,
    Wave,
    Victory,
    Defeat,
}

/// <summary>One entry in a wave's spawn schedule (ticks relative to wave start).</summary>
public readonly record struct SpawnEntry(string DefId, int TickOffset, float HpFactor, int RouteIndex, float LateralOffset);

public sealed class World
{
    public uint Seed;
    public long Tick;
    public MapDef Map;

    public int Money;
    public int Lives;

    /// <summary>Team scrap pool — funds tower upgrades. Personal shares live on
    /// each PlayerState and fund the gunsmith.</summary>
    public Dictionary<ScrapType, int> TeamScrap = new();

    public MatchPhase Phase = MatchPhase.Intermission;
    public float PhaseTimer = Balance.IntermissionSeconds;

    /// <summary>Servers set this so an empty lobby idles in intermission instead
    /// of burning waves before anyone joins. Harness worlds leave it false —
    /// towers-only runs legitimately have zero players.</summary>
    public bool WaitForPlayers;

    /// <summary>The party is still assembling: the intermission clock does
    /// not run and StartWave is ignored until the lowest-seated connected
    /// player launches (Command.Launch). Off by default so harness worlds and
    /// solo matches start the way they always have.</summary>
    public bool Lobby;

    /// <summary>Endless: the authored arc never ends. Past the last authored
    /// wave the tables cycle, hp keeps compounding on the wave index and the
    /// count grows per lap. There is no Victory — the run ends when the core
    /// does, and the wave reached is the score.</summary>
    public bool Endless;

    /// <summary>Who may launch from the lobby: the lowest connected seat, so a
    /// dedicated server's first joiner and a hosting player both qualify.</summary>
    public int LaunchSeat => Players.Values.Where(p => p.Connected).Select(p => p.Id).DefaultIfEmpty(1).Min();

    /// <summary>Index of the current (or just-cleared) wave; -1 before the first.</summary>
    public int WaveIndex = -1;
    public List<SpawnEntry> PendingSpawns = new();
    public long WaveStartTick;

    public List<Enemy> Enemies = new();
    public List<Tower> Towers = new();
    public List<Trap> Traps = new();
    public List<Projectile> Projectiles = new();
    public List<ScrapPickup> Pickups = new();
    public Dictionary<int, PlayerState> Players = new();

    public List<Command> PendingCommands = new();

    /// <summary>Events emitted this tick; drained by the shell/harness after each step.</summary>
    public List<SimEvent> Events = new();

    /// <summary>Per-route cumulative leg lengths, precomputed once. A teleport
    /// leg is zero: it is crossed, not walked, so nothing that counts metres
    /// should count it.</summary>
    public float[][] RouteLegLengths;

    /// <summary>How far each route actually walks, teleports excluded. The
    /// harness sizes its run cap off this — a map whose lane is four times
    /// longer takes four times as long to leak.</summary>
    public float[] RouteWalkLengths;

    /// <summary>Drivable vehicles, parked where the map put them. The sim owns
    /// their seats and takes the driver's word for their position.</summary>
    public List<Vehicle> Vehicles = new();

    /// <summary>Which vehicle and seat a player is in, if any. Four vehicles
    /// with two seats each is not worth an index.</summary>
    public (Vehicle Vehicle, int Seat)? SeatOf(int playerId)
    {
        foreach (var vehicle in Vehicles)
        {
            int seat = System.Array.IndexOf(vehicle.Seats, playerId);
            if (seat >= 0) return (vehicle, seat);
        }
        return null;
    }

    private int _nextId = 1;
    public int NextId() => _nextId++;

    /// <summary>Current counter value, for serialization — ids must survive a
    /// resume exactly or post-resume spawns diverge from the original run.</summary>
    public int NextIdValue => _nextId;

    public void EnsureNextIdAtLeast(int value)
    {
        if (_nextId < value) _nextId = value;
    }

    public World(uint seed, MapDef map)
    {
        Seed = seed;
        Map = map;
        Money = Balance.StartingMoney;
        Lives = Balance.StartingLives;

        RouteLegLengths = new float[map.Routes.Count][];
        RouteWalkLengths = new float[map.Routes.Count];
        for (int r = 0; r < map.Routes.Count; r++)
        {
            var route = map.Routes[r];
            var waypoints = route.Waypoints;
            var legs = new float[waypoints.Count - 1];
            float walked = 0f;
            for (int i = 0; i < legs.Length; i++)
            {
                legs[i] = route.IsTeleportLeg(i) ? 0f : waypoints[i].DistanceTo(waypoints[i + 1]);
                walked += legs[i];
            }
            RouteLegLengths[r] = legs;
            RouteWalkLengths[r] = walked;
        }

        foreach (var spawn in map.Vehicles)
        {
            Vehicles.Add(new Vehicle
            {
                Id = spawn.Id,
                DefId = spawn.DefId,
                Pos = spawn.Pos,
                YawDegrees = spawn.YawDegrees,
                Seats = new int[Content.Vehicles.All[spawn.DefId].Seats],
            });
        }
    }

    public void Enqueue(Command command) => PendingCommands.Add(command);

    public void Emit(SimEvent e) => Events.Add(e with { Tick = Tick });

    public bool IsOver => Phase is MatchPhase.Victory or MatchPhase.Defeat;

    public int ConnectedPlayerCount
    {
        get
        {
            int n = 0;
            foreach (var p in Players.Values)
                if (p.Connected) n++;
            return n;
        }
    }
}
