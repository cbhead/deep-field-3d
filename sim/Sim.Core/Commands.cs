namespace DeepField.Sim;

/// <summary>All mutation enters the sim through the command queue, applied at
/// tick boundaries — never mid-tick, never from the render or net layer directly.</summary>
public abstract record Command
{
    public sealed record PlaceTower(int PlayerId, string TowerId, string SocketId) : Command;

    public sealed record SellTower(int PlayerId, int TowerId) : Command;

    public sealed record StartWave(int PlayerId) : Command;

    /// <summary>Client-detected hitscan hit, server-sanity-checked (weapon cooldown).
    /// The heart of the FPS layer: clients raycast locally for instant feel; the sim
    /// applies the damage authoritatively.</summary>
    public sealed record PlayerHit(int PlayerId, int EnemyId, string WeaponId) : Command;
}
