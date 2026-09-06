# Deep Field 3D — Palette spec v1.0

Machine-readable twin: `palette.json`. Code adopts these values (GameRoot.TintEnemy, UI theme).
Albedo is flat and modulation-tolerant; state comes from this table, never from textures.

## Semantics
danger #C93B28 · warning #E8862B · success #7BC043 · info #2FB4BE
hp #7BC043 → hp-low #C93B28 · shield #2FB4BE · armor #7D8BA3 · currency #C89B3C · elite/boss #9B5BE8

## Factions
- Forge — accent #C89B3C, Overdrive #FF6F1A (shared with Overclock buff)
- Ember — accent #E8622B, Ignition Wave same hue (= burn)
- Tempest — accent #5B76E8, Chainsurge same hue (not Arc magenta)
- Glacier — accent #4FC0E8 *(provisional)*, Cryo Field same hue (= chill)
- Specter — accent #7FE65A *(provisional)*, Reveal Pulse same hue (= reveal)

The two provisional entries are derived from the rule the three specced
factions follow — accent = the hue of the status the ability applies — not
chosen. They stand until design says otherwise; design decides, code adopts.

## Scrap
alloy #A6B2C6 · flux #2FB4BE · plating #C89B3C · gravium #9B5BE8 · primecore #F4DCA4

## Statuses (albedo tint unless noted)
burn #E8622B (+emissive) · chill #4FC0E8 · freeze #A8F0F4 · shock #F05AE6 (+emissive) · shred #E9614C
mark — emissive only #E3BC66 · reveal — emissive only #7FE65A · tar #1B2233
Reactions: thermal shock = burn + chill pair; flash freeze = chill + shock pair.

## Enemies
Base albedo #7D8BA3, lerps to #C93B28 as hp falls. Weak points #E9614C. Warden shield #2FB4BE.
Elite rim lights: gilded #C89B3C · juggernaut #5A6880 · voltaic #F05AE6 · swift #65DCE4 · umbral #9B5BE8

## Towers
Body materials (all towers): steel_hull #606A7C · steel_plate #9AA6B7 · chassis #2B3A5C · trim #1B2436 · chrome #C3CCD8 · brass #B08A3E · hazard #D8A13A · concrete #7A7F88
One energy hue per tower (emissive ×0.95); projectile, muzzle and impact VFX inherit it:
- lance #2B5CFF
- skywatch #22D3EE
- detector #7FE65A
- nova #F0C83A
- overclock #FF6F1A
- filament #FF2E4A
- arc #F05AE6
- singularity #9B5BE8
- barricade #F0C83A
