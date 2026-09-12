/**
 * Deep Field 3D — shared traversal & interactive kit (DESIGN-BRIEF §3.6).
 * Metres, Y-up, origin at the footprint centre on the ground (or the pole
 * base). Every piece is a static mesh; states are separate files.
 */
export const SHARED = [];
const P = (o) => { SHARED.push({ ...o, file: o.file || o.id + '.glb', swatch: o.swatch || '#8d99ad', dir: 'shared/' }); };

/* Ladder: one rig builder, discrete height variants. Rung pitch is a fixed 30 cm at
   every height — the old single 6 m asset was Y-scaled to 5 m / 5.4 m in levels.js,
   which compressed the pitch to 25 cm and squashed the cage hoops. Heights are
   exported separately instead, so nothing scales. Adding a height is one row below. */
function ladderRig(K, id, h, o = {}) {
  const { part, grp, box, cyl, mats, THREE } = K, g = grp(id);
  for (const s of [-1, 1]) g.add(part(`ladder_stringer${s}`, box(.06, h, .08), mats.steel_hull, [s * .5, h / 2, 0]));
  const rungs = Math.floor(h / .3);
  for (let i = 0; i < rungs; i++) g.add(part('ladder_rung' + i, cyl(.02, .02, 1, 8), mats.chrome, [0, .3 + i * .3, 0], [0, 0, Math.PI / 2]));
  // Bolted wall flanges every 1.5 m so it reads as attached, not leaning.
  for (let i = 0, y = .8; y <= h - .3; i++, y = .8 + i * 1.5) {
    g.add(part('ladder_flange' + i, box(1.3, .1, .12), mats.steel_plate, [0, y, .12]));
    g.add(K.boltRing('ladder_flange_bolts' + i, .5, 2, y).translateZ(.18).rotateX(Math.PI / 2));
  }
  // Fall cage starts at 2.4 m — below that the climb is short enough not to need one.
  const hoops = [];
  for (let y = 2.4; y <= h - .6; y += .9) hoops.push(y);
  hoops.forEach((y, i) => g.add(part('ladder_hoop' + i, new THREE.TorusGeometry(.7, .025, 6, 16, Math.PI), mats.trim, [0, y, 0], [0, 0, 0])));
  if (hoops.length) {
    const len = h - hoops[0];
    for (const s of [-1, 1]) g.add(part(`ladder_hoop_rail${s}`, box(.03, len, .03), mats.trim, [s * .68, hoops[0] + len / 2, 0]));
  }
  // Intermediate rest landing: fixed-ladder practice breaks any run over ~9 m.
  // Cantilevers out on −Z, the climb side — +Z is the mounting face (flanges sit at
  // z .06–.18), so a landing there would bury itself in the wall it bolts to.
  // Offset a half pitch off centre so the slab lands between rungs, not through one.
  if (o.landing) {
    const y = Math.round(h / 2 / .3) * .3 + .15;
    g.add(part('ladder_landing', box(1.5, .06, 1.0), mats.steel_plate, [0, y, -.54]));
    for (const s of [-1, 1]) g.add(part(`ladder_landing_rail${s}`, cyl(.03, .03, 1.0, 8), mats.rail_steel || mats.trim, [s * .74, y + 1.05, -.54], [Math.PI / 2, 0, 0]));
    for (const s of [-1, 1]) g.add(part(`ladder_landing_post${s}`, box(.06, 1.05, .06), mats.trim, [s * .74, y + .53, -1.0]));
    g.add(part('ladder_landing_kick', box(1.5, .1, .04), mats.trim, [0, y + .08, -1.02]));
    g.add(K.hazardStripes('ladder_landing_hazard', 1.4, .05, [0, y + .04, -.04], [0, 0, 0], 7));
  }
  for (const s of [-1, 1]) g.add(part(`ladder_hook${s}`, new THREE.TorusGeometry(.3, .03, 6, 12, Math.PI), mats.brass, [s * .5, h + .3, .15], [0, Math.PI / 2, 0]));
  g.add(K.hazardStripes('ladder_foot_hazard', 1.2, .06, [0, .05, .08], [0, 0, 0], 6));
  return g;
}

/* Heights the maps actually need: [id suffix, metres, label context, options].
   `shared_ladder` keeps the bare id at 6 m so existing placements stay valid. */
[
  ['', 6, 'Foundry yard → upper deck (y 6)'],
  ['_250', 2.5, 'railcar roofs, kerb-wall hops — under the cage threshold, so no hoops'],
  ['_500', 5, 'Switchyard yard → mid deck (y 5)'],
  ['_540', 5.4, 'Switchyard mid deck (y 4.8) → catwalk (y 10.2)'],
  ['_1000', 10, 'ground → catwalk in one run', { landing: true }],
].forEach(([sfx, h, use, o]) => {
  const id = 'shared_ladder' + sfx, hoops = h >= 3;
  P({ id, label: `Ladder (${h} m)`, size: `${h} m`,
    stats: { Rungs: '30 cm', Cage: hoops ? 'from 2.4 m' : 'none', Area: `1.6×${(h + .4).toFixed(1)}×1.4` },
    note: `Fixed ${h} m ladder — ${use}. Rungs are a true 30 cm pitch at every height in the set, so no placement should ever scale one: pick the height instead.`
      + (hoops ? ' Cage hoops from 2.4 m with side rails to the top.' : '')
      + (o?.landing ? ' Breaks at half height for an intermediate rest landing with a guard rail, per fixed-ladder practice for runs over 9 m.' : '')
      + ' Bolted wall flanges every 1.5 m so it reads as attached, not leaning.',
    build(K) { return ladderRig(K, id, h, o || {}); } });
});

/* Zipline anchor: 1.2 m post on a base plate, sheave wheel, tension turnbuckle, interact lamp. */
P({ id: 'shared_zipline_anchor', label: 'Zipline anchor', size: '1.8 m', stats: { Interact: 'E', Cable: 'from sheave' },
  note: 'Post with a top sheave at 1.6 m — the cable leaves from `zip_sheave`. Brass lamp is the interact prompt anchor.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_zipline_anchor');
    g.add(part('zip_base', box(.9, .1, .9), mats.steel_plate, [0, .05, 0]));
    g.add(K.boltRing('zip_base_bolts', .38, 4, .11));
    g.add(part('zip_post', cyl(.14, .16, 1.5, 10), mats.steel_hull, [0, .85, 0]));
    g.add(part('zip_post_cap', cyl(.18, .14, .1, 10), mats.trim, [0, 1.65, 0]));
    const sh = grp('zip_sheave', [0, 1.6, .2]);
    sh.add(part('zip_sheave_wheel', cyl(.22, .22, .06, 20), mats.chrome, [0, 0, 0], [Math.PI / 2, 0, 0]));
    sh.add(part('zip_sheave_rim', new THREE.TorusGeometry(.22, .03, 8, 24), mats.trim, [0, 0, 0]));
    sh.add(part('zip_sheave_fork', box(.06, .5, .12), mats.steel_plate, [0, -.1, -.05]));
    g.add(sh);
    g.add(part('zip_turnbuckle', cyl(.05, .05, .4, 8), mats.brass, [.3, 1.1, 0], [0, 0, .5]));
    g.add(part('zip_brace', box(.08, 1.4, .08), mats.steel_hull, [.4, .72, -.3], [-.2, 0, .3]));
    g.add(part('zip_lamp', box(.12, .12, .04), mats.energy_fuse, [0, 1.2, -.17]));
    g.add(K.hazardStripes('zip_hazard', .9, .06, [0, .12, .46], [0, 0, 0], 5));
    return g;
  } });

P({ id: 'shared_zipline_cable', label: 'Zipline cable (1 m)', size: 'unit', stats: { Scale: 'Z to span', Sag: 'client' },
  note: 'Unit-length twisted cable along −Z with a clamp at each end; the client scales Z to the anchor distance.',
  build(K) {
    const { part, grp, cyl, mats, THREE } = K, g = grp('shared_zipline_cable');
    g.add(part('zipcable_core', cyl(.03, .03, 1, 8), mats.chrome, [0, 0, -.5], [Math.PI / 2, 0, 0]));
    for (let i = 0; i < 12; i++) g.add(part('zipcable_twist' + i, new THREE.TorusGeometry(.032, .006, 4, 10), mats.trim, [0, 0, -.04 - i * .083]));
    for (const z of [0, -1]) g.add(part('zipcable_clamp' + z, cyl(.05, .05, .08, 8), mats.steel_plate, [0, 0, z], [Math.PI / 2, 0, 0]));
    return g;
  } });

P({ id: 'shared_zipline_trolley', label: 'Zipline trolley', size: '0.9 m', stats: { Hangs: 'from cable', Grip: 'T-bar' },
  note: 'Twin-wheel trolley with a hanging T-bar handle and a brake lever; origin at the cable line.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_zipline_trolley');
    g.add(part('trolley_frame', box(.14, .3, .5), mats.steel_hull, [0, -.05, 0]));
    for (const z of [-.16, .16]) g.add(part('trolley_wheel' + z, cyl(.1, .1, .06, 16), mats.chrome, [0, .04, z], [0, 0, Math.PI / 2]));
    for (const z of [-.16, .16]) g.add(part('trolley_wheel_guard' + z, new THREE.TorusGeometry(.11, .015, 6, 16, Math.PI), mats.trim, [0, .04, z], [0, Math.PI / 2, 0]));
    g.add(part('trolley_brake', box(.03, .25, .04), mats.hazard, [.1, -.1, .1], [0, 0, -.5]));
    g.add(part('trolley_strap', cyl(.02, .02, .5, 8), mats.rubber, [0, -.45, 0]));
    g.add(part('trolley_tbar', cyl(.03, .03, .7, 10), mats.steel_plate, [0, -.72, 0], [0, 0, Math.PI / 2]));
    for (const s of [-1, 1]) g.add(part(`trolley_grip${s}`, cyl(.036, .036, .2, 10), mats.grip_rubber || mats.rubber, [s * .22, -.72, 0], [0, 0, Math.PI / 2]));
    g.add(part('trolley_lamp', box(.08, .04, .04), mats.energy_fuse, [0, -.2, .26]));
    return g;
  } });

/* Hero launcher pad, 2.2 m: three states. */
const launcher = (state) => P({ id: 'shared_launcher_' + state, label: `Hero launcher — ${state}`, size: '2.2 m', swatch: '#ff6f1a', stats: { Area: '2.2×1.2×2.2', Arcs: 'onto deck' },
  note: { idle: 'Pad flush, buff-orange chevrons dim: waiting.', charging: 'Pad raised 30 cm on the piston, chevrons lit, ring glowing — you are on it.', fired: 'Pad at full 1 m throw with the piston extended and steam ports open.' }[state],
  build(K) {
    const { part, grp, box, cyl, mats, THREE, D } = K, g = grp('shared_launcher_' + state);
    const lit = state !== 'idle', em = lit ? mats.energy_buff : mats.trim, ext = { idle: 0, charging: .3, fired: 1.0 }[state];
    g.add(part('launcher_base', cyl(1.1, 1.15, .2, 8), mats.chassis, [0, .1, 0]));
    g.add(K.hazardStripes('launcher_hazard', 1.6, .08, [0, .12, 1.1], [0, 0, 0], 8));
    g.add(K.boltRing('launcher_bolts', 1.0, 8, .21));
    g.add(part('launcher_cylinder', cyl(.35, .4, .3, 16), mats.steel_hull, [0, .35, 0]));
    g.add(part('launcher_piston', cyl(.2, .2, ext + .1, 14), mats.chrome, [0, .5 + ext / 2, 0]));
    g.add(part('launcher_pad', cyl(1.0, 1.05, .12, 8), mats.steel_plate, [0, .56 + ext, 0]));
    g.add(part('launcher_ring', new THREE.TorusGeometry(.8, .04, 8, 32), em, [0, .63 + ext, 0], [Math.PI / 2, 0, 0]));
    for (let i = 0; i < 3; i++) g.add(part('launcher_chevron' + i, box(.6 - i * .15, .02, .16), em, [0, .63 + ext, -.2 - i * .25]));
    // Guide posts telescope out of the base with the pad: flush stubs when idle, rising with the throw.
    if (lit) for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2 + Math.PI / 4, h = .3 + ext; g.add(part('launcher_guide' + i, box(.1, h, .1), mats.trim, [Math.cos(a) * 1.0, .2 + h / 2, Math.sin(a) * 1.0])); }
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2 + Math.PI / 4; g.add(part('launcher_guide_socket' + i, box(.16, .04, .16), mats.trim, [Math.cos(a) * 1.0, .21, Math.sin(a) * 1.0])); }
    if (state === 'fired') for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part('launcher_steam' + i, new THREE.ConeGeometry(.15, .5, 8), mats.steam || mats.steel_plate, [Math.cos(a) * .5, .5, Math.sin(a) * .5], [Math.sin(a) * 1.2, 0, -Math.cos(a) * 1.2])); }
    return g;
  } });
['idle', 'charging', 'fired'].forEach(launcher);

P({ id: 'shared_vent_grate', label: 'Vent grate', size: '3 m', stats: { Opening: '3.0×1.4', Hero: 'only' },
  note: 'Tunnel-mouth frame with a hinged bar grate swung open, warning lamp and hazard lip — marks the hero-only shortcut.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_vent_grate');
    g.add(part('vent_frame_top', box(3.4, .25, .3), mats.steel_hull, [0, 1.52, 0]));
    for (const s of [-1, 1]) g.add(part(`vent_frame_side${s}`, box(.25, 1.4, .3), mats.steel_hull, [s * 1.58, .7, 0]));
    g.add(K.boltRing('vent_frame_bolts', 1.4, 6, 1.6).translateZ(.16).rotateX(Math.PI / 2));
    const grate = grp('vent_grate_door', [-1.45, 0, .15]); grate.rotation.y = -1.2;
    grate.add(part('vent_grate_frame', box(2.9, 1.3, .05), mats.trim, [1.45, .7, 0]));
    for (let i = 0; i < 9; i++) grate.add(part('vent_grate_bar' + i, cyl(.02, .02, 1.2, 6), mats.chrome, [.3 + i * .3, .7, 0]));
    grate.add(part('vent_grate_hinge', cyl(.05, .05, 1.3, 8), mats.steel_plate, [0, .7, 0]));
    g.add(grate);
    g.add(part('vent_lamp', box(.3, .1, .1), mats.energy_fuse, [0, 1.5, -.18]));
    g.add(K.hazardStripes('vent_hazard', 3.0, .06, [0, .04, -.16], [0, 0, 0], 10));
    g.add(K.louvres('vent_louvre_l', .4, 1.0, 5).translateX(-1.9).translateY(.75));
    g.add(K.louvres('vent_louvre_r', .4, 1.0, 5).translateX(1.9).translateY(.75));
    return g;
  } });

/* Control point: 3 m pad, three states. */
const cp = (state) => P({ id: 'shared_controlpoint_' + state, label: `Control point — ${state}`, size: '3 m', swatch: '#4fd18a', stats: { Area: '3×1.5×3', Opens: 'bonus sightline' },
  note: { neutral: 'Pad with a dark pylon and a hollow ring: nobody home.', capturing: 'Ring lit in a partial arc (240°), pylon lamps climbing, relay dish raised halfway.', held: 'Full ring, all lamps, relay dish up and pointed at the bonus socket.' }[state],
  build(K) {
    const { part, grp, box, cyl, mats, THREE, D } = K, g = grp('shared_controlpoint_' + state);
    const em = mats.energy_scan, on = state !== 'neutral', arc = state === 'capturing' ? 240 * D : Math.PI * 2;
    g.add(part('cp_pad', cyl(1.5, 1.55, .2, 6), mats.chassis, [0, .1, 0]));
    g.add(part('cp_pad_inlay', cyl(1.2, 1.2, .02, 6), mats.steel_plate, [0, .21, 0]));
    g.add(part('cp_ring', new THREE.TorusGeometry(1.35, .04, 8, 36, arc), on ? em : mats.trim, [0, .22, 0], [Math.PI / 2, 0, 0]));
    g.add(part('cp_pylon', box(.4, 1.6, .4), mats.steel_hull, [0, 1.0, 0]));
    for (let i = 0; i < 4; i++) g.add(part('cp_lamp' + i, box(.42, .06, .06), (state === 'held' || (state === 'capturing' && i < 2)) ? em : mats.trim, [0, .5 + i * .3, 0]));
    const dish = grp('cp_dish', [0, 1.9, 0]); dish.rotation.x = state === 'neutral' ? 0 : state === 'capturing' ? -.6 : -1.2;
    dish.add(part('cp_dish_mast', cyl(.04, .04, .4, 8), mats.chrome, [0, .2, 0]));
    dish.add(part('cp_dish_plate', new THREE.CylinderGeometry(.35, .1, .12, 12, 1, true), mats.steel_plate, [0, .45, 0]));
    dish.add(part('cp_dish_feed', new THREE.SphereGeometry(.05, 8, 6), on ? em : mats.trim, [0, .55, 0]));
    g.add(dish);
    g.add(K.hazardStripes('cp_hazard', 1.5, .06, [0, .1, 1.53], [0, 0, 0], 6));
    for (const s of [-1, 1]) g.add(part(`cp_console${s}`, box(.3, .4, .2), mats.trim, [s * .5, .4, -.9], [-.5, 0, 0]));
    for (const s of [-1, 1]) g.add(part(`cp_screen${s}`, box(.24, .2, .02), on ? em : mats.optic_glass, [s * .5, .45, -1.0], [-.5, 0, 0]));
    return g;
  } });
['neutral', 'capturing', 'held'].forEach(cp);

P({ id: 'shared_armory_kiosk', label: 'Armory kiosk', size: '2.5 m', swatch: '#f0c83a', stats: { Area: '7×4×7', Menu: 'gunsmith' },
  note: 'Vending-machine armory: brass-trimmed cabinet with a display rack of three weapon silhouettes, a workbench lip, ammo drawers, an overhead sign lamp and a ground floodlight ring so it can be found from the yard.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_armory_kiosk');
    g.add(part('armory_plinth', box(2.8, .2, 2.0), mats.chassis, [0, .1, 0]));
    g.add(part('armory_cabinet', box(2.4, 2.3, 1.2), mats.steel_hull, [0, 1.35, .3]));
    g.add(part('armory_cabinet_trim', box(2.5, .08, 1.3), mats.brass, [0, 2.5, .3]));
    g.add(part('armory_window', box(2.0, 1.2, .04), mats.optic_glass, [0, 1.7, -.32]));
    for (let i = 0; i < 3; i++) g.add(part('armory_rack_gun' + i, box(.9, .08, .06), mats.trim, [-.4 + i * .05, 1.35 + i * .3, -.2], [0, 0, .1]));
    for (let i = 0; i < 3; i++) g.add(part('armory_rack_lamp' + i, box(.06, .06, .02), mats.energy_fuse, [.8, 1.35 + i * .3, -.31]));
    g.add(part('armory_bench', box(2.4, .1, .7), mats.steel_plate, [0, 1.0, -.6]));
    g.add(part('armory_bench_vise', box(.25, .2, .25), mats.trim, [.9, 1.15, -.65]));
    for (let i = 0; i < 4; i++) g.add(part('armory_drawer' + i, box(.5, .16, .04), mats.trim, [-.9 + i * .55, .5, -.32]));
    for (let i = 0; i < 4; i++) g.add(part('armory_drawer_handle' + i, cyl(.012, .012, .2, 6), mats.brass, [-.9 + i * .55, .5, -.36], [0, 0, Math.PI / 2]));
    g.add(part('armory_sign', box(1.6, .4, .1), mats.chassis, [0, 2.85, -.2]));
    g.add(part('armory_sign_face', box(1.5, .3, .02), mats.energy_fuse, [0, 2.85, -.26]));
    g.add(part('armory_sign_lamp', cyl(.06, .06, 1.6, 8), mats.brass, [0, 3.1, -.3], [0, 0, Math.PI / 2]));
    g.add(K.louvres('armory_vents', 1.0, .5, 5).translateY(1.8).translateZ(.91).rotateY(Math.PI));
    g.add(K.hazardStripes('armory_hazard', 2.8, .08, [0, .12, -1.01], [0, 0, 0], 8));
    g.add(part('armory_floor_ring', new THREE.RingGeometry(1.9, 2.1, 32), mats.energy_fuse, [0, .01, -.4], [-Math.PI / 2, 0, 0]));
    return g;
  } });

/* Core: the thing to protect. */
const core = (hit) => P({ id: hit ? 'shared_core_hit' : 'shared_core', label: hit ? 'Core — hit' : 'Core', size: '6 m', swatch: '#65dce4', stats: { Lives: 'HUD', Route: 'goal' },
  note: hit ? 'Breach frame: containment ring cracked open, arcs leaking, warning lamps red.' : 'A 4 m containment sphere in a gantry ring on a stepped plinth, four coolant stacks, gate arches on the route side. Big enough to read from spawn.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp(hit ? 'shared_core_hit' : 'shared_core');
    const em = hit ? mats.weak || mats.energy_buff : mats.energy_flak;
    g.add(part('core_plinth', cyl(3.2, 3.5, .5, 8), mats.chassis, [0, .25, 0]));
    g.add(part('core_plinth_step', cyl(2.4, 2.6, .4, 8), mats.steel_plate, [0, .7, 0]));
    g.add(K.hazardStripes('core_hazard', 4.6, .1, [0, .3, 3.5], [0, 0, 0], 12));
    g.add(part('core_sphere', new THREE.IcosahedronGeometry(1.8, 2), em, [0, 3.0, 0]));
    g.add(part('core_cage_ring_a', new THREE.TorusGeometry(2.1, .08, 8, 40), mats.steel_hull, [0, 3.0, 0], [Math.PI / 2, 0, 0]));
    g.add(part('core_cage_ring_b', new THREE.TorusGeometry(2.1, .08, 8, 40), mats.steel_hull, [0, 3.0, 0], [0, 0, 0]));
    g.add(part('core_cage_ring_c', new THREE.TorusGeometry(2.1, .06, 8, 40), mats.trim, [0, 3.0, 0], [0, Math.PI / 2, 0]));
    for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2 + Math.PI / 4; g.add(part('core_stack' + i, cyl(.35, .45, 5, 10), mats.steel_hull, [Math.cos(a) * 2.8, 2.5, Math.sin(a) * 2.8])); g.add(K.finStack('core_stack_fins' + i, 6, .1, .8, .6).translateX(Math.cos(a) * 2.8).translateY(4.2).translateZ(Math.sin(a) * 2.8)); g.add(part('core_stack_lamp' + i, cyl(.2, .2, .1, 10), hit ? mats.weak : mats.energy_flak, [Math.cos(a) * 2.8, 5.05, Math.sin(a) * 2.8])); }
    for (const s of [-1, 1]) g.add(part(`core_gate_post${s}`, box(.5, 4, .5), mats.steel_hull, [s * 2.2, 2, -3.2]));
    g.add(part('core_gate_lintel', box(5.0, .5, .6), mats.steel_plate, [0, 4.2, -3.2]));
    g.add(part('core_gate_sign', box(3, .3, .05), em, [0, 3.7, -3.5]));
    if (hit) {
      for (let i = 0; i < 6; i++) { const a = i * 1.05; g.add(part('core_arc' + i, box(.06, .06, 1.6), mats.energy_arc, [Math.cos(a) * 2.2, 3.0 + Math.sin(a) * 1.2, Math.sin(a) * .5], [a, .4, 0])); }
      g.add(part('core_crack', box(.1, 3.5, .1), mats.chassis, [0, 3.0, 2.0], [0, 0, .3]));
    }
    return g;
  } });
core(false); core(true);

P({ id: 'shared_spawn_portal', label: 'Spawn portal', size: '5 m', swatch: '#e9614c', stats: { Route: 'start', Marker: '3 m cube' },
  note: 'A 4 m blast-door arch with the door irised half-open, threat-red inner glow, siren stacks on the piers, rubble skirt. Sits over the route\'s first waypoint facing +X along the lane.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE, D } = K, g = grp('shared_spawn_portal');
    for (const s of [-1, 1]) { g.add(part(`portal_pier${s}`, box(1.0, 4.5, 1.2), mats.chassis, [s * 2.5, 2.25, 0])); g.add(part(`portal_siren${s}`, cyl(.2, .25, .4, 10), mats.hazard, [s * 2.5, 4.7, 0])); g.add(part(`portal_siren_lamp${s}`, cyl(.14, .14, .2, 10), mats.weak || mats.threat, [s * 2.5, 5.0, 0])); }
    g.add(part('portal_lintel', box(6.0, .8, 1.2), mats.steel_hull, [0, 4.4, 0]));
    g.add(K.hazardStripes('portal_hazard', 5.0, .16, [0, 4.4, -.61], [0, 0, 0], 10));
    g.add(part('portal_glow', new THREE.CylinderGeometry(1.7, 1.7, .1, 20), mats.weak || mats.energy_buff, [0, 2.0, .2], [Math.PI / 2, 0, 0]));
    for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; g.add(part('portal_iris' + i, box(1.4, 2.2, .2), mats.steel_plate, [Math.cos(a) * 1.3, 2.0 + Math.sin(a) * 1.3, 0], [0, 0, a + .8])); }
    g.add(part('portal_ring', new THREE.TorusGeometry(2.0, .15, 8, 32), mats.trim, [0, 2.0, 0]));
    for (let i = 0; i < 7; i++) g.add(part('portal_rubble' + i, new THREE.DodecahedronGeometry(.25 + (i % 3) * .1, 0), mats.concrete, [-3 + i * 1.0, .15, -1.0 + (i % 2) * .5]));
    return g;
  } });

P({ id: 'shared_airlane_pylon', label: 'Air-lane pylon', size: '9 m', swatch: '#22d3ee', stats: { Strand: 'at 8–10 m', Spacing: 'per waypoint' },
  note: 'Lattice mast carrying the visible air-lane strand: cyan beacon ring at the top, guy-wire eyelets, a hazard collar at 2 m. Place one per air waypoint; the strand mesh is a client spline between beacons.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_airlane_pylon');
    g.add(part('pylon_base', box(1.6, .3, 1.6), mats.chassis, [0, .15, 0]));
    g.add(K.boltRing('pylon_bolts', .65, 4, .31));
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) g.add(part(`pylon_leg${sx}${sz}`, box(.12, 8.5, .12), mats.steel_hull, [sx * .45, 4.5, sz * .45], [sz * .02, 0, -sx * .02]));
    for (let i = 0; i < 8; i++) for (const s of [-1, 1]) g.add(part(`pylon_brace${i}${s}`, box(.05, 1.3, .05), mats.trim, [s * .45, 1.0 + i * 1.0, 0], [.7 * (i % 2 ? 1 : -1), 0, 0]));
    g.add(K.hazardStripes('pylon_collar', 1.2, .12, [0, 2.0, .5], [0, 0, 0], 4));
    g.add(part('pylon_head', cyl(.5, .3, .4, 8), mats.steel_plate, [0, 8.9, 0]));
    g.add(part('pylon_beacon', new THREE.TorusGeometry(.6, .06, 8, 24), mats.energy_flak, [0, 9.0, 0], [Math.PI / 2, 0, 0]));
    g.add(part('pylon_beacon_core', new THREE.SphereGeometry(.15, 10, 8), mats.energy_flak, [0, 9.0, 0]));
    for (let i = 0; i < 3; i++) { const a = i / 3 * Math.PI * 2; g.add(part('pylon_eyelet' + i, new THREE.TorusGeometry(.08, .02, 6, 10), mats.brass, [Math.cos(a) * .5, 8.6, Math.sin(a) * .5], [Math.PI / 2, 0, 0])); }
    return g;
  } });

/* Socket markers: 2.2 m plates. */
const socket = (id, label, tag, base, note) => P({ id, label, size: '2.2 m', swatch: { ground: '#7fe65a', wall: '#6f9be6', trap: '#c9a04a', barricade: '#e9614c' }[tag], stats: { Tag: tag, State: base ? 'built' : 'empty' }, note,
  build(K) {
    const { part, grp, box, cyl, mats, THREE, D } = K, g = grp(id);
    const em = { ground: mats.energy_scan, wall: mats.energy_rail, trap: mats.energy_fuse, barricade: mats.weak || mats.energy_buff }[tag];
    g.add(part(id + '_plate', cyl(1.1, 1.15, .12, tag === 'trap' ? 8 : 12), mats.chassis, [0, .06, 0]));
    g.add(part(id + '_inlay', cyl(.95, .95, .02, tag === 'trap' ? 8 : 12), mats.steel_plate, [0, .13, 0]));
    g.add(K.boltRing(id + '_bolts', 1.02, tag === 'trap' ? 8 : 12, .13));
    if (base) {
      g.add(part(id + '_mount', cyl(.6, .7, .2, 8), mats.steel_hull, [0, .24, 0]));
      g.add(part(id + '_mount_ring', new THREE.TorusGeometry(.62, .03, 6, 24), em, [0, .35, 0], [Math.PI / 2, 0, 0]));
      for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part(id + '_clamp' + i, box(.2, .16, .3), mats.trim, [Math.cos(a) * .75, .22, Math.sin(a) * .75], [0, -a, 0])); }
      g.add(K.cableRun(id + '_feed', [[.9, .1, .5], [.75, .2, .2], [.6, .3, 0]], .025));
    } else {
      g.add(part(id + '_ring', new THREE.TorusGeometry(.8, .025, 6, tag === 'trap' ? 8 : 32), em, [0, .15, 0], [Math.PI / 2, 0, 0]));
      for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part(id + '_tick' + i, box(.06, .01, .3), em, [Math.cos(a) * .55, .15, Math.sin(a) * .55], [0, -a, 0])); }
      if (tag === 'barricade') g.add(K.hazardStripes(id + '_hazard', 1.8, .06, [0, .14, .0], [0, 0, 0], 6));
    }
    if (tag === 'wall') { g.add(part(id + '_stanchion', box(.3, 1.2, .3), mats.steel_hull, [0, .6, 1.0])); g.add(part(id + '_stanchion_lamp', box(.32, .08, .08), em, [0, 1.1, 1.0])); }
    return g;
  } });
socket('socket_ground_empty', 'Socket — ground, empty', 'ground', false, 'Ring-lit plate: buildable. Green scan ring, four ticks.');
socket('socket_ground_base', 'Socket — ground, built', 'ground', true, 'Same plate with the tower mount collar, clamps and a power feed — the tower chassis sits on `_mount`.');
socket('socket_wall_empty', 'Socket — wall, empty', 'wall', false, 'Deck-edge plate with a stanchion so it reads as an elevated socket; rail-blue ring.');
socket('socket_wall_base', 'Socket — wall, built', 'wall', true, 'Deck-edge plate with mount collar and stanchion.');
socket('socket_trap_empty', 'Socket — trap, empty', 'trap', false, 'Octagonal plate flush with the path, fuse-gold ring — a trap goes here.');
socket('socket_barricade_empty', 'Socket — barricade, empty', 'barricade', false, 'Threat-red ring with a hazard band across the cut: the barricade lesson.');

/* ── M3 traversal + economy (Spire debuts these) ─────────────────────── */
const tele = (state) => P({ id: 'shared_teleporter_pad_' + state, label: `Teleporter pad — ${state}`, size: '3 m', swatch: '#65dce4', stats: { Area: '3.2×2.5×3.2', Pair: 'lobby ↔ roof', Cooldown: 'personal' },
  note: { idle: 'Hex pad with a dark ring and folded emitter petals: available.', charged: 'Petals raised, ring lit teal, a light column forming — you are being sent.', cooldown: 'Petals down, ring amber with a sweeping progress arc (`tele_arc`), column gone: wait.' }[state],
  build(K) {
    const { part, grp, box, cyl, mats, THREE, D } = K, g = grp('shared_teleporter_pad_' + state);
    const on = state === 'charged', cool = state === 'cooldown', em = on ? mats.energy_flak : cool ? mats.energy_fuse : mats.trim;
    g.add(part('tele_base', cyl(1.6, 1.7, .18, 6), mats.chassis, [0, .09, 0]));
    g.add(part('tele_plate', cyl(1.3, 1.3, .06, 6), mats.steel_plate, [0, .21, 0]));
    g.add(part('tele_ring', new THREE.TorusGeometry(1.1, .05, 8, 36, cool ? Math.PI * 1.3 : Math.PI * 2), em, [0, .25, 0], [Math.PI / 2, 0, 0]));
    if (cool) g.add(part('tele_arc', new THREE.TorusGeometry(1.1, .05, 8, 36, Math.PI * .7), mats.trim, [0, .25, 0], [Math.PI / 2, 0, Math.PI * 1.3]));
    for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2, p = grp('tele_petal' + i, [Math.cos(a) * 1.35, .24, Math.sin(a) * 1.35]); p.rotation.y = -a; p.rotation.z = on ? -1.1 : -.15; p.add(part('tele_petal_blade' + i, box(.5, .06, .3), mats.steel_hull, [.25, 0, 0])); p.add(part('tele_petal_lamp' + i, box(.3, .02, .1), em, [.3, .04, 0])); g.add(p); }
    g.add(K.boltRing('tele_bolts', 1.5, 6, .19));
    g.add(part('tele_core', cyl(.35, .4, .04, 12), em, [0, .26, 0]));
    if (on) { g.add(part('tele_column', new THREE.CylinderGeometry(.9, 1.1, 2.4, 16, 1, true), mats.shield_bubble || em, [0, 1.45, 0])); for (let i = 0; i < 4; i++) g.add(part('tele_column_ring' + i, new THREE.TorusGeometry(.95, .02, 6, 32), em, [0, .6 + i * .55, 0], [Math.PI / 2, 0, 0])); }
    g.add(part('tele_console', box(.5, .9, .3), mats.chassis, [1.9, .45, -.9], [0, .6, 0])); g.add(part('tele_console_screen', box(.36, .24, .03), em, [1.85, .72, -.83], [-.3, .6, 0]));
    return g;
  } });
['idle', 'charged', 'cooldown'].forEach(tele);

P({ id: 'shared_snipernest', label: 'Sniper nest', size: '5×3 m', swatch: '#b08a3e', stats: { Area: '5×3×5', Bonus: 'hero perch', Reach: 'climb only' },
  note: 'A perch that only pays if you commit to the climb: a 5 m grated platform with a waist-high sandbag-and-plate parapet on three sides, a swing-out rifle rest with a brass rangefinder post, a spotting scope on a tripod, an ammo crate and a shaded lamp. Open side faces the ladder.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_snipernest');
    g.add(part('nest_deck', box(5, .1, 5), mats.deck_grate || mats.steel_hull, [0, .25, 0]));
    for (let i = 0; i < 12; i++) g.add(part('nest_grate' + i, box(4.8, .03, .04), mats.trim, [0, .29, -2.2 + i * .4]));
    for (const [x, z, w] of [[-2.4, 0, 5], [2.4, 0, 5], [0, -2.4, 5]]) { const along = z === 0; g.add(part(`nest_edge${x}${z}`, box(along ? .2 : w, .3, along ? w : .2), mats.steel_hull, [x, .15, z])); }
    for (const [x, z, r] of [[-2.2, 0, 0], [2.2, 0, 0], [0, -2.2, Math.PI / 2]]) { for (let i = 0; i < 6; i++) { const t = -2 + i * .8; g.add(part(`nest_bag${x}${z}${i}`, new THREE.SphereGeometry(.34, 10, 8), mats.strap || mats.polymer, [x + (r ? t : 0), .5 + (i % 2) * .05, z + (r ? 0 : t)])); g.getObjectByName(`nest_bag${x}${z}${i}`).scale.set(r ? 1.2 : .7, .55, r ? .7 : 1.2); } g.add(part(`nest_plate${x}${z}`, box(r ? 4.6 : .06, .5, r ? .06 : 4.6), mats.steel_plate, [x + (r ? 0 : Math.sign(x) * .3), 1.0, z + (r ? -.3 : 0)])); }
    g.add(part('nest_rest_arm', box(.08, .08, 1.2), mats.steel_hull, [-1.2, 1.05, -1.6], [0, .4, 0])); g.add(part('nest_rest_pad', box(.4, .08, .16), mats.rubber, [-1.0, 1.1, -2.1]));
    g.add(part('nest_rf_post', cyl(.03, .04, .9, 8), mats.brass, [1.6, .75, -2.0])); g.add(part('nest_rf_lens', cyl(.06, .06, .08, 12), mats.optic_glass, [1.6, 1.25, -2.05], [Math.PI / 2, 0, 0]));
    for (let i = 0; i < 3; i++) { const a = i / 3 * Math.PI * 2; g.add(part('nest_tripod' + i, cyl(.02, .02, 1.2, 6), mats.trim, [1.2 + Math.cos(a) * .3, .85, .8 + Math.sin(a) * .3], [Math.sin(a) * .35, 0, -Math.cos(a) * .35])); }
    g.add(part('nest_scope', cyl(.05, .07, .5, 12), mats.gun_dark || mats.trim, [1.2, 1.5, .8], [Math.PI / 2 - .2, 0, 0]));
    g.add(part('nest_crate', box(.8, .5, .5), mats.chassis, [-1.6, .55, 1.6])); g.add(K.hazardStripes('nest_crate_hazard', .7, .06, [-1.6, .55, 1.86], [0, 0, 0], 4));
    g.add(part('nest_lamp_post', cyl(.03, .03, 1.6, 8), mats.steel_hull, [2.2, 1.1, 2.2])); g.add(part('nest_lamp_hood', new THREE.ConeGeometry(.25, .2, 10), mats.trim, [2.0, 1.95, 2.0])); g.add(part('nest_lamp', new THREE.SphereGeometry(.08, 8, 6), mats.lamp_warm || mats.energy_fuse, [2.0, 1.85, 2.0]));
    return g;
  } });

const cache = (opened) => P({ id: opened ? 'shared_cache_opened' : 'shared_cache_hidden', label: opened ? 'Scrap cache — opened' : 'Scrap cache — hidden', size: '1.2 m', swatch: '#c9a04a', stats: { Yield: 'bonus scrap', Sweep: 'inert', Find: 'exploration' },
  note: opened ? 'Lid thrown back, brass interior lit, four scrap chunks visible inside, a spent lock.' : 'A strapped brass-cornered crate half under a tarp with a faint locator lamp — meant to be noticed, not signposted.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp(opened ? 'shared_cache_opened' : 'shared_cache_hidden');
    g.add(part('cache_body', box(1.2, .7, .8), mats.chassis, [0, .35, 0]));
    for (const [x, z] of [[-.6, -.4], [.6, -.4], [-.6, .4], [.6, .4]]) g.add(part(`cache_corner${x}${z}`, box(.1, .72, .1), mats.brass, [x, .36, z]));
    for (const x of [-.35, .35]) g.add(part('cache_strap' + x, box(.08, .74, .84), mats.strap || mats.rubber, [x, .36, 0]));
    const lid = grp('cache_lid', [0, .7, .4]); lid.rotation.x = opened ? -2.2 : 0; lid.add(part('cache_lid_plate', box(1.22, .08, .82), mats.steel_hull, [0, .04, -.4])); lid.add(part('cache_lid_handle', new THREE.TorusGeometry(.1, .02, 6, 12, Math.PI), mats.brass, [0, .1, -.4])); g.add(lid);
    if (opened) { g.add(part('cache_glow', box(1.0, .02, .6), mats.energy_fuse, [0, .6, 0])); for (const [n, x, z, m] of [['alloy', -.3, -.1, mats.steel_plate], ['flux', .1, .15, mats.energy_flak], ['plating', .35, -.2, mats.chassis], ['gravium', -.05, -.25, mats.energy_field]]) g.add(part('cache_scrap_' + n, new THREE.DodecahedronGeometry(.11, 0), m, [x, .68, z], [x * 3, z * 3, 0])); g.add(part('cache_lock_spent', box(.14, .1, .06), mats.trim, [0, .2, .44], [0, 0, .5])); }
    else { g.add(part('cache_tarp', box(1.0, .04, .7), mats.polymer, [-.25, .74, -.1], [0, .1, .06])); g.add(part('cache_lock', box(.14, .12, .06), mats.brass, [0, .5, .44])); g.add(part('cache_lamp', new THREE.SphereGeometry(.03, 8, 6), mats.energy_fuse, [.5, .62, .42])); }
    return g;
  } });
cache(false); cache(true);

P({ id: 'shared_elevator_shaft', label: 'Cargo lift shaft (5 m bay)', size: '5×5×5 m', swatch: '#4a4f58', stats: { Stack: 'along Y', Rise: '40 m on Spire' },
  note: 'One 5 m bay of the open lift shaft: four corner columns with lattice bracing, guide rails on two faces, a floor-level landing gate on the third, a lit floor-number plate. Stack 8 for the Spire.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_elevator_shaft');
    for (const [x, z] of [[-2.4, -2.4], [2.4, -2.4], [-2.4, 2.4], [2.4, 2.4]]) g.add(part(`shaft_col${x}${z}`, box(.3, 5, .3), mats.steel_hull, [x, 2.5, z]));
    for (const y of [.15, 4.85]) for (const [x, z, w] of [[0, -2.4, 4.8], [0, 2.4, 4.8], [-2.4, 0, 4.8], [2.4, 0, 4.8]]) g.add(part(`shaft_beam${y}${x}${z}`, box(x ? .2 : w, .25, x ? w : .2), mats.steel_hull, [x, y, z]));
    for (const s of [-1, 1]) for (let i = 0; i < 2; i++) g.add(part(`shaft_brace${s}${i}`, box(.08, 6.6, .08), mats.trim, [s * 2.4, 2.5, 0], [i ? .75 : -.75, 0, 0]));
    for (const s of [-1, 1]) g.add(part('shaft_rail' + s, box(.12, 5, .2), mats.chrome, [s * 2.0, 2.5, -2.3]));
    g.add(part('shaft_gate_frame', box(3.2, 2.4, .1), mats.steel_hull, [0, 1.3, 2.42])); g.add(part('shaft_gate_void', box(2.8, 2.0, .06), mats.trim, [0, 1.3, 2.42])); for (let i = 0; i < 6; i++) g.add(part('shaft_gate_bar' + i, box(.05, 2.0, .08), mats.chrome, [-1.25 + i * .5, 1.3, 2.45]));
    g.add(K.hazardStripes('shaft_gate_hazard', 3.2, .08, [0, .3, 2.48], [0, 0, 0], 8));
    g.add(part('shaft_plate', box(.5, .3, .04), mats.trim, [1.9, 3.6, 2.44])); g.add(part('shaft_plate_lamp', box(.4, .2, .02), mats.energy_fuse, [1.9, 3.6, 2.47]));
    return g;
  } });

P({ id: 'shared_elevator', label: 'Cargo lift car', size: '4×3 m', swatch: '#d8a13a', stats: { Area: '4.4×3×4.4', Speed: 'slow — a decision', Interact: 'E' },
  note: 'The honest way up: an open cage car with a chequer-plate floor, waist rail, a roof frame with a hanging work lamp, a call panel with a lit floor readout (`lift_readout`), rubber buffers underneath and the winch cable rising from the roof yoke.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_elevator');
    g.add(part('lift_floor', box(4, .2, 4), mats.steel_plate, [0, .3, 0])); for (let i = 0; i < 8; i++) g.add(part('lift_tread' + i, box(3.8, .02, .06), mats.trim, [0, .41, -1.75 + i * .5]));
    for (const [x, z] of [[-1.9, -1.9], [1.9, -1.9], [-1.9, 1.9], [1.9, 1.9]]) g.add(part(`lift_post${x}${z}`, box(.12, 2.6, .12), mats.steel_hull, [x, 1.6, z]));
    for (const [x, z, w] of [[0, -1.9, 3.8], [-1.9, 0, 3.8], [1.9, 0, 3.8]]) { g.add(part(`lift_rail${x}${z}`, box(x ? .06 : w, .06, x ? w : .06), mats.chrome, [x, 1.3, z])); g.add(part(`lift_mesh${x}${z}`, box(x ? .02 : w, .8, x ? w : .02), mats.grating || mats.trim, [x, .85, z])); }
    g.add(part('lift_roof', box(4.2, .12, 4.2), mats.steel_hull, [0, 2.95, 0])); g.add(part('lift_yoke', box(.3, .6, .3), mats.steel_hull, [0, 3.3, 0])); g.add(part('lift_cable', cyl(.04, .04, 6, 8), mats.chrome, [0, 6.6, 0]));
    g.add(part('lift_lamp_cord', cyl(.01, .01, .5, 6), mats.rubber, [1.0, 2.65, -1.0])); g.add(part('lift_lamp_hood', new THREE.ConeGeometry(.2, .16, 10), mats.trim, [1.0, 2.36, -1.0])); g.add(part('lift_lamp', new THREE.SphereGeometry(.07, 8, 6), mats.lamp_warm || mats.energy_fuse, [1.0, 2.28, -1.0]));
    g.add(part('lift_panel', box(.36, .6, .1), mats.chassis, [-1.75, 1.5, -1.6], [0, Math.PI / 2, 0])); g.add(part('lift_readout', box(.24, .16, .03), mats.energy_fuse, [-1.69, 1.68, -1.6], [0, Math.PI / 2, 0])); for (let i = 0; i < 4; i++) g.add(part('lift_btn' + i, cyl(.03, .03, .02, 8), mats.chrome, [-1.69, 1.42 - Math.floor(i / 2) * .1, -1.66 + (i % 2) * .12], [0, 0, Math.PI / 2]));
    for (const [x, z] of [[-1.5, -1.5], [1.5, -1.5], [-1.5, 1.5], [1.5, 1.5]]) g.add(part(`lift_buffer${x}${z}`, cyl(.14, .16, .2, 8), mats.rubber, [x, .1, z]));
    g.add(K.hazardStripes('lift_edge_hazard', 3.8, .08, [0, .42, 1.98], [0, 0, 0], 8));
    return g;
  } });

/* ── M4 operated / dynamic map elements ───────────────────────────────── */
P({ id: 'shared_floodgate', label: 'Floodgate + lever', size: '5×4 m', swatch: '#4fc0e8', stats: { Interact: 'lever (E)', Cooldown: 'very long', Effect: 'purges one lane' },
  note: 'The panic button someone has to run to: a 5 m sluice frame across the lane mouth with a raised steel gate on chains, a header tank with sight-glass and overflow, hazard chevrons on the sill, and a big two-hand lever on a pedestal beside it with a lit ready lamp (`gate_lamp`). Gate slides down on `gate_leaf`; `vfx_lanewash` spawns at the sill.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_floodgate');
    for (const s of [-1, 1]) { g.add(part(`gate_pier${s}`, box(.6, 4.0, 1.0), mats.concrete, [s * 2.3, 2.0, 0])); g.add(part(`gate_pier_cap${s}`, box(.8, .2, 1.2), mats.steel_hull, [s * 2.3, 4.1, 0])); g.add(part(`gate_guide${s}`, box(.12, 3.2, .3), mats.chrome, [s * 1.95, 1.9, 0])); }
    g.add(part('gate_header', box(5.2, .6, 1.0), mats.steel_hull, [0, 4.5, 0])); g.add(part('gate_tank', cyl(.55, .55, 3.6, 16), mats.steel_plate, [0, 5.4, 0], [0, 0, Math.PI / 2])); g.add(part('gate_tank_glass', new THREE.TorusGeometry(.56, .05, 8, 24), mats.energy_flak, [0, 5.4, 0], [0, 0, Math.PI / 2])); g.add(part('gate_overflow', cyl(.08, .08, 4.2, 8), mats.trim, [2.2, 3.2, .6]));
    const leaf = grp('gate_leaf', [0, 2.6, 0]); leaf.add(part('gate_leaf_plate', box(3.9, 2.8, .16), mats.steel_plate, [0, 0, 0])); for (let i = 0; i < 4; i++) leaf.add(part('gate_leaf_rib' + i, box(3.9, .12, .06), mats.steel_hull, [0, -1.2 + i * .8, .11])); leaf.add(K.hazardStripes('gate_leaf_hazard', 3.6, .10, [0, -1.34, .12], [0, 0, 0], 8)); g.add(leaf);
    for (const s of [-1, 1]) for (let i = 0; i < 6; i++) g.add(part(`gate_chain${s}${i}`, new THREE.TorusGeometry(.05, .014, 5, 8), mats.chrome, [s * 1.6, 4.2 - i * .1, 0], [i % 2 ? 0 : Math.PI / 2, 0, 0]));
    g.add(part('gate_sill', box(4.2, .2, 1.2), mats.concrete, [0, .1, 0])); g.add(K.hazardStripes('gate_sill_hazard', 4.0, .08, [0, .21, .55], [0, 0, 0], 8));
    const ped = grp('gate_lever_ped', [3.4, 0, .9]); ped.add(part('gate_ped', box(.6, 1.0, .6), mats.chassis, [0, .5, 0])); ped.add(part('gate_ped_plate', box(.5, .06, .5), mats.steel_plate, [0, 1.02, 0])); const lever = grp('gate_lever', [0, 1.05, 0]); lever.rotation.x = -.6; lever.add(part('gate_lever_arm', cyl(.03, .04, 1.1, 8), mats.chrome, [0, .55, 0])); lever.add(part('gate_lever_knob', new THREE.SphereGeometry(.09, 10, 8), mats.hazard, [0, 1.1, 0])); ped.add(lever); ped.add(part('gate_lamp', new THREE.SphereGeometry(.06, 8, 6), mats.energy_scan, [.2, 1.1, .2])); g.add(ped);
    return g;
  } });

P({ id: 'shared_crusher', label: 'Crusher piston', size: '4×6 m', swatch: '#ff6f1a', stats: { 'Kill zone': '3×3 m', Operate: 'console socket', Cooldown: 'long' },
  note: 'Authored killzone: a hydraulic gantry over the lane with a 3 m square ram head on twin cylinders (`crusher_head` drives down 4 m), warning strobes on the frame, a scorched anvil plate in the road, and an operator console with a big red button off to the side.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_crusher');
    for (const [x, z] of [[-1.9, -1.9], [1.9, -1.9], [-1.9, 1.9], [1.9, 1.9]]) g.add(part(`crusher_leg${x}${z}`, box(.35, 6, .35), mats.steel_hull, [x, 3, z]));
    g.add(part('crusher_beam', box(4.4, .6, 4.4), mats.steel_hull, [0, 6.1, 0])); for (const s of [-1, 1]) g.add(part('crusher_strobe' + s, new THREE.SphereGeometry(.12, 8, 6), mats.weak || mats.energy_buff, [s * 1.6, 6.5, -2.2]));
    for (const s of [-1, 1]) { g.add(part('crusher_cyl' + s, cyl(.28, .28, 2.2, 14), mats.steel_plate, [s * .9, 4.7, 0])); g.add(part('crusher_rod' + s, cyl(.16, .16, 1.6, 12), mats.chrome, [s * .9, 3.6, 0])); }
    const head = grp('crusher_head', [0, 2.8, 0]); head.add(part('crusher_head_block', box(3.0, .8, 3.0), mats.chassis, [0, 0, 0])); head.add(part('crusher_head_face', box(2.8, .1, 2.8), mats.steel_plate, [0, -.45, 0])); for (let i = 0; i < 4; i++) head.add(part('crusher_tooth' + i, new THREE.ConeGeometry(.15, .3, 4), mats.trim, [-1.05 + i * .7, -.6, 1.05], [Math.PI, 0, 0])); head.add(K.hazardStripes('crusher_head_hazard', 2.8, .16, [0, 0, 1.51], [0, 0, 0], 8)); g.add(head);
    g.add(part('crusher_anvil', box(3.4, .12, 3.4), mats.steel_hull, [0, .06, 0])); g.add(part('crusher_scorch', new THREE.CylinderGeometry(1.2, 1.2, .002, 16), mats.slag || mats.trim, [0, .125, 0]));
    g.add(part('crusher_console', box(.7, 1.1, .5), mats.chassis, [3.2, .55, 1.2])); g.add(part('crusher_button', cyl(.14, .14, .08, 12), mats.weak || mats.energy_buff, [3.2, 1.14, 1.2])); g.add(part('crusher_console_screen', box(.4, .2, .03), mats.energy_buff, [3.2, .9, .95]));
    return g;
  } });

P({ id: 'shared_gate_operated', label: 'Operated gate', size: '4×3.5 m', swatch: '#c9a04a', stats: { Routes: 'reroutes enemies', Operate: 'wheel (E)', States: 'open / closed via `gate_arm`' },
  note: 'Player-operated route switch: a boom-arm barrier across a lane fork on a counterweighted pivot (`gate_arm`, rotate to raise), a hand wheel and gearbox on the post, a signal head that shows which route is live (`gate_signal_a/b`), rumble strip on the sill.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_gate_operated');
    g.add(part('gate_post', box(.5, 3.2, .5), mats.steel_hull, [-2.0, 1.6, 0])); g.add(part('gate_post_foot', box(.9, .3, .9), mats.concrete, [-2.0, .15, 0]));
    g.add(part('gate_gearbox', box(.5, .5, .4), mats.chassis, [-2.0, 2.2, .45])); g.add(part('gate_wheel', new THREE.TorusGeometry(.32, .03, 8, 20), mats.hazard, [-2.0, 1.4, .55], [0, 0, 0])); for (let i = 0; i < 4; i++) g.add(part('gate_spoke' + i, box(.04, .6, .03), mats.chrome, [-2.0, 1.4, .55], [0, 0, i * Math.PI / 4]));
    const arm = grp('gate_arm', [-2.0, 2.6, 0]); arm.add(part('gate_boom', box(4.4, .16, .16), mats.steel_plate, [1.7, 0, 0])); arm.add(K.hazardStripes('gate_boom_hazard', 4.2, .16, [1.7, 0, .09], [0, 0, 0], 10)); arm.add(part('gate_counter', box(.7, .5, .5), mats.chassis, [-.9, -.1, 0])); arm.add(part('gate_pivot', cyl(.12, .12, .6, 12), mats.chrome, [0, 0, 0], [Math.PI / 2, 0, 0])); g.add(arm);
    g.add(part('gate_rest', box(.2, 1.0, .2), mats.steel_hull, [2.2, .5, 0]));
    g.add(part('gate_signal_mast', cyl(.05, .06, 3.6, 8), mats.steel_hull, [-2.0, 1.8, -.6])); g.add(part('gate_signal_head', box(.36, .7, .3), mats.trim, [-2.0, 3.8, -.6])); g.add(part('gate_signal_a', new THREE.SphereGeometry(.09, 10, 8), mats.energy_scan, [-2.0, 3.98, -.44])); g.add(part('gate_signal_b', new THREE.SphereGeometry(.09, 10, 8), mats.trim, [-2.0, 3.62, -.44]));
    for (let i = 0; i < 6; i++) g.add(part('gate_rumble' + i, box(.16, .04, 3.2), mats.hazard, [-1.2 + i * .6, .02, 0]));
    return g;
  } });

const dwall = (state) => P({ id: 'shared_wall_destructible' + (state ? '_' + state : ''), label: 'Destructible wall' + (state ? ' — ' + state : ''), size: '6×4 m', swatch: '#6e6f72', stats: { Hp: 'structure', Opens: 'socket / shortcut', State: state || 'intact' },
  note: { '': 'A 6 m infill wall of cinder block with a cracked render, a keystone plate marked with a hazard X so it reads as breakable, exposed rebar at one corner.', broken: 'Same footprint blown through: a ragged 3 m hole, tilted slabs, hanging rebar, scorch — the new route or socket shows through.', debris: 'Loose rubble field for the floor after the break: block chunks, render flakes, rebar stubs, dust ring. Scatter-only, no collision.' }[state],
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('shared_wall_destructible' + (state ? '_' + state : ''));
    if (state !== 'debris') {
      const blocks = (n, x0, w) => { for (let r = 0; r < 10; r++) for (let c = 0; c < Math.floor(w / .6); c++) { const x = x0 + (c + .5) * .6 + (r % 2) * .3 - .15, y = .2 + r * .4; if (state === 'broken' && Math.hypot(x, y - 1.6) < 1.5) continue; g.add(part(`${n}_${r}_${c}`, box(.58, .38, .5), r % 3 ? mats.concrete : mats.concrete_dark || mats.concrete, [x, y, 0])); } };
      blocks('wall_block', -3, 6);
      for (const s of [-1, 1]) g.add(part(`wall_col${s}`, box(.3, 4.2, .6), mats.steel_hull, [s * 3.1, 2.1, 0]));
      g.add(part('wall_cap', box(6.6, .2, .7), mats.steel_hull, [0, 4.1, 0]));
      if (!state) { g.add(part('wall_key', box(.9, .9, .06), mats.hazard, [0, 1.6, .28])); for (const r of [.785, -.785]) g.add(part('wall_key_x' + r, box(1.0, .1, .02), mats.trim, [0, 1.6, .32], [0, 0, r])); for (let i = 0; i < 3; i++) g.add(part('wall_crack' + i, box(.05, 1.2, .02), mats.trim, [-1.8 + i * 1.7, 2.4, .27], [0, 0, (i - 1) * .3])); g.add(part('wall_rebar', cyl(.02, .02, .8, 6), mats.rust || mats.trim, [2.6, 3.9, .1], [0, 0, .6])); }
      else { for (let i = 0; i < 5; i++) g.add(part('wall_rebar' + i, cyl(.02, .02, .9, 6), mats.rust || mats.trim, [-1.2 + i * .6, 1.6 + (i % 2) * .6, .1], [0, 0, .5 + i * .4])); for (let i = 0; i < 4; i++) g.add(part('wall_slab' + i, box(.9, .5, .4), mats.concrete, [-1.0 + i * .7, .3 + (i % 2) * .2, .6 + (i % 3) * .3], [(i % 2) * .4, i * .5, .2])); g.add(part('wall_scorch', new THREE.RingGeometry(1.2, 1.7, 24), mats.slag || mats.trim, [0, 1.6, .26])); }
    } else {
      for (let i = 0; i < 18; i++) { const a = i / 18 * Math.PI * 2 + i * .7, r = .6 + (i % 4) * .5; g.add(part('debris_chunk' + i, new THREE.DodecahedronGeometry(.12 + (i % 3) * .08, 0), i % 3 ? mats.concrete : mats.concrete_dark || mats.concrete, [Math.cos(a) * r, .1 + (i % 2) * .05, Math.sin(a) * r * .6 + .8], [i, i * .7, 0])); }
      for (let i = 0; i < 4; i++) g.add(part('debris_rebar' + i, cyl(.015, .015, .7, 6), mats.rust || mats.trim, [-1 + i * .7, .05, 1.2 + (i % 2) * .4], [Math.PI / 2 - .2, i * .8, 0]));
      g.add(part('debris_dust', new THREE.RingGeometry(1.4, 2.4, 24), mats.slag_pale || mats.trim, [0, .01, .8], [-Math.PI / 2, 0, 0]));
    }
    return g;
  } });
['', 'broken', 'debris'].forEach(dwall);

P({ id: 'prop_barrel_explosive', label: 'Prop — explosive barrel', size: '1.1 m', swatch: '#ff2e4a', stats: { Trigger: 'damage', Blast: 'r 4 m', Sim: 'deterministic event' },
  note: 'Red drum with hazard diamond, ribbed bands, a leaking bung and a drip pool. Reads as "shoot me" from across the map.',
  build(K) {
    const { part, grp, cyl, mats, THREE } = K, g = grp('prop_barrel_explosive');
    g.add(part('barrel_body', cyl(.34, .34, 1.0, 20), mats.container_red || mats.hazard, [0, .5, 0])); for (const y of [.25, .5, .75]) g.add(part('barrel_band' + y, new THREE.TorusGeometry(.35, .025, 6, 24), mats.trim, [0, y, 0], [Math.PI / 2, 0, 0]));
    g.add(part('barrel_lid', cyl(.32, .34, .06, 20), mats.trim, [0, 1.02, 0])); g.add(part('barrel_bung', cyl(.06, .06, .05, 10), mats.chrome, [.18, 1.06, 0]));
    g.add(part('barrel_diamond', new THREE.BoxGeometry(.3, .3, .02), mats.hazard, [0, .55, .35], [0, 0, Math.PI / 4])); g.add(part('barrel_glyph', new THREE.BoxGeometry(.05, .16, .02), mats.trim, [0, .55, .36]));
    g.add(part('barrel_leak', new THREE.CylinderGeometry(.3, .3, .01, 14), mats.slag || mats.trim, [.4, .005, .1])); g.add(part('barrel_lamp', new THREE.SphereGeometry(.04, 8, 6), mats.weak || mats.energy_buff, [-.2, 1.08, .1]));
    return g;
  } });

P({ id: 'prop_container_droppable', label: 'Prop — droppable container', size: '3×2.6 m', swatch: '#2b4a7a', stats: { Trigger: 'release (E)', Hangs: 'from hook', Crushes: 'below' },
  note: 'A half-size container hanging from a release hook on a chain — the hook, chain and drop-release bracket are the tell. Corner castings, corrugation, a faded stencil plate.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('prop_container_droppable');
    g.add(part('drop_body', box(3, 2.4, 2.4), mats.container_blue || mats.chassis, [0, 1.3, 0])); for (let i = 0; i < 9; i++) g.add(part('drop_rib' + i, box(.08, 2.2, 2.44), mats.trim, [-1.3 + i * .32, 1.3, 0]));
    for (const [x, z] of [[-1.45, -1.15], [1.45, -1.15], [-1.45, 1.15], [1.45, 1.15]]) g.add(part(`drop_casting${x}${z}`, box(.2, 2.5, .2), mats.steel_hull, [x, 1.3, z]));
    g.add(part('drop_plate', box(1.0, .4, .04), mats.steel_plate, [0, 1.6, 1.22]));
    g.add(part('drop_bracket', box(.6, .3, .6), mats.steel_hull, [0, 2.65, 0])); g.add(part('drop_hook', new THREE.TorusGeometry(.25, .05, 8, 16, Math.PI * 1.4), mats.chrome, [0, 3.1, 0], [0, 0, -.3])); for (let i = 0; i < 8; i++) g.add(part('drop_chain' + i, new THREE.TorusGeometry(.07, .02, 5, 8), mats.chrome, [0, 3.5 + i * .14, 0], [i % 2 ? 0 : Math.PI / 2, 0, 0]));
    g.add(part('drop_release', box(.3, .2, .16), mats.hazard, [.35, 2.75, .25])); g.add(K.hazardStripes('drop_hazard', 2.8, .12, [0, .2, 1.22], [0, 0, 0], 8));
    return g;
  } });

/* ── Scrap pickups (economy/) — one silhouette per type, so what a wave paid reads at a glance ── */
export const PICKUPS = [];
const pickup = (id, label, swatch, note, build) => PICKUPS.push({ id: 'pickup_' + id, file: `pickup_${id}.glb`, label, swatch, size: '0.4 m', stats: { Rarity: { alloy: 'common', flux: 'uncommon', plating: 'uncommon', gravium: 'rare', primecore: 'boss' }[id], Bob: '`_spin` group' }, note, dir: 'economy/', build });
pickup('alloy', 'Pickup — Alloy', '#9aa6b7', 'Common: a bundle of three steel ingots strapped together, bright cut ends.', (K) => { const { part, grp, box, mats } = K, g = grp('pickup_alloy'), s = grp('pickup_alloy_spin', [0, .25, 0]); for (let i = 0; i < 3; i++) s.add(part('alloy_ingot' + i, box(.12, .1, .36), mats.steel_plate, [-.12 + i * .12, (i % 2) * .08, 0], [0, 0, (i - 1) * .12])); s.add(part('alloy_strap', box(.42, .04, .06), mats.strap || mats.rubber, [0, .02, 0])); g.add(s); g.add(part('alloy_ring', new K.THREE.TorusGeometry(.3, .012, 6, 24), mats.steel_plate, [0, .02, 0], [Math.PI / 2, 0, 0])); return g; });
pickup('flux', 'Pickup — Flux', '#22d3ee', 'Uncommon: a cyan energy cell in a chrome cage, glowing from within — shielded/energy enemies drop it.', (K) => { const { part, grp, cyl, mats, THREE } = K, g = grp('pickup_flux'), s = grp('pickup_flux_spin', [0, .3, 0]); s.add(part('flux_cell', cyl(.08, .08, .3, 10), mats.energy_flak, [0, 0, 0])); for (const y of [-.12, .12]) s.add(part('flux_cap' + y, cyl(.1, .1, .04, 10), mats.chrome, [0, y, 0])); for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; s.add(part('flux_bar' + i, cyl(.012, .012, .3, 6), mats.chrome, [Math.cos(a) * .1, 0, Math.sin(a) * .1])); } g.add(s); g.add(part('flux_ring', new THREE.TorusGeometry(.3, .012, 6, 24), mats.energy_flak, [0, .02, 0], [Math.PI / 2, 0, 0])); return g; });
pickup('plating', 'Pickup — Plating', '#5a5f6a', 'Uncommon: a torn armour plate with rivets and a scorched edge — what armoured enemies leave behind.', (K) => { const { part, grp, box, mats, THREE } = K, g = grp('pickup_plating'), s = grp('pickup_plating_spin', [0, .22, 0]); s.rotation.z = .3; s.add(part('plating_slab', box(.42, .06, .32), mats.carapace_pale || mats.steel_plate, [0, 0, 0])); s.add(part('plating_back', box(.44, .03, .34), mats.carapace_dark || mats.trim, [0, -.04, 0])); for (const [x, z] of [[-.16, -.11], [.16, -.11], [-.16, .11], [.16, .11]]) s.add(part(`plating_rivet${x}${z}`, new THREE.CylinderGeometry(.02, .02, .02, 6), mats.chrome, [x, .04, z])); s.add(part('plating_scorch', box(.14, .062, .34), mats.slag || mats.trim, [.15, 0, 0])); g.add(s); g.add(part('plating_ring', new THREE.TorusGeometry(.3, .012, 6, 24), mats.steel_plate, [0, .02, 0], [Math.PI / 2, 0, 0])); return g; });
pickup('primecore', 'Pickup — Prime Core', '#9b5be8', 'Very rare — boss drop. A soul-violet core lattice suspended between two brass yokes, orbited by four rings, throwing a light column so it reads as the rarest thing on the floor.', (K) => { const { part, grp, mats, THREE } = K, g = grp('pickup_primecore'), s = grp('pickup_primecore_spin', [0, .42, 0]); s.add(part('prime_core', new THREE.IcosahedronGeometry(.14, 1), mats.energy_field)); s.add(part('prime_cage', new THREE.IcosahedronGeometry(.19, 1), mats.chrome)); s.getObjectByName('prime_cage').material = mats.chrome; for (let i = 0; i < 4; i++) s.add(part('prime_ring' + i, new THREE.TorusGeometry(.26 + i * .03, .01, 6, 32), mats.energy_field, [0, 0, 0], [i * .8, i * .5, 0])); for (const y of [-.3, .3]) s.add(part('prime_yoke' + y, new THREE.TorusGeometry(.12, .02, 6, 16, Math.PI), mats.brass, [0, y, 0], [y > 0 ? 0 : Math.PI, 0, 0])); g.add(s); g.add(part('prime_column', new THREE.CylinderGeometry(.2, .3, 2.2, 12, 1, true), mats.energy_field, [0, 1.1, 0])); g.add(part('prime_ground', new THREE.TorusGeometry(.4, .015, 6, 32), mats.energy_field, [0, .02, 0], [Math.PI / 2, 0, 0])); return g; });
pickup('gravium', 'Pickup — Gravium', '#9b5be8', 'Rare: a violet crystal shard hovering in a gravity ring, small debris orbiting it — heavies and elites only.', (K) => { const { part, grp, mats, THREE } = K, g = grp('pickup_gravium'), s = grp('pickup_gravium_spin', [0, .34, 0]); s.add(part('gravium_shard', new THREE.OctahedronGeometry(.16, 0), mats.energy_field)); s.getObjectByName('gravium_shard').scale.set(.6, 1.4, .6); s.add(part('gravium_ring', new THREE.TorusGeometry(.24, .015, 6, 28), mats.chrome, [0, 0, 0], [Math.PI / 2 + .4, 0, 0])); for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; s.add(part('gravium_debris' + i, new THREE.TetrahedronGeometry(.03, 0), mats.trim, [Math.cos(a) * .24, Math.sin(a) * .1, Math.sin(a) * .24])); } g.add(s); g.add(part('gravium_ground', new THREE.TorusGeometry(.3, .012, 6, 24), mats.energy_field, [0, .02, 0], [Math.PI / 2, 0, 0])); return g; });
