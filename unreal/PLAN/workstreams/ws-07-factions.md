---
ws: 07
slug: factions
title: Factions & abilities
state: unclaimed
owner: 
claimed_at: 
lease_expires: 
branch: 
last_commit: 
editor_heavy: false
phase: P2-P4
size: M
critical: true
blocked_on: 
---
# WS-07 — Factions & abilities

## Scope / DoD
**Scope.** 5 predicted GAs, passives, level curve, lobby uniqueness, XP from host match record (cap 60), level-5 signatures, Specter weak-point highlight, resonance combos.

**Definition of done.** Abilities land at aim point; per-level factors match Factions.cs; combo ICD shared with Overclock Surge.

**Spec.** A1 factions, B§1.7, B§2.3, B§3.10 (in `unreal/PLAN/PROGRAMME.md`). Size M, phase P2-P4.

## Contracts I consume
- C4
- C5
- C8

## Contracts / interfaces I provide
- GA_Faction_*

## Interfaces I changed
<!-- dated list: what, RFC #, dependents notified -->

## Needs INT
<!-- e.g. "add plugin X to .uproject" -->

## Open questions

## Assignment from INT (2026-09-25) — audited, with rulings

**WS-07 is the last unclaimed critical-path workstream.** WS-03, 04, 05, 10a, 12, 19 and 28 are all
active. G2 depends on this one through exactly one criterion — the *ability* clause of `DF.Net.Feel` at
150 ms (predicted cue ≤1 frame, server confirm ≤400 ms) — and the DAG has only `WS-02 → WS-07 → G2`.

Five readers audited every source against the tree before this was written. **Half of your passive work
already exists, none of your ability work does, and thirteen documents contradict each other.** The
rulings below settle the blocking ones so you do not stall; argue back on any of them with evidence.

### Claim it yourself
§6.2: edit only this file's frontmatter (`state: active`, your own handle in `owner`, `claimed_at`,
`lease_expires` +24 h, `branch: ws/07-factions/<topic>`), push that one-file commit to `unreal/main`.

### This needs a machine with the engine
It is GAS C++ and it must be compiled and tested. Since `fcc1e1d` — the last commit anything compiled —
**43+ commits and ~9,100 lines of C++ have landed unverified**, zero runners are registered, and
`unreal-win` reports *skipped*. Land through `unreal/Build/int-merge.sh`, never the merge button
(`CONTRACTS/ci.md`); it refuses at the build step, which is the only step that would have caught the
one error that broke the trunk. If you have no engine, take a content-and-schema slice (the two
contract fixes below are pure JSON) and leave the abilities.

### Already built — do not rebuild it
- **All five passives, working**: `UDFGE_FactionPassive` plus five subclasses, magnitudes fed from
  `balance.json` at spec time through `UDFMMC_BalanceDial` — `forgeBuildDiscount` 0.9,
  `emberBurnDurationFactor` 1.3, `tempestRateFactor` 1.12, `glacierChilledDamageFactor` 1.25, all four
  present in content and matching `Balance.cs`. Tempest and Glacier write `UDFCombatSet.RateFactor` /
  `ChilledBonus` by MultiplyCompound; the other three are tag-only. `ClassForFaction` / `AllClasses` exist.
  WS-02 left "WS-07 grants the passives" open — **granting is your job, authoring them is done.**
- **All 15 `DF.Faction.*` / `DF.Ability.*` / `DF.Ability.Combo.*` tags are NATIVE** in
  `DFGameplayTagList.inl`, so the ini-registration hazard that cost WS-02 its whole cue layer does not
  apply here and no tag work is needed.
- `UDFGameplayAbility` (LocalPredicted, InstancedPerActor, ReplicateNo) and `UDFAbilitySet` exist.
- `FDFDetMath::PowInt` is in DFCore (ruling R2), so the level curve ports bit for bit.
- `factions.json` matches `Factions.cs` **field-for-field and value-for-value** (verified), its schema
  exists, and `validate-content-json.py` validates it — so a content edit here is verifiable with no engine.
  `DT_Factions` is produced by `-run=DFContentImport` and committed.

### Absent — this is the work
No `GA_Faction_*` and nothing for overdrive, ignitionWave, chainSurge, cryoField or revealPulse. **No cue
assets and no per-ability cue leaves**, so G2's predicted-cue half currently has nothing to fire — that is
on the critical path, not a polish item. No combo table: `FDFComboRow` is named in the C2 contract but
absent from `DFContentRows.h`, and there is no `combos.json`. No level-5 signature ids. No numbers
anywhere for Specter's outline radius, the +0.2 weak-point bonus, or the 10 s ICD beyond PROGRAMME prose.
`gas.md`'s `ActivationOwnedTags` convention appears nowhere in the tree.

### Port these faithfully — the sim is stranger than the plan's summary
All five abilities live in one `switch` in `ApplyUseAbility` (`Step.cs:849-914`). Quirks you must keep:
- **`chainSurge` does not chain.** Flat AoE, one hit per enemy in radius, damage `6` then Shock.
- **`revealPulse` ignores its radius** — map-wide, 3 s, deliberately (radius is 0). It is a no-op on
  non-stealth enemies; on stealth it removes the speed bonus and makes them targetable.
- **`overdrive` targets towers within radius of the PLAYER, not the aim point**, and **assigns**
  `BuffFactor` rather than accumulating. Traps and barricades also carry a `BuffFactor` nothing reads.
- **Ember's passive applies before the refresh branch, keyed on the APPLIER's faction** — it stretches any
  burn that player lands, not just ignitionWave.
- **Tempest's "reloadSpeed" scales fire rate and melee swing, not reload** (`Step.cs:548`, `:600`).
- **Glacier's bonus keys on the Movement channel being active**, not on chill's id — so tar counts.
- **Forge's discount truncates to int after the multiply.**
- `FactionDef.Passive` **has no reader anywhere**: all five strings are labels, and behaviour runs off
  hard-coded `FactionId` comparisons at five call sites. Do not build a dispatch on the string.
- **The sim has zero tests asserting any ability's effect.** You are writing the first ones, not porting
  goldens — so `CONTRACTS/ci.md`'s falsification rule is the whole safety net: for each test, name the
  line you would revert to turn it red, and actually do it once.

### INT rulings on the contradictions
1. **MaxLevel is 5, not 20.** `UDFLocalProgressionProvider::MaxLevel` is 20 **with a live test asserting
   it** — both are wrong against `Factions.cs`. The sim is the spec. WS-11 owns the provider: send it a
   contract-append PR fixing the constant and the test, and say so here.
2. **The level curve is additive, as the sim, not multiplicative as A1 says.** Cooldown
   `PowInt(0.94, L-1)`; radius `1 + 0.06(L-1)`; magnitude `1 + 0.05(L-1)`. A1's "radius ×1.06, magnitude
   ×1.05" diverges from L=3 and is **wrong**; correct A1 in the same PR, the way RFC-0002 corrected
   `gas.md`. Cite `Factions.cs`.
3. **`MagnitudeFactor` is a dead dial for ember, glacier and specter.** `ApplyStatus` takes no magnitude,
   so three of five factions have **no per-level power scaling at all**. Port that faithfully — do not
   invent scaling, which would be a balance change nobody asked for — and raise it as a design question
   for WS-27 and the human in your Open questions.
4. **Abilities do not inherit source factors.** The sim applies build factors at the weapon call site
   (`Step.cs:558`) and none inside `Damage()`; `chainSurge` passes its magnitude straight through. So set
   `bAppliesSourceFactors = false`, the same choice WS-02 made for DoTs and reaction bursts, and pin it
   with a test — an Overclocked hero's `chainSurge` must hit for 6, not 7.2.
5. **The aim point is server-authoritative.** Nothing specifies it today. Per ADR-0004 and B§5.3, the
   client sends its aim with the activation and **the server re-traces and clamps**; never trust a
   client-supplied world position. Mirror the shots rule's spirit (`§3.3`), and record the trace length
   you choose as an Open question, since no document states one.
6. **Specter's weak-point outline is WS-18's, not yours.** The registry gives WS-18 "Weak points +
   Specter passive". You own `revealPulse` and the level-5 signature hook; the per-viewer outline, the
   40 m radius and the +0.2 bonus are WS-18's. That collision is now resolved — do not build it.
7. **`UDFCombatSet` is not on the hero**, which means Tempest's `RateFactor` and Glacier's `ChilledBonus`
   have **no attribute to write** today. The hero registers only `UDFHealthSet` and `UDFHeroSet`. WS-03
   owns the hero's ASC: ask WS-03 (active) to register `UDFCombatSet`, and do not add it yourself. **This
   blocks two of the five passives actually doing anything — check it first.**
8. **Overdrive goes through a seam WS-04 provides, not tower internals.** The sim writes
   `tower.BuffTimer`/`BuffFactor` directly; `FDFTowerMath::EffectiveRate` already takes a `BuffFactor`.
   WS-04 is active — agree with it on a rate-factor an ability can modify, and keep the ability on your
   side of the boundary.
9. **`FDFFactionRow.Signature` is a live C2 break.** The header declares it; `factions.schema.json` is
   `additionalProperties: false` without it. Under C2 an optional field with a default is an **append**,
   so add `signature` to the schema (WS-01's file, contract-append). Pure JSON, verifiable with no engine
   — a good first commit.
10. **XP: the sim governs what exists; the plan's additions arrive with the systems that emit them.**
    Today: +2 per reaction, +1 per kill, +5 per revive, **no cap** (`Step.cs`). The plan's +3 combo, +10
    boss phase and +4 cache belong to the systems that produce those events, so **only the combo award is
    yours**. Three documents give three different cap semantics (B§1.7 "cap 60/match", B§5 "cap 60", the
    provider's "per source"); I am **not** ruling on that — it is a design decision for the human, and
    100 XP/level × 5 levels makes the reading matter. Put it at the top of your Open questions.
11. **Lobby faction uniqueness has no implementation and no spec** beyond `factionTaken` as an example
    refusal string. Enforce it host-side on `SetFaction`, refuse with that reason per `messages.md`, and
    coordinate with WS-11 (paused) who owns the lobby.
12. **Combos need a content home before code.** `FDFComboRow` is contracted but absent. Add the row
    struct (C2 append) and `combos.json` with its schema, then build the resolver. The 10 s ICD is shared
    with WS-16's Overclock Surge, which is **unclaimed** — so define the ICD where you can own it and
    leave WS-16 a documented hook rather than waiting.

## Session log
<!-- append-only: date · session · what landed · what's next -->
- 2026-09-25 · INT · Unblocked by today's rulings: `FDFDetMath::PowInt` is in DFCore (`Determinism/DFDetMath.h`, R2), so the level curve (`CooldownFactor = PowInt(0.94, L-1)`, `RadiusFactor`, `MagnitudeFactor`, `LevelForXp`) can be ported bit for bit into DFGameplay/Factions and replace WS-11's placeholder `UDFLocalProgressionProvider::LevelForXp`. Faction and level replicate from a **`UDFFactionStateComponent`** you write and WS-28 attaches to `ADFPlayerState` (ADR-0024).
