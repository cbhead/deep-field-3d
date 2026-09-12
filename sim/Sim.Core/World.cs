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
    // --- Where it is, in lane-graph coordinates.
    //
    // This was (RouteIndex, Leg, LegProgress): an index into a list of
    // polylines and a leg of that polyline. It is now an itinerary, a step
    // along that itinerary's edges, and a segment of the edge — the same three
    // numbers, one level down, addressing a graph instead of a list.
    //
    // The arithmetic is deliberately unchanged. SegmentProgress accumulates per
    // segment and resets at each one exactly as LegProgress did, rather than
    // being one distance along the whole edge, because a single accumulator
    // rounds differently and this port has to be provably free. Position is
    // still recomputed from scratch every tick, so nothing compounds.
    public int ItineraryIndex;    // which itinerary this enemy is following
    public int EdgeIndex;         // the graph edge it is on, or -1 once it leaked
    public int Segment;           // index into that edge's segments
    public float SegmentProgress; // meters along the current segment

    /// <summary>How far down its itinerary's list of vias it has got. An
    /// itinerary names places to pass through, not edges to walk, so this is
    /// what "the long way round" survives an edge closing as: the enemy still
    /// wants the barn, it just has to find another way there.</summary>
    public int ViaCursor;

    /// <summary>The edge it came off, or -1. One edge of memory, which is all
    /// knockback needs: a launcher moves 8 m and the shortest edge on any map is
    /// longer than that. It exists because the path is no longer a fixed list —
    /// once an enemy can route around a closed edge, "the previous step" is a
    /// fact about what happened, not about the itinerary.</summary>
    public int PrevEdgeIndex = -1;

    /// <summary>The edge this enemy has committed to breaking through, or -1.
    /// Intent, as opposed to <see cref="Sieging"/>, which is a fact about this
    /// tick recomputed from what happens to be in reach. Serialized, because a
    /// resumed Ram that has forgotten what it was walking at is a Ram that
    /// turns round.</summary>
    public int BreachTargetIndex = -1;

    /// <summary>Legs consumed since spawning. Only the wire wants it — the
    /// teleport event carries a leg index and is in the hashed log — and it is
    /// counted rather than derived because once an enemy can deviate from its
    /// itinerary there is no nominal leg to derive it from.</summary>
    public int LegCounter;
    public float TotalTraveled;   // meters walked; keys the burrow cycle
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

    /// <summary>Stopped at a junction with nowhere open to go. Should never
    /// happen — see <see cref="SimEvent.EnemyStranded"/>.</summary>
    public bool Stranded;

    /// <summary>Metres still to walk before this reaches a core.
    ///
    /// The metric towers and bots pick targets on. It replaces TotalTraveled,
    /// which measured metres *walked* and was therefore incomparable between
    /// routes of different lengths: on the Toaster a Skiff 100 m down `direct`
    /// (139 m long) and a Drifter 100 m down `long` (471 m) sorted as equally
    /// advanced, when one was 39 m from the core and the other 371 m. Worse, an
    /// enemy that had just come out of a warp — having crossed most of the map
    /// for free — sorted as barely started, so the wave that arrives behind you
    /// was the wave towers ignored.
    ///
    /// Remaining distance has none of that: it is the same question the defence
    /// is actually asking, in the same units, wherever the enemy came from.</summary>
    public float RemainingToCore(World w)
    {
        if (EdgeIndex < 0) return 0f;                            // leaked

        var lengths = w.EdgeSegmentLengths[EdgeIndex];
        float onThisEdge = -SegmentProgress;
        for (int i = Segment; i < lengths.Length; i++) onThisEdge += lengths[i];

        float ahead = w.DistToCore[w.Graph.NodeIndex[w.Graph.Edges[EdgeIndex].To]];
        // Cut off from every core: as far away as it is possible to be, so it
        // sorts last rather than first. Infinity would poison the comparison.
        return float.IsPositiveInfinity(ahead) ? float.MaxValue : onThisEdge + ahead;
    }

    /// <summary>Which leg of the underlying route this enemy is on — the read
    /// side of <see cref="AtRouteLeg"/>.
    ///
    /// Two callers, and both are about talking to something outside the sim's
    /// own coordinates: <see cref="SimEvent.EnemyTeleported"/> puts a leg index
    /// on the wire and in the hashed log, and the tests are written in legs
    /// because that is how the maps read. Not a second source of truth — route
    /// legs map one-to-one onto the itinerary's segments, in order.</summary>
    public int RouteLeg(World w) => LegCounter;

    /// <summary>Put this enemy at a leg of a route, in the coordinates the
    /// harness fixtures and the unit tests are written in.
    ///
    /// Sixteen gate fixtures say things like "leg 3 of the ground route, 14 m
    /// along" and one of them explains that leg 3 runs (0,14) to (0,-8) so 14 m
    /// puts it four metres from socket g4. That is a good way to write a
    /// fixture and a bad thing to have to rewrite as an edge and a segment when
    /// the storage changes underneath it. Route legs map one-to-one onto the
    /// itinerary's flattened segments, in order, so this is an exact
    /// translation rather than an approximation.</summary>
    /// <remarks>Deliberately does not touch <see cref="TotalTraveled"/>. Several
    /// fixtures set it by hand to a round number and then assert against that
    /// number — the launcher gate measures a setback from exactly 60 — so
    /// computing a "truer" value here would quietly move what those gates
    /// measure. Position is what this translates; distance walked stays the
    /// caller's business, as it was.</remarks>
    public Enemy AtRouteLeg(World w, int routeIndex, int leg, float progress)
    {
        ItineraryIndex = routeIndex;
        LegCounter = leg;
        var edges = w.ItineraryEdges[routeIndex];
        var itinerary = w.Graph.Itineraries[routeIndex];
        int walked = 0;
        for (int step = 0; step < edges.Length; step++)
        {
            int count = w.EdgeSegmentLengths[edges[step]].Length;
            if (walked + count > leg)
            {
                EdgeIndex = edges[step];
                Segment = leg - walked;
                SegmentProgress = progress;
                // The via it is heading for is the one at the far end of this
                // edge; everything before that it has already passed.
                ViaCursor = step + 1;
                return this;
            }
            walked += count;
        }
        // Past the end: leaked, which is what the caller asked for.
        EdgeIndex = -1;
        Segment = 0;
        SegmentProgress = 0f;
        ViaCursor = itinerary.Via.Count;
        return this;
    }
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

    /// <summary>The map's routes as a graph. Movement reads this now; the
    /// routes themselves are only still read to build it.</summary>
    public readonly LaneGraph Graph;

    /// <summary>Edge indices, in order, for each itinerary. Itinerary i is
    /// route i — the derivation walks the routes in order — so an enemy's
    /// itinerary index is still the index its wave plan gave it.</summary>
    public readonly int[][] ItineraryEdges;

    /// <summary>Segment lengths of each edge, by edge index. Computed exactly
    /// as <see cref="RouteLegLengths"/> computes leg lengths, from the same
    /// points in the same order, because the port from legs to segments has to
    /// be arithmetically free. A warp edge has one segment of length zero: it
    /// is crossed, not walked.</summary>
    public readonly float[][] EdgeSegmentLengths;

    /// <summary>Which lane edges are currently passable. Serialized as the set
    /// of closed ids, so a save naming an edge a later map no longer has is
    /// ignored rather than throwing.</summary>
    public bool[] EdgeOpen = System.Array.Empty<bool>();

    /// <summary>Metres from each graph node to the nearest core.</summary>
    public float[] DistToCore = System.Array.Empty<float>();

    /// <summary>Metres from every node to every node, indexed by target then by
    /// source. Rebuilt with <see cref="DistToCore"/> whenever an edge opens or
    /// closes — once per mutation, not per enemy per tick. The graph is tens of
    /// edges wide, so the whole table costs less than one tick of the sight
    /// checks the sim already runs.</summary>
    public float[][] DistToNode = System.Array.Empty<float[]>();

    /// <summary>The same two tables computed as if every gate were open — what a
    /// siege enemy routes on. A Ram has to be able to *want* the closed lane,
    /// or it can never reach the wall it exists to break.</summary>
    public float[] DistToCoreOpen = System.Array.Empty<float>();
    public float[][] DistToNodeOpen = System.Array.Empty<float[]>();

    /// <summary>Open or close every gated edge from what is standing on its
    /// socket, then rebuild the routing tables. Called after anything that can
    /// put a barricade up or take one down.</summary>
    public void RefreshEdgeState(string cause = "")
    {
        foreach (var gate in Map.LaneGates)
        {
            int e = Graph.EdgeIndexOf(gate.EdgeId);
            if (e < 0) continue;
            bool open = !Towers.Any(t => t.SocketId == gate.SocketId
                && Content.Towers.All[t.DefId].Kind == TowerKind.Barricade);
            if (open && !EdgeOpen[e] && cause.Length > 0)
                Emit(new SimEvent.LaneOpened(gate.EdgeId, cause));
            EdgeOpen[e] = open;
        }
        RefreshRouting();
    }

    /// <summary>Structure health standing in the way of this edge — the wall a
    /// siege enemy would have to chew through to use it.</summary>
    public float BlockingHp(int edgeIndex)
    {
        float hp = 0f;
        string edgeId = Graph.Edges[edgeIndex].Id;
        foreach (var gate in Map.LaneGates)
        {
            if (gate.EdgeId != edgeId) continue;
            foreach (var tower in Towers)
                if (tower.SocketId == gate.SocketId
                    && Content.Towers.All[tower.DefId].Kind == TowerKind.Barricade)
                    hp += tower.Hp;
        }
        return hp;
    }

    /// <summary>Whether closing this edge would leave some spawn unable to reach
    /// any core. The rule the whole mutable layer rests on: the map can be
    /// shaped and cannot be sealed.
    ///
    /// Asked of *every* spawn, not only the ones a wave is currently using —
    /// making a content invariant depend on match state is how it stops being
    /// provable at author time.</summary>
    public bool WouldSeal(int edgeIndex)
    {
        if (!EdgeOpen[edgeIndex]) return false;          // already shut
        EdgeOpen[edgeIndex] = false;
        bool sealed_ = !Graph.EverySpawnReachesCore(EdgeOpen);
        EdgeOpen[edgeIndex] = true;
        return sealed_;
    }

    /// <summary>Recompute the routing tables. Called once at construction and
    /// once per edge state change; never inside the movement loop.</summary>
    public void RefreshRouting()
    {
        DistToCore = Graph.DistanceToCore(EdgeOpen);
        DistToNode = new float[Graph.Nodes.Count][];
        DistToCoreOpen = Graph.DistanceToCore();
        DistToNodeOpen = new float[Graph.Nodes.Count][];
        for (int i = 0; i < Graph.Nodes.Count; i++)
        {
            DistToNode[i] = Graph.DistanceToNode(Graph.Nodes[i].Id, EdgeOpen);
            DistToNodeOpen[i] = Graph.DistanceToNode(Graph.Nodes[i].Id);
        }
    }

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

        Graph = LaneGraph.FromRoutes(map);

        EdgeSegmentLengths = new float[Graph.Edges.Count][];
        for (int e = 0; e < Graph.Edges.Count; e++)
        {
            var edge = Graph.Edges[e];
            var lengths = new float[edge.Waypoints.Count - 1];
            for (int i = 0; i < lengths.Length; i++)
                lengths[i] = edge.Kind == LaneEdgeKind.Warp
                    ? 0f
                    : edge.Waypoints[i].DistanceTo(edge.Waypoints[i + 1]);
            EdgeSegmentLengths[e] = lengths;
        }

        EdgeOpen = new bool[Graph.Edges.Count];
        for (int e = 0; e < EdgeOpen.Length; e++) EdgeOpen[e] = true;
        RefreshRouting();

        var edgeIndex = new Dictionary<string, int>();
        for (int e = 0; e < Graph.Edges.Count; e++) edgeIndex[Graph.Edges[e].Id] = e;

        ItineraryEdges = new int[Graph.Itineraries.Count][];
        for (int i = 0; i < Graph.Itineraries.Count; i++)
        {
            var itinerary = Graph.Itineraries[i];
            var steps = new int[itinerary.Via.Count - 1];
            for (int s = 0; s < steps.Length; s++)
                steps[s] = edgeIndex[Graph.Edges.First(e =>
                    e.From == itinerary.Via[s] && e.To == itinerary.Via[s + 1]
                    && e.Layer == itinerary.Layer).Id];
            ItineraryEdges[i] = steps;
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
