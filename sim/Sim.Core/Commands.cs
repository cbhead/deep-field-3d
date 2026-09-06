namespace DeepField.Sim;

/// <summary>All mutation enters the sim through the command queue, applied at
/// tick boundaries — never mid-tick, never from the render or net layer directly.</summary>
public abstract record Command
{
    // ---- Session -----------------------------------------------------------

    /// <summary>Player joins (host, net join, or harness bot). Faction must be
    /// unclaimed — exclusivity is a sim rule, not a lobby courtesy. Level comes
    /// from the client's local profile (trusted friends; the sim just clamps).</summary>
    public sealed record Join(int PlayerId, string Name, string FactionId, int FactionLevel = 1) : Command;

    public sealed record Leave(int PlayerId) : Command;

    /// <summary>Client-authoritative avatar transform, streamed in and stamped
    /// into PlayerState. Position feeds contact damage, revive range, abilities.</summary>
    public sealed record PlayerSync(int PlayerId, Vec3 Pos) : Command;

    // ---- Building ----------------------------------------------------------

    public sealed record PlaceTower(int PlayerId, string TowerId, string SocketId) : Command;

    public sealed record SellTower(int PlayerId, int TowerId) : Command;

    /// <summary>Level one path by one. Costs money per level plus the scrap
    /// breakpoint recipe at L4, paid from the team pool.</summary>
    public sealed record UpgradeTower(int PlayerId, int TowerId, int PathIndex) : Command;

    public sealed record StartWave(int PlayerId) : Command;

    // ---- Hero layer --------------------------------------------------------

    /// <summary>Client-detected hitscan hit, server-sanity-checked (weapon
    /// cooldown). Clients raycast locally for instant feel; the sim applies
    /// damage authoritatively.</summary>
    public sealed record PlayerHit(int PlayerId, int EnemyId, string WeaponId) : Command;

    /// <summary>Buy a weapon at the armory (money) and/or switch to an owned one.</summary>
    public sealed record BuyWeapon(int PlayerId, string WeaponId) : Command;

    public sealed record SelectWeapon(int PlayerId, string WeaponId) : Command;

    /// <summary>Craft an attachment onto a weapon's slot, paid in personal scrap
    /// (replacing whatever occupied the slot; no refunds — scrap is spent).</summary>
    public sealed record CraftAttachment(int PlayerId, string WeaponId, string AttachmentId) : Command;

    /// <summary>Craft (once) and select an ammo type for a weapon.</summary>
    public sealed record SelectAmmo(int PlayerId, string WeaponId, string AmmoId) : Command;

    /// <summary>Faction signature ability at an aim point (Ember) or self
    /// (Forge — TargetPos ignored).</summary>
    public sealed record UseAbility(int PlayerId, Vec3 TargetPos) : Command;

    /// <summary>Held-interaction revive; progress accrues sim-side while both
    /// players stay in range.</summary>
    public sealed record Revive(int PlayerId, int TargetPlayerId) : Command;
}
