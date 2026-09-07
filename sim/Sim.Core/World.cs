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

    /// <summary>Index of the current (or just-cleared) wave; -1 before the first.</summary>
    public int WaveIndex = -1;
    public List<SpawnEntry> PendingSpawns = new();
    public long WaveStartTick;

    public List<Enemy> Enemies = new();
    public List<Tower> Towers = new();
    public List<Trap> Traps = new();
    public List<Projectile> Projectiles = new();
    public Dictionary<int, PlayerState> Players = new();

    public List<Command> PendingCommands = new();

    /// <summary>Events emitted this tick; drained by the shell/harness after each step.</summary>
    public List<SimEvent> Events = new();

    /// <summary>Per-route cumulative leg lengths, precomputed once.</summary>
    public float[][] RouteLegLengths;

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
        for (int r = 0; r < map.Routes.Count; r++)
        {
            var waypoints = map.Routes[r].Waypoints;
            var legs = new float[waypoints.Count - 1];
            for (int i = 0; i < legs.Length; i++)
                legs[i] = waypoints[i].DistanceTo(waypoints[i + 1]);
            RouteLegLengths[r] = legs;
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
