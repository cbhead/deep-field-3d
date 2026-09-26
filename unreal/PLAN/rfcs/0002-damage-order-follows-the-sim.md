# RFC-0002 — C4's damage order follows Step.cs (the contract text was wrong)

- **Contract:** C4 (GAS attribute sets and the damage execution), rule **R**.
- **State:** **Landed** 2026-09-21 by INT, with `ws/02-gameplay-core/gas`.
- **Author:** INT, from the WS-02 review (one confirmed blocker: "C4 damage order changed in code; gas.md still states the old order and no RFC exists").

## Motivation

`CONTRACTS/gas.md` stated the damage execution as:

> source factors → front-arc/rear factor → **flat armor** → **vulnerability** (`DamageTakenFactor`, mark) → shield then health

`sim/Sim.Core/Step.cs:2036-2053` — the frozen spec every rule is ported from — orders it the other way,
and carries two effects the contract text never mentioned:

> arc factor → **vulnerability** (mark) → **shred front-arc leak ×1.35** (an arc-armored target under
> shred leaks regardless of aspect) → **flat armor, floored at 0.5** → shield then health

This is not a design change anyone made. The contract was written from memory during the P1 freeze and
**contradicted the sim it exists to encode**; WS-02 implemented the sim, correctly, and the text stayed
wrong. It is filed as an RFC anyway because C4 is rule-R and the *written* rule is changing — a reader
who trusted `gas.md` was being misled, which is exactly what the RFC procedure exists to surface.

## Why the order is load-bearing

Mark before armor means the mark amplifies the hit and armor then takes its flat bite out of the
amplified number; the other order taxes first and amplifies the remainder. They differ on every armored
hit, by `DamageTakenFactor × FlatArmor`:

> 10 damage, marked (×1.25), Ram (FlatArmor 2, unshredded).
> As the sim and the code: `max(0.5, 10 × 1.25 − 2)` = **10.5**.
> As `gas.md` stated: `(10 − 2) × 1.25` = **10.0**.

Any workstream that sized a weapon or a tower against the written contract was off by that much on
every armored target — WS-03, WS-04, WS-05 and WS-06 all consume it.

## Exact change

`CONTRACTS/gas.md`, the "Damage execution" paragraph: replaced with the sim's order, naming the shred
front-arc leak and the post-armor floor, and citing `Step.cs`. The sentence "the execution has no
literals" is replaced by the truth — three values live as `constexpr` defaults in
`DFDamageMath.h` (`RearThresholdDegrees` 150, `ShredFrontArcLeakFactor` 1.35, `PostArmorDamageFloor`
0.5) pending their `balance.json` dials, each carrying the `Step.cs`/`ContentTypes.cs` line it comes
from.

**No code changes.** `FDFDamageMath::Compute` already implements this; `DF.Unit.Damage.OrderMatchesSim`
already pins it against the sim's numbers.

## Dependents

WS-03, WS-04, WS-05, WS-06, WS-07, WS-21, WS-25 — everything that deals or receives damage. None needs
a code change: the implementation was always the sim's, so anyone who built against the *code* is
correct and anyone who built against the *text* has a number to recheck. No "Needs rebase" is issued;
the digest carries the worked example above instead, which is the actionable part.

## Follow-up (WS-27, recorded in its file)

The three literals become `balance.json` dials — `shredFrontArcLeakFactor`, `postArmorDamageFloor`,
`rearThresholdDegrees` — so C4's "every number comes from the content rows" becomes true again rather
than nearly true. Until then the constants are the live values and are documented as such.

## Lesson

A contract written from memory next to a frozen reference implementation will drift from it silently,
and the tests cannot catch it because they test the code. When a contract claims to encode a spec,
**cite the spec's file and line in the contract text** — this RFC does, and `gas.md` now does.
