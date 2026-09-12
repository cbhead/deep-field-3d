/**
 * Deep Field 3D — Switchyard environment kit (DESIGN-BRIEF §3.6).
 * Rail-yard theme: bare ballast terrain (track lives only in the lane module), the freight cut (a
 * sunken channel with retaining walls the ground shortcut runs through), a
 * mid deck at y 5, an upper catwalk at y 10, and a rail-yard dressing set.
 * Metres, Y-up, origin at footprint centre on the ground.
 */
import { buildSpaceSky } from './space-sky.js';
import { floorMaterials } from './floor-textures.js';
export const SWITCHYARD = [];
const P = (o) => { SWITCHYARD.push({ ...o, file: o.file || o.id + '.glb', swatch: o.swatch || '#8d99ad', dir: 'maps/switchyard/' }); };

export function makeSwitchyardMats(THREE, mats) {
  const mat = (name, color, o = {}) => { const m = new THREE.MeshStandardMaterial({ color, roughness: .85, metalness: .1, flatShading: false, ...o }); m.name = name; mats[name] = m; return m; };
  mat('ballast', 0x4a4742, { roughness: .98 });
  mat('ballast_dark', 0x33312e, { roughness: .98 });
  mat('sleeper', 0x3a2c22, { roughness: .95 });
  mat('rail_steel', 0x9aa3ad, { roughness: .35, metalness: .6, flatShading: false });
  mat('rust', 0x6b3f2a, { roughness: .9, metalness: .15 });
  mat('rust_pale', 0x8c5a3c, { roughness: .85, metalness: .15 });
  mat('container_red', 0x8a2f1c, { roughness: .7, metalness: .25 });
  mat('container_blue', 0x2b4a7a, { roughness: .7, metalness: .25 });
  mat('container_green', 0x3d5a3a, { roughness: .7, metalness: .25 });
  mat('concrete_wall', 0x6e6f72, { roughness: .92 });
  mat('signal_red', 0xff2e4a, { roughness: .3, emissive: new THREE.Color(0xff2e4a), emissiveIntensity: 1.2, flatShading: false });
  mat('signal_green', 0x7fe65a, { roughness: .3, emissive: new THREE.Color(0x7fe65a), emissiveIntensity: 1.1, flatShading: false });
  mat('lamp_cool', 0xdce8ff, { roughness: .3, emissive: new THREE.Color(0xbfd4ff), emissiveIntensity: 1.1 });
  mat('sky_night', 0x0b0f18, { roughness: 1, metalness: 0, side: THREE.BackSide, flatShading: false });
  mat('sky_sodium', 0x2a2114, { roughness: 1, metalness: 0, emissive: new THREE.Color(0x5a4218), emissiveIntensity: .5, side: THREE.BackSide, flatShading: false });
  return mats;
}

/* Track run 4 m: two rails on sleepers in ballast — the module both terrain and cut floor reuse. */
function track(K, id, len = 4) {
  const { part, grp, box, mats } = K, g = grp(id);
  const n = Math.round(len / .6);
  for (let i = 0; i < n; i++) g.add(part(id + '_sleeper' + i, box(.24, .14, 2.4), mats.sleeper, [-len / 2 + (i + .5) * (len / n), .07, 0]));
  for (const s of [-1, 1]) { g.add(part(`${id}_rail${s}`, box(len, .16, .07), mats.rail_steel, [0, .22, s * .72])); g.add(part(`${id}_rail_foot${s}`, box(len, .03, .14), mats.rail_steel, [0, .15, s * .72])); }
  for (let i = 0; i < n; i++) for (const s of [-1, 1]) g.add(part(`${id}_clip${i}${s}`, box(.08, .04, .2), mats.rust, [-len / 2 + (i + .5) * (len / n), .16, s * .72]));
  return g;
}

/* Terrain tile 20×20: ballast bed, three tiers implied by height variants (see levels: yard 0, deck 5, catwalk 10 are structures). */
P({ id: 'switchyard_terrain', label: 'Terrain tile (20 m)', size: '20 m', swatch: '#4a4742', stats: { Tile: '20×20', Grid: '6×4', Relief: '±0.12 m' },
  note: 'Tileable ballast bed baked into a 1024² albedo/roughness/normal set (30k pebbles, oil, rust dust along the old siding lines, puddles with silt rims, faded yard markings, weeds at the margins) plus a drain with grates. No track: the terrain is bare ballast so the lane module (switchyard_path_ground) owns every rail in the yard and nothing double-lays under the route. Sleeper impressions in the bake keep the lifted-siding read. Four variants add standing water, weed tufts, a switch stand, a cable trough or dropped sleepers. The "three tiers" are the yard (0 m), mid deck (5 m) and catwalk (10 m) structures placed on top.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { part, grp, mats, THREE, box, cyl } = K, g = grp('switchyard_terrain'), F = floorMaterials(THREE);
    const geo = new THREE.PlaneGeometry(20, 20, 24, 24); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position;
    for (let i = 0; i < p.count; i++) { const x = p.getX(i), z = p.getZ(i); const edge = Math.abs(x) > 9.9 || Math.abs(z) > 9.9; p.setY(i, edge ? 0 : Math.sin(x * .9 + v) * Math.cos(z * 1.4) * .07 + Math.sin(x * 2.7 + z * 1.9 + v) * .025); }
    geo.computeVertexNormals();
    const ground = part('terrain_ballast', geo, F.switchyard); ground.rotation.y = (v % 2) * Math.PI; ground.receiveShadow = true; g.add(ground);
    // No embedded sidings: rails live only in switchyard_path_ground, so the lane never
    // double-lays over terrain track. The ballast bake carries the lifted-siding marks.
    g.add(part('terrain_drain', box(20, .06, .5), mats.ballast_dark, [0, -.02, 0]));
    for (let i = 0; i < 5; i++) g.add(part('terrain_drain_grate' + i, box(.6, .02, .56), mats.rust, [-8 + i * 4, .012, 0]));
    // Variant details: standing water, weed tufts, dropped sleepers, a switch stand, a cable trough.
    const puddle = (name, x, z, rx, rz, rot) => { const m = part(name, new THREE.CircleGeometry(1, 18), F.puddle, [x, .015, z], [-Math.PI / 2, 0, 0]); m.scale.set(rx, rz, 1); m.rotation.z = rot; g.add(m); };
    const tufts = (name, x, z, n) => { for (let i = 0; i < n; i++) { const a = i * 2.4, r = .2 + (i % 3) * .3; g.add(part(`${name}_${i}`, new THREE.ConeGeometry(.08, .28 + (i % 2) * .12, 5), mats.container_green, [x + Math.cos(a) * r, .13, z + Math.sin(a) * r], [(i % 2 ? .3 : -.2), a, .2])); } };
    const switchStand = (name, x, z) => { const d = grp(name, [x, 0, z]); d.add(part(name + '_base', box(.5, .12, .5), mats.concrete, [0, .06, 0])); d.add(part(name + '_post', cyl(.05, .06, 1.1, 8), mats.rust, [0, .65, 0])); d.add(part(name + '_target', box(.4, .4, .04), mats.hazard, [0, 1.25, 0], [0, .4, 0])); d.add(part(name + '_lamp', new THREE.SphereGeometry(.07, 8, 6), mats.signal_green, [0, 1.42, 0])); d.add(part(name + '_lever', box(.04, .8, .04), mats.chrome, [.3, .5, 0], [0, 0, -.5])); return d; };
    const trough = (name, x, z, len) => { const d = grp(name, [x, 0, z]); d.add(part(name + '_body', box(len, .18, .5), mats.concrete_wall, [0, .09, 0])); for (let i = 0; i < Math.floor(len / 1.2); i++) d.add(part(name + '_lid' + i, box(1.1, .04, .46), i % 4 === 2 ? mats.rust : mats.concrete, [-len / 2 + .6 + i * 1.2, .2, 0])); d.add(part(name + '_cable', cyl(.04, .04, 1.4, 6), mats.rubber, [-len / 2 + 3, .14, .1], [0, 0, Math.PI / 2])); return d; };
    const details = [
      () => { puddle('terrain_puddle', -4, 3, 2.2, 1.3, .4); tufts('terrain_tufts', 7, -8.5, 7); },
      () => { g.add(switchStand('terrain_switchstand', 6.5, -4.2)); tufts('terrain_tufts', -8.5, 8.5, 6); puddle('terrain_puddle', 2, -2.5, 1.4, .9, 1.2); },
      () => { g.add(trough('terrain_trough', 0, 3.6, 16)); tufts('terrain_tufts', 8.5, 8.2, 5); },
      () => { puddle('terrain_puddle', 5, 2.5, 2.8, 1.6, -.3); for (let i = 0; i < 3; i++) g.add(part('terrain_sleeper_drop' + i, box(2.4, .14, .24), mats.sleeper, [-6 + i * .3, .07, -3 + i * .5], [0, .15 * i - .1, 0])); tufts('terrain_tufts', -8.4, -8.4, 6); },
    ];
    details[v % 4]();
    return g;
  } });

P({ id: 'switchyard_terrain_scatter', label: 'Terrain scatter (sleepers + spoil)', size: '5 m', swatch: '#3a2c22', stats: { Place: 'off-route' },
  note: 'Stacked spare sleepers, a spoil heap and a loose rail length. Scatter in the margins only.',
  build(K) {
    const { part, grp, mats, THREE, box } = K, g = grp('switchyard_terrain_scatter');
    for (let i = 0; i < 5; i++) g.add(part('scatter_sleeper' + i, box(2.4, .14, .24), mats.sleeper, [-1.5, .07 + (i % 3) * .15, -1 + (i % 2) * .3 + Math.floor(i / 3) * .3], [0, .1 * i, 0]));
    g.add(part('scatter_spoil', new THREE.ConeGeometry(1.4, .8, 9), mats.ballast_dark, [1.5, .4, .5]));
    g.add(part('scatter_rail', box(4, .1, .07), mats.rust_pale, [0, .06, 1.8], [0, .3, 0]));
    return g;
  } });

/* Freight cut: 4 m channel run AT GRADE (sim routes/sockets sit at y 0). The "cut" reads through low
   ballast embankments and concrete kerb walls either side of a 6 m track lane, not through a drop. */
P({ id: 'switchyard_cut_channel', label: 'Freight cut (4 m run)', size: '8×4 m', swatch: '#6e6f72', stats: { Floor: 'y 0 (sim)', Lane: '6 m', Barricade: 'b1 at (−8,−1)' },
  note: 'One 4 m run of the freight cut the shortcut route uses: track at grade, 1.2 m concrete kerb walls with weep pipes and cap rails, ballast embankments outside them, catenary-style lamp post. Floor stays at y 0 — b1/t2/t4 sit on it. Place only where no other route or socket crosses (x −18…−10).',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_cut_channel');
    g.add(part('cut_floor', box(4, .06, 6), mats.ballast_dark, [0, .03, 0]));
    const t = track(K, 'cut_track', 4); t.position.y = .02; g.add(t);
    for (const s of [-1, 1]) {
      g.add(part(`cut_wall${s}`, box(4, 1.2, .4), mats.concrete_wall, [0, .6, s * 3.2]));
      g.add(part(`cut_wall_cap${s}`, box(4, .1, .6), mats.steel_hull, [0, 1.25, s * 3.2]));
      g.add(part(`cut_embankment${s}`, new THREE.BoxGeometry(4, .9, 1.6), mats.ballast, [0, .2, s * 4.0], [s * .55, 0, 0]));
      for (let i = 0; i < 2; i++) g.add(part(`cut_weep${s}${i}`, cyl(.05, .05, .3, 8), mats.rust, [-1 + i * 2, .35, s * 3.0], [Math.PI / 2, 0, 0]));
      g.add(part(`cut_stain${s}`, box(.5, .8, .02), mats.ballast_dark, [-1, .5, s * 2.99]));
      g.add(K.hazardStripes(`cut_edge_hazard${s}`, 3.6, .08, [0, 1.26, s * 3.5], [0, 0, 0], 8));
    }
    g.add(part('cut_lamp_post', cyl(.05, .06, 2.4, 8), mats.trim, [1.6, 2.4, 3.2]));
    g.add(part('cut_lamp', box(.3, .1, .2), mats.lamp_cool, [1.6, 3.6, 3.0]));
    return g;
  } });

/* Ground-lane module 4 m: ballast bed with a track, kerb sleepers, hazard-tipped ends — the Switchyard roadway. */
P({ id: 'switchyard_path_ground', label: 'Lane module (4 m)', size: '4×3.4 m', swatch: '#4a4742', stats: { Lane: '3.4 m', Repeat: 'along route' },
  note: 'One 4 m run of the 3.4 m ground lane: packed ballast, a single track down the centre, sleeper kerbs and mile-marker posts so the lane reads as the yard\'s main line.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('switchyard_path_ground');
    g.add(part('lane_bed', box(4, .1, 3.4), mats.ballast_dark, [0, .05, 0]));
    const t = track(K, 'lane_track', 4); t.position.y = .06; g.add(t);
    for (const s of [-1, 1]) g.add(part(`lane_kerb${s}`, box(4, .16, .2), mats.sleeper, [0, .1, s * 1.6]));
    g.add(part('lane_marker', box(.08, .6, .08), mats.rust_pale, [-1.8, .4, 1.55]));
    g.add(part('lane_marker_cap', box(.12, .1, .12), mats.hazard, [-1.8, .72, 1.55]));
    return g;
  } });

/* Retaining wall 10 m: the tier faces between yard and deck footings + the boundary. */
P({ id: 'switchyard_retainingwall', label: 'Retaining wall (10 m)', size: '10×6 m', swatch: '#6e6f72', stats: { Height: '6 m', Use: 'perimeter + tier faces' },
  note: 'Precast concrete panel wall with steel soldier piles, a cap beam with a chain-link fence, rust-streaked drainage scuppers and a stencilled yard marker plate. Doubles as the map perimeter.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_retainingwall');
    for (let i = 0; i < 4; i++) g.add(part('rw_panel' + i, box(2.4, 5.6, .5), mats.concrete_wall, [-3.75 + i * 2.5, 2.8, 0]));
    for (let i = 0; i < 5; i++) g.add(part('rw_pile' + i, box(.3, 6, .7), mats.rust, [-5 + i * 2.5, 3, .05]));
    g.add(part('rw_cap', box(10.2, .4, .9), mats.steel_hull, [0, 6.0, 0]));
    for (let i = 0; i < 6; i++) g.add(part('rw_fence_post' + i, cyl(.04, .04, 1.6, 6), mats.trim, [-5 + i * 2, 7.0, 0]));
    g.add(part('rw_fence', box(10, 1.5, .02), mats.optic_glass, [0, 7.0, 0]));
    g.add(part('rw_fence_rail', cyl(.03, .03, 10, 6), mats.chrome, [0, 7.75, 0], [0, 0, Math.PI / 2]));
    for (const x of [-3.75, 1.25]) { g.add(part('rw_scupper' + x, cyl(.12, .12, .8, 8), mats.rust, [x, 4.8, .3], [Math.PI / 2, 0, 0])); g.add(part('rw_scupper_stain' + x, box(.5, 4.5, .02), mats.ballast_dark, [x, 2.4, .26])); }
    g.add(part('rw_marker', box(1.2, .8, .06), mats.hazard, [3.5, 4.2, .28]));
    g.add(part('rw_marker_text', box(.9, .3, .02), mats.trim, [3.5, 4.2, .32]));
    g.add(part('rw_lamp_arm', box(.08, .08, .7), mats.trim, [-1.25, 6.4, .5]));
    g.add(part('rw_lamp', box(.5, .15, .3), mats.lamp_cool, [-1.25, 6.3, .9]));
    return g;
  } });

/* Mid deck 4 m bay: 24×8 at (−6, 4.8, −18); heavy steel viaduct deck on trestle bents. */
P({ id: 'switchyard_middeck', label: 'Mid deck bay (4 m)', size: '4×8 m', swatch: '#6b3f2a', stats: { Top: 'y +5.0', Runs: '6 = 24 m' },
  note: 'One 4 m bay of the 24×8 m mid deck at y 5: riveted plate-girder edges, chequer-plate floor, a trestle bent (two raked legs + cross brace) per bay, rust weathering. Guard rail on the +Z edge with an opening at the ladder bay.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_middeck');
    g.add(part('mid_slab', box(4, .16, 8), mats.steel_plate, [0, 4.92, 0]));
    for (let i = 0; i < 8; i++) for (let j = 0; j < 4; j++) g.add(part(`mid_chequer${i}${j}`, box(.4, .02, .4), mats.trim, [-1.5 + j * 1, 5.01, -3.5 + i]));
    for (const s of [-1, 1]) { g.add(part(`mid_girder${s}`, box(4, .9, .16), mats.rust, [0, 4.4, s * 3.95])); for (let i = 0; i < 6; i++) g.add(part(`mid_rivet${s}${i}`, new THREE.CylinderGeometry(.04, .04, .02, 6), mats.chrome, [-1.5 + i * .6, 4.4, s * 4.04], [Math.PI / 2, 0, 0])); g.add(part(`mid_flange${s}`, box(4, .1, .4), mats.rust_pale, [0, 3.95, s * 3.9])); }
    for (const z of [-2.5, 0, 2.5]) g.add(part('mid_stringer' + z, box(4, .3, .1), mats.steel_hull, [0, 4.7, z]));
    // Trestle bent.
    for (const s of [-1, 1]) g.add(part(`mid_leg${s}`, box(.3, 4.8, .3), mats.rust, [0, 2.4, s * 3.0], [s * .12, 0, 0]));
    g.add(part('mid_brace_a', box(.12, 5.6, .12), mats.trim, [0, 2.5, 0], [.9, 0, 0]));
    g.add(part('mid_brace_b', box(.12, 5.6, .12), mats.trim, [0, 2.5, 0], [-.9, 0, 0]));
    g.add(part('mid_bent_cap', box(.4, .3, 7.4), mats.steel_hull, [0, 4.75, 0]));
    for (const s of [-1, 1]) g.add(part(`mid_foot${s}`, box(.8, .3, .8), mats.concrete, [0, .15, s * 3.3]));
    // Rail on +Z edge.
    for (const x of [-1.9, 1.9]) g.add(part('mid_rail_post' + x, box(.08, 1.1, .08), mats.trim, [x, 5.55, 3.9]));
    g.add(part('mid_rail_top', cyl(.035, .035, 4, 8), mats.rail_steel, [0, 6.05, 3.9], [0, 0, Math.PI / 2]));
    g.add(part('mid_rail_mid', cyl(.02, .02, 4, 8), mats.trim, [0, 5.6, 3.9], [0, 0, Math.PI / 2]));
    g.add(K.hazardStripes('mid_kick', 3.6, .08, [0, 5.1, 3.93], [0, 0, 0], 8));
    g.add(part('mid_lamp', box(.3, .1, .2), mats.lamp_cool, [0, 4.3, 3.7]));
    return g;
  } });

/* Upper catwalk 4 m bay: 22×5 at (3, 9.8, 3), thin grated walkway on tall lattice legs. */
P({ id: 'switchyard_catwalk', label: 'Catwalk bay (4 m)', size: '4×5 m', swatch: '#9aa3ad', stats: { Top: 'y +10.0', Runs: '6 = 22 m' },
  note: 'One 4 m bay of the 22×5 m catwalk at y 10: open grating, tube handrails both sides with mesh infill, angle-iron trusses under, a hanging floodlight. Legs are the separate lattice columns at x −6 and 12. Sockets w3/w4 sit here — the only ones that see the whole air strand.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_catwalk');
    g.add(part('cat_grate', box(4, .08, 5), mats.deck_grate || mats.steel_hull, [0, 9.96, 0]));
    for (let i = 0; i < 9; i++) g.add(part('cat_grate_bar' + i, box(4, .02, .03), mats.trim, [0, 10.01, -2.2 + i * .55]));
    for (const s of [-1, 1]) {
      g.add(part(`cat_edge${s}`, box(4, .25, .1), mats.rust, [0, 9.85, s * 2.45]));
      for (const x of [-1.9, 1.9]) g.add(part(`cat_post${s}${x}`, cyl(.035, .035, 1.1, 8), mats.trim, [x, 10.55, s * 2.4]));
      g.add(part(`cat_handrail${s}`, cyl(.035, .035, 4, 8), mats.rail_steel, [0, 11.1, s * 2.4], [0, 0, Math.PI / 2]));
      g.add(part(`cat_midrail${s}`, cyl(.02, .02, 4, 8), mats.trim, [0, 10.6, s * 2.4], [0, 0, Math.PI / 2]));
      g.add(part(`cat_mesh${s}`, box(3.8, .9, .01), mats.optic_glass, [0, 10.55, s * 2.4]));
      g.add(K.hazardStripes(`cat_kick${s}`, 3.6, .06, [0, 10.08, s * 2.43], [0, 0, 0], 8));
    }
    for (const z of [-1.6, 0, 1.6]) g.add(part('cat_truss' + z, box(4, .2, .08), mats.steel_hull, [0, 9.75, z]));
    for (let i = 0; i < 4; i++) g.add(part('cat_truss_diag' + i, box(.06, .06, 2.4), mats.trim, [-1.5 + i, 9.7, 0], [0, (i % 2 ? .6 : -.6), 0]));
    g.add(part('cat_lamp_arm', box(.06, .5, .06), mats.trim, [0, 9.45, 0]));
    g.add(part('cat_lamp', new THREE.CylinderGeometry(.25, .12, .25, 10), mats.steel_hull, [0, 9.15, 0]));
    g.add(part('cat_lamp_lens', new THREE.CylinderGeometry(.22, .22, .03, 10), mats.lamp_cool, [0, 9.02, 0]));
    return g;
  } });

P({ id: 'switchyard_column', label: 'Lattice column (10 m)', size: '1.2×9.8 m', swatch: '#6b3f2a', stats: { At: 'x −6, 12 · z 3', Mid: 'scale Y .49' },
  note: 'Riveted lattice column for the catwalk (and, at half height, the mid-deck support pillar). Concrete plinth, four angle chords with zigzag lacing, a cap plate.',
  build(K) {
    const { part, grp, box, mats, THREE } = K, g = grp('switchyard_column');
    g.add(part('col_plinth', box(1.8, .5, 1.8), mats.concrete, [0, .25, 0]));
    g.add(K.boltRing('col_bolts', .7, 8, .51));
    for (const sx of [-1, 1]) for (const sz of [-1, 1]) g.add(part(`col_chord${sx}${sz}`, box(.16, 9.3, .16), mats.rust, [sx * .5, 5.15, sz * .5]));
    for (let i = 0; i < 12; i++) for (const f of [0, 1]) g.add(part(`col_lace${i}${f}`, box(.06, 1.2, .06), mats.rust_pale, [f ? .5 : 0, .9 + i * .75, f ? 0 : .5], [f ? .0 : .7 * (i % 2 ? 1 : -1), 0, f ? .7 * (i % 2 ? 1 : -1) : 0]));
    for (const y of [3.3, 6.6]) g.add(part('col_batten' + y, box(1.2, .12, 1.2), mats.steel_hull, [0, y, 0]));
    g.add(part('col_cap', box(1.6, .3, 1.6), mats.steel_plate, [0, 9.65, 0]));
    return g;
  } });

P({ id: 'switchyard_skybox', label: 'Skybox', size: '1.4 km', swatch: '#1c3a6a', stats: { Type: 'deep space (shader)', Body: 'ice world + moon', Stars: 'black-body tinted' },
  note: 'Realistic deep-space sky: black-body starfield in three magnitude layers, Milky Way band with dust lanes, faint nebulosity, sun disc with corona aligned to the key light, and a blue ice world with cloud decks with soft terminator, Fresnel atmosphere and limb glow. Full sphere, unlit, fog-exempt. GLB stand-in for the HDR bake.',
  build(K) { return buildSpaceSky(K.THREE, 'switchyard'); } });

/* Dressing. */
P({ id: 'switchyard_dress_railcar', label: 'Dressing — railcar', size: '12 m', swatch: '#8a2f1c', stats: { Length: '12 m', Height: '3.6 m', Blocks: 'sight — place off-route' },
  note: 'Rusted boxcar on two bogies: corrugated sides, sliding door ajar, roof walk, ladder, brake wheel, buffers and coupler. Big enough to occlude — only on sidings the sockets do not look across.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_dress_railcar');
    g.add(part('car_body', box(12, 2.6, 2.8), mats.container_red, [0, 2.2, 0]));
    for (let i = 0; i < 20; i++) for (const s of [-1, 1]) g.add(part(`car_corr${i}${s}`, box(.12, 2.2, .06), mats.rust, [-5.7 + i * .6, 2.2, s * 1.43]));
    g.add(part('car_door', box(2.4, 2.2, .1), mats.rust_pale, [1.4, 2.1, -1.48]));
    g.add(part('car_door_rail', box(6, .1, .12), mats.trim, [1, 3.3, -1.5]));
    g.add(part('car_door_opening', box(2.0, 2.0, .05), mats.trim, [-1.0, 2.1, -1.42]));
    g.add(part('car_roof', box(12.2, .2, 3.0), mats.rust, [0, 3.6, 0]));
    g.add(part('car_roofwalk', box(11, .06, .8), mats.steel_hull, [0, 3.74, 0]));
    g.add(part('car_frame', box(12.4, .3, 2.6), mats.trim, [0, .85, 0]));
    for (const x of [-4, 4]) { g.add(part('car_bogie' + x, box(2.4, .5, 2.2), mats.trim, [x, .55, 0])); for (const dx of [-.8, .8]) for (const s of [-1, 1]) g.add(part(`car_wheel${x}${dx}${s}`, cyl(.45, .45, .12, 16), mats.rail_steel, [x + dx, .45, s * 1.0], [Math.PI / 2, 0, 0])); g.add(part('car_spring' + x, box(2.0, .3, 1.6), mats.rust, [x, .8, 0])); }
    for (const s of [-1, 1]) { g.add(part(`car_buffer${s}`, cyl(.18, .22, .5, 10), mats.trim, [s * 6.4, .95, .8], [0, 0, Math.PI / 2])); g.add(part(`car_buffer_b${s}`, cyl(.18, .22, .5, 10), mats.trim, [s * 6.4, .95, -.8], [0, 0, Math.PI / 2])); g.add(part(`car_coupler${s}`, box(.6, .3, .3), mats.steel_hull, [s * 6.4, .8, 0])); }
    for (let i = 0; i < 6; i++) g.add(part('car_ladder_rung' + i, cyl(.02, .02, .5, 6), mats.chrome, [5.6, 1.4 + i * .4, -1.55], [0, 0, Math.PI / 2]));
    g.add(part('car_brake_wheel', new THREE.TorusGeometry(.3, .03, 6, 14), mats.rust_pale, [6.3, 2.8, 0], [0, Math.PI / 2, 0]));
    g.add(part('car_placard', box(.6, .6, .04), mats.hazard, [-4.5, 2.4, -1.5], [0, 0, Math.PI / 4]));
    return g;
  } });

P({ id: 'switchyard_dress_container', label: 'Dressing — container (6 m)', size: '6 m', swatch: '#2b4a7a', stats: { Stack: 'to 2', Height: '2.6 m' },
  note: 'Corrugated 6 m shipping container with corner castings, twist-lock pads, door bars and a faded ID plate. Rotate/stack pairs to build sightline-safe backdrops along the perimeter.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_dress_container');
    g.add(part('cont_body', box(6, 2.6, 2.4), mats.container_blue, [0, 1.3, 0]));
    for (let i = 0; i < 12; i++) for (const s of [-1, 1]) g.add(part(`cont_corr${i}${s}`, box(.16, 2.3, .06), mats.trim, [-2.75 + i * .5, 1.3, s * 1.23]));
    for (const sx of [-1, 1]) for (const sy of [0, 1]) for (const sz of [-1, 1]) g.add(part(`cont_casting${sx}${sy}${sz}`, box(.3, .3, .3), mats.steel_hull, [sx * 2.9, .15 + sy * 2.3, sz * 1.1]));
    for (const z of [-.5, .5]) { g.add(part('cont_doorbar' + z, cyl(.03, .03, 2.3, 8), mats.chrome, [3.02, 1.3, z])); g.add(part('cont_doorhandle' + z, box(.04, .05, .3), mats.rust_pale, [3.06, 1.1, z + .2])); }
    g.add(part('cont_plate', box(.04, .5, 1.0), mats.steel_plate, [3.02, 2.1, -.5]));
    g.add(part('cont_roof', box(6.02, .04, 2.42), mats.container_blue, [0, 2.62, 0]));
    g.add(part('cont_rust', box(1.4, .8, .02), mats.rust, [-1.5, .6, -1.24]));
    return g;
  } });

P({ id: 'switchyard_dress_signaltower', label: 'Dressing — signal tower', size: '8 m', swatch: '#ff2e4a', stats: { Heads: '2', Mast: '8 m' },
  note: 'Colour-light signal gantry: lattice mast, two signal heads with hoods (red over green), a relay cabinet at the foot, access ladder with cage, and a number plate. Red head marks the freight cut when b1 is built.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_dress_signaltower');
    g.add(part('sig_base', box(1.2, .3, 1.2), mats.concrete, [0, .15, 0]));
    g.add(part('sig_cabinet', box(.8, 1.2, .5), mats.container_green, [.9, .6, 0]));
    g.add(part('sig_cabinet_door', box(.02, 1.0, .4), mats.trim, [1.31, .6, 0]));
    g.add(part('sig_mast', cyl(.12, .16, 7.5, 8), mats.rust, [0, 4.05, 0]));
    for (let i = 0; i < 12; i++) g.add(part('sig_rung' + i, cyl(.015, .015, .4, 6), mats.chrome, [0, 1.0 + i * .55, .2], [0, 0, Math.PI / 2]));
    for (let i = 0; i < 4; i++) g.add(part('sig_hoop' + i, new THREE.TorusGeometry(.4, .02, 6, 12, Math.PI), mats.trim, [0, 3.5 + i * 1.0, .2]));
    g.add(part('sig_arm', box(2.2, .15, .15), mats.steel_hull, [.9, 7.6, 0]));
    for (const [y, m, x] of [[7.0, mats.signal_red, 1.8], [6.3, mats.signal_green, 1.8]]) { g.add(part('sig_head' + y, box(.5, .6, .4), mats.trim, [x, y, 0])); g.add(part('sig_lamp' + y, cyl(.16, .16, .05, 12), m, [x, y, -.22], [Math.PI / 2, 0, 0])); g.add(part('sig_hood' + y, new THREE.CylinderGeometry(.2, .2, .3, 12, 1, true, 0, Math.PI), mats.trim, [x, y + .1, -.3], [Math.PI / 2, 0, 0])); }
    g.add(part('sig_head_back', box(.4, 1.4, .1), mats.trim, [1.8, 6.65, .25]));
    g.add(part('sig_plate', box(.5, .3, .04), mats.steel_plate, [0, 2.5, -.2]));
    g.add(part('sig_plate_text', box(.3, .12, .01), mats.trim, [0, 2.5, -.225]));
    g.add(K.cableRun('sig_cable', [[.9, 1.2, 0], [.5, 2.0, .1], [.15, 3.0, .1]], .02));
    return g;
  } });

P({ id: 'switchyard_dress_buffer', label: 'Dressing — buffer stop', size: '2.6 m', swatch: '#d8a13a', stats: { Height: '1.6 m', Ends: 'sidings' },
  note: 'Rail-built buffer stop: two raked rail struts, a hazard-painted beam with twin buffer heads, sleeper cribbing and a red marker lamp. Terminates each disused siding.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('switchyard_dress_buffer');
    for (let i = 0; i < 3; i++) g.add(part('buf_crib' + i, box(2.4, .14, .24), mats.sleeper, [0, .07 + i * .15, .4 + i * .3]));
    for (const s of [-1, 1]) { g.add(part(`buf_strut${s}`, box(.1, 2.0, .16), mats.rust, [s * .72, .9, .7], [.7, 0, 0])); g.add(part(`buf_post${s}`, box(.16, 1.3, .1), mats.rust, [s * .72, .65, 0])); }
    g.add(part('buf_beam', box(2.0, .4, .3), mats.steel_hull, [0, 1.2, 0]));
    g.add(K.hazardStripes('buf_hazard', 1.8, .3, [0, 1.2, -.16], [0, 0, 0], 6));
    for (const s of [-1, 1]) { g.add(part(`buf_head${s}`, cyl(.22, .22, .1, 12), mats.trim, [s * .72, 1.2, -.3], [Math.PI / 2, 0, 0])); g.add(part(`buf_shank${s}`, cyl(.08, .08, .3, 8), mats.chrome, [s * .72, 1.2, -.15], [Math.PI / 2, 0, 0])); }
    g.add(part('buf_lamp_post', cyl(.03, .03, .5, 6), mats.trim, [0, 1.6, 0]));
    g.add(part('buf_lamp', new THREE.SphereGeometry(.1, 8, 6), mats.signal_red, [0, 1.9, 0]));
    return g;
  } });
