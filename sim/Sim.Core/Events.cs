using System.Globalization;

namespace DeepField.Sim;

/// <summary>Discrete happenings pushed out per tick; the shell drains them into
/// VFX/SFX, the harness logs them, the determinism gate hashes them.
/// Continuous state (positions, hp) is pulled, never evented.</summary>
public abstract record SimEvent
{
    public long Tick { get; init; }

    /// <summary>Culture-invariant line for the determinism hash.</summary>
    public abstract string LogLine();

    private protected static string F(float v) => v.ToString("R", CultureInfo.InvariantCulture);

    public sealed record WaveStarted(int WaveIndex, int EnemyCount) : SimEvent
    {
        public override string LogLine() => $"{Tick} waveStarted {WaveIndex} {EnemyCount}";
    }

    public sealed record WaveCleared(int WaveIndex) : SimEvent
    {
        public override string LogLine() => $"{Tick} waveCleared {WaveIndex}";
    }

    public sealed record EnemySpawned(int EnemyId, string DefId, int WaveIndex) : SimEvent
    {
        public override string LogLine() => $"{Tick} enemySpawned {EnemyId} {DefId} {WaveIndex}";
    }

    public sealed record EnemyDamaged(int EnemyId, float Amount, string Source) : SimEvent
    {
        public override string LogLine() => $"{Tick} enemyDamaged {EnemyId} {F(Amount)} {Source}";
    }

    public sealed record EnemyDied(int EnemyId, string DefId, int Bounty, string Source) : SimEvent
    {
        public override string LogLine() => $"{Tick} enemyDied {EnemyId} {DefId} {Bounty} {Source}";
    }

    public sealed record EnemyLeaked(int EnemyId, string DefId, int LivesLost) : SimEvent
    {
        public override string LogLine() => $"{Tick} enemyLeaked {EnemyId} {DefId} {LivesLost}";
    }

    public sealed record TowerPlaced(int TowerId, string DefId, string SocketId, int PlayerId) : SimEvent
    {
        public override string LogLine() => $"{Tick} towerPlaced {TowerId} {DefId} {SocketId} {PlayerId}";
    }

    public sealed record TowerSold(int TowerId, int Refund) : SimEvent
    {
        public override string LogLine() => $"{Tick} towerSold {TowerId} {Refund}";
    }

    /// <summary>Refusals are answered out loud — the build ghost shows the reason.</summary>
    public sealed record BuildRejected(int PlayerId, string TowerId, string SocketId, string Reason) : SimEvent
    {
        public override string LogLine() => $"{Tick} buildRejected {PlayerId} {TowerId} {SocketId} {Reason}";
    }

    public sealed record TowerFired(int TowerId, int TargetId) : SimEvent
    {
        public override string LogLine() => $"{Tick} towerFired {TowerId} {TargetId}";
    }

    public sealed record MatchEnded(bool Victory, int WavesCleared, int LivesLeft) : SimEvent
    {
        public override string LogLine() => $"{Tick} matchEnded {(Victory ? "victory" : "defeat")} {WavesCleared} {LivesLeft}";
    }
}
