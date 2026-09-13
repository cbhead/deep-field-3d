# VFX — forward manifest for Claude Design

**Status:** open asks, written 2026-09-13 against `main`.
**Audience:** Claude Design. This is a request list; nothing in the delivered
art has been altered.
**Scope note:** **no backend work is planned or needed, and none is asked for.**
The effects layer now drives every VFX file in the drop except two, and both
of those wait on a *system* rather than on art. What this document asks for is
three effects the code already calls by name and gets nothing back for.
**Read first:** [docs/ART-INTEGRATION.md](ART-INTEGRATION.md) for the delivery
contract and the `_spin` / `_pulse` / `_rise` / `_fade` convention;
[FORWARD-MANIFEST-hero.md](FORWARD-MANIFEST-hero.md) is the open one on models.

---

## 1. Where this actually stands

The 2026-09-07 drop shipped 27 effects that nothing drew, and
`docs/ASSET-USAGE.md` carried them as one row reading *"No status/ability/
reaction VFX system yet"*. There is one now. Of those 27:

| | |
|---|---|
| Drawn by the game today | **25** |
| Waiting on a system rather than on art | **2** — `vfx_overclock_link`, `vfx_lanewash` |

The two are not oversights and are not asks. `vfx_overclock_link` is a conduit
from a pylon to each tower it feeds, and the Overclock tower does not exist in
the sim — its chassis and thirty stage modules are delivered and waiting with
it. `vfx_lanewash` is the floodgate's panic button, and `shared_floodgate` is
one of the map elements the M5 mutable-map work will drive. Both are already
recorded against those milestones; neither needs anything new drawn.

The six projectile tier variants (`proj_lance_bolt_t2` and friends) are drawn
now too, on the rule projectiles.js states: **the tier follows the damage
path, level 7 buys T2 and level 10 buys T3.**

So are `proj_arc_beam` and `proj_filament_beam`, which had never been on screen
at all — not because of anything wrong with the models, but because neither
tower creates a projectile in the sim and the client only drew projectiles.
Both now follow their own notes: a unit-length streak along −Z, turned at the
target and stretched in Z to the distance. The Arc's second copy for the hop is
there too. Two nodes design named on them are **not** driven yet and are worth
knowing about rather than being quietly ignored: `filament_beam_ramp` is
supposed to brighten from 0.3 to 2.0 emissive as the beam's heat ramps, and the
sim tracks exactly that number (`Tower.RampSeconds`) — it is not wired because
driving emissive means duplicating the material per instance, which is the same
call recorded under `_fade` below. If the ramp is worth seeing, say so and it
is a small change on this side.

## 2. What is asked for

Three names. The client calls all three today and draws nothing, which is the
honest state rather than a placeholder: they are in
`docs/asset-manifest.tsv` as requested-and-missing and in
`docs/DESIGN-BRIEF.md` §3.7 under the same heading. Drop the files in and they
appear — no code change.

### 2.1 `vfx_reaction_corrode.glb` — the third reaction

The reaction table has had three rows since M3 and art for two of them. Thermal
Shock (chill + burn) and Flash Freeze (chill + shock) are the showpieces;
**Corrode is poison + shred**, and it is the one a team brings to something
that is both armoured and durable.

| | |
|---|---|
| Inputs | `poison` + `shred`, both consumed |
| Burst | 10% of the victim's max HP |
| Emits | nothing — the reaction is the whole payload |
| Life | ~0.6 s, matching `vfx_reaction_thermalshock` |
| Size | ~2.6 m, authored around a 1.8 m body |
| Swatch | `#7fe65a` venom against `#cfd7e4` steel |

What it has to read as, at 30 m, in one frame: **armour flaking off a body that
is already rotting.** The two inputs have to both be legible or the combo
teaches nothing — the same rule Thermal Shock's note states, where ember and
frost are both present on purpose. Suggested groups, following the set's own
convention: steel plate chips flung outward on `_pulse` (`vfx_status_shred`
already has the vocabulary), venom sheeting into the seams they leave, a
corroded ground ring, and a sickly haze shell on `_fade`.

### 2.2 `vfx_ability_cryofield.glb` — Glacier's ability

Glacier is one of the two M3 factions and the only one of the five whose
ability has never been drawn.

| | |
|---|---|
| Radius | 8 m at level 1, ×1.06 per faction level |
| Applies | `chill` to everything inside, at the aim point |
| Life | ~1.1 s, matching `vfx_ability_ignitionwave` |
| Size | 16 m |
| Swatch | `#4fc0e8` |

It is deliberately the mirror of Ember's Ignition Wave, and the pair should
read as a pair: an expanding ring at the radius on `_pulse`, a directional
splash from the hero toward the aim point along −Z, a frozen ground disc, and
crystals rising on `_rise` where Ember has embers. The identity line worth
holding on to is in the Singularity's own note — **the built tower does this
passively in one small sphere and Glacier does it on demand, anywhere** — so
this wants to feel aimed, not placed.

### 2.3 `vfx_ability_revealpulse.glb` — Specter's ability

| | |
|---|---|
| Radius | map-wide, deliberately: the def carries 0 because there is no radius |
| Applies | `reveal` to everything not burrowed |
| Life | ~1.4 s |
| Swatch | `#7fe65a` |

**The Detector's sweep stands in for it today**, scaled up four times, and that
is close enough to be worth saying out loud: it reads as the right picture at
the wrong scale. What it is missing is the thing that makes the ability
different from the tower — the tower answers *"cover this approach"* and
lasts, the ability answers *"where is it right now"* and does not. A single
horizon-wide wavefront that passes through the player and keeps going would say
that; a ring centred on them says the opposite.

## 3. What the code already gives you

Every file in `game/assets/vfx/` is loaded by name, hung on the client's
effects layer, and driven through four named sub-groups. Nothing here is new —
it is the same contract vfx.js was authored against — but it is now all
actually driven, which it was not before:

| suffix | what the client does with it |
|---|---|
| `_spin` | rotates about Y — 6 rad/s on a burst, 2.5 on a held effect |
| `_pulse` | breathes ±18%, **or** is scaled 0 → 1 as a gauge when the client has a progress to show — the revive ring grows with the revive clock. Design's note asks for the ring's *arc* to be the progress; the client scales the group instead, because a swept arc means rebuilding the geometry per frame. If a future effect wants a true sweep, name the segments and they can be culled the way the upgrade chevrons are |
| `_rise` | lifts; **loops** on a held effect so a burn keeps licking upward instead of walking off the top of the enemy, and is driven **downward** for `vfx_status_poison`, whose beads fall |
| `_fade` | scales out over the life of a burst — not alpha, because alpha means a material duplicated per instance and a shell shrinking into its own burst reads the same at the speed these live at |

Three conventions beyond that are worth keeping, because the client relies on
all three:

- **Author it the way it stands.** A ground ring lies in XZ, a column runs up
  Y, and the client applies no rotation at all to any of it. Only things that
  genuinely aim get turned: a muzzle flash, a tracer, a beam — those are the
  ones authored along −Z. `vfx_wave_start` is the one exception and it is
  design's own: its ground chevrons run along **+X**, so that effect gets a yaw
  and nothing else.
- **A unit-length streak along −Z is scaled, not repeated.** `proj_arc_beam`
  and `proj_filament_beam` both say so in their notes and both work exactly
  that way now — the client turns the model at the target and stretches Z to
  the distance, leaving X and Y alone so the beam does not fatten with range.

- **A named sub-group can be lifted out on its own.** `overdrive_tower_crown`
  is instanced separately onto every tower an Overdrive reached. If an effect
  has a per-target piece, name it and the client can place it.
- **Tiers can be culled.** `vfx_tower_upgrade` ships three chevron tiers as
  `up_chevron<tier>_<k>` and the client shows one, two or three of them by the
  level bought. The note asking for that is design's own, and it works.

## 4. How to check it

```sh
./play --headless -- --shot foundry /tmp/vfx.txt vfx
./play -- --shot foundry /tmp/vfx vfx     # windowed: writes /tmp/vfx.png beside it
```

Fires one of every effect the client can draw and writes a report saying how
many entry points drew something and which names still have no model behind
them. It is the ratchet for this document: when the three above land, the
report's last line goes from five names to two, and the two are the tracers
already recorded in §3.4 of the brief. Run windowed it also writes the picture
— every effect in the set at one point on the deck, which is a pile, and is the
frame to look at when judging whether two of them read as two things.

Its companion, `--shot foundry <txt> statuses`, is the one that proves a status
is drawn on a body in a live match rather than on a bare node in a probe. Both
run in CI.
