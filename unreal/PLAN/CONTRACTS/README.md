# Contracts C1–C16 (+ map-authoring-3d)

Each document here is the human-readable twin of a header, asset or file format that more than one workstream depends on. The code is canonical; the document explains it and records the change rule. Change rules (PROGRAMME.md Section 3.1): **A** append-only via a `contract-append` PR, **R** RFC (`../rfcs/`), **I** integration-owned.

| # | Contract | Doc | Owner | Rule |
|---|---|---|---|---|
| C1 | Tag registry | [tags.md](tags.md) | WS-00 (native) / each WS (own ini) | native R; ini A |
| C2 | Content row schema | [content-rows.md](content-rows.md) | WS-01 | R (optional field with default = A) |
| C3 | Content JSON format | [content-json.md](content-json.md) | WS-01 | R; `$schema` bump on break |
| C4 | GAS attribute sets | [gas.md](gas.md) | WS-02 | R (new attribute = A) |
| C5 | Status / reaction semantics | [status.md](status.md) | WS-02 | R |
| C6 | Lane graph + socket asset | [lanegraph.md](lanegraph.md) | WS-09 | R; socket ids never renamed |
| C7 | Naming | [naming.md](naming.md) | WS-00 | A (new prefix) |
| C8 | Palette / tint material contract | [palette.md](palette.md) | WS-00 defines, WS-31 implements | R |
| C9 | Tower rig | [rig.md](rig.md) | WS-04 | R |
| C10 | Niagara parameter contract | [vfx.md](vfx.md) | WS-14 | R |
| C11 | Audio event contract | [audio.md](audio.md) | WS-13 | A (mappings), R (inputs) |
| C12 | UI view models | [viewmodel.md](viewmodel.md) | WS-12 | A (fields), R (rename) |
| C13 | Save / profile | [profile.md](profile.md) | WS-11 | R; migrations by Version |
| C14 | Online API | [online.md](online.md) | WS-11 | R |
| C15 | Message inventory | [messages.md](messages.md) | WS-00 | A (new), R (fields) |
| C16 | Collision & input | [collision-input.md](collision-input.md) | WS-00 | I |
| — | Map authoring in 3D (successor of `docs/MAP-AUTHORING.md` §4) | [map-authoring-3d.md](map-authoring-3d.md) | WS-09 | R |
| — | CI lanes, the self-hosted runner, test naming | [ci.md](ci.md) | WS-15 | A |
| — | What a tower may shoot: `Source/DFCore/Public/Combat/DFTargetable.h` — `IDFTargetable` (id, layer, position, aim point, dead, burrowed, stealthy, blocks sight, RemainingToCore; appended with defaults: GetKnockbackMass, ApplyKnockback, for traps) and `UDFTargetRegistry` (world subsystem; bodies register in BeginPlay). DFTowers and DFEnemies are siblings, so this is how a tower reads an enemy (2026-09-25) | (header) | WS-04 (consumer) · WS-05 (implements) | A |
| — | The team's purse: `Source/DFCore/Public/Economy/DFEconomySeams.h` — `IDFTeamWallet` (GetMoney, GetTeamScrap, all-or-nothing TrySpend(money, scrap bundle), AddMoney). Spenders below DFMatch (build, gunsmith, traps) find it on the game state or its components; WS-06's `UDFEconomyStateComponent` implements it (2026-09-25) | (header) | WS-04 (consumer) · WS-06 (implements) | A |
| — | What an enemy can break and a hero can mend: `Source/DFCore/Public/Combat/DFStructure.h` — `IDFStructure` (id, position, hp, max hp (0 = indestructible), ApplySiegeDamage, ApplyRepair) and `UDFStructureRegistry` (world subsystem, build order) with the sim's two picks ported once: `Siege` / `FindSiegeTarget` (Step.cs SiegeStructures) and `RepairNearby` / `FindRepairTarget` (ApplyPlayerMelee) (2026-09-25) | (header) | WS-04 (implements) · WS-05 (Siege) · WS-03 (RepairNearby) | A |
| — | Placeholder looks: `Source/DFCore/Public/Look/DFShapeLook.h` — `DFShapeLook::Tint(Mesh, Colour)` puts the engine's BasicShapeMaterial (vector `Color`) on the slot and tints it; UE 5.8's basic shapes carry DefaultMaterial, which takes no colour. `/Engine/BasicShapes` is always cooked (DefaultGame.ini) (2026-09-27) | (header) | INT · every placeholder (WS-04, WS-05, WS-09, WS-03) | A |
| — | Who a player is, below DFMatch: `Source/DFCore/Public/Match/DFSeatHolder.h` — `IDFSeatHolder` (`GetMatchSeat`: the 1-based seat every C15 PlayerId carries, 0 unseated) and `IDFSeatHolder::SeatOf(Who)` (Who itself, or the player state of Who as a pawn or a controller; 0 for anything else). `ADFPlayerState` implements it; `ADFEnemy` reads the seat behind a hit's instigator (the hero's pawn) into `EnemyDamaged.SourcePlayerId` and `EnemyKilled.KillerPlayerId`, and `ADFMatchState` credits that seat's record (2026-09-27) | (header) | WS-28 (implements) · WS-05 (reads) | A |
| — | Deterministic RNG streams and float helpers: `Source/DFCore/Public/Determinism/DFDetRng.h`, `DFDetMath.h` (ports of `Util/Rng.cs`, `Math/DetMath.cs`, pinned by WS-05's golden vectors; moved from DFEnemies by ruling R2, 2026-09-25) | (headers) | WS-05 | R (what a stream returns); A (new helpers) |
