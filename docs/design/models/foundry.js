/**
 * Deep Field 3D — Foundry environment kit (DESIGN-BRIEF §3.6).
 * Industrial works: dark slag terrain, cast-iron roadway plates, a riveted
 * steel upper deck, vent tunnel, boundary walls and a dressing set. Pieces are
 * modular; the map viewer instances them at the GameRoot.cs coordinates.
 * Metres, Y-up, origin at footprint centre on the ground.
 */
import { buildSpaceSky } from './space-sky.js';
import { floorMaterials } from './floor-textures.js';
export const FOUNDRY = [];
const P = (o) => { FOUNDRY.push({ ...o, file: o.file || o.id + '.glb', swatch: o.swatch || '#8d99ad', dir: 'maps/foundry/' }); };

export function makeFoundryMats(THREE, mats) {
  const mat = (name, color, o = {}) => { const m = new THREE.MeshStandardMaterial({ color, roughness: .85, metalness: .1, flatShading: false, ...o }); m.name = name; mats[name] = m; return m; };
  mat('slag', 0x2a2a30, { roughness: .95 });
  mat('slag_pale', 0x3c3a40, { roughness: .95 });
  mat('road_plate', 0x4a4e58, { roughness: .7, metalness: .35 });
  mat('road_seam', 0x22252c, { roughness: .8, metalness: .2 });
  mat('deck_grate', 0x555c6b, { roughness: .6, metalness: .45 });
  mat('brick', 0x5a3a2e, { roughness: .95 });
  mat('molten', 0xff8a1f, { roughness: .3, metalness: 0, emissive: new THREE.Color(0xff6a00), emissiveIntensity: 1.4, flatShading: false });
  mat('ember_glow', 0xe8622b, { roughness: .4, emissive: new THREE.Color(0xe8622b), emissiveIntensity: .9 });
  mat('lamp_warm', 0xfff0c8, { roughness: .3, emissive: new THREE.Color(0xffe2a8), emissiveIntensity: 1.2 });
  mat('steam', 0xd9dde6, { roughness: 1, metalness: 0, transparent: true, opacity: .35, flatShading: false, depthWrite: false });
  mat('sky_deep', 0x0d1119, { roughness: 1, metalness: 0, side: THREE.BackSide, flatShading: false });
  mat('sky_glow', 0x3a1f14, { roughness: 1, metalness: 0, emissive: new THREE.Color(0x6a2c12), emissiveIntensity: .6, side: THREE.BackSide, flatShading: false });
  return mats;
}

/* Terrain tile 20×20 m: slag with an irregular, low-relief surface and cooled-lava cracks. */
P({ id: 'foundry_terrain', label: 'Terrain tile (20 m)', size: '20 m', swatch: '#2a2a30', stats: { Tile: '20×20', Grid: '6×4 tiles', Relief: '±0.12 m', Maps: 'albedo · rough · normal · emissive' },
  note: 'Tileable cast-iron floor: 8×8 riveted plates baked into a 1024² albedo/roughness/normal set (soot, rust bleed, scorch rings, tread ridges, stencilled bay numbers) with an emissive map for the seams still running molten. Four variants (rotation + a different set of physical details: drain grates, access hatch, conduit, cooled slag pool) so the 24-tile grid never repeats visibly. Flat edges so tiles seam.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { part, grp, mats, THREE, box, cyl } = K, g = grp('foundry_terrain'), F = floorMaterials(THREE);
    const geo = new THREE.PlaneGeometry(20, 20, 24, 24); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position;
    for (let i = 0; i < p.count; i++) { const x = p.getX(i), z = p.getZ(i); const edge = Math.abs(x) > 9.9 || Math.abs(z) > 9.9; p.setY(i, edge ? 0 : (Math.sin(x * 1.3 + v) * Math.cos(z * .9 - v) * .09 + Math.sin(x * 3.1 + z * 2.2) * .025)); }
    geo.computeVertexNormals();
    const ground = part('terrain_ground', geo, F.foundry); ground.rotation.y = v * Math.PI / 2; ground.receiveShadow = true; g.add(ground);
    // Physical details per variant — all low (< 12 cm) so nothing fights the roadway or sockets.
    const grate = (name, x, z) => { const d = grp(name, [x, 0, z]); d.add(part(name + '_frame', box(1.3, .06, 1.3), mats.trim, [0, .03, 0])); d.add(part(name + '_pit', box(1.1, .02, 1.1), mats.slag, [0, .04, 0])); for (let i = 0; i < 7; i++) d.add(part(name + '_bar' + i, box(1.1, .03, .05), mats.steel_hull, [0, .07, -.45 + i * .15])); d.add(part(name + '_glow', new THREE.PlaneGeometry(1.0, 1.0), mats.ember_glow, [0, .045, 0], [-Math.PI / 2, 0, 0])); return d; };
    const hatch = (name, x, z) => { const d = grp(name, [x, 0, z]); d.add(part(name + '_ring', cyl(.9, .95, .08, 16), mats.steel_hull, [0, .04, 0])); d.add(part(name + '_lid', cyl(.78, .78, .04, 16), mats.steel_plate, [0, .1, 0])); d.add(K.boltRing(name + '_bolts', .84, 12, .09)); d.add(part(name + '_handle', new THREE.TorusGeometry(.12, .02, 6, 12, Math.PI), mats.chrome, [0, .13, 0], [0, 0, 0])); return d; };
    const conduit = (name, x, z, len, rot) => { const d = grp(name, [x, 0, z]); d.rotation.y = rot; d.add(part(name + '_pipe', cyl(.07, .07, len, 10), mats.brass, [0, .09, 0], [0, 0, Math.PI / 2])); for (let i = 0; i < Math.floor(len / 2); i++) d.add(part(name + '_clamp' + i, box(.12, .14, .2), mats.trim, [-len / 2 + 1 + i * 2, .07, 0])); return d; };
    const pool = (name, x, z, r) => { const d = grp(name, [x, 0, z]); d.add(part(name + '_glass', cyl(r, r * 1.05, .03, 14), F.slag_glass, [0, .02, 0])); d.add(part(name + '_crust', new THREE.TorusGeometry(r * 1.02, .05, 6, 14), mats.slag_pale, [0, .03, 0], [Math.PI / 2, 0, 0])); d.add(part(name + '_vein', box(.06, .01, r * 1.6), mats.molten, [0, .04, 0], [0, .7, 0])); return d; };
    const details = [
      () => { g.add(grate('terrain_grate_a', -6.2, 3.8)); g.add(hatch('terrain_hatch', 5.5, -5.5)); },
      () => { g.add(grate('terrain_grate_a', 4.2, 6.5)); g.add(conduit('terrain_conduit', -3, -7.4, 12, 0)); g.add(pool('terrain_pool', -6.8, 2.0, .9)); },
      () => { g.add(hatch('terrain_hatch', -4.5, -4.5)); g.add(grate('terrain_grate_a', 6.8, -1.2)); g.add(grate('terrain_grate_b', 6.8, 1.4)); },
      () => { g.add(pool('terrain_pool', 3.5, 4.5, 1.2)); g.add(conduit('terrain_conduit', 7.5, 0, 14, Math.PI / 2)); },
    ];
    details[v % 4]();
    return g;
  } });

P({ id: 'foundry_terrain_scatter', label: 'Terrain scatter (boulders + crack)', size: '6 m', swatch: '#3c3a40', stats: { Boulders: '5', Crack: 'emissive', Place: 'off-route' },
  note: 'Slag boulder cluster with a cooled-lava crack seam. Scatter these by hand in levels.js, never within 2.5 m of a route, socket or placed piece.',
  build(K) {
    const { part, grp, mats, THREE } = K, g = grp('foundry_terrain_scatter');
    g.add(part('scatter_crack', new THREE.BoxGeometry(.25, .04, 5), mats.ember_glow, [0, -.005, 0], [0, .4, 0]));
    g.add(part('scatter_crack_b', new THREE.BoxGeometry(.18, .04, 3), mats.ember_glow, [1.2, -.005, -1.5], [0, -.6, 0]));
    [[-2, .6], [-.6, .4], [1.5, .75], [2.4, .35], [.4, .5]].forEach(([x, r], i) => g.add(part('scatter_boulder' + i, new THREE.DodecahedronGeometry(r, 0), mats.slag_pale, [x, r * .45, (i % 2 ? 1.2 : -1.0)], [i, i * .7, 0])));
    return g;
  } });

/* Roadway plate 4×3.4 m: cast plates with rivet rows and a centre drain seam. */
P({ id: 'foundry_path_ground', label: 'Roadway plate (4 m)', size: '4×3.4 m', swatch: '#4a4e58', stats: { Lane: '3.4 m wide', Repeat: 'along route' },
  note: 'One 4 m run of the 3.4 m-wide ground lane: two cast-iron plates, riveted, with a centre drain seam and worn tread ridges; a kerb lip both sides so the lane reads as a channel from the deck.',
  build(K) {
    const { part, grp, box, mats, THREE } = K, g = grp('foundry_path_ground');
    for (const s of [-1, 1]) g.add(part(`road_plate${s}`, box(4, .1, 1.6), mats.road_plate, [0, .05, s * .85]));
    g.add(part('road_seam', box(4, .12, .12), mats.road_seam, [0, .05, 0]));
    for (let i = 0; i < 4; i++) g.add(part('road_drain' + i, box(.5, .02, .08), mats.chassis, [-1.5 + i, .115, 0]));
    for (const s of [-1, 1]) for (let i = 0; i < 5; i++) g.add(part(`road_rivet${s}${i}`, new THREE.CylinderGeometry(.04, .04, .02, 6), mats.chrome, [-1.6 + i * .8, .11, s * 1.55]));
    for (let i = 0; i < 6; i++) g.add(part('road_tread' + i, box(.06, .015, 3.0), mats.road_seam, [-1.75 + i * .7, .11, 0]));
    for (const s of [-1, 1]) g.add(part(`road_kerb${s}`, box(4, .18, .16), mats.trim, [0, .09, s * 1.78]));
    g.add(K.hazardStripes('road_kerb_hazard', 4, .05, [0, .12, -1.87], [0, 0, 0], 8));
    return g;
  } });

/* Deck section 4×10 m: grated steel, I-beam edges, rivets. */
P({ id: 'foundry_deck', label: 'Upper deck section (4 m)', size: '4×10 m', swatch: '#555c6b', stats: { Top: 'y +6.0', Runs: '7 = 28 m' },
  note: 'One 4 m bay of the 28×10 m upper deck at y 5.8–6.0: grating panels between I-beam joists, rivet rows on the flanges, a service lamp under the edge. Instance 7 along X at x −12…12 (centre x 2, z −17).',
  build(K) {
    const { part, grp, box, mats, THREE } = K, g = grp('foundry_deck');
    g.add(part('deck_slab', box(4, .12, 10), mats.deck_grate, [0, 5.94, 0]));
    for (let i = 0; i < 14; i++) g.add(part('deck_grate_bar' + i, box(4, .03, .04), mats.trim, [0, 6.02, -4.6 + i * .7]));
    for (const s of [-1, 1]) g.add(part(`deck_edge_beam${s}`, box(4, .4, .2), mats.steel_hull, [0, 5.8, s * 4.9]));
    for (const z of [-3.3, 0, 3.3]) g.add(part('deck_joist' + z, box(4, .3, .12), mats.steel_hull, [0, 5.72, z]));
    g.add(part('deck_end_beam', box(.2, .4, 10), mats.steel_hull, [-1.9, 5.8, 0]));
    for (let i = 0; i < 4; i++) g.add(part('deck_rivet' + i, new THREE.CylinderGeometry(.04, .04, .02, 6), mats.chrome, [-1.5 + i, 6.01, 4.9]));
    g.add(part('deck_lamp_arm', box(.06, .06, .5), mats.trim, [0, 5.5, 4.9]));
    g.add(part('deck_lamp', new THREE.SphereGeometry(.12, 8, 6), mats.lamp_warm, [0, 5.45, 5.15]));
    return g;
  } });

P({ id: 'foundry_deck_rail', label: 'Deck guard rail (4 m)', size: '4 m', swatch: '#8d99ad', stats: { Height: '1.0 m', Posts: '2' },
  note: 'One 4 m run of deck rail: two posts, top rail, mid rail, kick plate and a hazard band. Sits on the deck\'s +Z edge at z −12.2. Open bays where the ladder and zipline anchor land.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_deck_rail');
    for (const x of [-1.9, 1.9]) g.add(part('rail_post' + x, box(.1, 1.1, .1), mats.steel_hull, [x, .55, 0]));
    g.add(part('rail_top', cyl(.04, .04, 4, 8), mats.chrome, [0, 1.05, 0], [0, 0, Math.PI / 2]));
    g.add(part('rail_mid', cyl(.025, .025, 4, 8), mats.trim, [0, .6, 0], [0, 0, Math.PI / 2]));
    g.add(part('rail_kick', box(4, .15, .03), mats.steel_plate, [0, .1, 0]));
    g.add(K.hazardStripes('rail_hazard', 3.6, .06, [0, .1, .02], [0, 0, 0], 8));
    for (const x of [-1.9, 1.9]) g.add(part('rail_foot' + x, box(.24, .04, .24), mats.steel_plate, [x, .02, 0]));
    return g;
  } });

P({ id: 'foundry_pillar', label: 'Deck pillar', size: '1.2×5.8 m', swatch: '#3e4a63', stats: { At: 'x −10, 14 · z −17', Cap: 'flared' },
  note: 'Riveted box-section column with a flared cap plate and base plinth, rust streaks as dark bands, a conduit run up one face.',
  build(K) {
    const { part, grp, box, mats, THREE } = K, g = grp('foundry_pillar');
    g.add(part('pillar_base', box(1.8, .3, 1.8), mats.chassis, [0, .15, 0]));
    g.add(K.boltRing('pillar_base_bolts', .8, 8, .31));
    g.add(part('pillar_shaft', box(1.2, 5.2, 1.2), mats.steel_hull, [0, 2.9, 0]));
    for (const y of [1.5, 3.0, 4.5]) g.add(part('pillar_band' + y, box(1.3, .12, 1.3), mats.trim, [0, y, 0]));
    for (let i = 0; i < 3; i++) for (const s of [-1, 1]) g.add(part(`pillar_rivet${i}${s}`, new THREE.CylinderGeometry(.04, .04, .02, 6), mats.chrome, [s * .5, 1.0 + i * 1.5, .61], [Math.PI / 2, 0, 0]));
    g.add(part('pillar_cap', box(1.8, .3, 1.8), mats.steel_plate, [0, 5.65, 0]));
    g.add(K.cableRun('pillar_conduit', [[.4, .3, .65], [.4, 3.0, .7], [.4, 5.5, .65]], .04));
    g.add(part('pillar_junction', box(.3, .3, .12), mats.trim, [.4, 3.0, .68]));
    g.add(part('pillar_junction_lamp', box(.08, .08, .04), mats.ember_glow, [.4, 3.0, .75]));
    return g;
  } });

P({ id: 'foundry_vent_tunnel', label: 'Vent tunnel (14 m)', size: '3.6×1.5×14 m', swatch: '#3e4a63', stats: { At: 'x 0 · z −6.5', Roof: 'y 1.4' },
  note: 'The hero-only shortcut under the deck: corrugated duct walls, roof ribs, grated floor strip, warm lamps every 3.5 m, hazard lips at both mouths. Mount `shared_vent_grate` at each end.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_vent_tunnel');
    g.add(part('tunnel_roof', box(3.6, .25, 14), mats.steel_hull, [0, 1.52, 0]));
    for (const s of [-1, 1]) g.add(part(`tunnel_wall${s}`, box(.25, 1.4, 14), mats.steel_hull, [s * 1.8, .7, 0]));
    for (let i = 0; i < 8; i++) { g.add(part('tunnel_rib' + i, box(3.7, .12, .12), mats.trim, [0, 1.42, -6.3 + i * 1.8])); for (const s of [-1, 1]) g.add(part(`tunnel_rib_w${i}${s}`, box(.12, 1.4, .12), mats.trim, [s * 1.87, .7, -6.3 + i * 1.8])); }
    for (let i = 0; i < 4; i++) g.add(part('tunnel_lamp' + i, box(.4, .06, .12), mats.lamp_warm, [0, 1.38, -5.25 + i * 3.5]));
    g.add(part('tunnel_floor_grate', box(1.2, .03, 14), mats.deck_grate, [0, .015, 0]));
    for (let i = 0; i < 3; i++) g.add(part('tunnel_pipe' + i, cyl(.08, .08, 14, 8), mats.brass, [1.5, .5 + i * .3, 0], [Math.PI / 2, 0, 0]));
    for (const z of [-7, 7]) g.add(K.hazardStripes('tunnel_lip' + z, 3.6, .1, [0, 1.6, z], [0, 0, 0], 10));
    return g;
  } });

P({ id: 'foundry_wall_boundary', label: 'Boundary wall (10 m)', size: '10×6 m', swatch: '#5a3a2e', stats: { Perimeter: '110×80', Height: '6 m', Maps: 'brick albedo · rough · normal' },
  note: 'Refractory brick wall on a baked 2.5 m brick set (stretcher courses, glazed and soot-blackened bricks, crazing, salt bloom, mortar joints in the normal map) with real geometry breaking the face: a stepped stone plinth, three battered buttresses with chamfered caps, a corbelled cornice under the steel cap rail, iron tie plates with bolt heads, spalled bricks standing proud, weep pipes, a slag-chute stain and one furnace window glowing molten. Repeat around the 110×80 m playfield.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_wall_boundary'), F = floorMaterials(THREE);
    /* Box-projected UVs in metres (after the kit's refinement pass) so the 2.5 m brick tile never stretches. */
    const brickBox = (name, w, h, d, pos) => { const m = part(name, box(w, h, d), F.brick, pos), ge = m.geometry, p = ge.attributes.position, n = ge.attributes.normal, uv = ge.attributes.uv; for (let i = 0; i < p.count; i++) { const ax = Math.abs(n.getX(i)), ay = Math.abs(n.getY(i)); const u = ax > .5 ? p.getZ(i) : p.getX(i), v = ay > .5 ? p.getZ(i) : p.getY(i); uv.setXY(i, (u + pos[0]) / 2.5, (v + pos[1]) / 2.5); } uv.needsUpdate = true; return m; };
    g.add(brickBox('wall_body', 10, 5.6, 1.1, [0, 3.2, 0]));
    g.add(part('wall_plinth', box(10.2, .5, 1.5), mats.slag_pale, [0, .25, 0]));
    g.add(part('wall_plinth_step', box(10.1, .15, 1.3), mats.slag, [0, .57, 0]));
    // Cornice: two corbel courses stepping out under the cap, then the steel rail.
    g.add(brickBox('wall_corbel_a', 10.05, .24, 1.25, [0, 6.05, 0])); g.add(brickBox('wall_corbel_b', 10.1, .2, 1.4, [0, 6.27, 0]));
    g.add(part('wall_cap', box(10.2, .22, 1.6), mats.steel_hull, [0, 6.48, 0]));
    for (let i = 0; i < 5; i++) g.add(part('wall_cap_rivet' + i, new THREE.CylinderGeometry(.04, .04, .02, 6), mats.chrome, [-4 + i * 2, 6.6, .7]));
    // Buttresses: battered (wider at the foot), chamfered stone cap, on the enemy-facing side.
    for (const x of [-5, 0, 5]) { g.add(brickBox('wall_buttress' + x, .9, 4.2, .8, [x, 2.6, .95])); g.add(part('wall_buttress_foot' + x, box(1.1, .5, 1.0), mats.slag_pale, [x, .25, .95])); g.add(part('wall_buttress_cap' + x, new THREE.CylinderGeometry(.62, .72, .35, 4), mats.slag_pale, [x, 4.87, .95], [0, Math.PI / 4, 0])); }
    // Iron tie plates (X pattress) with a bolt head, staggered along the face.
    for (const [x, y] of [[-3.6, 3.9], [-1.3, 2.3], [1.6, 4.3], [3.7, 2.9]]) { for (const rz of [.785, -.785]) g.add(part(`wall_tie${x}_${rz > 0 ? 'a' : 'b'}`, box(.7, .1, .05), mats.trim, [x, y, .57], [0, 0, rz])); g.add(part('wall_tie_bolt' + x, cyl(.07, .07, .05, 8), mats.chrome, [x, y, .6], [Math.PI / 2, 0, 0])); }
    // Spalled bricks standing proud of the face; weep pipes near the foot.
    for (const [x, y] of [[-4.3, 1.1], [-2.1, 4.9], [-.4, 3.4], [2.2, 1.5], [3.9, 5.2], [4.6, 2.4]]) g.add(brickBox(`wall_spall${x}`, .25, .075, .12, [x, y, .6]));
    for (const x of [-2.5, 2.5]) g.add(part('wall_weep' + x, cyl(.05, .05, .3, 8), mats.trim, [x, .85, .6], [Math.PI / 2, 0, 0]));
    // Furnace window with bars and a sill; molten glow.
    g.add(part('wall_window_frame', box(1.9, 1.5, .3), mats.steel_hull, [2.5, 3.5, .62]));
    g.add(part('wall_window_sill', box(2.1, .12, .5), mats.slag_pale, [2.5, 2.7, .7]));
    g.add(part('wall_window', box(1.4, 1.0, .1), mats.molten, [2.5, 3.5, .75]));
    for (let i = 0; i < 5; i++) g.add(part('wall_window_bar' + i, box(.06, 1.0, .12), mats.trim, [1.9 + i * .3, 3.5, .78]));
    // Slag-chute stain: a dark wash and a crust lip where it spilled over the plinth.
    g.add(part('wall_stain', box(1.3, 3.2, .015), mats.slag, [-2.5, 2.0, .565]));
    g.add(part('wall_stain_crust', box(1.5, .18, .4), mats.slag_pale, [-2.5, .72, .8]));
    g.add(part('wall_lamp_arm', box(.08, .08, .8), mats.trim, [-2.5, 5.6, .9]));
    g.add(part('wall_lamp', box(.4, .15, .3), mats.lamp_warm, [-2.5, 5.5, 1.3]));
    return g;
  } });

P({ id: 'foundry_skybox', label: 'Skybox', size: '1.4 km', swatch: '#7a3a18', stats: { Type: 'deep space (shader)', Body: 'rust gas giant + moon', Stars: 'black-body tinted' },
  note: 'Realistic deep-space sky: black-body starfield in three magnitude layers, Milky Way band with dust lanes, faint nebulosity, sun disc with corona aligned to the key light, and a banded rust gas giant with soft terminator, Fresnel atmosphere and limb glow. Full sphere, unlit, fog-exempt. GLB stand-in for the HDR bake.',
  build(K) { return buildSpaceSky(K.THREE, 'foundry'); } });

/* Dressing set. */
P({ id: 'foundry_dress_crucible', label: 'Dressing — crucible', size: '5×9 m', swatch: '#ff8a1f', stats: { Molten: 'emissive', Footprint: '5×9 m', Flow: '−Z, 6 m' },
  note: 'Tilting crucible on a trunnion frame, caught mid-pour: a molten stream arcs from the lip into a channel of cooling slag and spreads into a puddle field — a hot core, orange skin, dark crust rims and cracked black slag at the edges — running 6 m out along −Z. The map\'s biggest light source; place with the pour side away from route sightlines.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_dress_crucible');
    for (const s of [-1, 1]) { g.add(part(`crucible_frame${s}`, box(.5, 3.2, .5), mats.steel_hull, [s * 2.2, 1.6, 0])); g.add(part(`crucible_frame_foot${s}`, box(1.2, .3, 1.2), mats.chassis, [s * 2.2, .15, 0])); }
    g.add(part('crucible_trunnion', cyl(.2, .2, 4.8, 12), mats.chrome, [0, 3.0, 0], [0, 0, Math.PI / 2]));
    const b = grp('crucible_body', [0, 3.0, 0]); b.rotation.x = -.62; // tipped to pour
    b.add(part('crucible_shell', new THREE.CylinderGeometry(1.5, 1.0, 2.4, 14), mats.steel_hull, [0, -.3, 0]));
    b.add(part('crucible_lining', new THREE.CylinderGeometry(1.35, .9, 2.3, 14), mats.brick, [0, -.25, 0]));
    b.add(part('crucible_melt', cyl(1.3, 1.3, .1, 14), mats.molten, [0, .8, 0]));
    b.add(part('crucible_crust', new THREE.TorusGeometry(1.4, .12, 8, 20), mats.slag, [0, .9, 0], [Math.PI / 2, 0, 0]));
    b.add(part('crucible_lip', box(1.0, .2, .8), mats.steel_hull, [0, .85, -1.6]));
    b.add(part('crucible_lip_melt', box(.7, .06, .8), mats.molten, [0, .97, -1.6]));
    for (let i = 0; i < 3; i++) b.add(part('crucible_band' + i, new THREE.TorusGeometry(1.5 - i * .17, .06, 6, 20), mats.trim, [0, .5 - i * .8, 0], [Math.PI / 2, 0, 0]));
    g.add(b);
    g.add(part('crucible_gear', cyl(.6, .6, .2, 16), mats.brass, [2.6, 3.0, 0], [0, 0, Math.PI / 2]));
    for (let i = 0; i < 12; i++) { const a = i / 12 * Math.PI * 2; g.add(part('crucible_tooth' + i, box(.1, .15, .2), mats.brass, [2.6, 3.0 + Math.cos(a) * .65, Math.sin(a) * .65], [-a, 0, 0])); }
    // Pour: the lip in world space is at roughly (0, 3.9, -1.5) after the tip; a tube follows the stream down to the ground.
    const stream = new THREE.CatmullRomCurve3([new THREE.Vector3(0, 3.95, -1.55), new THREE.Vector3(0, 3.4, -1.95), new THREE.Vector3(0, 2.2, -2.25), new THREE.Vector3(0, .9, -2.4), new THREE.Vector3(0, .12, -2.5)]);
    g.add(part('crucible_stream', new THREE.TubeGeometry(stream, 24, .12, 10, false), mats.molten));
    g.add(part('crucible_stream_core', new THREE.TubeGeometry(stream, 24, .05, 8, false), mats.lamp_warm));
    g.add(part('crucible_splash', new THREE.ConeGeometry(.55, .28, 12, 1, true), mats.molten, [0, .16, -2.5], [Math.PI, 0, 0]));
    // Channel: molten run in a slag trough, then a puddle field spreading outward with crust rims.
    const ch = grp('crucible_channel', [0, 0, -2.5]);
    ch.add(part('channel_bed', box(1.1, .12, 3.2), mats.slag, [0, .06, -1.6]));
    ch.add(part('channel_melt', box(.7, .04, 3.0), mats.molten, [0, .12, -1.6]));
    for (const s of [-1, 1]) ch.add(part(`channel_bank${s}`, box(.22, .22, 3.2), mats.slag_pale, [s * .58, .11, -1.6], [0, 0, s * .2]));
    for (let i = 0; i < 5; i++) ch.add(part('channel_ripple' + i, box(.5, .012, .06), mats.lamp_warm, [Math.sin(i * 2.1) * .08, .145, -.4 - i * .6]));
    const pool = (n, x, z, r, sy = .7, rot = 0, hot = true) => { const p = grp(n, [x, 0, z]); p.rotation.y = rot; p.scale.z = sy; p.add(part(n + '_crust', cyl(r * 1.18, r * 1.24, .07, 16), mats.slag, [0, .035, 0])); p.add(part(n + '_skin', cyl(r, r * 1.02, .05, 16), hot ? mats.molten : mats.slag_pale, [0, .075, 0])); if (hot) { p.add(part(n + '_core', cyl(r * .55, r * .6, .012, 12), mats.lamp_warm, [0, .105, 0])); for (let i = 0; i < 3; i++) p.add(part(n + '_vein' + i, box(.05, .01, r * 1.6), mats.lamp_warm, [Math.cos(i * 2.1) * r * .3, .102, 0], [0, i * 1.1, 0])); } return p; };
    ch.add(pool('pool_main', 0, -4.0, 1.5, .75));
    ch.add(pool('pool_a', -1.7, -4.9, .8, .8, .5));
    ch.add(pool('pool_b', 1.6, -5.2, .95, .7, -.4));
    ch.add(pool('pool_c', .4, -6.1, .6, .9, .9));
    ch.add(pool('pool_cold_a', -2.4, -3.3, .5, .8, .2, false));
    ch.add(pool('pool_cold_b', 2.5, -4.0, .45, .7, -.7, false));
    // Cracked black slag at the edges + drips from the lip.
    for (let i = 0; i < 9; i++) { const a = i / 9 * Math.PI * 2, r = 2.1 + (i % 3) * .3; ch.add(part('slag_crust' + i, new THREE.DodecahedronGeometry(.28 + (i % 2) * .12, 0), mats.slag, [Math.cos(a) * r, .12, -4.2 + Math.sin(a) * r * .75], [i, i * .7, 0])); }
    for (let i = 0; i < 4; i++) ch.add(part('crack_glow' + i, box(.06, .012, 1.2 + (i % 2) * .6), mats.ember_glow, [-1.2 + i * .8, .04, -4.2 - (i % 2) * .8], [0, .6 + i * .9, 0]));
    g.add(ch);
    for (const [x, z] of [[-.18, -1.9], [.22, -2.1], [-.05, -2.3]]) g.add(part(`crucible_drip${x}`, new THREE.ConeGeometry(.05, .3, 8), mats.molten, [x, 3.3 - Math.abs(x) * 2, z], [Math.PI, 0, 0]));
    return g;
  } });

P({ id: 'foundry_dress_pipes', label: 'Dressing — pipe run (8 m)', size: '8 m', swatch: '#b08a3e', stats: { Height: '3 m', Valves: '2' },
  note: 'Overhead pipe rack: three lines of differing gauge on two goalpost supports, flanges, a bypass elbow, two wheel valves and a pressure gauge. Runs along X.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_dress_pipes');
    for (const x of [-3.5, 3.5]) { for (const z of [-.8, .8]) g.add(part(`pipes_post${x}${z}`, box(.2, 3.0, .2), mats.steel_hull, [x, 1.5, z])); g.add(part('pipes_cross' + x, box(.2, .2, 2.0), mats.steel_hull, [x, 3.0, 0])); g.add(part('pipes_foot' + x, box(.6, .15, 2.2), mats.chassis, [x, .07, 0])); }
    const lines = [[-.6, .22, mats.steel_plate], [0, .15, mats.brass], [.6, .1, mats.trim]];
    lines.forEach(([z, r, m], i) => {
      g.add(part('pipes_line' + i, cyl(r, r, 8, 12), m, [0, 3.2 + r, z], [0, 0, Math.PI / 2]));
      for (const x of [-3.5, -1, 1.5, 3.5]) g.add(part(`pipes_flange${i}${x}`, cyl(r * 1.3, r * 1.3, .1, 12), mats.trim, [x, 3.2 + r, z], [0, 0, Math.PI / 2]));
    });
    g.add(part('pipes_elbow', new THREE.TorusGeometry(.5, .15, 8, 12, Math.PI / 2), mats.brass, [1.0, 3.35, 0], [0, 0, 0]));
    g.add(part('pipes_drop', cyl(.15, .15, 1.6, 12), mats.brass, [1.5, 2.5, -.5]));
    for (const x of [-2.2, 2.6]) { g.add(part('pipes_valve' + x, cyl(.12, .12, .3, 8), mats.chassis, [x, 3.55, -.6])); g.add(part('pipes_valve_wheel' + x, new THREE.TorusGeometry(.25, .03, 6, 12), mats.hazard, [x, 3.75, -.6], [Math.PI / 2, 0, 0])); }
    g.add(part('pipes_gauge', cyl(.15, .15, .06, 12), mats.chrome, [-.5, 3.7, -.6]));
    g.add(part('pipes_gauge_face', cyl(.12, .12, .01, 12), mats.lamp_warm, [-.5, 3.74, -.6]));
    g.add(part('pipes_steam', new THREE.ConeGeometry(.2, .8, 8), mats.steam, [2.6, 4.3, -.6]));
    return g;
  } });

P({ id: 'foundry_dress_gantry', label: 'Dressing — gantry crane (12 m)', size: '12×9 m', swatch: '#c9a04a', stats: { Span: '12 m', Hook: 'chain block' },
  note: 'Overhead travelling crane: two lattice legs, a box girder, trolley with a chain block and hook, hazard-striped end stops, cab with a lit window. Spans the yard at 9 m — clear of the 8 m air lane only where placed off-route.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_dress_gantry');
    for (const x of [-6, 6]) { for (const z of [-1, 1]) g.add(part(`gantry_leg${x}${z}`, box(.4, 8.5, .4), mats.steel_hull, [x, 4.25, z * 1.2])); for (let i = 0; i < 5; i++) g.add(part(`gantry_lattice${x}${i}`, box(.1, 2.6, .1), mats.trim, [x, .9 + i * 1.7, 0], [(i % 2 ? .75 : -.75), 0, 0])); g.add(part('gantry_foot' + x, box(1.2, .3, 3.2), mats.chassis, [x, .15, 0])); g.add(K.hazardStripes('gantry_stop' + x, 1.0, .5, [x, 9.3, 1.0], [0, 0, 0], 3)); }
    g.add(part('gantry_girder', box(13, .9, 1.0), mats.steel_plate, [0, 8.9, 0]));
    g.add(part('gantry_rail', box(13, .1, .12), mats.chrome, [0, 9.4, .35]));
    for (let i = 0; i < 6; i++) g.add(part('gantry_girder_rib' + i, box(.15, 1.0, 1.1), mats.trim, [-5.5 + i * 2.2, 8.9, 0]));
    const t = grp('gantry_trolley', [-2, 9.5, 0]);
    t.add(part('gantry_trolley_body', box(1.6, .6, 1.4), mats.hazard, [0, .3, 0]));
    t.add(part('gantry_drum', cyl(.3, .3, 1.0, 12), mats.chrome, [0, .3, 0], [0, 0, Math.PI / 2]));
    t.add(part('gantry_chain', cyl(.04, .04, 4.0, 6), mats.trim, [0, -2.0, 0]));
    t.add(part('gantry_block', box(.4, .5, .3), mats.steel_hull, [0, -4.2, 0]));
    t.add(part('gantry_hook', new THREE.TorusGeometry(.3, .06, 8, 12, Math.PI * 1.5), mats.chrome, [0, -4.8, 0], [0, 0, -.6]));
    g.add(t);
    g.add(part('gantry_cab', box(1.6, 1.4, 1.2), mats.steel_hull, [4.5, 8.1, 1.2]));
    g.add(part('gantry_cab_window', box(1.2, .7, .05), mats.lamp_warm, [4.5, 8.2, .58]));
    g.add(part('gantry_lamp', new THREE.SphereGeometry(.15, 8, 6), mats.lamp_warm, [0, 8.4, 0]));
    return g;
  } });

P({ id: 'foundry_dress_lightrig', label: 'Dressing — light rig', size: '7 m', swatch: '#fff0c8', stats: { Heads: '4', Mast: '7 m' },
  note: 'Yard floodlight mast: lattice pole, cross-arm with four hooded flood heads, junction box, ladder rungs and a hazard collar. Point heads down-route to justify the level lighting.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_dress_lightrig');
    g.add(part('light_base', box(1.2, .3, 1.2), mats.chassis, [0, .15, 0]));
    g.add(K.boltRing('light_bolts', .5, 4, .31));
    g.add(part('light_mast', cyl(.15, .2, 6.5, 8), mats.steel_hull, [0, 3.5, 0]));
    for (let i = 0; i < 10; i++) g.add(part('light_rung' + i, cyl(.02, .02, .5, 6), mats.chrome, [0, 1.0 + i * .55, .22], [0, 0, Math.PI / 2]));
    g.add(K.hazardStripes('light_collar', .9, .12, [0, 1.6, .2], [0, 0, 0], 3));
    g.add(part('light_box', box(.4, .5, .3), mats.trim, [.3, 2.2, 0]));
    g.add(part('light_box_lamp', box(.06, .06, .04), mats.ember_glow, [.3, 2.4, -.17]));
    g.add(part('light_arm', box(3.2, .15, .15), mats.steel_plate, [0, 6.7, 0]));
    for (let i = 0; i < 4; i++) { const x = -1.2 + i * .8; const h = grp('light_head' + i, [x, 6.55, .1]); h.rotation.x = -.9; h.add(part(`light_hood${i}`, box(.6, .4, .5), mats.steel_hull, [0, 0, 0])); h.add(part(`light_lens${i}`, box(.5, .3, .04), mats.lamp_warm, [0, 0, -.27])); h.add(part(`light_visor${i}`, box(.64, .04, .3), mats.trim, [0, .22, -.2])); g.add(h); }
    g.add(part('light_cap', cyl(.25, .15, .2, 8), mats.trim, [0, 6.85, 0]));
    g.add(part('light_beacon', new THREE.SphereGeometry(.1, 8, 6), mats.weak || mats.ember_glow, [0, 7.05, 0]));
    return g;
  } });

P({ id: 'foundry_dress_steamvent', label: 'Dressing — steam vent', size: '2 m', swatch: '#d9dde6', stats: { Plume: 'client particles', Grate: '1.2 m' },
  note: 'Floor vent: recessed grate in a riveted collar, a stub stack with a flap lid propped open, a layered steam plume (nine translucent puffs widening and thinning as they rise, sheared downwind, with a hot core at the mouth — the client swaps in particles), scorch ring. Safe filler along the lane edges — solid parts never rise above 1.5 m.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('foundry_dress_steamvent');
    g.add(part('vent_collar', cyl(.9, .95, .2, 10), mats.chassis, [0, .1, 0]));
    g.add(K.boltRing('vent_collar_bolts', .8, 10, .21));
    g.add(part('vent_grate', cyl(.7, .7, .04, 10), mats.deck_grate, [0, .2, 0]));
    for (let i = 0; i < 7; i++) g.add(part('vent_grate_bar' + i, box(1.3, .03, .04), mats.trim, [0, .23, -.6 + i * .2]));
    g.add(part('vent_stack', cyl(.35, .4, 1.0, 10), mats.steel_hull, [.0, .7, -.6]));
    g.add(part('vent_stack_band', new THREE.TorusGeometry(.38, .04, 6, 14), mats.trim, [0, 1.0, -.6], [Math.PI / 2, 0, 0]));
    const lid = grp('vent_lid', [0, 1.2, -.95]); lid.rotation.x = -1.0; lid.add(part('vent_lid_plate', cyl(.4, .4, .05, 10), mats.steel_plate, [0, 0, .35])); g.add(lid);
    // Steam: a plume of overlapping puffs that widen, thin and shear downwind (+X) as they rise; a hot bright core at the mouth.
    const puffMat = (i, n) => { const m = mats.steam.clone(); m.name = 'steam_' + i; m.opacity = .5 * Math.pow(1 - i / n, 1.4) + .03; m.color.lerp(new THREE.Color(0x8a8f99), i / n * .6); return m; };
    const N = 9, plume = grp('vent_plume');
    for (let i = 0; i < N; i++) { const t = i / (N - 1), r = .22 + t * .75, y = 1.35 + t * 2.6, x = t * t * .9, z = -.6 + Math.sin(i * 2.1) * .08 * t; plume.add(part('vent_puff' + i, new THREE.IcosahedronGeometry(r, 2), puffMat(i, N), [x, y, z], [i * .7, i * 1.3, 0])); if (i > 2 && i % 2) plume.add(part('vent_wisp' + i, new THREE.IcosahedronGeometry(r * .45, 1), puffMat(i + 2, N), [x + r * .8, y + r * .3, z + (i % 4 ? .3 : -.3)])); }
    plume.add(part('vent_steam_core', new THREE.SphereGeometry(.18, 10, 8), mats.lamp_warm, [0, 1.28, -.6]));
    g.add(plume);
    g.add(part('vent_scorch', new THREE.RingGeometry(.95, 1.3, 20), mats.slag, [0, .01, 0], [-Math.PI / 2, 0, 0]));
    g.add(part('vent_glow', new THREE.RingGeometry(.6, .68, 10), mats.ember_glow, [0, .19, 0], [-Math.PI / 2, 0, 0]));
    return g;
  } });
