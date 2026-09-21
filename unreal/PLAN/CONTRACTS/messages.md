# C15 — Message inventory

**Canonical:** `unreal/DeepField/Source/DFCore/Public/Messages/DFMessages.h` (one `USTRUCT FDFMsg_<Name>` per message with the same fields as the `Events.cs` record) and `UDFMessageBus` (`Broadcast(Tag, Payload)`, `Subscribe(Tag, Handler)`). **Owner:** WS-00. **Rule:** A for new messages; R for field changes.

Transport (ADR-0004): the host broadcasts locally then `ADFEventRelay::NetMulticast_Event(FDFEventEnvelope)` for team-wide messages, or `Client_Refused(Tag, Reason)` for refusals to the issuing client only. UI, audio and VFX subscribe to messages and cues; nothing polls.

**Who broadcasts (INT ruling, 2026-09-21).** A system that *produces* a discrete fact builds the payload;
**DFMatch broadcasts it.** `ADFWaveDirector::DescribeWave` returns the `FDFMsg_Wave` fields (lap, threat =
the hp scale, condition tag) and never touches the bus; DFMatch's relay is the one multicast path, so a
client can never receive `WaveStarted` for a wave the match state has not entered. This follows from
ADR-0004 (one relay) and from the module layering — DFEnemies is layer 3 and cannot see DFMatch — and it
generalises: towers, economy and world systems hand DFMatch a payload rather than broadcasting their own.
The exception is a refusal, which goes `Client_Refused` to the issuing client from wherever it was refused.

Messages mirror `sim/Sim.Core/Events.cs:17-317` one-to-one (same names, same fields) — the port must keep the list; new ones are appended:

**Lobby/match:** `PlayerJoined, PlayerLeft, JoinRejected{reason,yourBuild,hostBuild}, FactionSet, MatchLaunched, WaveStarted, WaveCleared, Intermission, Victory, Defeat, CoreBreached{lives}, ConditionAnnounced{next}, EndlessLap, TierSet, EarlyCallVote{player,vote}, EarlyCalled{bonus}`.
**Building:** `TowerPlaced, BuildRejected{reason}, TowerUpgraded, UpgradeRejected, TowerSold, SellRejected{notOwner|limit}, TowerDestroyed, TrapTriggered, TrapRearmed, BarricadeState, StructureDamaged, BreachTargeted, OverclockFed{source,target,rate}`.
**Combat:** `TowerFired, ProjectileLanded, BeamHeld, EnemySpawned, EnemyDamaged{source,zone,amount}, EnemyKilled{killer,melee}, EnemyLeaked, EnemyTeleported, StatusApplied, StatusExpired, ReactionTriggered, ShieldPopped, ShieldRegen, Burrowed, Surfaced, ClusterSplit, Healed, Enraged, Knockdown, EliteSpawned{mods}, BossArrived, BossPhase{phase}, PlateRemoved, Tethered, TetherBroken, Phased, Leaped`.
**Player:** `PlayerHit, PlayerDowned, PlayerRevived, ReviveProgress, PlayerRespawned, ReloadStarted, Reloaded, AbilityUsed, AbilityRejected, ComboTriggered{a,b,combo}, Pinged{kind,target}, Dragging, Carrying, Mantled, HardLanding`.
**Economy/gunsmith:** `ScrapDropped, ScrapCollected, ScrapBanked, WeaponBought, PurchaseRejected, WeaponSelected, AttachmentCrafted, CraftRejected, AmmoSelected, PackAPunched, MeleeBought, MeleeUpgraded, CacheOpened`.
**Gates/mutables/vehicles:** `GateOperated, GateRejected{wouldSeal|body|cooldown}, MutableChanged{id,state}, FloodgatePulled, WallBroken, BarrelExploded, VehicleEntered, VehicleExited, VehicleRejected{reason}, VehicleRammed, VehicleWrecked, VehicleRespawned`.
**Online:** `ConnectionState, JoinRequest{id,name}, JoinApproved, Kicked, HostMigrationOffered, SaveResumed`.

Every refusal is a message with a machine-readable `reason` so the UI can say *why* ("factionTaken", "insufficientScrap", "wouldSeal", "notOwner").
