/**
 * Deep Field 3D — VFX meshes (DESIGN-BRIEF §3.7).
 * Mesh-based effects the client animates procedurally (scale/rotate/fade the
 * named sub-groups). Every file is a static hero frame at mid-life so it also
 * works as a still. Origin at the affected thing's feet; status effects wrap a
 * 1.6 m Drifter-sized volume and scale to the target. Palette:
 *   burn ember 0xe8622b · chill frost 0x4fc0e8 · shock magenta 0xf05ae6 ·
 *   shred steel 0xcfd7e4 · mark gold 0xf0c83a · shield teal 0x2fb4be ·
 *   thermal shock = ember+frost · flash freeze = frost+magenta → ice white
 * Sub-group naming: `<id>_spin` (rotate Y), `<id>_pulse` (scale), `<id>_rise`
 * (translate Y), `<id>_fade` (opacity) — so the client can drive without lookups.
 */
export const VFX = [];
const P = (o) => { VFX.push({ ...o, file: o.id + '.glb', dir: 'vfx/', swatch: o.swatch || '#c3ccd8' }); };

export function makeVfxMats(THREE, mats) {
  const glow = (name, hex, i = 1.2, opacity = 1) => { const m = new THREE.MeshStandardMaterial({ color: hex, roughness: .3, metalness: 0, emissive: new THREE.Color(hex), emissiveIntensity: i, transparent: opacity < 1, opacity, depthWrite: opacity >= 1, side: THREE.DoubleSide }); m.name = name; mats[name] = m; return m; };
  glow('fx_burn', 0xe8622b, 1.4); glow('fx_burn_core', 0xffc46a, 1.8); glow('fx_burn_soft', 0xe8622b, .8, .45);
  glow('fx_frost', 0x4fc0e8, 1.2); glow('fx_frost_soft', 0x4fc0e8, .7, .4); glow('fx_ice', 0xdff6ff, .9, .7);
  glow('fx_shock', 0xf05ae6, 1.5); glow('fx_shock_soft', 0xf05ae6, .8, .4);
  glow('fx_shred', 0xcfd7e4, .6); glow('fx_mark', 0xf0c83a, 1.3); glow('fx_mark_soft', 0xf0c83a, .7, .4);
  glow('fx_shield', 0x2fb4be, 1.0, .5); glow('fx_shield_shard', 0x65dce4, 1.2, .8);
  glow('fx_dirt', 0x8a6f55, 0, 1); mats.fx_dirt.emissiveIntensity = 0; mats.fx_dirt.roughness = .95;
  glow('fx_sac', 0xe3bc66, .9); glow('fx_breach', 0xff2e4a, 1.6); glow('fx_breach_soft', 0xff2e4a, .8, .4);
  glow('fx_revive', 0x7fe65a, 1.2); glow('fx_revive_soft', 0x7fe65a, .6, .35);
  glow('fx_forge', 0xf0c83a, 1.3); glow('fx_ember', 0xe8622b, 1.3); glow('fx_tempest', 0xf05ae6, 1.3);
  glow('fx_place', 0x7fe65a, 1.1, .6); glow('fx_sell', 0xb08a3e, 1.0, .7); glow('fx_upgrade', 0x65dce4, 1.2, .7);
  glow('fx_wave', 0xff2e4a, 1.2, .8); glow('fx_clear', 0x7fe65a, 1.2, .8); glow('fx_white', 0xffffff, 1.0, .9);
  glow('fx_smoke', 0x3a3a44, 0, .5); mats.fx_smoke.emissiveIntensity = 0;
  glow('fx_venom', 0x7fe65a, 1.3); glow('fx_venom_soft', 0x7fe65a, .6, .4); glow('fx_venom_dark', 0x2f6a1e, .4, .8);
  glow('fx_reveal', 0x7fe65a, 1.2, .9); glow('fx_reveal_soft', 0x7fe65a, .5, .3);
  glow('fx_stun', 0xfff1c8, 1.4); glow('fx_stun_soft', 0xf0c83a, .6, .4); glow('fx_buff', 0xff6f1a, 1.3); glow('fx_buff_soft', 0xff6f1a, .6, .35); glow('fx_wash', 0x4fc0e8, .9, .5);
  return mats;
}

/* helpers */
const ring = (K, name, r, tube, m, y = 0, arc = Math.PI * 2, seg = 40) => K.part(name, new K.THREE.TorusGeometry(r, tube, 8, seg, arc), m, [0, y, 0], [Math.PI / 2, 0, 0]);
const disc = (K, name, r0, r1, m, y = 0, seg = 32) => K.part(name, new K.THREE.RingGeometry(r0, r1, seg), m, [0, y, 0], [-Math.PI / 2, 0, 0]);
const at = (mesh, x, z) => { mesh.position.x = x; mesh.position.z = z; return mesh; };
function spikes(K, name, n, r, len, m, y, tilt = 0, rand = 0) {
  const { THREE, grp, part } = K, g = grp(name);
  for (let i = 0; i < n; i++) { const a = i / n * Math.PI * 2 + (rand ? Math.sin(i * 7.3) * rand : 0); const s = part(`${name}_${i}`, new THREE.ConeGeometry(len * .12, len, 5), m, [Math.cos(a) * r, y, Math.sin(a) * r]); s.rotation.set(Math.cos(a) * tilt, 0, -Math.sin(a) * tilt); s.rotation.z += Math.PI * (tilt < 0 ? 1 : 0); s.rotation.y = -a; g.add(s); }
  return g;
}
function shards(K, name, n, r, size, m, y, spread = .6) {
  const { THREE, grp, part } = K, g = grp(name);
  for (let i = 0; i < n; i++) { const a = i / n * Math.PI * 2 + i * .3, rr = r * (1 + Math.sin(i * 3.1) * spread * .5); g.add(part(`${name}_${i}`, new THREE.TetrahedronGeometry(size * (.6 + (i % 3) * .3), 0), m, [Math.cos(a) * rr, y + Math.sin(i * 2.2) * spread, Math.sin(a) * rr], [i, i * .7, i * 1.3])); }
  return g;
}
function bolt(K, name, from, to, m, r = .035, jag = .12, segs = 7) {
  const { THREE } = K, a = new THREE.Vector3(...from), b = new THREE.Vector3(...to), pts = [];
  for (let i = 0; i <= segs; i++) { const p = a.clone().lerp(b, i / segs); if (i && i < segs) p.add(new THREE.Vector3(Math.sin(i * 9.1) * jag, Math.cos(i * 5.3) * jag, Math.sin(i * 3.7) * jag)); pts.push(p); }
  return K.part(name, new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), segs * 3, r, 5, false), m);
}
const flame = (K, name, h, r, m, pos, rot = [0, 0, 0]) => K.part(name, new K.THREE.ConeGeometry(r, h, 6), m, pos, rot);

/* ── reactions ─────────────────────────────────────────────────────── */
P({ id: 'vfx_reaction_thermalshock', label: 'Reaction — Thermal Shock', swatch: '#e8622b', size: '3 m', stats: { Burst: '12% max HP', Inputs: 'chill + burn', Life: '~0.6 s' },
  note: 'The M1 showpiece: a frost shell shattering outward as a fire core detonates through it. Frost shards fly out on `_pulse`, the ember ring expands, steam cones rise. Ember and frost both present so the co-op combo is legible.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_reaction_thermalshock');
    g.add(part('ts_core', new THREE.IcosahedronGeometry(.55, 1), mats.fx_burn_core, [0, 1.0, 0]));
    g.add(part('ts_fire_shell', new THREE.IcosahedronGeometry(.9, 1), mats.fx_burn_soft, [0, 1.0, 0]));
    const pulse = grp('ts_pulse'); pulse.add(shards(K, 'ts_ice_shards', 14, 1.3, .22, mats.fx_ice, 1.1, .9)); pulse.add(shards(K, 'ts_ice_shards_b', 10, 1.0, .15, mats.fx_frost, .8, .6)); g.add(pulse);
    g.add(ring(K, 'ts_ring_ember', 1.4, .06, mats.fx_burn, .12));
    g.add(ring(K, 'ts_ring_frost', 1.1, .04, mats.fx_frost, .08));
    g.add(disc(K, 'ts_scorch', .4, 1.2, mats.fx_burn_soft, .02));
    const rise = grp('ts_rise'); for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; rise.add(flame(K, 'ts_steam' + i, .9, .22, mats.fx_smoke, [Math.cos(a) * .7, 1.7, Math.sin(a) * .7], [Math.sin(a) * .5, 0, -Math.cos(a) * .5])); }
    for (let i = 0; i < 8; i++) { const a = i / 8 * Math.PI * 2 + .2; rise.add(flame(K, 'ts_flame' + i, .7 + (i % 2) * .3, .14, mats.fx_burn, [Math.cos(a) * .5, 1.3, Math.sin(a) * .5], [Math.sin(a) * .3, 0, -Math.cos(a) * .3])); }
    g.add(rise);
    return g;
  } });

P({ id: 'vfx_reaction_flashfreeze', label: 'Reaction — Flash Freeze', swatch: '#dff6ff', size: '2.6 m', stats: { Emits: 'freeze 1.2 s', Inputs: 'chill + shock', Life: 'held' },
  note: 'A column of ice locking the target: hex-prism ice pillar with an inner magenta discharge frozen mid-arc, frost crystals bursting from the base ring. The pillar is `_fade`; arcs are `_spin`.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_reaction_flashfreeze');
    const fade = grp('ff_fade');
    fade.add(part('ff_pillar', new THREE.CylinderGeometry(.55, .75, 2.2, 6), mats.fx_ice, [0, 1.1, 0]));
    fade.add(part('ff_pillar_cap', new THREE.ConeGeometry(.55, .5, 6), mats.fx_ice, [0, 2.45, 0]));
    for (let i = 0; i < 5; i++) fade.add(part('ff_facet' + i, new THREE.BoxGeometry(.06, 1.6 - i * .2, .02), mats.fx_white, [Math.cos(i * 1.26) * .62, 1.1 + i * .1, Math.sin(i * 1.26) * .62], [0, -i * 1.26, 0]));
    g.add(fade);
    const spin = grp('ff_spin'); spin.add(bolt(K, 'ff_arc_a', [-.4, .3, .2], [.35, 1.9, -.2], mats.fx_shock, .03, .1)); spin.add(bolt(K, 'ff_arc_b', [.4, .5, .3], [-.2, 1.7, -.35], mats.fx_shock, .025, .1)); g.add(spin);
    g.add(spikes(K, 'ff_crystals', 9, .9, .7, mats.fx_frost, .3, .9, .3));
    g.add(spikes(K, 'ff_crystals_b', 6, .5, 1.0, mats.fx_ice, .4, .5, .4));
    g.add(ring(K, 'ff_ring', 1.1, .05, mats.fx_frost, .1));
    g.add(ring(K, 'ff_ring_shock', 1.3, .03, mats.fx_shock, .06));
    g.add(disc(K, 'ff_rime', .3, 1.2, mats.fx_frost_soft, .02));
    return g;
  } });

/* ── statuses (wrap a 1.6 m target) ────────────────────────────────── */
P({ id: 'vfx_status_burn', label: 'Status — burn', swatch: '#e8622b', size: '1.8 m', stats: { DPS: '6', Duration: '3 s', Blocked: 'by shield' },
  note: 'Flame tongues licking up around the body on `_rise`, an ember ring at the feet, a smoke wisp from the shoulders. Flames lean with the target\'s travel (−Z) so movement reads.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_burn');
    const rise = grp('burn_rise');
    for (let i = 0; i < 10; i++) { const a = i / 10 * Math.PI * 2, h = .5 + (i % 3) * .25, r = .45; rise.add(flame(K, 'burn_flame' + i, h, .1 + (i % 2) * .04, i % 3 ? mats.fx_burn : mats.fx_burn_core, [Math.cos(a) * r, .5 + (i % 2) * .45 + h / 2, Math.sin(a) * r + .1], [.35, 0, 0])); }
    for (let i = 0; i < 4; i++) rise.add(flame(K, 'burn_lick' + i, .35, .07, mats.fx_burn_core, [Math.cos(i * 1.6) * .3, 1.55 + (i % 2) * .15, Math.sin(i * 1.6) * .3], [.5, 0, 0]));
    g.add(rise);
    g.add(ring(K, 'burn_ring', .6, .035, mats.fx_burn, .08));
    g.add(disc(K, 'burn_scorch', .2, .7, mats.fx_burn_soft, .02));
    g.add(part('burn_smoke', new THREE.SphereGeometry(.22, 8, 6), mats.fx_smoke, [.1, 1.95, .2]));
    g.add(part('burn_smoke_b', new THREE.SphereGeometry(.15, 8, 6), mats.fx_smoke, [-.15, 2.2, .35]));
    return g;
  } });

P({ id: 'vfx_status_chill', label: 'Status — chill', swatch: '#4fc0e8', size: '1.8 m', stats: { Speed: '×0.65', Duration: '1.5 s', Source: 'Singularity · cryo' },
  note: 'Frost rime crusting the lower body: ice crystal spikes from the feet, drifting hex flakes on `_spin`, a slow frost ring, and a translucent cold shell that reads as a speed penalty rather than a stop.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_chill');
    g.add(part('chill_shell', new THREE.CylinderGeometry(.45, .55, 1.0, 8, 1, true), mats.fx_frost_soft, [0, .55, 0]));
    g.add(spikes(K, 'chill_crystals', 8, .5, .45, mats.fx_ice, .2, .8, .3));
    g.add(spikes(K, 'chill_crystals_b', 5, .3, .3, mats.fx_frost, .15, .4, .5));
    const spin = grp('chill_spin'); for (let i = 0; i < 9; i++) { const a = i / 9 * Math.PI * 2; spin.add(part('chill_flake' + i, new THREE.CylinderGeometry(.06, .06, .01, 6), mats.fx_ice, [Math.cos(a) * .75, .4 + (i % 3) * .5, Math.sin(a) * .75], [.3, a, 0])); } g.add(spin);
    g.add(ring(K, 'chill_ring', .7, .03, mats.fx_frost, .06));
    g.add(disc(K, 'chill_rime', .25, .8, mats.fx_frost_soft, .02, 8));
    return g;
  } });

P({ id: 'vfx_status_shock', label: 'Status — shock', swatch: '#f05ae6', size: '1.8 m', stats: { Control: '0.25 s stagger', Source: 'Arc · Chain Surge', Fuel: 'Flash Freeze' },
  note: 'Magenta arcs crawling over the body between three orbit nodes on `_spin`, a ground discharge ring, a short vertical strike. Brief and loud — it is a stagger, not a lock.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_shock');
    const spin = grp('shock_spin');
    const nodes = [[.5, 1.3, 0], [-.35, .6, .4], [-.1, 1.7, -.5], [.3, .3, -.4]];
    nodes.forEach((p, i) => spin.add(part('shock_node' + i, new THREE.OctahedronGeometry(.07, 0), mats.fx_shock, p)));
    for (let i = 0; i < nodes.length; i++) spin.add(bolt(K, 'shock_arc' + i, nodes[i], nodes[(i + 1) % nodes.length], mats.fx_shock, .02, .09, 6));
    g.add(spin);
    g.add(bolt(K, 'shock_strike', [0, 2.3, 0], [0, 1.8, .1], mats.fx_shock, .03, .08, 4));
    g.add(part('shock_strike_flash', new THREE.SphereGeometry(.12, 8, 6), mats.fx_white, [0, 2.3, 0]));
    g.add(ring(K, 'shock_ring', .65, .03, mats.fx_shock, .06, Math.PI * 2, 12));
    g.add(spikes(K, 'shock_ground', 6, .5, .25, mats.fx_shock_soft, .1, 1.2, .5));
    return g;
  } });

P({ id: 'vfx_status_shred', label: 'Status — shred', swatch: '#cfd7e4', size: '1.8 m', stats: { Armor: '−2', Duration: '4 s', Softens: 'Aegis front arc' },
  note: 'Armour plates flaking off the target: steel chips orbiting outward on `_pulse`, exposed-seam glow lines, sparks at the feet, a cracked-plate ring. Reads as "hit it now".',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_shred');
    const pulse = grp('shred_pulse');
    for (let i = 0; i < 12; i++) { const a = i / 12 * Math.PI * 2 + i * .2, r = .55 + (i % 3) * .15; pulse.add(part('shred_chip' + i, new THREE.BoxGeometry(.14, .1, .02), mats.fx_shred, [Math.cos(a) * r, .5 + (i % 4) * .35, Math.sin(a) * r], [i * .5, -a, i * .3])); }
    g.add(pulse);
    for (let i = 0; i < 5; i++) g.add(part('shred_seam' + i, new THREE.BoxGeometry(.02, .35 + (i % 2) * .2, .02), mats.fx_mark, [Math.cos(i * 1.3) * .32, .7 + i * .2, Math.sin(i * 1.3) * .32], [.2, 0, .3 * (i % 2 ? 1 : -1)]));
    g.add(spikes(K, 'shred_sparks', 10, .3, .18, mats.fx_mark, .12, -1.1, .6));
    g.add(ring(K, 'shred_ring', .65, .025, mats.fx_shred, .05, Math.PI * 1.6, 8));
    return g;
  } });

P({ id: 'vfx_status_mark', label: 'Status — mark', swatch: '#f0c83a', size: '2.2 m', stats: { Damage: '×1.25 taken', Duration: '4 s', Source: 'rifle alt-fire' },
  note: 'Priority-target reticle: a gold bracket ring hovering over the head on `_spin`, a diamond pin, a thin ground ring, and a light column so the team sees the mark through crowds.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_mark');
    const spin = grp('mark_spin', [0, 2.1, 0]);
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; spin.add(part('mark_bracket' + i, new THREE.TorusGeometry(.45, .03, 6, 12, Math.PI / 3), mats.fx_mark, [0, 0, 0], [Math.PI / 2, 0, a - Math.PI / 6])); }
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2 + Math.PI / 4; spin.add(part('mark_tick' + i, new THREE.BoxGeometry(.04, .02, .16), mats.fx_mark, [Math.cos(a) * .6, 0, Math.sin(a) * .6], [0, -a + Math.PI / 2, 0])); }
    g.add(spin);
    g.add(part('mark_pin', new THREE.OctahedronGeometry(.12, 0), mats.fx_mark, [0, 2.1, 0]).rotateX(0));
    g.add(part('mark_pin_stem', new THREE.CylinderGeometry(.015, .015, .3, 6), mats.fx_mark, [0, 2.35, 0]));
    g.add(part('mark_column', new THREE.CylinderGeometry(.3, .5, 2.0, 12, 1, true), mats.fx_mark_soft, [0, 1.0, 0]));
    g.add(ring(K, 'mark_ring', .7, .025, mats.fx_mark, .05));
    return g;
  } });

P({ id: 'vfx_status_poison', label: 'Status — poison', swatch: '#7fe65a', size: '1.8 m', stats: { Channel: 'toxin', Ignores: 'armor + shield', Source: 'poison stream · toxin ammo' },
  note: 'Venom eating through the body: dripping green beads on `_rise` (they fall, so drive it downward), a dark sickly haze shell, corroded pits on the plates, a bubbling ground pool with a soft ring. Reads as "chip that cannot be stopped" — the Mender\'s answer.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_poison');
    g.add(part('poison_haze', new THREE.SphereGeometry(.62, 14, 10), mats.fx_venom_soft, [0, 1.0, 0])); g.getObjectByName('poison_haze').scale.set(1, 1.35, 1);
    const rise = grp('poison_rise'); for (let i = 0; i < 12; i++) { const a = i / 12 * Math.PI * 2, r = .35 + (i % 3) * .12; rise.add(part('poison_drip' + i, new THREE.SphereGeometry(.035 + (i % 2) * .015, 8, 6), mats.fx_venom, [Math.cos(a) * r, .5 + (i % 4) * .3, Math.sin(a) * r])); rise.add(part('poison_drip_tail' + i, new THREE.ConeGeometry(.02, .12, 6), mats.fx_venom, [Math.cos(a) * r, .58 + (i % 4) * .3, Math.sin(a) * r])); } g.add(rise);
    for (let i = 0; i < 7; i++) { const a = i / 7 * Math.PI * 2 + .4; g.add(part('poison_pit' + i, new THREE.CylinderGeometry(.06, .04, .02, 8), mats.fx_venom_dark, [Math.cos(a) * .42, .7 + (i % 3) * .3, Math.sin(a) * .42], [Math.PI / 2 - .3, a, 0])); }
    g.add(disc(K, 'poison_pool', 0, .55, mats.fx_venom_dark, .015, 24)); g.add(ring(K, 'poison_ring', .6, .03, mats.fx_venom_soft, .03));
    const pulse = grp('poison_pulse'); for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; pulse.add(part('poison_bubble' + i, new THREE.SphereGeometry(.05, 8, 6), mats.fx_venom, [Math.cos(a) * .3, .06, Math.sin(a) * .3])); } g.add(pulse);
    return g;
  } });

P({ id: 'vfx_status_reveal', label: 'Status — reveal', swatch: '#7fe65a', size: '2.2 m', stats: { Channel: 'detection', Source: 'Detector · Reveal Pulse', On: 'Shade / burrowed Mole' },
  note: 'Detection lock: a green scan-line cage of horizontal rings sweeping the body on `_rise`, a rotating reticle square over the head on `_spin`, a ground disc with four ticks, a thin light column so towers-can-see-it reads from range.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_reveal');
    const rise = grp('reveal_rise'); for (let i = 0; i < 6; i++) rise.add(ring(K, 'reveal_scan' + i, .48, .008, mats.fx_reveal, .2 + i * .3, Math.PI * 2, 32)); g.add(rise);
    g.add(part('reveal_column', new THREE.CylinderGeometry(.5, .5, 2.2, 16, 1, true), mats.fx_reveal_soft, [0, 1.1, 0]));
    const spin = grp('reveal_spin', [0, 2.1, 0]); for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; spin.add(part('reveal_bracket' + i, new THREE.BoxGeometry(.16, .02, .02), mats.fx_reveal, [Math.cos(a) * .32, 0, Math.sin(a) * .32], [0, -a + Math.PI / 2, 0])); } spin.add(part('reveal_pin', new THREE.OctahedronGeometry(.06, 0), mats.fx_reveal)); g.add(spin);
    g.add(disc(K, 'reveal_disc', .5, .58, mats.fx_reveal, .02, 32)); for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part('reveal_tick' + i, new THREE.BoxGeometry(.03, .01, .16), mats.fx_reveal, [Math.cos(a) * .7, .02, Math.sin(a) * .7], [0, -a, 0])); }
    return g;
  } });

P({ id: 'vfx_detector_pulse', label: 'Detector — pulse', swatch: '#7fe65a', size: '26 m', stats: { Radius: '13 m', Period: '2 s', Reveals: 'stealth · burrowed' },
  note: 'The tower\'s reveal sweep: a 13 m ground ring expanding on `_pulse` with a trailing soft band, a slow radar wedge on `_spin`, and a low dome edge so the covered volume reads. Unit-authored at full radius; the client scales 0 → 1 over the period.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_detector_pulse');
    const pulse = grp('pulse_pulse'); pulse.add(ring(K, 'pulse_ring', 13, .08, mats.fx_reveal, .05, Math.PI * 2, 96)); pulse.add(disc(K, 'pulse_band', 11.8, 13, mats.fx_reveal_soft, .04, 96)); g.add(pulse);
    const spin = grp('pulse_spin'); spin.add(part('pulse_wedge', new THREE.RingGeometry(.4, 13, 32, 1, 0, Math.PI / 5), mats.fx_reveal_soft, [0, .06, 0], [-Math.PI / 2, 0, 0])); g.add(spin);
    g.add(part('pulse_dome', new THREE.SphereGeometry(13, 48, 12, 0, Math.PI * 2, Math.PI * .42, Math.PI * .08), mats.fx_reveal_soft, [0, 0, 0]));
    for (let i = 0; i < 12; i++) { const a = i / 12 * Math.PI * 2; g.add(part('pulse_tick' + i, new THREE.BoxGeometry(.12, .02, .6), mats.fx_reveal, [Math.cos(a) * 13, .04, Math.sin(a) * 13], [0, -a, 0])); }
    return g;
  } });

P({ id: 'vfx_status_stun', label: 'Status — stun', swatch: '#fff1c8', size: '1.8 m', stats: { Channel: 'control', Source: 'Launcher · freeze end · boss slam', Fills: 'ccResist' },
  note: 'Dazed: a ring of pale sparks orbiting the head on `_spin` (the classic tell, kept because it reads at 30 m), a jittering vertical column that flickers, feet locked in a gold ground ring, and a thin ccResist gauge arc that fills as the stun runs.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_status_stun');
    const spin = grp('stun_spin', [0, 1.95, 0]); for (let i = 0; i < 7; i++) { const a = i / 7 * Math.PI * 2; spin.add(part('stun_spark' + i, new THREE.OctahedronGeometry(.045, 0), mats.fx_stun, [Math.cos(a) * .34, Math.sin(a * 2) * .03, Math.sin(a) * .34])); } spin.add(ring(K, 'stun_orbit', .34, .006, mats.fx_stun_soft, 0)); g.add(spin);
    g.add(part('stun_column', new THREE.CylinderGeometry(.34, .40, 1.8, 12, 1, true), mats.fx_stun_soft, [0, .95, 0]));
    for (let i = 0; i < 5; i++) g.add(bolt(K, 'stun_jitter' + i, [Math.cos(i * 1.3) * .3, .3 + i * .3, Math.sin(i * 1.3) * .3], [Math.cos(i * 1.3 + 1) * .3, .6 + i * .3, Math.sin(i * 1.3 + 1) * .3], mats.fx_stun, .012, .05, 4));
    g.add(ring(K, 'stun_lock', .55, .04, mats.fx_stun, .03)); g.add(ring(K, 'stun_resist', .7, .025, mats.fx_stun_soft, .03, Math.PI * .8));
    return g;
  } });

P({ id: 'vfx_overclock_link', label: 'Overclock — link', swatch: '#ff6f1a', size: '1 m unit', stats: { To: 'every fed tower', Length: 'unit −Z', Pulse: '`_pulse` travels' },
  note: 'Buff-orange conduit from the pylon to each fed tower: a unit-length twisted pair of tubes along −Z with a pulse bead on `link_pulse` the client slides source → target, a soft sheath, and end collars. Scale Z to the tower distance; instance one per link.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_overclock_link');
    for (let k = 0; k < 2; k++) { const pts = []; for (let i = 0; i <= 12; i++) { const a = i / 12 * Math.PI * 3 + k * Math.PI; pts.push(new THREE.Vector3(Math.cos(a) * .04, Math.sin(a) * .04, -i / 12)); } g.add(part('link_strand' + k, new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 24, .012, 5, false), mats.fx_buff)); }
    g.add(part('link_sheath', new THREE.CylinderGeometry(.08, .08, 1, 10, 1, true), mats.fx_buff_soft, [0, 0, -.5], [Math.PI / 2, 0, 0]));
    for (const z of [0, -1]) g.add(part('link_collar' + z, new THREE.TorusGeometry(.10, .015, 6, 16), mats.fx_buff, [0, 0, z]));
    const pulse = grp('link_pulse', [0, 0, -.3]); pulse.add(part('link_bead', new THREE.SphereGeometry(.07, 10, 8), mats.fx_stun)); g.add(pulse);
    return g;
  } });

P({ id: 'vfx_lanewash', label: 'Floodgate — lane wash', swatch: '#4fc0e8', size: '4 × 6 m unit', stats: { From: 'shared_floodgate', Along: '−Z', Life: '~2 s' },
  note: 'The panic button: a wall of water rolling down the lane — a 3.4 m-wide breaking crest on `_rise`, foam shards on `_pulse`, a sheet of surface plane behind it, spray cones off the kerbs. Unit-authored 6 m long; the client scales Z along the lane leg and translates it down-route.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_lanewash');
    g.add(part('wash_sheet', new THREE.BoxGeometry(3.4, .3, 6), mats.fx_wash, [0, .15, -3]));
    const rise = grp('wash_rise'); rise.add(part('wash_crest', new THREE.CylinderGeometry(.7, .9, 3.4, 12, 1, false, 0, Math.PI), mats.fx_wash, [0, .9, -5.6], [0, 0, Math.PI / 2])); rise.add(part('wash_lip', new THREE.CylinderGeometry(.25, .25, 3.4, 8), mats.fx_white, [0, 1.5, -6.0], [0, 0, Math.PI / 2])); g.add(rise);
    const pulse = grp('wash_pulse'); for (let i = 0; i < 14; i++) pulse.add(part('wash_foam' + i, new THREE.SphereGeometry(.12 + (i % 3) * .06, 8, 6), mats.fx_white, [-1.5 + (i % 7) * .5, .6 + (i % 2) * .5, -5.8 - (i % 3) * .3])); g.add(pulse);
    for (const s of [-1, 1]) for (let i = 0; i < 3; i++) g.add(part(`wash_spray${s}${i}`, new THREE.ConeGeometry(.25, .9, 8), mats.fx_wash, [s * 1.8, .6, -1.5 - i * 1.8], [-.4, 0, s * .8]));
    return g;
  } });

/* ── shields ───────────────────────────────────────────────────────── */
P({ id: 'vfx_shield_pop', label: 'Shield — pop', swatch: '#65dce4', size: '3 m', stats: { Warden: '25 shield', Life: '~0.4 s' },
  note: 'The Warden bubble bursting: hex shards flying outward on `_pulse`, a bright equatorial ring, a residual half-shell fading on `_fade`.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_shield_pop');
    const pulse = grp('pop_pulse');
    for (let i = 0; i < 18; i++) { const a = i / 18 * Math.PI * 2, el = Math.sin(i * 2.3) * .9, r = 1.4 + (i % 3) * .2; pulse.add(part('pop_shard' + i, new THREE.CylinderGeometry(.16, .16, .02, 6), mats.fx_shield_shard, [Math.cos(a) * Math.cos(el) * r, Math.max(.2, 1.2 + Math.sin(el) * r), Math.sin(a) * Math.cos(el) * r], [el, -a, i * .4])); }
    g.add(pulse);
    g.add(ring(K, 'pop_ring', 1.3, .05, mats.fx_shield_shard, 1.2));
    const fade = grp('pop_fade'); fade.add(part('pop_shell', new THREE.SphereGeometry(1.15, 12, 8, 0, Math.PI * 2, Math.PI * .45, Math.PI * .55), mats.fx_shield, [0, 1.25, 0])); g.add(fade);
    g.add(part('pop_flash', new THREE.IcosahedronGeometry(.5, 1), mats.fx_white, [0, 1.2, 0]));
    return g;
  } });

P({ id: 'vfx_shield_regen', label: 'Shield — regen', swatch: '#2fb4be', size: '2.6 m', stats: { After: 'damage lull', Life: '~1.2 s' },
  note: 'The bubble regrowing from the emitter masts: three rising seam arcs on `_rise`, a partial lower shell filling upward, hex motes converging on `_pulse`, teal ring at the feet.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_shield_regen');
    g.add(part('regen_shell', new THREE.SphereGeometry(1.15, 14, 10, 0, Math.PI * 2, Math.PI * .5, Math.PI * .5), mats.fx_shield, [0, 1.25, 0]));
    const rise = grp('regen_rise'); for (let i = 0; i < 3; i++) { const a = i / 3 * Math.PI * 2 + Math.PI / 2; rise.add(part('regen_seam' + i, new THREE.TorusGeometry(1.15, .015, 6, 32, Math.PI * .6), mats.fx_shield_shard, [0, 1.2, 0], [Math.PI / 2 + .0, a, Math.PI * .7])); } g.add(rise);
    const pulse = grp('regen_pulse'); for (let i = 0; i < 10; i++) { const a = i / 10 * Math.PI * 2; pulse.add(part('regen_mote' + i, new THREE.CylinderGeometry(.07, .07, .01, 6), mats.fx_shield_shard, [Math.cos(a) * 1.5, .3 + (i % 4) * .5, Math.sin(a) * 1.5], [.4, a, 0])); } g.add(pulse);
    g.add(ring(K, 'regen_ring', 1.2, .03, mats.fx_shield_shard, .05));
    for (let i = 0; i < 3; i++) { const a = i / 3 * Math.PI * 2 + Math.PI / 2; g.add(part('regen_emitter_glow' + i, new THREE.SphereGeometry(.09, 8, 6), mats.fx_shield_shard, [Math.cos(a) * .38, 2.12, Math.sin(a) * .38])); }
    return g;
  } });

/* ── enemy events ──────────────────────────────────────────────────── */
P({ id: 'vfx_burrow_spray', label: 'Mole — burrow spray', swatch: '#8a6f55', size: '3 m', stats: { On: 'dive / surface', Life: '~0.5 s' },
  note: 'Dirt fountain: a dark soil cone with clods flung outward on `_pulse`, a dust dome on `_fade`, torn ground plates, and the fissure ring the burrowed mesh continues.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_burrow_spray');
    g.add(part('spray_cone', new THREE.ConeGeometry(.6, 1.8, 9), mats.fx_dirt, [0, .9, 0]));
    const pulse = grp('spray_pulse'); for (let i = 0; i < 14; i++) { const a = i / 14 * Math.PI * 2 + i * .4, r = .7 + (i % 3) * .35; pulse.add(part('spray_clod' + i, new THREE.DodecahedronGeometry(.08 + (i % 3) * .04, 0), mats.fx_dirt, [Math.cos(a) * r, .6 + (i % 4) * .45, Math.sin(a) * r], [i, i * .5, 0])); } g.add(pulse);
    const fade = grp('spray_fade'); fade.add(part('spray_dust', new THREE.SphereGeometry(1.2, 10, 7, 0, Math.PI * 2, 0, Math.PI / 2), mats.fx_smoke, [0, .1, 0])); g.add(fade);
    for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; g.add(part('spray_plate' + i, new THREE.BoxGeometry(.4, .04, .3), mats.concrete, [Math.cos(a) * .7, .15, Math.sin(a) * .7], [Math.sin(a) * -.6, -a, Math.cos(a) * .6])); }
    g.add(disc(K, 'spray_fissure', .45, .55, mats.fx_burn, .02, 9));
    return g;
  } });

P({ id: 'vfx_cluster_split', label: 'Cluster — split', swatch: '#e3bc66', size: '3 m', stats: { Spawns: '5 Motes', Life: '~0.5 s' },
  note: 'The Cluster bursting: five amber sac shells arcing outward to the Mote landing ring on `_pulse`, membrane strands, a carapace shard burst, amber flash at the belly height.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_cluster_split');
    g.add(part('split_flash', new THREE.IcosahedronGeometry(.45, 1), mats.fx_sac, [0, 1.1, 0]));
    const pulse = grp('split_pulse');
    for (let i = 0; i < 5; i++) { const a = i / 5 * Math.PI * 2; const p = [Math.cos(a) * 1.1, .8 + (i % 2) * .3, Math.sin(a) * 1.1]; pulse.add(part('split_sac' + i, new THREE.SphereGeometry(.2, 10, 8, 0, Math.PI * 1.4), mats.fx_sac, p, [0, -a, .6])); pulse.add(K.cableRun('split_strand' + i, [[0, 1.1, 0], [p[0] * .5, 1.3, p[2] * .5], p], .02, mats.fx_sac)); }
    g.add(pulse);
    g.add(shards(K, 'split_carapace', 10, .8, .16, mats.fx_shred, 1.0, .7));
    g.add(ring(K, 'split_landing', 1.4, .03, mats.fx_sac, .05));
    for (let i = 0; i < 5; i++) { const a = i / 5 * Math.PI * 2; g.add(at(disc(K, 'split_pad' + i, .15, .3, mats.fx_sac, .03, 12), Math.cos(a) * 1.4, Math.sin(a) * 1.4)); }
    return g;
  } });

P({ id: 'vfx_core_breach', label: 'Core — breach', swatch: '#ff2e4a', size: '8 m', stats: { On: 'enemy reaches goal', Life: '~1 s' },
  note: 'Alarm hit on the core: threat-red shockwave ring on `_pulse`, a vertical warning column, arcs jumping off the gantry, a crack flash at the sphere, four siren cones. Sized to the 6 m core.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_core_breach');
    const pulse = grp('breach_pulse'); pulse.add(ring(K, 'breach_wave', 3.6, .12, mats.fx_breach, .3)); pulse.add(ring(K, 'breach_wave_b', 2.6, .08, mats.fx_breach_soft, .5)); g.add(pulse);
    g.add(part('breach_column', new THREE.CylinderGeometry(.6, 1.2, 7, 16, 1, true), mats.fx_breach_soft, [0, 3.5, 0]));
    g.add(part('breach_flash', new THREE.IcosahedronGeometry(1.0, 1), mats.fx_white, [0, 3.0, -1.6]));
    for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; g.add(bolt(K, 'breach_arc' + i, [Math.cos(a) * 2.0, 3.0 + Math.sin(i) * .8, Math.sin(a) * 2.0], [Math.cos(a) * 3.4, 1.5 + (i % 2) * 2.5, Math.sin(a) * 3.4], mats.fx_breach, .04, .2)); }
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2 + Math.PI / 4; g.add(part('breach_siren' + i, new THREE.ConeGeometry(.5, 2.2, 8, 1, true), mats.fx_breach_soft, [Math.cos(a) * 2.8, 5.3, Math.sin(a) * 2.8], [Math.PI * .5 + Math.sin(a) * .5, 0, Math.cos(a) * .5])); }
    g.add(disc(K, 'breach_ground', 1.0, 3.8, mats.fx_breach_soft, .04, 8));
    return g;
  } });

P({ id: 'vfx_revive_beam', label: 'Revive beam', swatch: '#7fe65a', size: '2.6 m', stats: { Hold: 'R', Life: 'channel' },
  note: 'The hold-R channel: a green light column over the downed hero, a progress ring filling on `_pulse` (arc = progress), rising motes on `_rise`, and a hand-to-chest tether line from the reviver origin 0.6 m in front.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_revive_beam');
    g.add(part('revive_column', new THREE.CylinderGeometry(.5, .7, 2.2, 16, 1, true), mats.fx_revive_soft, [0, 1.1, 0]));
    const pulse = grp('revive_pulse'); pulse.add(ring(K, 'revive_progress', .9, .05, mats.fx_revive, .1, Math.PI * 1.3)); g.add(pulse);
    g.add(ring(K, 'revive_track', .9, .02, mats.fx_revive_soft, .1));
    const rise = grp('revive_rise'); for (let i = 0; i < 12; i++) { const a = i / 12 * Math.PI * 2; rise.add(part('revive_mote' + i, new THREE.OctahedronGeometry(.05, 0), mats.fx_revive, [Math.cos(a) * .55, .3 + (i % 4) * .5, Math.sin(a) * .55])); } g.add(rise);
    g.add(K.cableRun('revive_tether', [[0, .8, -.9], [0, 1.1, -.4], [0, .5, .2]], .02, mats.fx_revive));
    g.add(part('revive_heart', new THREE.OctahedronGeometry(.14, 0), mats.fx_revive, [0, .5, .2]));
    g.add(part('revive_hand_glow', new THREE.SphereGeometry(.12, 8, 6), mats.fx_revive_soft, [0, .8, -.9]));
    return g;
  } });

/* ── faction abilities ─────────────────────────────────────────────── */
P({ id: 'vfx_ability_overdrive', label: 'Ability — Overdrive', swatch: '#f0c83a', size: '20 m', stats: { Radius: '10 m', Duration: '4 s', Effect: 'tower rate ×1.5' },
  note: 'Forge\'s builder surge: a 10 m gold ground ring with gear-tooth ticks on `_spin`, a low dome on `_fade`, radial power conduits from the hero, and a per-tower crown ring (`overdrive_tower_crown`, place on each buffed tower at chassis height).',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_ability_overdrive');
    const spin = grp('od_spin'); spin.add(ring(K, 'od_ring', 10, .12, mats.fx_forge, .15, Math.PI * 2, 64)); for (let i = 0; i < 24; i++) { const a = i / 24 * Math.PI * 2; spin.add(part('od_tooth' + i, new THREE.BoxGeometry(.5, .1, .8), mats.fx_forge, [Math.cos(a) * 10.4, .15, Math.sin(a) * 10.4], [0, -a, 0])); } g.add(spin);
    const fade = grp('od_fade'); fade.add(part('od_dome', new THREE.SphereGeometry(10, 32, 12, 0, Math.PI * 2, 0, Math.PI / 2), mats.fx_mark_soft, [0, 0, 0])); g.add(fade);
    for (let i = 0; i < 8; i++) { const a = i / 8 * Math.PI * 2; g.add(part('od_conduit' + i, new THREE.BoxGeometry(.15, .05, 9), mats.fx_forge, [Math.cos(a) * 5, .08, Math.sin(a) * 5], [0, -a + Math.PI / 2, 0])); }
    g.add(part('od_pillar', new THREE.CylinderGeometry(.3, .5, 3, 12, 1, true), mats.fx_mark_soft, [0, 1.5, 0]));
    const crown = grp('overdrive_tower_crown', [0, 2.0, 0]); crown.add(ring(K, 'od_crown_ring', 1.2, .05, mats.fx_forge, 0, Math.PI * 2, 24)); for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; crown.add(part('od_crown_gear' + i, new THREE.BoxGeometry(.12, .3, .12), mats.fx_forge, [Math.cos(a) * 1.2, .15, Math.sin(a) * 1.2])); } g.add(crown);
    return g;
  } });

P({ id: 'vfx_ability_ignitionwave', label: 'Ability — Ignition Wave', swatch: '#e8622b', size: '12 m', stats: { Radius: '6 m', Applies: 'burn', Half: 'of Thermal Shock' },
  note: 'Ember\'s aim-point wave: an expanding 6 m fire ring on `_pulse` with flame crests, a scorched ground disc, a directional splash cone from the hero toward the aim point (−Z), and embers rising on `_rise`.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_ability_ignitionwave');
    const pulse = grp('iw_pulse'); pulse.add(ring(K, 'iw_ring', 6, .18, mats.fx_burn, .3, Math.PI * 2, 48)); for (let i = 0; i < 20; i++) { const a = i / 20 * Math.PI * 2; pulse.add(flame(K, 'iw_crest' + i, 1.2 + (i % 3) * .4, .3, i % 2 ? mats.fx_burn : mats.fx_burn_core, [Math.cos(a) * 6, .9, Math.sin(a) * 6], [Math.sin(a) * .35, 0, -Math.cos(a) * .35])); } g.add(pulse);
    g.add(disc(K, 'iw_scorch', .5, 5.6, mats.fx_burn_soft, .03, 32));
    g.add(part('iw_splash', new THREE.ConeGeometry(.9, 8, 12, 1, true), mats.fx_burn_soft, [0, 1.0, 4], [-Math.PI / 2, 0, 0]));
    const rise = grp('iw_rise'); for (let i = 0; i < 16; i++) { const a = i / 16 * Math.PI * 2 + i, r = 1 + (i % 5); rise.add(part('iw_ember' + i, new THREE.TetrahedronGeometry(.1, 0), mats.fx_burn_core, [Math.cos(a) * r, .4 + (i % 4) * .6, Math.sin(a) * r], [i, i, i])); } g.add(rise);
    g.add(part('iw_core', new THREE.IcosahedronGeometry(.7, 1), mats.fx_burn_core, [0, .7, 0]));
    return g;
  } });

P({ id: 'vfx_ability_chainsurge', label: 'Ability — Chain Surge', swatch: '#f05ae6', size: '14 m', stats: { Radius: '7 m', Damage: '6 / enemy', Applies: 'shock' },
  note: 'Tempest\'s AoE burst: a central strike pillar, six radial chain bolts to hit-point nodes on `_spin`, a 7 m magenta ring on `_pulse`, ground scorch spokes. Pair with chill for Flash Freeze columns.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_ability_chainsurge');
    g.add(part('cs_pillar', new THREE.CylinderGeometry(.25, .6, 6, 8, 1, true), mats.fx_shock_soft, [0, 3, 0]));
    g.add(bolt(K, 'cs_strike', [0, 7, 0], [0, .4, 0], mats.fx_shock, .08, .35, 8));
    g.add(part('cs_flash', new THREE.IcosahedronGeometry(.6, 1), mats.fx_white, [0, .6, 0]));
    const spin = grp('cs_spin'); for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2, r = 4 + (i % 3) * 1.4; const p = [Math.cos(a) * r, .9, Math.sin(a) * r]; spin.add(bolt(K, 'cs_chain' + i, [0, .8, 0], p, mats.fx_shock, .04, .3, 8)); spin.add(part('cs_node' + i, new THREE.OctahedronGeometry(.25, 0), mats.fx_shock, p)); spin.add(at(ring(K, 'cs_node_ring' + i, .6, .03, mats.fx_shock, .06, Math.PI * 2, 12), p[0], p[2])); } g.add(spin);
    const pulse = grp('cs_pulse'); pulse.add(ring(K, 'cs_ring', 7, .12, mats.fx_shock, .15, Math.PI * 2, 48)); g.add(pulse);
    for (let i = 0; i < 12; i++) { const a = i / 12 * Math.PI * 2; g.add(part('cs_spoke' + i, new THREE.BoxGeometry(.08, .02, 6.5), mats.fx_shock_soft, [Math.cos(a) * 3.5, .03, Math.sin(a) * 3.5], [0, -a + Math.PI / 2, 0])); }
    return g;
  } });

/* ── tower lifecycle ───────────────────────────────────────────────── */
P({ id: 'vfx_tower_place', label: 'Tower — place', swatch: '#7fe65a', size: '3 m', stats: { On: 'socket build', Life: '~0.5 s' },
  note: 'Construction flash on the 2.1 m pad: a green holo-cage of the chassis volume on `_fade`, corner clamps dropping on `_rise`, a pad ring on `_pulse`, sparks.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_tower_place');
    const fade = grp('place_fade'); fade.add(part('place_holo', new THREE.CylinderGeometry(.9, 1.0, 2.4, 8, 1, true), mats.fx_place, [0, 1.2, 0])); fade.add(part('place_holo_cap', new THREE.CylinderGeometry(.9, .9, .02, 8), mats.fx_place, [0, 2.4, 0])); g.add(fade);
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2 + Math.PI / 4; g.add(part('place_edge' + i, new THREE.BoxGeometry(.04, 2.4, .04), mats.fx_revive, [Math.cos(a) * .95, 1.2, Math.sin(a) * .95])); }
    const rise = grp('place_rise'); for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; rise.add(part('place_clamp' + i, new THREE.BoxGeometry(.3, .5, .2), mats.steel_plate, [Math.cos(a) * 1.15, 1.6, Math.sin(a) * 1.15], [0, -a, 0])); } g.add(rise);
    const pulse = grp('place_pulse'); pulse.add(ring(K, 'place_ring', 1.3, .05, mats.fx_revive, .08)); g.add(pulse);
    g.add(spikes(K, 'place_sparks', 8, .6, .3, mats.fx_mark, .15, -1.0, .5));
    return g;
  } });

P({ id: 'vfx_tower_sell', label: 'Tower — sell', swatch: '#b08a3e', size: '3 m', stats: { On: 'refund', Life: '~0.6 s' },
  note: 'Deconstruction: the chassis volume dissolving into brass scrap cubes rising on `_rise`, a refund coin-column, the pad ring collapsing on `_pulse`. Warm brass so it reads as money back, not damage.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_tower_sell');
    g.add(part('sell_ghost', new THREE.CylinderGeometry(.85, .95, 1.6, 8, 1, true), mats.fx_sell, [0, .8, 0]));
    const rise = grp('sell_rise'); for (let i = 0; i < 18; i++) { const a = i / 18 * Math.PI * 2 + i * .5, r = .3 + (i % 4) * .25; rise.add(part('sell_cube' + i, new THREE.BoxGeometry(.14, .14, .14), i % 3 ? mats.brass : mats.fx_sell, [Math.cos(a) * r, .4 + (i % 5) * .5, Math.sin(a) * r], [i, i * .7, 0])); } g.add(rise);
    g.add(part('sell_column', new THREE.CylinderGeometry(.25, .35, 3, 12, 1, true), mats.fx_sell, [0, 1.5, 0]));
    for (let i = 0; i < 5; i++) g.add(part('sell_coin' + i, new THREE.CylinderGeometry(.16, .16, .03, 12), mats.brass, [Math.sin(i) * .15, 2.2 + i * .25, Math.cos(i) * .15], [.3, i, 0]));
    const pulse = grp('sell_pulse'); pulse.add(ring(K, 'sell_ring', .9, .04, mats.fx_sell, .08)); g.add(pulse);
    return g;
  } });

P({ id: 'vfx_tower_upgrade', label: 'Tower — upgrade', swatch: '#65dce4', size: '3.2 m', stats: { On: 'path level-up', Life: '~0.7 s' },
  note: 'Level-up: three arcane-teal chevrons climbing the chassis on `_rise`, a bright cap ring at the new module height on `_pulse`, a light column, data-tick rings. The chevron count (1–3) can be culled per tier.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_tower_upgrade');
    g.add(part('up_column', new THREE.CylinderGeometry(.7, .9, 3.0, 16, 1, true), mats.fx_upgrade, [0, 1.5, 0]));
    const rise = grp('up_rise'); for (let i = 0; i < 3; i++) for (let k = 0; k < 4; k++) { const a = k / 4 * Math.PI * 2; rise.add(part(`up_chevron${i}_${k}`, new THREE.ConeGeometry(.22, .3, 3), mats.fx_upgrade, [Math.cos(a) * 1.0, .6 + i * .7, Math.sin(a) * 1.0], [0, -a + Math.PI / 2, 0])); } g.add(rise);
    const pulse = grp('up_pulse'); pulse.add(ring(K, 'up_cap', 1.1, .06, mats.fx_upgrade, 2.6)); g.add(pulse);
    for (let i = 0; i < 3; i++) g.add(ring(K, 'up_tick' + i, .8 + i * .15, .015, mats.fx_upgrade, .3 + i * .9, Math.PI * 1.5, 24));
    g.add(part('up_star', new THREE.OctahedronGeometry(.22, 0), mats.fx_white, [0, 2.9, 0]));
    return g;
  } });

/* ── wave beats ────────────────────────────────────────────────────── */
P({ id: 'vfx_wave_start', label: 'Wave — start', swatch: '#ff2e4a', size: '8 m', stats: { At: 'spawn portal', Life: '~1.5 s' },
  note: 'Portal opening beat: a threat-red shock ring rolling down the lane on `_pulse`, the iris glow flaring, siren cones sweeping on `_spin`, ground chevrons pointing +X along the route.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_wave_start');
    g.add(part('ws_iris_flare', new THREE.CylinderGeometry(1.9, 1.9, .2, 24), mats.fx_wave, [0, 2.0, .3], [Math.PI / 2, 0, 0]));
    const pulse = grp('ws_pulse'); pulse.add(ring(K, 'ws_ring', 3.5, .1, mats.fx_wave, .2)); pulse.add(ring(K, 'ws_ring_b', 2.2, .06, mats.fx_breach_soft, .35)); g.add(pulse);
    const spin = grp('ws_spin'); for (const s of [-1, 1]) spin.add(part('ws_sweep' + s, new THREE.ConeGeometry(.8, 4, 10, 1, true), mats.fx_breach_soft, [s * 2.5, 5.0, 0], [Math.PI / 2, 0, s * .8])); g.add(spin);
    for (let i = 0; i < 4; i++) g.add(part('ws_chevron' + i, new THREE.ConeGeometry(.6, 1.0, 3), mats.fx_wave, [1.5 + i * 1.4, .06, 0], [Math.PI / 2, 0, -Math.PI / 2]));
    for (const s of [-1, 1]) g.add(part('ws_lamp_glow' + s, new THREE.SphereGeometry(.4, 10, 8), mats.fx_breach_soft, [s * 2.5, 5.0, 0]));
    return g;
  } });

P({ id: 'vfx_wave_clear', label: 'Wave — clear', swatch: '#7fe65a', size: '8 m', stats: { At: 'core', Life: '~2 s' },
  note: 'All-clear beat: green success rings rising off the core on `_rise`, a fountain of scrap-bounty motes on `_pulse`, four upward light beams from the stacks, a calm ground ring.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('vfx_wave_clear');
    const rise = grp('wc_rise'); for (let i = 0; i < 3; i++) rise.add(ring(K, 'wc_ring' + i, 2.4 + i * .6, .06 - i * .01, mats.fx_clear, 3.0 + i * .8, Math.PI * 2, 48)); g.add(rise);
    const pulse = grp('wc_pulse'); for (let i = 0; i < 16; i++) { const a = i / 16 * Math.PI * 2 + i * .3, r = .8 + (i % 4) * .5; pulse.add(part('wc_mote' + i, new THREE.OctahedronGeometry(.12, 0), i % 2 ? mats.brass : mats.fx_clear, [Math.cos(a) * r, 3.5 + (i % 5) * .5, Math.sin(a) * r])); } g.add(pulse);
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2 + Math.PI / 4; g.add(part('wc_beam' + i, new THREE.CylinderGeometry(.15, .3, 6, 10, 1, true), mats.fx_revive_soft, [Math.cos(a) * 2.8, 8.0, Math.sin(a) * 2.8])); }
    g.add(ring(K, 'wc_ground', 3.6, .05, mats.fx_clear, .1, Math.PI * 2, 64));
    g.add(part('wc_halo', new THREE.SphereGeometry(2.1, 16, 12), mats.fx_revive_soft, [0, 3.0, 0]));
    return g;
  } });
