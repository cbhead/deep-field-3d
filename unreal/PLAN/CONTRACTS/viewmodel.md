# C12 — UI view models

**Canonical:** `unreal/DeepField/Source/DFUI/Public/ViewModels/*.h` (UE MVVM `UMVVMViewModelBase` subclasses). **Owner:** WS-12. **Rule:** A for new fields; R for rename/remove. Mirrors `game/scripts/ui/GameView.cs`: **every widget reads only these; no widget or view model calls `HasAuthority()` / `GetNetMode()`** (CI grep on `Source/DFUI`).

`UDFMatchViewModel` — `Money, Lives, WaveIndex, TotalWaves, Phase, PhaseSecondsLeft, bLobby, bEndless, Threat, EnemiesRemaining, TeamScrap{}, ActiveCondition, NextCondition, Tier, BestWave, PendingSpend, Boss{bActive, Phase, HpFrac, Plates}, Players[] (UDFPlayerViewModel), Structures[] (UDFStructureViewModel), Pickups[], Vehicles[], LaneGateStates{}, MutableStates{}, ConnectionState, EarlyCallVotes{}, Pings[]`; helpers `Local()`, `AtSocket(FName)`, `TeamScrapOf(type)`, `PersonalScrapOf(player,type)`, `SeatOf(player)`.

`UDFPlayerViewModel` — `PlayerId, Name, Faction, FactionLevel, Position, Hp, MaxHp, bDowned, bConnected, ReviveProgress, BleedoutSecondsLeft, Weapon, Melee, AbilityCooldownFrac, Scrap{}, MatchXp, Kills, DamageDealt, TowersBuilt, Revives, OwnedWeapons[], CraftedAmmo[], Builds{} (FDFWeaponBuild), Seat, bHost, bCarrying, bDragging, bInNest`.

`UDFStructureViewModel` — `StructureId, DefTag, SocketId, OwnerPlayerId, PathLevels[3], bTrap, Charges, HpFrac, Heat, FedBy, bSellable (per ownership rules)`.

`UDFPickupViewModel` — `Id, ScrapType, Position, SecondsLeft, bPersonal`. `UDFVehicleViewModel` — `VehicleId, DefTag, Position, Yaw, Seats[], HpFrac, bWreck`.

Sources: `ADFMatchState`, the `ADFPlayerState` array, structure/pickup/vehicle actors — **replicated properties only**; the listen host reads its own authoritative actors through the same properties, so the code path is identical on host and client. Discrete UI events (toasts, refusals, combo callouts, boss horn) come from `DF.Message.*` subscriptions, never from polling.
