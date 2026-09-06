using DeepField.Sim.Content;

namespace DeepField.Sim;

/// <summary>Flat mutable entity archetypes, exactly the 2D game's shape:
/// three typed lists with inline systems, marked dead during the tick,
/// removed in cleanup. No ECS ceremony at these entity counts.</summary>
public sealed class Enemy
{
    public int Id;
    public string DefId = "";
    public float Hp;
    public float MaxHp;
    public int Leg;               // index into route legs
    public float LegProgress;     // meters along current leg
    public float TotalTraveled;   // meters along whole route — the "first" targeting metric
    public Vec3 Pos;
    public int Bounty;
    public int LeakDamage;
    public int WaveIndex;
    public bool Dead;
}

public sealed class Tower
{
    public int Id;
    public string DefId = "";
    public string SocketId = "";
    public Vec3 Pos;
    public float Cooldown;
    public int Spent;
    public int Kills;
    public float DamageDealt;
}

public sealed class Projectile
{
    public int Id;
    public int FiredBy;
    public int TargetId;
    public Vec3 Pos;
    public float Speed;
    public float Damage;
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
public readonly record struct SpawnEntry(string DefId, int TickOffset, float HpFactor);

public sealed class World
{
    public uint Seed;
    public long Tick;
    public MapDef Map;

    public int Money;
    public int Lives;

    public MatchPhase Phase = MatchPhase.Intermission;
    public float PhaseTimer = Balance.IntermissionSeconds;

    /// <summary>Index of the current (or just-cleared) wave; -1 before the first.</summary>
    public int WaveIndex = -1;
    public List<SpawnEntry> PendingSpawns = new();
    public long WaveStartTick;

    public List<Enemy> Enemies = new();
    public List<Tower> Towers = new();
    public List<Projectile> Projectiles = new();

    /// <summary>Per-player weapon cooldowns (seconds remaining), keyed by player id.
    /// Players are not sim entities — they are command sources.</summary>
    public Dictionary<int, float> WeaponCooldowns = new();

    public List<Command> PendingCommands = new();

    /// <summary>Events emitted this tick; drained by the shell/harness after each step.</summary>
    public List<SimEvent> Events = new();

    /// <summary>Cumulative route leg lengths, precomputed once.</summary>
    public float[] LegLengths;
    public float RouteLength;

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

        LegLengths = new float[map.Route.Count - 1];
        RouteLength = 0f;
        for (int i = 0; i < LegLengths.Length; i++)
        {
            LegLengths[i] = map.Route[i].DistanceTo(map.Route[i + 1]);
            RouteLength += LegLengths[i];
        }
    }

    public void Enqueue(Command command) => PendingCommands.Add(command);

    public void Emit(SimEvent e) => Events.Add(e with { Tick = Tick });

    public bool IsOver => Phase is MatchPhase.Victory or MatchPhase.Defeat;
}
