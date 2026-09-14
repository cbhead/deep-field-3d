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

    /// <summary>Lobby: re-pick a faction before launch. Refused with
    /// "factionTaken" if another player holds it.</summary>
    public sealed record SetFaction(int PlayerId, string FactionId, int FactionLevel = 1) : Command;

    /// <summary>Lobby: start the match. Only the launch seat may.</summary>
    public sealed record Launch(int PlayerId) : Command;

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
    /// <summary>Flip a lever: shut an open lane, or reopen one you shut.
    ///
    /// One command with a toggle rather than separate open/close verbs. The
    /// player presses one key at one gate and the gate does the other thing —
    /// splitting that into two commands would put a state machine on the wire
    /// for no gain, and every extra command is three more entries in Protocol
    /// and one more sample Gate 22 has to carry.</summary>
    public sealed record OperateGate(int PlayerId, string GateId) : Command;

    public sealed record PlayerHit(int PlayerId, int EnemyId, string WeaponId) : Command;

    /// <summary>A swing. Unlike a hitscan hit the client does not name a
    /// target: melee reach is short enough that the server can decide who is in
    /// the arc itself, which is both more honest and less to trust. AimPoint is
    /// where the player is looking; the swing direction is derived from it.</summary>
    public sealed record PlayerMelee(int PlayerId, Vec3 AimPoint) : Command;

    /// <summary>Buy a melee platform at the armory, and/or switch to an owned
    /// one. The wrench is owned from spawn and costs nothing.</summary>
    public sealed record BuyMelee(int PlayerId, string MeleeId) : Command;

    /// <summary>Craft a melee attachment into its slot, paid in personal scrap.</summary>
    public sealed record CraftMeleeAttachment(int PlayerId, string MeleeId, string AttachmentId) : Command;

    /// <summary>Spend personal scrap on the next mastery level for a platform.</summary>
    public sealed record UpgradeMelee(int PlayerId, string MeleeId) : Command;

    /// <summary>Reload the held weapon. Fired on a dry trigger automatically,
    /// and bound to a key so a player can top up before a wave rather than
    /// discovering the magazine is short halfway through one.</summary>
    public sealed record Reload(int PlayerId) : Command;

    /// <summary>Buy a weapon at the armory (money) and/or switch to an owned one.</summary>
    public sealed record BuyWeapon(int PlayerId, string WeaponId) : Command;

    public sealed record SelectWeapon(int PlayerId, string WeaponId) : Command;

    /// <summary>Craft an attachment onto a weapon's slot, paid in personal scrap
    /// (replacing whatever occupied the slot; no refunds — scrap is spent).</summary>
    public sealed record CraftAttachment(int PlayerId, string WeaponId, string AttachmentId) : Command;

    /// <summary>Pack a Punch: buy the next level of an uncapped damage-and-rate
    /// track on one weapon, for Alloy.</summary>
    public sealed record PackAPunch(int PlayerId, string WeaponId) : Command;

    /// <summary>Craft (once) and select an ammo type for a weapon.</summary>
    public sealed record SelectAmmo(int PlayerId, string WeaponId, string AmmoId) : Command;

    /// <summary>Faction signature ability at an aim point (Ember) or self
    /// (Forge — TargetPos ignored).</summary>
    public sealed record UseAbility(int PlayerId, Vec3 TargetPos) : Command;

    /// <summary>Held-interaction revive; progress accrues sim-side while both
    /// players stay in range.</summary>
    public sealed record Revive(int PlayerId, int TargetPlayerId) : Command;

    // ---- Vehicles ----------------------------------------------------------

    /// <summary>Take a seat. Refused out loud — seatTaken, alreadySeated,
    /// notNear, downed, badSeat, unknownVehicle — because the client shows the
    /// reason, and because "nothing happened" is the worst possible answer to a
    /// keypress.</summary>
    public sealed record EnterVehicle(int PlayerId, string VehicleId, int SeatIndex) : Command;

    /// <summary>Get out. Never refused: a player not in a seat has already got
    /// what they asked for.</summary>
    public sealed record ExitVehicle(int PlayerId) : Command;

    /// <summary>The driver's client-authoritative vehicle transform, streamed
    /// exactly as PlayerSync streams an avatar. Accepted only from whoever is
    /// in seat 0; anyone else's is dropped without a word, which is what
    /// PlayerSync does with a seat that does not exist.</summary>
    public sealed record VehicleSync(int PlayerId, string VehicleId, Vec3 Pos, float YawDegrees) : Command;
}
