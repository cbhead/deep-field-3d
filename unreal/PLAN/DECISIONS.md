# Decision log (ADRs)

Append-only. Format: `## ADR-NNNN <title>` · Date · Status (Proposed | Accepted | Superseded by ADR-NNNN) · Context · Decision · Consequences · Links. INT assigns numbers; anyone may append a `Proposed` entry. Every RFC that lands references or creates an ADR. Full rationale for 0001–0018 is in [PROGRAMME.md](PROGRAMME.md) Section 2 and Section 3.2.

## ADR-0001 No Lyra fork
Date 2026-09-17 · Accepted · Fresh C++ project; ~10 Lyra source files copied as patterns (AbilitySet, PawnExtension/Hero components, InputConfig, GameplayMessage usage, CommonUser session flow) with Epic headers. Consequence: no 15–25 GB Lyra content on an 8 GB / 16 GB-free Mac; we own every class.

## ADR-0002 CharacterMovementComponent, not Mover 2.0
Date 2026-09-17 · Accepted · `UDFCharacterMovement` with custom modes (Walk, Sprint, Crouch, ADS, Ladder, Zipline, Launch, Lift, Seated, Downed) behind `IDFMovementIntent`. Consequence: proven CMC+GAS prediction; a Mover swap is a module change.

## ADR-0003 Online Services (OSSv2) EOS behind a facade, with a spike gate
Date 2026-09-17 · Accepted · `UDFOnlineSubsystem` over `OnlineServicesEOS`; a 2-day spike must prove login + invite-only session + P2P relay NetDriver on the UE 5.8.2 launcher build; fallback is OSSv1 `OnlineSubsystemEOS` inside the facade. Record the verdict as a new ADR.

## ADR-0004 Server-authoritative on the listen host; messages for discrete state
Date 2026-09-17 · Accepted · Replicated properties (push model) for continuous state; `FDFMsg_*` gameplay messages mirroring `Events.cs` for discrete state via `ADFEventRelay`; GameplayCues for cosmetic bursts; Iris off. UI/audio/VFX never branch on authority (CI grep).

## ADR-0005 Content numbers live in text (JSON), bootstrapped once from Sim.Core
Date 2026-09-17 · Accepted · `tools/content-export` dumps `sim/Sim.Core/Content` to `unreal/content/json/*.json` once; from then on the JSON (schema-validated) is the hand-edited source of truth; a commandlet writes DataTables; asset bindings live in Primary Data Assets; the C# content files are frozen as the historical spec. Consequence: new content never requires C# edits; no `.uasset` is ever edited for a number.

## ADR-0006 No World Partition; External Actors on; one sublevel per owner
Date 2026-09-17 · Accepted; amended by ADR-0025 (generated sublevels) · `L_<Map>` (persistent), `_Gameplay`, `_Terrain`, `_Art`, `_Lighting`, `_Audio`; OFPA on every level; legacy HLOD only where a belt needs it.

## ADR-0007 Tower rig = static-mesh component chain; skeletal reserved for characters/weapons/vehicles
Date 2026-09-17 · Accepted · `Foot → Yaw → Pitch → Muzzle` static meshes with sockets (`S_Yaw S_Pitch S_Muzzle S_Crown S_Hit S_Stage_<path>`), stage modules as static meshes on `S_Stage_<path>`, aim by `UDFTowerRigComponent` under `DA_Tower_<id>.Rig` limits, Nanite throughout. Skeletal + Control Rig for enemies, boss, heroes, FP arms, weapons, vehicles, animated traps/traversal.

## ADR-0008 Vehicles: Chaos wheeled, driver-client-authoritative with server clamp
Date 2026-09-17 · Accepted · Driver's transform + velocity replicated and clamped (`TopSpeed × 1.15`); seats and ram damage server-owned; non-drivers interpolate; off the critical path. Chaos suspension is required by ADR-0018; the only fallback is a simplified Chaos setup.

## ADR-0009 Native C++ GameplayTags for contracts; per-workstream tag ini for local tags
Date 2026-09-17 · Accepted · `DFGameplayTags.h/.cpp` for every contract tag; `Config/Tags/DF_<ws>.ini` append-only per workstream.

## ADR-0010 Git LFS with single-owner directories and locks
Date 2026-09-17 · Accepted; amended by ADR-0028 (the Mac light clone has no machine) · LFS for `*.uasset *.umap *.ubulk *.uexp *.fbx *.glb *.png *.tga *.exr *.wav *.psd`; one owner directory per binary (`OWNERSHIP.md`, CI-enforced); `git lfs lock` on `.umap` and shared assets; `.lfsconfig fetchexclude` for Megascans and `L_*_Art*` so Mac sessions clone light; Megascans restored from Fab (`restore_fab.py`), never from LFS; 25 GB cap on tracked art.

## ADR-0011 Content root and naming
Date 2026-09-17 · Accepted · `unreal/DeepField/Content/DF/`; prefixes `SM_ SK_ SKEL_ PHYS_ ABP_ AS_ AM_ CR_ M_ MI_ MF_ MPC_ T_ NS_ NE_ BP_ DT_ DA_ WBP_ ST_ MS_ SC_ ATT_ L_ DL_ PCG_ GC_ RT_` (regex in `CONTRACTS/naming.md`).

## ADR-0012 Rendering stack
Date 2026-09-17 · Accepted; amended by ADR-0028 (no Mac tiers) · Nanite for every static mesh and for landscape; skeletal LOD chains until the GPU-box gate decides Nanite skinning; VSM everywhere (cascaded SM on Mac Low); software Lumen floor, hardware Lumen on Windows High; Sky Atmosphere + Volumetric Clouds replace skybox domes; Substrate off; Niagara Fluids P2.

## ADR-0013 Claude Design → Unreal lane
Date 2026-09-17 · Accepted · three.js export → glTF (+`extras.bone/socket/family/smooth`, `variants`) → headless Blender `tools/ue-bridge/blender/convert.py` (cm, Z-up, +X, sockets, rigid weights to family armature, auto-UV, AO/curvature/ID bakes, texture dedupe) → FBX + loose textures → Interchange pipelines via `tools/ue-bridge/ue/import_drop.py`.

## ADR-0014 Audio sources
Date 2026-09-17 · Accepted · MetaSounds; CC0/open-source libraries and generated audio only; music as procedural MetaSound stems or CC0 stems; every source logged in `Content/DF/Audio/LICENSES.md`.

## ADR-0015 Launch scope
Date 2026-09-17 · Accepted · 6 sectors (foundry, switchyard, spire, toaster redesigned + Sluice + Crown); full proposed roster (PROGRAMME.md B§4); Versus post-launch; Level 1 invite-only with L2/L3 seams (`IDFSessionBackend`, `IDFProgressionProvider`, `DF.Team.*`).

## ADR-0016 Machine plan
Date 2026-09-17 · Superseded by ADR-0023 and ADR-0028 · Mac M1 8 GB only until a Windows GPU workstation exists; external NVMe SSD is a P0 blocker; art sublevels fetch-excluded on the Mac; render/perf/Windows lanes recorded as unverified in the ledger until the box exists; two editor-heavy slots (Section 6.7).

## ADR-0017 Tint/palette is a parameter contract
Date 2026-09-17 · Accepted · `DA_Palette` from `docs/palette.json`; `UDFTintComponent` writes Custom Primitive Data / MID params by the fixed names in `CONTRACTS/palette.md`; no colour literals in code.

## ADR-0018 Real terrain everywhere; every map is a layout redesign
Date 2026-09-17 · Accepted · Every map is a Landscape with genuine relief, flat only where a place would be flat. Enemies, towers, vehicles, projectiles, scrap, VFX and validation are terrain-aware (PROGRAMME.md Section 3.2). Terrain is text-authored (`unreal/content/terrain/<map>.terrain.json` → `build_heightmap.py` → heightmap + masks); hand sculpting is a recorded polish delta. Line of sight is real (terrain and static world block it); the validator computes coverage with traces. The existing `level.json` is the brief, not the output. Supersedes the flat-world rules of `docs/MAP-AUTHORING.md` §4 where they conflict (see `CONTRACTS/map-authoring-3d.md`).

## ADR-0019 Message bus is ours; GameplayMessageRouter is not in the engine
Date 2026-09-19 · Accepted · The UE 5.8.2 launcher build ships neither `GameplayMessageRouter` nor `CommonUser` (both are Lyra plugins). `UDFMessageBus` (DFCore, ~120 lines, `FInstancedStruct` payloads, parent-tag fan-out) is the implementation of C15's transport; Lyra remains a pattern source only (ADR-0001). Resolves risk R9.

## ADR-0020 Installed-build constraints: BuildSettingsVersion.V7, no `.inl` native tags
Date 2026-09-19 · Accepted · Targets must use `BuildSettingsVersion.V7` (the installed engine's build environment refuses V5 overrides); native gameplay tags are defined by expanding `UE_DEFINE_GAMEPLAY_TAG` by hand in `DFGameplayTags.cpp` because the macro's static-assert forbids X-macro lists in `.inl` files; generated module log categories are `LogDF<Module>` (the engine already owns `LogAudio` etc.).

## ADR-0021 Working copies: the SSD clone is the Unreal working copy
Date 2026-09-19 · Superseded by ADR-0028 · `/Volumes/Toshiba/Deepfield-Unreal/deepfield-3d` (a clone of `unreal/main`, APFS, 3.6 TB) is where every editor-heavy session works; DDC at `/Volumes/Toshiba/Deepfield-Unreal/DDC` (`DefaultEngine.ini` points at `%GAMEDIR%../../../DDC`). Worktrees for parallel sessions are created from this clone on the SSD (`git worktree add /Volumes/Toshiba/Deepfield-Unreal/wt-ws-NN ws/NN-slug/topic`). The internal-disk checkout stays for text-only work. Supplements ADR-0016.

## ADR-0022 OSSv2 spike verdict: keep OnlineServicesEOS behind UDFOnlineSubsystem
Date 2026-09-19 · Accepted 2026-09-19 by INT (landed with ws/11-online/ossv2-spike, 9a450a6; the EOS block in rfcs/needs-int-eos-config.md is applied when the portal product exists) · Context: ADR-0003 gated the online layer on a spike proving OSSv2 on the 5.8.2 launcher build. Verified on the Mac: the launcher build ships `OnlineServicesEOS` (Auth: Auto/ExchangeCode/Developer/PersistentAuth/AccountPortal, Presence, Social), `OnlineServicesEOSGS` (Lobbies with schema-filtered search, invite/kick/attributes/join-policy, TitleFile, PlayerSanctions), `SocketSubsystemEOS` (`NetDriverEOS`, `RelayControl=ForceRelays`, `[EOS:<puid>]` connect strings, Ip passthrough for non-EOS URLs) and EOS SDK 1.19.1 for Mac and Win64; `UDFOnlineSubsystem` builds against `UE::Online` and `DF.Online.NullLogin`/`NullSession` prove login → lobby → join code → approval → handshake headless on the Null services. Decision: OSSv2 is the provider behind the facade; Null is the test provider; switching to EOS is config only (`unreal/PLAN/rfcs/needs-int-eos-config.md`); lobby search keys live on the hard-coded `LobbyBase` schema (only a base schema may be `Searchable`). Consequences: no OSSv1 code; the two-machine relay test is the remaining gate and runs when the portal product exists; a relay failure would be absorbed inside `IDFSessionBackend` and the lobby plumbing, never above C13/C14. Links: `workstreams/ws-11-online.md` "Spike verdict", `CONTRACTS/online.md`.

## ADR-0023 The GPU workstation exists; the Mac keeps the 8 GB floor
Date 2026-09-24 · Accepted; its Mac half superseded by ADR-0028 · Supersedes the "Mac-only initially" half of ADR-0016. The Windows GPU
workstation is in hand. The split: **the Mac** runs every gameplay C++ workstream, logic tests under
`-nullrhi`, the content pipeline, graybox levels and Mac packaging, and remains the 8 GB memory floor
(if it runs there it runs anywhere); **the GPU box** runs what the Mac has never been able to prove —
Nanite/Lumen/VSM verification, the Megascans-heavy `L_<Map>_Art` sublevels (still `fetchexclude`d on
the Mac), Windows packaging and the EOS overlay, perf budgets, Gauntlet, the endless soak, the
Nanite-skeletal gate, and the self-hosted CI runner. Bring-up is `unreal/Build/windows-bringup.md`,
written on the Mac and not yet executed; whoever runs it corrects it in place. The render, perf and
Windows lanes stay marked unverified in the ledger until that box has actually produced a result —
owning the hardware is not the same as having measured anything on it.

## ADR-0024 Match state is a host of components; DFMatch and the phase belong to WS-28
Date 2026-09-25 · Accepted by INT (ruling R1, `rfcs/needs-int-rulings-2026-09-25.md`) · Context: §3 places GameMode/GameState/PlayerState/EventRelay/campaign in DFMatch (layer 4), but §5 gave no workstream DFMatch beyond WS-00's skeleton, so WS-12's real view-model feed, WS-05's `DF.Message.Wave*` and WS-11's PreLogin handshake all waited on an owner that did not exist. Decision: (1) a new critical-path workstream, **WS-28 Match flow**, owns `Source/DFMatch/**`: `ADFGameMode`, `ADFMatchState`, `ADFPlayerState`, `ADFPlayerController` (the Server RPC surface, one RPC per `Commands.cs` command), `ADFEventRelay`, the phase machine, end conditions, the lobby gate, endless, the campaign chain and the host match record. (2) **A domain's replicated fields live in a ModularGameplay state component written by the domain's owner in its own module**, and the DFMatch actors host it: economy (money, lives, team scrap, bounty) WS-06; lane and mutable edge states WS-09; faction and level WS-07; personal scrap and builds WS-06; downed, revive and seat WS-03; phase, wave, timer, threat, endless, seats and stats stay on the actors (WS-28). Layering forces this: the domains sit below DFMatch and cannot name its classes, while DFMatch can host theirs. (3) The phase machine ports `Step.cs` `UpdateWaves` + `CheckEndState` exactly, lives-first (a last enemy that leaks the core to zero is a Defeat); the wave director releases spawns and reports cleared, WS-28 sends the `Wave*` messages with `DescribeWave`'s payload. Consequences: a domain adds a replicated match field by writing a component and a one-line attach PR to DFMatch; no workstream edits another's replication; C12 is unchanged (DFUI sits above all of them). If WS-28 is unclaimed for one INT cycle, INT lands the shells and hands them over. Links: `workstreams/ws-28-match-flow.md`, PROGRAMME.md §4.2 and §5.2.

## ADR-0025 Generated sublevels keep their actors in the .umap (amends ADR-0006)
Date 2026-09-25 · Accepted by INT (ruling R5) · Context: ADR-0006 put External Actors (OFPA) on every level. WS-09's `-run=DFLevelImport` (`L_<Map>_Gameplay`) and WS-30's `-run=DFTerrainImport` (`L_<Map>_Terrain`, and the placeholder `L_<Map>` it seeds) both save with actors inside the `.umap`, for the same four reasons: a generated level is rewritten wholesale by a commandlet, so external-actor packages would churn a new file per actor per run; `SaveWorld`'s stale-external-package cleanup reaches a dialog an `-unattended` commandlet cannot answer; no `__ExternalActors__` glob exists, so the first external tree would fail `ownership-check.py` for every owner; and ADR-0010's lock model is one lockable file per level. Decision: **a sublevel a commandlet writes from text keeps its actors in the `.umap`** (`L_<Map>_Gameplay`, `L_<Map>_Terrain`); **a hand-authored sublevel uses External Actors** (`L_<Map>_Art`, `_Lighting`, `_Audio`, and `L_<Map>` once a WS-10x owns it), and INT adds its `Content/__ExternalActors__/DF/Maps/<Map>/<Level>/**` (+ `__ExternalObjects__`) glob to the owner's row in `OWNERSHIP.md` when the first one is created. Consequences: the importers keep `bUseExternalActors=false`; two people never hand-edit a generated sublevel (they edit the text and re-import), which is why the one-file lock is enough there; the hand-authored levels, where concurrent editing actually happens, get per-actor files.

## ADR-0026 Join codes work from a chat message; the host still admits every stranger
Date 2026-09-25 · Accepted by INT (ruling R4, decided on player experience) · Context: EOS `INVITEONLY` lobbies cannot be found by search, so a join code alone could never reach one (WS-11). The alternative — the code travels only inside a friend invite — makes the code pointless for the case players actually use it for: pasting it into a chat for someone who is not (yet) on the host's friends list. Decision: (1) **The host shares a code on purpose** ("Share code" in the lobby / pause menu). While a shared code is live the lobby is `PublicAdvertised` (`ModifyLobbyJoinPolicy`); when it expires (10 min) the lobby returns to `InvitationOnly`. After each admission the code rotates and the share panel shows the new one, still live for its remaining lifetime. Works in the lobby and mid-match (join-in-progress). (2) **The host's friends are admitted without a prompt** when they arrive by code — they could have been invited directly, so a prompt only adds friction. (3) **Everyone else needs the host's approval**, and approval is **non-modal**: a toast with the joiner's name and "hold [key] to admit" that never pauses play or steals input; an unanswered request waits in the lobby layer, and an admitted player spawns at the next intermission or through join-in-progress. Why approval stays: the join code is a *searchable* lobby attribute, and searchable attributes are returned with search results, so while a lobby is advertised anyone listing Deep Field lobbies can read its live code. Approval (PreLogin refuses `notInvited`, now wired through the C14 seams, R3) is what keeps an advertised lobby safe, so it never goes away for non-friends. Consequences: WS-11 adds the share/advertise state (the rotator today is always live), the policy flip, friend auto-admission, and `DF.Online.JoinCodePolicyReverts` on Null (the real proof is step (5) of its EOS checklist); WS-12 builds the share panel and the non-modal admit toast. Links: `rfcs/needs-int-rulings-2026-09-25.md` R4, ADR-0022, C14.

## ADR-0027 A generated artefact is verified by the data it was generated from, never by its bytes
Date 2026-09-25 · Accepted by INT (ruling R6) · Context: WS-30's DoD asked for the Foundry Landscape to regenerate "byte-identically", and WS-09 found that re-saving an unchanged `L_Foundry_Gameplay` is not byte-stable (while `DA_LaneGraph_Foundry` is). Package bytes carry per-save GUIDs the generators do not control, so byte-equality tests the engine's serializer, not our input. PROGRAMME.md §5.4 already reads "the Landscape data deterministically (package GUIDs excepted)"; the registry and the WS-30 file had paraphrased it as "byte-identically". Decision: every generator's determinism check compares the data the binary was generated from — for terrain the heightmap samples, landscape GUID, bounds and probe samples; for a gameplay level the actor set by stable id with each actor's transform and properties; for a DataAsset its properties. This is ci.md's "something must prove the artefact came from the input", with a data fingerprint as the proof. Consequences: WS-30's data-identity result meets its DoD; reimport-in-place (a byte-stable `.umap`) stays a worthwhile follow-up and becomes required only for a CI job that re-imports on every run and must leave the tree clean; WS-09's re-save item becomes "diff the actor set, not the bytes".

## ADR-0028 The Mac is retired; the Windows GPU workstation is the only engine machine and Windows the only platform
Date 2026-09-25 · Accepted (project owner) · Supersedes ADR-0016, ADR-0021 and the Mac half of ADR-0023;
amends ADR-0010 and ADR-0012. Context: ADR-0016 made the M1 Mac the only engine machine, ADR-0021 put
the working copy on its external SSD, and ADR-0023 split the work between the Mac and the newly arrived
GPU box. The owner has retired the Mac as a development machine and dropped macOS as a shipping
platform. Decision: **the Windows GPU workstation runs everything** — every workstream, build, test,
import, cook and package, and the self-hosted CI runner. **Windows is the only target platform.**
Consequences:
- The 8 GB memory floor, the two editor-heavy slots (PROGRAMME.md §6.7, `EDITOR-SLOTS.md`), the
  machine-wide editor lock and the `/Volumes/Toshiba` working-copy paths no longer bind anyone.
  `editor_heavy` stays in workstream frontmatter as information and gates nothing.
- ADR-0012 loses its Mac tiers: device profiles are `Windows_High` and `Windows_Medium` only; VSM
  everywhere, software Lumen on Medium, hardware Lumen on High. Appendix C§8's Mac column is gone.
- ADR-0010's `fetchexclude` light clone existed for the Mac. No machine needs it; until `.lfsconfig`
  drops it, a clone overrides it with `git config lfs.fetchexclude ""`.
- G2's "a packaged Mac build runs it" becomes a packaged Windows build; G4b's budgets are Windows only.
- The zsh scripts in `unreal/Build/` (`test.sh`, `pr-check.sh`, `int-merge.sh`, `ci-local.sh`,
  `editor-lock.sh`, `smoke-listen.sh`) are Mac-pathed and now run nowhere. Their rules still hold —
  the landing gate is `DF_GATE_FILTER`, a verdict comes from the JSON report, never test a binary
  older than its source. On Windows `unreal/Build/deepfield.ps1` (`deepfield build|test|check|…`)
  already keeps `test.sh`'s rules; the rest are followed by hand per `unreal/README.md` until WS-15
  ports them onto that script. The Python checks are unaffected.
- `.github/workflows/unreal-mac.yml` never ran (no runner was registered) and has no machine; WS-15
  replaces it with a Windows lane on the box's runner.
- Follow-ups outside the docs (INT/WS-15): remove `Mac` from `TargetPlatforms` in `DeepField.uproject`;
  drop the `fetchexclude` from `.lfsconfig` (and `deepfield.ps1`'s `-LightClone`); retire
  `unreal-mac.yml` (here and on `main`); port `pr-check`/`ci-local`/`int-merge` onto `deepfield.ps1`;
  retarget the Mac wording in `check-test-coverage.py`'s `EXCLUDED` reasons and in
  `machine-inventory.ps1`'s readiness labels (which then regenerates `machines/windows-gpu.md`).
- ADR-0023's last sentence stands: the render, perf and Windows lanes stay unverified until the box
  has produced a result. History (digests, session logs, earlier ADRs, RFCs) is left as written; where
  it says "the Mac", it records what happened then.
Links: ADR-0016, ADR-0021, ADR-0023, `unreal/README.md`, `unreal/Build/windows-bringup.md`, `CONTRACTS/ci.md`.
