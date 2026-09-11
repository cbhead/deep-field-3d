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

    /// <summary>The party launched: the intermission clock starts.</summary>
    public sealed record MatchLaunched(int ByPlayerId) : SimEvent
    {
        public override string LogLine() => $"{Tick} matchLaunched {ByPlayerId}";
    }

    /// <summary>A lobby re-pick went through.</summary>
    public sealed record FactionChanged(int PlayerId, string FactionId) : SimEvent
    {
        public override string LogLine() => $"{Tick} factionChanged {PlayerId} {FactionId}";
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

    // ---- M1 additions ------------------------------------------------------

    public sealed record PlayerJoined(int PlayerId, string Name, string FactionId) : SimEvent
    {
        public override string LogLine() => $"{Tick} playerJoined {PlayerId} {Name} {FactionId}";
    }

    public sealed record JoinRejected(int PlayerId, string Reason) : SimEvent
    {
        public override string LogLine() => $"{Tick} joinRejected {PlayerId} {Reason}";
    }

    public sealed record StatusApplied(int EnemyId, string StatusId, string Source) : SimEvent
    {
        public override string LogLine() => $"{Tick} statusApplied {EnemyId} {StatusId} {Source}";
    }

    public sealed record ReactionTriggered(int EnemyId, string ReactionId, float Damage) : SimEvent
    {
        public override string LogLine() => $"{Tick} reaction {EnemyId} {ReactionId} {F(Damage)}";
    }

    public sealed record TowerUpgraded(int TowerId, string PathId, int NewLevel) : SimEvent
    {
        public override string LogLine() => $"{Tick} towerUpgraded {TowerId} {PathId} {NewLevel}";
    }

    public sealed record UpgradeRejected(int PlayerId, int TowerId, string Reason) : SimEvent
    {
        public override string LogLine() => $"{Tick} upgradeRejected {PlayerId} {TowerId} {Reason}";
    }

    /// <summary>The personal half of a drop landed on the floor as a thing to
    /// walk over.</summary>
    public sealed record ScrapSpawned(int PickupId, string ScrapType, int Amount, float X, float Y, float Z) : SimEvent
    {
        public override string LogLine() =>
            $"{Tick} scrapSpawned {PickupId} {ScrapType} {Amount} {X:0.##} {Y:0.##} {Z:0.##}";
    }

    /// <summary>A player gained scrap. <c>PickupId</c> 0 means it never hit the
    /// floor — they killed the thing themselves and took the personal half on
    /// the spot; any other id is a pickup they walked over.</summary>
    public sealed record ScrapCollected(int PickupId, int PlayerId, string ScrapType, int Amount) : SimEvent
    {
        public override string LogLine() => $"{Tick} scrapCollected {PickupId} {PlayerId} {ScrapType} {Amount}";
    }

    /// <summary>Nobody came for it; the team pool gets it instead.</summary>
    public sealed record ScrapExpired(int PickupId, string ScrapType, int Amount) : SimEvent
    {
        public override string LogLine() => $"{Tick} scrapExpired {PickupId} {ScrapType} {Amount}";
    }

    public sealed record ScrapDropped(int EnemyId, string ScrapCounts) : SimEvent
    {
        public override string LogLine() => $"{Tick} scrapDropped {EnemyId} {ScrapCounts}";
    }

    public sealed record WeaponBought(int PlayerId, string WeaponId) : SimEvent
    {
        public override string LogLine() => $"{Tick} weaponBought {PlayerId} {WeaponId}";
    }

    public sealed record PurchaseRejected(int PlayerId, string WeaponId, string Reason) : SimEvent
    {
        public override string LogLine() => $"{Tick} purchaseRejected {PlayerId} {WeaponId} {Reason}";
    }

    public sealed record AttachmentCrafted(int PlayerId, string WeaponId, string AttachmentId) : SimEvent
    {
        public override string LogLine() => $"{Tick} attachmentCrafted {PlayerId} {WeaponId} {AttachmentId}";
    }

    /// <summary>A weapon went through the Pack a Punch. <c>Level</c> is what it
    /// is now, <c>Cost</c> what this one took.</summary>
    public sealed record PackedAPunch(int PlayerId, string WeaponId, int Level, int Cost) : SimEvent
    {
        public override string LogLine() => $"{Tick} packedAPunch {PlayerId} {WeaponId} {Level} {Cost}";
    }

    public sealed record ReloadStarted(int PlayerId, string WeaponId, float Seconds) : SimEvent
    {
        public override string LogLine() => $"{Tick} reloadStarted {PlayerId} {WeaponId} {Seconds:0.##}";
    }

    public sealed record Reloaded(int PlayerId, string WeaponId) : SimEvent
    {
        public override string LogLine() => $"{Tick} reloaded {PlayerId} {WeaponId}";
    }

    public sealed record StructureDamaged(int TowerId, int EnemyId, float Hp) : SimEvent
    {
        public override string LogLine() => $"{Tick} structureDamaged {TowerId} {EnemyId} {Hp:0.##}";
    }

    public sealed record StructureDestroyed(int TowerId, string DefId, string SocketId) : SimEvent
    {
        public override string LogLine() => $"{Tick} structureDestroyed {TowerId} {DefId} {SocketId}";
    }

    public sealed record StructureRepaired(int TowerId, float Hp) : SimEvent
    {
        public override string LogLine() => $"{Tick} structureRepaired {TowerId} {Hp:0.##}";
    }

    public sealed record MeleeSwing(int PlayerId, string MeleeId, bool Connected) : SimEvent
    {
        public override string LogLine() => $"{Tick} meleeSwing {PlayerId} {MeleeId} {(Connected ? 1 : 0)}";
    }

    public sealed record MeleeSelected(int PlayerId, string MeleeId) : SimEvent
    {
        public override string LogLine() => $"{Tick} meleeSelected {PlayerId} {MeleeId}";
    }

    public sealed record MeleeMastery(int PlayerId, string MeleeId, int Level) : SimEvent
    {
        public override string LogLine() => $"{Tick} meleeMastery {PlayerId} {MeleeId} {Level}";
    }

    public sealed record AmmoSelected(int PlayerId, string WeaponId, string AmmoId) : SimEvent
    {
        public override string LogLine() => $"{Tick} ammoSelected {PlayerId} {WeaponId} {AmmoId}";
    }

    public sealed record CraftRejected(int PlayerId, string ItemId, string Reason) : SimEvent
    {
        public override string LogLine() => $"{Tick} craftRejected {PlayerId} {ItemId} {Reason}";
    }

    public sealed record AbilityUsed(int PlayerId, string AbilityId) : SimEvent
    {
        public override string LogLine() => $"{Tick} abilityUsed {PlayerId} {AbilityId}";
    }

    public sealed record PlayerDamaged(int PlayerId, float Amount, string Source) : SimEvent
    {
        public override string LogLine() => $"{Tick} playerDamaged {PlayerId} {F(Amount)} {Source}";
    }

    public sealed record PlayerDowned(int PlayerId) : SimEvent
    {
        public override string LogLine() => $"{Tick} playerDowned {PlayerId}";
    }

    public sealed record PlayerRevived(int PlayerId, int ByPlayerId) : SimEvent
    {
        public override string LogLine() => $"{Tick} playerRevived {PlayerId} {ByPlayerId}";
    }

    public sealed record PlayerRespawned(int PlayerId) : SimEvent
    {
        public override string LogLine() => $"{Tick} playerRespawned {PlayerId}";
    }
}
