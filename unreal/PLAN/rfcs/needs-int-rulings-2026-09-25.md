# Needs INT — the open rulings, drafted (2026-09-25)

Status: **Accepted and applied by INT, 2026-09-25** — every ruling, R4 as amended below (decided on
player experience). The outcome table says where each one landed; the ruling texts are kept as the
record of what was asked and why. Code applied in a cloud session is **not yet built**: the first Mac
build and `DF.Unit+DF.Content+DF.Online` run after landing is its verification.

| # | Outcome | Applied in |
|---|---|---|
| R1 | Accepted | ADR-0023; `registry.json` + `workstreams/ws-28-match-flow.md`; `OWNERSHIP.md`; PROGRAMME.md §4.2, §5.1, §5.2; INT notes in ws-00/03/05/06/07/09/12 |
| R2 | Accepted | `Source/DFCore/Public/Determinism/` (moved, DFEnemies includes updated); `CONTRACTS/README.md` |
| R3 | Accepted | `Source/DFCore/Public/Online/DFJoinSeams.h`; `ADFGameMode::PreLogin`; `UDFOnlineSubsystem : IDFJoinValidator`; `FDFMsg_Player.OnlineId`; `online.md`, `messages.md` |
| R4 | Accepted, **amended** (player experience) | ADR-0025: explicit "Share code", advertised only while live, host's friends admitted without a prompt, non-modal admit toast; WS-11 / WS-12 implement |
| R5 | Accepted | ADR-0024 (ADR-0006 status line points to it) |
| R6 | Accepted | ADR-0026; WS-30's DoD restored to §5.4's wording in `registry.json` and its file |
| R7 | Accepted | `OWNERSHIP.md` header + WS-09 row |
| R8 | Accepted | `OWNERSHIP.md` row |
| R9 | Accepted | `rfcs/0003-closable-by-and-levers.md`; `lanegraph.md` |
| R10 | Accepted | `DFGameplayTagList.inl` (4 leaves); `DFContentTagCoverageTest.cpp` roots; `tags.md` |
| R11 | Accepted | `palette.md` |
| R12 | Accepted | `modules.json` |
| R13 | Accepted | `UDFStatusComponent::TryGetTimeRemaining` + `DF.Unit.Status.TimeRemainingUnknownBeforeServerClock`; `status.md` |
| R14 | Accepted | `ci.md` "Decided (R14)"; enforced by `test-gate-check.py` |

Original draft note: each ruling below was a recommendation to accept, amend, or reject one at a time. The "On acceptance" lines list the exact edits; INT (or the
workstream named) makes them in the usual way. Evidence comes from `origin/unreal/main` at `d9798bf`
and the open `ws/*` branches. Where a ruling proposes an ADR, INT numbers it at acceptance; the next
free number is ADR-0023.

The order is by what each ruling unblocks: R1 unblocks the most.

| # | Ruling | Asked by | Unblocks |
|---|---|---|---|
| R1 | A new workstream, WS-28 *Match flow*, owns DFMatch: match state, player state, the phase machine | WS-12, WS-05, WS-11 | WS-12's real feed, WS-05's `Wave*` messages, the PreLogin handshake, G2 |
| R2 | Move `DFDetRng.h`/`DFDetMath.h` from DFEnemies to `DFCore/Public/Determinism/` now | WS-05 | WS-07's level curve, WS-09's hazard stream, WS-06's scrap |
| R3 | Online seams in DFCore; `OnlineId` on `FDFMsg_Player` | WS-11 | the PreLogin handshake, join approval by id |
| R4 | Join codes: advertise the lobby only while a code is live (needs R3 first) | WS-11 | the G2 two-machine test |
| R5 | Generated sublevels keep their actors in the `.umap` (an amendment to ADR-0006) | WS-09, WS-30 | re-import on a checked-out tree, ownership checks |
| R6 | A generated binary is verified by its data, not its bytes | WS-30, WS-09 | WS-30's DoD, a future re-import CI job |
| R7 | WS-30 alone owns the terrain lane's tools, and `OWNERSHIP.md`'s header says what the checker does | WS-30 | — (correctness of the ledger) |
| R8 | An `OWNERSHIP.md` row for `L_Test_Status` (WS-02) | WS-02 | WS-02's DoD level being committed |
| R9 | C6 `ClosableBy`: record the implemented shape as RFC-0003 (landed-as-built) | WS-09 | contract text matching the code |
| R10 | Tags: a native `DF.Vehicle.*` root now; the connection states already exist | WS-12 | the vehicle view model's `DefTag` |
| R11 | `palette.md` names `DFTintLayout.h` | WS-02 | WS-31's materials |
| R12 | `modules.json` mirrors DFEditor's two added dependencies | WS-30 | — (ledger accuracy) |
| R13 | A client that doesn't yet know the server clock shows no countdown | WS-02, WS-12 | closes WS-02's one open minor |
| R14 | The landing test filter: get ground truth, then widen | INT (ci.md) | 15 tests that have never gated a landing |

Not INT rulings, and still yours as the human: register the Mac as the self-hosted runner, copy
`unreal-mac.yml`/`unreal-checks.yml` to `main` (a push to the default branch), create the EOS portal
product, and install the GPU box.

---

## R1 — WS-28 *Match flow* owns DFMatch

### The question
- **WS-12:** "Who writes `ADFMatchState` / `ADFPlayerState`? `Source/DFMatch/**` is WS-00's and only
  `ADFGameMode` exists. The replicated-state → view-model feed (the real one) is blocked on those classes."
- **WS-05:** "Who owns the match phase. The director owns *what spawns when* and *when the wave is
  over*; it does not own intermission, lives, victory/defeat, and it does not broadcast
  `DF.Message.Wave*` … confirm the phase machine is yours and that `OnWaveCleared` → intermission →
  `BeginWave` is the shape you want."
- **WS-11:** the handshake call belongs in `ADFGameMode::PreLogin`, "WS-00's file".

### What the plan says, and where the gap is
- §3 puts `GameMode/GameState/PlayerState/EventRelay/campaign` in **DFMatch** (layer 4 of
  `modules.json`). DFMatch can see every gameplay module below it. DFUI and DFOnline sit above it.
- §3.3 lists the replicated fields: `ADFMatchState` (money, lives, wave, phase, timer, threat, team
  scrap, lane/mutable edge states as FastArray) and `ADFPlayerState` (faction, level, scrap, builds,
  stats, downed, revive progress, seat). It also calls for `ADFEventRelay` for discrete state and
  says every mutation is a Server RPC on `ADFPlayerController`, mapped 1:1 onto `Commands.cs`.
- §5's registry gives **no workstream** DFMatch beyond WS-00's "`Source/DFMatch` skeleton".
  `OWNERSHIP.md` hands `Source/DFMatch/**` to WS-00, and WS-00 is in `review` with only CI left
  in its DoD. What exists is `ADFGameMode` (44 lines: PreLogin/PostLogin/Logout, and the player
  joined/left messages). WS-12 contract-appended `EDFMatchPhase` and `FDFWeaponBuild` to
  `DFCore/Public/Match/DFMatchTypes.h`.
- Nothing on the G2 path runs a match without these classes. Its gate needs a slice that builds, fights
  and wins on Foundry, and G3 needs "4 solo matches to victory in automation".

### Recommended ruling
1. **Register WS-28, *Match flow (DFMatch)*.** Critical path, phase P2–P3, size L, `editor_heavy:
   false`, needs WS-02 and WS-05's director (both on main). It **owns** `Source/DFMatch/**` and
   `Content/DF/Match/**`. It **provides** `ADFGameMode` (it inherits WS-00's skeleton),
   `ADFMatchState`, `ADFPlayerState`, `ADFPlayerController` (the Server RPC surface, one RPC per
   `Commands.cs` command, each refusal a `DF.Message.*Rejected` to the issuer only), `ADFEventRelay`,
   the phase machine, end conditions, the lobby Launch gate, endless, and the campaign/sector chain.
2. **Each field belongs to the workstream that owns its rules. The state actors host it.** A
   domain's replicated fields live in a component that the domain's workstream writes in its own
   module (a ModularGameplay `UGameStateComponent`/`UPlayerStateComponent`, which DFGameplay
   already depends on). WS-28 attaches the component and never edits it:

   | field (§3.3) | lives on | owner, module |
   |---|---|---|
   | phase, wave index, phase timer, threat multiplier, endless flag, seat roster | `ADFMatchState` itself | WS-28, DFMatch |
   | money, lives, team scrap, bounty | `UDFEconomyStateComponent` on the match state | WS-06, `DFGameplay/Economy` (§5.2 already gives WS-06 "money/lives/bounty") |
   | lane and mutable edge states (FastArray) | `UDFLaneStateComponent` on the match state | WS-09, DFWorld |
   | faction, level | `UDFFactionStateComponent` on the player state | WS-07, `DFGameplay/Factions` |
   | personal scrap, weapon builds | `UDFLoadoutStateComponent` on the player state | WS-06 |
   | downed, revive progress, seat (vehicle) | `UDFHeroStateComponent` on the player state | WS-03, DFPlayer |
   | stats (kills, damage, xp events) | `ADFPlayerState` itself | WS-28 (it writes the host match record WS-11 reads) |

   **Why components.** Layering forces it: DFGameplay, DFWorld and DFPlayer sit below DFMatch and
   cannot name `ADFMatchState`, but DFMatch can host components from all of them. It also means no
   workstream edits another's replication, and it follows the ownership lines §5.2 already draws.
   WS-12's view models read each component through the actor (DFUI sits above all of them), so C12
   does not change.
3. **The phase machine is WS-28's, and WS-05's proposed shape is accepted as is.** It is a port of
   `Step.cs` `UpdateWaves` + `CheckEndState` (Step.cs:940–1000, 1977–2003). Pinned details:
   - Intermission runs `Balance.IntermissionSeconds` (8 s). The clock does not run while `Lobby`
     (until the lowest-seated connected player sends Launch), nor while `WaitForPlayers` with nobody
     connected. The `StartWave` command (early call) sets the timer to 0 in intermission and does
     nothing in the lobby.
   - At the boundary: players whose bleedout expired respawn (a WS-03 hook), `WaveIndex++`, and then
     `ADFWaveDirector::BeginWave(WaveIndex, max(1, ConnectedPlayerCount))`. WS-28 then broadcasts
     `DF.Message.WaveStarted` with the director's `DescribeWave` payload. The director never
     broadcasts it.
   - On `OnWaveCleared`: broadcast `WaveCleared`, then Victory if `!Endless && WaveIndex + 1 >=
     TotalWaves`, otherwise Intermission with the timer reset.
   - **The lives check comes first, every tick.** `CheckEndState` tests `Lives <= 0` before the
     cleared test, so a last enemy that leaks the core to zero is a **Defeat**, not a Victory. The
     port must keep that order. A `DF.Unit.Match.LastLeakIsDefeat` test should pin it.
   - Enemies leak by message (WS-05 raises it); the economy component takes the lives off; WS-28
     reads lives from the component. WS-28 never writes lives.
4. **First WS-28 PR = the shells.** `ADFMatchState`/`ADFPlayerState`/`ADFPlayerController` with
   WS-28's own fields replicated and empty component slots; the phase machine against the director;
   `DF.Unit.Match.*` tests of the phase order in a test world. That PR alone unblocks WS-12's real
   feed and WS-05's messages. **If no session claims WS-28 within one INT cycle, INT lands the shells
   under WS-00's existing row** (a skeleton extension, which WS-00's scope allows) and the files pass
   to WS-28 at claim time.
5. **PreLogin.** R3's handshake call goes into `ADFGameMode::PreLogin`. INT applies it now as a
   one-line contract-append. From then on the file is WS-28's.

### Alternatives considered
- **Leave DFMatch with WS-00/INT.** Rejected: INT reviews and lands everything, so owning the most
  coupled gameplay code would mean reviewing its own work, and WS-00 is closing.
- **Give it to WS-05.** Rejected: the phase isn't about enemies. WS-05 is XL and already the longest
  chain, and DFEnemies (layer 3) can't host player state that DFPlayer's hero writes to.
- **Give it to WS-12.** Rejected: UI must never own authority (C12's no-net-branching rule).
- **One owner for every field on the state actors.** Rejected: that one workstream would edit
  WS-06's, WS-07's and WS-09's replication, and layering would stop DFGameplay code from even
  naming the fields it computes.

### On acceptance
- `registry.json`: append `{"ws": "28", "slug": "match-flow", "title": "Match flow (DFMatch)",
  "phase": "P2-P3", "critical": true, "editor_heavy": false, "size": "L", "consumes": ["C4", "C6",
  "C12", "C14", "C15"], "provides": ["ADFMatchState", "ADFPlayerState", "ADFPlayerController",
  "ADFEventRelay"], …}` with the scope and DoD above. Then `plan-scaffold.py` creates
  `workstreams/ws-28-match-flow.md`.
- `OWNERSHIP.md`: drop `unreal/DeepField/Source/DFMatch/**` from the WS-00 row and add
  `| unreal/DeepField/Source/DFMatch/**, Content/DF/Match/** | WS-28 |`.
- `PROGRAMME.md` §4.2: `WS02 --> WS28`, `WS05 --> WS28`, `WS28 --> G2`, `WS28 --> WS12` (real
  feed). §5.2 gets the WS-28 row.
- `DECISIONS.md`, ADR-0023 *Match state is a host of components; DFMatch owns the phase*:
  context and decision as items 2–3 above. Consequence: a domain adds a replicated match field by
  writing a component in its own module and asking WS-28 to attach it (a one-line PR).
- Reply in WS-05's and WS-12's open questions and in WS-11's Needs INT (strike the item, cite ADR-0023).

---

## R2 — The deterministic RNG and math move to DFCore now

**Asked by WS-05:** `DFDetRng.h`/`DFDetMath.h` are header-only with no DF dependencies. WS-05
proposed moving them "when the second consumer shows up". **It has:** WS-07's level curve is
`DetMath.PowInt(0.94f, level - 1)` (`Factions.cs`), and DFGameplay (layer 1) cannot include
DFEnemies (layer 3). WS-09's `HazardStream` (B§2.10) and WS-06's scrap need the same streams.

**Ruling:** accept the move now. It is a WS-05 contract-append of
`Source/DFCore/Public/Determinism/{DFDetRng.h, DFDetMath.h}` with names, namespaces and FP pragmas
unchanged, and it updates DFEnemies' includes in the same PR. `DF.Unit.WavePlan.{RngMatchesSim,
DetMathMatchesSim}` stay green, which proves nothing moved but the path.
**On acceptance:** strike WS-05's open question. C-contract README: record "determinism headers:
DFCore, append-only, WS-05 edits by RFC".

---

## R3 — Online seams in DFCore; `OnlineId` on `FDFMsg_Player`

**Asked by WS-11 (Needs INT and open questions).** DFMatch (layer 4) cannot call DFOnline (layer 5).

**Ruling:**
1. One DFCore contract-append by WS-11, `Source/DFCore/Public/Online/`, declaring three interfaces:
   - `IDFJoinValidator::ValidateJoinOptions(const FString& Options, FString& OutError)`
   - `IDFSanctionsCheck`
   - `IDFAntiCheatCheck`

   The last two are pass-through at L1, as C14 says.
2. `ADFGameMode::PreLogin` loops over the game instance's subsystems implementing
   `IDFJoinValidator`, exactly as WS-11 wrote it. **The reflection interim is rejected:** a
   `FindObject` on a class path and `ProcessEvent` on a hand-built parameter struct fail silently on
   a rename, which is the bug class this programme keeps paying for.
3. `FString OnlineId` (default empty) is appended to `FDFMsg_Player`. This is a C15 append: no
   existing field or tag changes.

**On acceptance:** WS-11 lands the three items in one PR, and INT reviews the PreLogin line (R1
item 5). Strike the three Needs-INT items.

---

## R4 — Join codes advertise the lobby only while a code is live *(decided on player experience: see ADR-0025, which amends this text)*

**Asked by WS-11.** EOS `INVITEONLY` lobbies can't be found by search, so a join code alone
can't reach one.

**Recommended ruling: option A.** Keep the lobby `InvitationOnly` and flip it to `PublicAdvertised`
(`ModifyLobbyJoinPolicy`) only while a code is live. Flip it back when the code expires (10 min) or
is used, and always at Launch. Why this is safe enough for L1 (B§5: host trusted, clients not): a
stranger who finds the lobby is only a lobby *member*, never a player, until `ApproveJoin`, because
PreLogin refuses `notInvited`. **That refusal only exists once R3 lands:** today `ADFGameMode::PreLogin`
does not call `ValidateJoinOptions` at all, so R4 must not ship before R3. The code is 32⁶ and rotates on every approval. The only lobby-level
data a member sees is the `DFLobby` attributes: map, tier, build and content hash, with no address,
since EOS resolves the host through `[EOS:<puid>]` only after a join.
**Option B** (the code travels only inside a friend invite) is safer but drops the
"send the code in a chat" join. Choose B if codes are only for friends-of-friends anyway.
**On acceptance:** WS-11 wires `ModifyLobbyJoinPolicy` and adds a `DF.Online.JoinCodePolicyReverts`
test on Null. The real proof is step (5) of WS-11's credential checklist.

**As decided (ADR-0025).** Option A, shaped for players:
- the rotator is always live today, so the host now **shares** a code on purpose (a "Share code" panel
  in the lobby and pause menu), and the lobby is advertised only while that shared code is live;
- the host's **friends** arriving by code are admitted without a prompt;
- everyone else is admitted through a **non-modal** toast that never pauses play.

The risk found while deciding: the join code is a searchable attribute, and search results return
searchable attributes, so while a lobby is advertised anyone listing Deep Field lobbies can read its code.
Approval for non-friends is therefore not optional, and the ADR says so.

---

## R5 — Generated sublevels keep their actors in the `.umap` (an ADR-0006 amendment)

**Asked by WS-09** (for `L_<Map>_Gameplay`) and **WS-30** (for `L_<Map>_Terrain` and the placeholder
`L_<Map>`). ADR-0006 says "External Actors on". Both importers save with actors inside the
`.umap` and give the same reasons:
- a generated level is rewritten wholesale by a commandlet, so external-actor files would churn per
  run (new GUIDs);
- `SaveWorld`'s stale-external-package cleanup opens a dialog that a `-unattended` commandlet can't
  answer;
- no `__ExternalActors__` glob exists, so the first external tree fails `ownership-check.py` for
  every owner;
- ADR-0010's lock model is one file per level.

**Ruling:** amend ADR-0006: *a sublevel written by a commandlet from text (`L_<Map>_Gameplay`,
`L_<Map>_Terrain`) keeps its actors in the `.umap`; a hand-authored sublevel (`_Art`, `_Lighting`,
`_Audio`, `L_<Map>` once a WS-10x owns it) uses External Actors, and INT adds its
`Content/__ExternalActors__/DF/Maps/<Map>/<Level>/**` glob to the owner's row when the first one is
created.* The importer's `bUseExternalActors=false` stays.
**On acceptance:** append the amendment to ADR-0006 in `DECISIONS.md`, and strike the item from WS-09's
Needs INT and WS-30's open questions.

---

## R6 — A generated binary is verified by its data, not its bytes

**Asked by WS-30:** does data identity (heightmap samples, landscape GUID, bounds and probes all
byte-equal across runs) meet §5.4's "regenerates the Foundry Landscape byte-identically", or is
a byte-stable `.umap` required? **WS-09 has the same problem:** re-saving an unchanged
`L_Foundry_Gameplay` is not byte-stable, while `DA_LaneGraph_Foundry` is.

**Ruling:** data identity meets the DoD, and §5.4 already says so: its WS-30 DoD reads "a
`terrain.json` regenerates the Foundry heightmap byte-identically and the Landscape data
deterministically (package GUIDs excepted)". The workstream file's question paraphrased that as
"byte-identically" for both halves. A package's bytes include per-save GUIDs the generator
doesn't control, so byte-equality tests the engine's serializer, not our input. **The rule, for every
generator:** the check compares the data the binary was generated from. For terrain that is the
heightmap samples and the landscape GUID. For a gameplay level it is the actor set by stable id and
each actor's transform and properties. This is ci.md's "something must prove the artefact came from
the input", applied: the proof is a data fingerprint, not the bytes. WS-30's reimport-in-place stays
worth doing, but **as a follow-up**, not a DoD item. It becomes required only when a CI job
re-imports on every run and must leave the tree clean.
**On acceptance:** no PROGRAMME change. Close WS-30's question citing §5.4's wording. (Applying it found the
source of the confusion: `registry.json` — and so the WS-30 file generated from it — had shortened the
DoD to "byte-identically"; both now carry §5.4's wording.)
WS-09's re-save item becomes "diff the actor set", tracked in its file.

---

## R7 — WS-30 alone owns the terrain lane's tools; the ownership header matches the checker

**Asked by WS-30.** `tools/ue-bridge/terrain/**` appears in **both** the WS-30 row and the WS-09 row.
§5.4 lists the terrain lane (`build_heightmap.py`) under WS-30, and the directory holds only
`build_heightmap.py` and its test. Separately, `OWNERSHIP.md`'s header says "First match wins;
later rows are more specific", but `ownership-check.py` accepts a file when **any** row matching
it names the workstream (`owners_of` returns every match). The header describes a rule the tool
doesn't apply.

**Ruling:** remove `tools/ue-bridge/terrain/**` from the WS-09 row. WS-09 sends changes to the
heightmap tool as PRs to WS-30. (WS-09's level importer is the `DFLevelImport` commandlet, which has its
own row; the `build_level.py` both workstreams' scope lines mention was never written.) Rewrite the header: *"A file belongs to every workstream
whose row matches it; overlapping rows are deliberate shared ownership and each overlap says why."*
**On acceptance:** those two `OWNERSHIP.md` edits. `ownership-check.py --ws 30` is unaffected.

---

## R8 — An `OWNERSHIP.md` row for `L_Test_Status`

**Asked by WS-02** (in `DFStatusLevelTests.cpp`: the DoD level "is NOT committed: `Content/DF/Dev/**`
is WS-15's glob … until INT adds the row this workstream's file asks for"). The test falls back to
`L_Dev_Empty` and spawns its rig, so nothing is broken, but WS-02's DoD names the level.
**Ruling:** add `| unreal/DeepField/Content/DF/Dev/L_Test_Status* | WS-02 (its DoD level, built by
Source/DFGameplay/Dev/make-l-test-status.py) |`. Any later `L_Test_<X>` for a workstream's DoD gets
the same kind of row.
**On acceptance:** the row. WS-02 then commits the level under an LFS lock.

---

## R9 — C6 `ClosableBy`: record the implemented shape as RFC-0003 (landed-as-built)

**Asked by WS-09.** `lanegraph.md` shows `ClosableBy: SocketId|MutableId|None` as one field. The
code has `EDFLaneClosableBy {None, Socket, Mutable, Lever}` plus `ClosableById`, because the sim's
operated gates (levers) close edges too, and the doc never listed them.
**Ruling:** this is a shape change (one field → two, plus a new closer kind), so it is an **RFC**,
not an append. Nothing outside WS-09 reads it yet, so it lands as **RFC-0003, landed-as-built**:
the RFC text records the change, `lanegraph.md` is corrected to the two fields, and the other
optional additions WS-09 listed (`Traversal[].Position/Label`, `Nodes[].Layer`, `NearMisses[]`,
`ADFLaneGraphInfo`) are recorded in the same RFC as appends.
**On acceptance:** WS-09 writes `rfcs/0003-closable-by-and-levers.md`; INT fixes `lanegraph.md` line 9.

---

## R10 — Tags: a native `DF.Vehicle.*` root now; the connection states already exist

**Asked by WS-12.**
- **Connection states: already settled.** WS-11's `Config/Tags/DF_Online.ini` registers
  `DF.Online.Connection.{Offline, LoggingIn, LoggedIn, Hosting, Joining, Connected, Failed}`, one
  per `EDFConnectionState`. WS-12's connection screen reads those, and nothing is added.
- **`DF.Vehicle.*`: add it now, native, by INT as a C1 append** (WS-08 is unclaimed and the root is
  a contract C12 already names): `DF.Vehicle.{Buggy, Dagator, Grnmchn, Vehickle}` from
  `vehicles.json`'s ids, through `DFTags::ForContentId`'s PascalCase rule. `DF.Content.TagCoverage`
  can then enforce the vehicle table like the others (its roots list has no vehicles entry today, so it
needs one), and WS-12 can drop its `VehicleId` workaround.

**On acceptance:** four `DF_TAG` lines in `DFGameplayTagList.inl`, plus a `vehicles` → `DF.Vehicle` entry in
`DFContentTagCoverageTest.cpp`'s roots (WS-01's file: a one-line PR to them).

---

## R11 — `palette.md` names `DFTintLayout.h`

**Asked by WS-02.** `Source/DFGameplay/Public/Tint/DFTintLayout.h` is the C8 Custom Primitive Data
index layout: 36 floats, append-only, never renumbered. WS-31's masters read it. `palette.md`
names `UDFTintComponent.h` as canonical but not the layout.
**Ruling:** accept. Add one sentence to `palette.md`'s canonical list: *"`DFTintLayout.h` is the
permanent CPD index layout (append-only; a material reads a float by the index there)."*

---

## R12 — `modules.json` mirrors DFEditor's two added dependencies

**Asked by WS-30.** `DFEditor.Build.cs` lists `ImageWrapper` and `PhysicsCore`, but DFEditor's
`private` list in `modules.json` does not. `layering-check.py` only reads DF dependencies, so
nothing fails, but the table stops describing the module.
**Ruling:** accept. Add both to DFEditor's `private` list.

---

## R13 — A client that doesn't yet know the server clock shows no countdown

**Asked by INT's landing note on WS-02 (2026-09-21).** `UDFStatusComponent::Now()` falls back to the
local clock when the world has no game state. That is right for the dev map and unit-test worlds,
but wrong for a joining client in the window before the `AGameStateBase` channel opens.
**Ruling:** "unknown" is its own answer. Append `bool TryGetTimeRemaining(EDFStatusChannel,
float& OutSeconds) const`, which returns `false` on a **client** with no game state. `TimeRemaining()`
keeps its current behaviour for the server and for worlds with no net mode. The UI rule (WS-12): a
status with no known remaining time shows its icon **without** the countdown ring, never a zero or
a full ring. The window is one actor-channel open, so blanking the ring is invisible in practice and
never wrong.
**On acceptance:** WS-02 adds the method and a test (a client-mode world with no game state →
`false`). No C12 view model carries a status timer yet; whichever view first shows a status
countdown (the enemy overhead or the HUD) reads it through `TryGetTimeRemaining`.

---

## R14 — The landing test filter: get ground truth, then widen

**From INT's own ci.md note (2026-09-21):** 15 of 87 registered tests (DF.Online, DF.Editor, DF.UI,
DF.Func) have never gated a landing. `test-gate-check.py` (on `claude/happy-babbage-t6qrhw`, in
review) now fails any test that is neither gated nor excluded with a reason, and it seeds those four
suites as reasoned exclusions.
**Ruling:** the ordered plan in ci.md stands:
1. one `editor-lock.sh test.sh DF` run on the Mac;
2. each red test recorded against its owner;
3. every green suite moved into the landing filter (`int-merge.sh` and `ci-local.sh` together — the
   check fails if they disagree);
4. a row in `test-gate-exclusions.tsv` only for what stays out, with its reason.

Target: DF.Online and DF.UI are headless and should join at once; DF.Editor and DF.Func load
maps, so they join once their run time is known.
