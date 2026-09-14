/**
 * Deep Field 3D — Spire environment kit (DESIGN-BRIEF §4 · Maps.Spire, sector 3).
 * A 40×40 m tower block, a floor every 10 m, roof at 40 m with the core on it.
 * Interior and exterior are both on screen constantly (the fire escape climbs
 * the outside in view of the atrium), so both halves get real finish.
 * Modules mirror GameRoot.BuildSpireStructures: 4 m floor bays, 8 m facade and
 * roof bays, 3 m fire-escape landing segments. Metres, Y-up.
 *
 * Origins — read before mounting:
 *   spire_floor        centred on the plate, slab top at +0.2  → mount at the plate's local centre (localY 0), NOT GroundLocal.
 *   spire_facade/lobby ground centre, authored 0…40 m         → GroundLocal (as today).
 *   spire_roof         ground centre, authored at 39.6…40 m   → GroundLocal (as today).
 *   spire_fireescape   centred on the landing, deck top +0.2  → localY 0.
 *   spire_*_flight / spire_stairwell: origin at the BOTTOM step, rising +10 m.
 */
import { buildSpaceSky } from './space-sky.js';
import { floorMaterials } from './floor-textures.js';
export const SPIRE = [];
const P = (o) => { SPIRE.push({ ...o, file: o.file || o.id + '.glb', swatch: o.swatch || '#8d99ad', dir: 'maps/spire/' }); };

export function makeSpireMats(THREE, mats) {
  const mat = (name, color, o = {}) => { const m = new THREE.MeshStandardMaterial({ color, roughness: .85, metalness: .1, flatShading: false, ...o }); m.name = name; mats[name] = m; return m; };
  mat('concrete_dark', 0x4e5158, { roughness: .95 });
  mat('paving', 0x6c6f74, { roughness: .92 });
  mat('asphalt', 0x2c2e33, { roughness: .96 });
  mat('paint_yellow', 0xd8b04a, { roughness: .7 });
  mat('paint_white', 0xd9dbe0, { roughness: .7 });
  mat('curtain_glass', 0x16283a, { roughness: .12, metalness: .55 });
  /* Glazing has to read LIGHTER than the hole next to it, or a solid panel and
     an open light are the same black strip from the plaza — which is exactly
     what happened when the solid lights were first cut in. `curtain_glass`
     stays as the dark pane for the awnings and sliders hung IN an opening
     (silhouetted against daylight, so dark is right). The two below are for
     glazing that must be seen as glazing: sky reflection, and the opaque
     shadow-box panel that fills the lower half of a real curtain-wall bay. */
  mat('glass_sky', 0x93a9c0, { roughness: .09, metalness: .5 });
  mat('glass_spandrel', 0x6d7f90, { roughness: .34, metalness: .25 });
  mat('office_lit', 0xffe2b0, { roughness: .3, emissive: new THREE.Color(0xffd79a), emissiveIntensity: .75 });
  mat('office_dim', 0x46607a, { roughness: .3, emissive: new THREE.Color(0x4a6c8c), emissiveIntensity: .3 });
  mat('mullion', 0x3a3f4a, { roughness: .45, metalness: .7 });
  mat('spandrel', 0x8a8e96, { roughness: .8, metalness: .05 });
  mat('spandrel_alt', 0x767b85, { roughness: .82, metalness: .05 });
  mat('escape_steel', 0x4a4f58, { roughness: .5, metalness: .7 });
  mat('escape_rust', 0x7a4a2c, { roughness: .9, metalness: .15 });
  mat('grating', 0x5a606b, { roughness: .6, metalness: .55 });
  mat('roof_felt', 0x35332f, { roughness: .98 });
  mat('gravel_roof', 0x6a655c, { roughness: .98 });
  mat('hvac_grey', 0x9aa0a8, { roughness: .55, metalness: .5 });
  mat('tank_steel', 0x7f8790, { roughness: .5, metalness: .6 });
  mat('neon_pink', 0xff4f7a, { roughness: .3, emissive: new THREE.Color(0xff4f7a), emissiveIntensity: 1.3 });
  mat('lamp_white', 0xe8f0ff, { roughness: .3, emissive: new THREE.Color(0xdbe6ff), emissiveIntensity: 1.1 });
  mat('sodium', 0xffb35c, { roughness: .3, emissive: new THREE.Color(0xff9a2e), emissiveIntensity: 1.2 });
  mat('beacon_red', 0xff2e4a, { roughness: .3, emissive: new THREE.Color(0xff2e4a), emissiveIntensity: 1.4 });
  // Painted steel pipe. A sprinkler main IS red, but it is oxide paint on
  // steel, not a beacon: `beacon_red` is emissive and belongs to the mast lamp
  // and the core, and a self-lit stripe under every bay of a 115-bay floor
  // plate outshone the ceiling troughs that are supposed to light it. This
  // reads as pipe — low saturation, a little sheen, no emission — and sits in
  // the same family as the rust and the trim.
  mat('pipe_oxide', 0x7a4038, { roughness: .62, metalness: .35 });
  // Office floor finish. The corridor runner is a shade or two darker than the
  // screed either side of it, which is how a real office marks circulation and
  // is what makes the lane legible without a single painted mark on it.
  mat('carpet_corridor', 0x3f4550, { roughness: .97, metalness: 0 });
  mat('carpet_corridor_alt', 0x474e5a, { roughness: .97, metalness: 0 });
  mat('carpet_seam', 0x343a44, { roughness: .98, metalness: 0 });
  return mats;
}

/* Deterministic per-module variation without touching the sim's RNG. */
const h = (i, j = 0) => { const x = Math.sin(i * 12.9898 + j * 78.233) * 43758.5453; return x - Math.floor(x); };

/* Terrain tile 20×20: plaza paving on a baked texture, four low-relief variants. */
P({ id: 'spire_terrain', label: 'Terrain tile (20 m)', size: '20 m', swatch: '#6c6f74', stats: { Tile: '20×20', Grid: '6×4', Relief: 'flat (kerbs 12 cm)' },
  note: 'Tileable plaza: 2 m concrete pavers baked into a 1024² albedo/roughness/normal set (expansion joints, grime along the joints, tyre scuff, chewing-gum specks, a faded yellow keep-clear box, wet patches) with an emissive map for the recessed uplights. Variants add a drain gully, a manhole with a cable cover, a planter kerb, or a bike-rack row. Flat edges so tiles seam.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { part, grp, mats, THREE, box, cyl } = K, g = grp('spire_terrain'), F = floorMaterials(THREE);
    const geo = new THREE.PlaneGeometry(20, 20, 4, 4); geo.rotateX(-Math.PI / 2);
    const ground = part('terrain_ground', geo, F.spire); ground.rotation.y = v * Math.PI / 2; ground.receiveShadow = true; g.add(ground);
    const gully = (n, x, z, rot) => { const d = grp(n, [x, 0, z]); d.rotation.y = rot; d.add(part(n + '_frame', box(6, .05, .5), mats.trim, [0, .025, 0])); for (let i = 0; i < 14; i++) d.add(part(n + '_bar' + i, box(.06, .06, .42), mats.grating, [-2.8 + i * .43, .055, 0])); return d; };
    const manhole = (n, x, z) => { const d = grp(n, [x, 0, z]); d.add(part(n + '_ring', cyl(.55, .6, .06, 20), mats.trim, [0, .03, 0])); d.add(part(n + '_lid', cyl(.48, .48, .03, 20), mats.escape_steel, [0, .07, 0])); d.add(K.boltRing(n + '_bolts', .52, 8, .075)); d.add(part(n + '_cover', box(1.6, .05, .5), mats.paint_yellow, [1.8, .025, .0])); return d; };
    const planter = (n, x, z) => { const d = grp(n, [x, 0, z]); d.add(part(n + '_kerb', box(4, .5, 1.2), mats.concrete_dark, [0, .25, 0])); d.add(part(n + '_soil', box(3.8, .05, 1.0), mats.dirt || mats.trim, [0, .5, 0])); for (let i = 0; i < 3; i++) d.add(part(n + '_shrub' + i, new THREE.IcosahedronGeometry(.42, 1), mats.hvac_grey, [-1.2 + i * 1.2, .85, 0])); return d; };
    const racks = (n, x, z) => { const d = grp(n, [x, 0, z]); for (let i = 0; i < 4; i++) d.add(part(n + '_hoop' + i, new THREE.TorusGeometry(.4, .03, 6, 14, Math.PI), mats.chrome, [-1.5 + i, .05, 0])); return d; };
    [() => { g.add(manhole('terrain_manhole', 5, -4)); }, () => { g.add(gully('terrain_gully', -3, 6, 0)); g.add(planter('terrain_planter', 6, -7)); }, () => { g.add(planter('terrain_planter', -6, 5)); g.add(racks('terrain_racks', 4, 7)); }, () => { g.add(gully('terrain_gully', 6, -2, Math.PI / 2)); g.add(manhole('terrain_manhole', -5, -6)); }][v % 4]();
    return g;
  } });

P({ id: 'spire_terrain_scatter', label: 'Terrain scatter (street furniture)', size: '5 m', swatch: '#4e5158', stats: { Place: 'off-route' },
  note: 'Bollard row, a bench and a newspaper box under a sodium street lamp. Scatter at the plaza margins only.',
  build(K) {
    const { part, grp, mats, THREE, box, cyl } = K, g = grp('spire_terrain_scatter');
    for (let i = 0; i < 4; i++) g.add(part('scatter_bollard' + i, cyl(.14, .16, 1.0, 10), mats.escape_steel, [-1.8 + i * 1.2, .5, 1.6]));
    for (let i = 0; i < 4; i++) g.add(part('scatter_bollard_band' + i, cyl(.15, .15, .1, 10), mats.paint_yellow, [-1.8 + i * 1.2, .85, 1.6]));
    g.add(part('scatter_bench_seat', box(2.0, .08, .5), mats.escape_rust, [0, .45, -.2])); for (const x of [-.8, .8]) g.add(part('scatter_bench_leg' + x, box(.08, .45, .5), mats.trim, [x, .22, -.2]));
    g.add(part('scatter_newsbox', box(.6, 1.1, .5), mats.chassis, [1.9, .55, -.6])); g.add(part('scatter_newsbox_window', box(.5, .5, .04), mats.curtain_glass, [1.9, .7, -.33]));
    g.add(part('scatter_lamp_post', cyl(.08, .12, 6, 10), mats.escape_steel, [-2.4, 3, -.8])); g.add(part('scatter_lamp_arm', box(1.4, .08, .08), mats.escape_steel, [-1.75, 5.9, -.8])); g.add(part('scatter_lamp_head', box(.7, .18, .35), mats.trim, [-1.1, 5.85, -.8])); g.add(part('scatter_lamp_lens', box(.6, .04, .3), mats.sodium, [-1.1, 5.75, -.8]));
    return g;
  } });

/* Lane module 4×3.4: asphalt with painted edge lines. */
P({ id: 'spire_path_ground', label: 'Lane module (4 m)', size: '4×3.4 m', swatch: '#2c2e33', stats: { Lane: '3.4 m', Repeat: 'along route' },
  note: 'One 4 m run of the 3.4 m ground lane at street level: asphalt with white edge lines, a centre dash and granite kerbs. Inside the building the same lane reads as a taped-off corridor. Flat legs only — the climbs are the stair and fire-escape flights.',
  build(K) {
    const { part, grp, box, mats } = K, g = grp('spire_path_ground');
    g.add(part('road_asphalt', box(4, .1, 3.2), mats.asphalt, [0, .05, 0]));
    for (const s of [-1, 1]) { g.add(part(`road_edge${s}`, box(4, .012, .12), mats.paint_white, [0, .105, s * 1.45])); g.add(part(`road_kerb${s}`, box(4, .16, .2), mats.concrete_dark, [0, .08, s * 1.7])); }
    for (let i = 0; i < 2; i++) g.add(part('road_dash' + i, box(1.2, .012, .1), mats.paint_yellow, [-1 + i * 2, .105, 0]));
    g.add(part('road_patch', box(1.1, .012, .8), mats.concrete_dark, [.9, .102, -.6]));
    return g;
  } });

/* Perimeter: the street on the far side, 10 m at a time. */
function frontage(K, id, v = 0) {
  const { part, grp, box, cyl, mats, THREE } = K, g = grp(id);
  const lit = (i) => { const r = h(v * 13 + 7, i); return r < .34 ? mats.office_lit : r < .62 ? mats.office_dim : mats.curtain_glass; };
  /* Carriageway, kerb and pavement in front of the block — the enclosure has to
     read as a street, so the road surface belongs to the wall piece. */
  g.add(part('street_asphalt', box(10, .1, 4.4), mats.asphalt, [0, .05, 3.9]));
  g.add(part('street_gutter', box(10, .012, .34), mats.concrete_dark, [0, .105, 2.0]));
  for (const z of [3.5, 3.78]) g.add(part('street_dyl' + z, box(10, .012, .1), mats.paint_yellow, [0, .108, z]));
  g.add(part('street_kerb', box(10, .18, .3), mats.concrete_dark, [0, .09, 1.75]));
  g.add(part('street_pavement', box(10, .16, 1.6), mats.paving, [0, .08, .9]));
  for (let i = 0; i < 5; i++) g.add(part('street_flag' + i, box(.03, .02, 1.5), mats.concrete_dark, [-4 + i * 2, .17, .9]));
  /* Three storeys of block: plinth, ground-floor units, two upper floors. */
  g.add(part('block_body', box(10, 12.4, 1.4), mats.spandrel, [0, 6.2, -.6]));
  g.add(part('block_plinth', box(10.1, .9, 1.55), mats.concrete_dark, [0, .45, -.6]));
  for (let f = 0; f < 2; f++) {
    const y = 5.4 + f * 3.4;
    g.add(part(`block_band${f}`, box(10.05, .3, 1.5), mats.concrete_dark, [0, y - .9, -.6]));
    for (let i = 0; i < 4; i++) {
      g.add(part(`block_win${f}_${i}`, box(1.5, 2.1, .26), lit(f * 4 + i), [-3.4 + i * 2.26, y + .6, .02]));
      g.add(part(`block_win_frame${f}_${i}`, box(1.66, 2.26, .18), mats.mullion, [-3.4 + i * 2.26, y + .6, -.02]));
      g.add(part(`block_sill${f}_${i}`, box(1.8, .12, .34), mats.concrete_dark, [-3.4 + i * 2.26, y - .52, .06]));
    }
  }
  g.add(part('block_cornice', box(10.2, .34, 1.7), mats.concrete_dark, [0, 12.4, -.6]));
  g.add(part('block_parapet', box(10.1, .8, 1.45), mats.spandrel, [0, 12.95, -.6]));
  g.add(part('block_coping', box(10.2, .12, 1.6), mats.mullion, [0, 13.4, -.6]));
  g.add(part('block_downpipe', cyl(.09, .09, 12.6, 8), mats.escape_rust, [4.7, 6.3, .05]));
  g.add(part('block_hopper', box(.3, .3, .3), mats.escape_rust, [4.7, 12.5, .05]));
  /* Ground floor by variant — a repeated shopfront on a lattice is the terrain
     tile's mistake at street scale. */
  if (v === 0) {                                                         // trading shopfronts
    for (const x of [-3.2, 0, 3.2]) {
      g.add(part('shop_glass' + x, box(2.6, 3.0, .22), mats.office_lit, [x, 2.6, .04]));
      g.add(part('shop_riser' + x, box(2.7, .8, .3), mats.mullion, [x, .7, .06]));
      for (const m of [-1.35, 1.35]) g.add(part(`shop_mullion${x}_${m}`, box(.14, 3.0, .32), mats.mullion, [x + m, 2.6, .08]));
      g.add(part('shop_fascia' + x, box(2.9, .7, .36), mats.trim, [x, 4.5, .1]));
      g.add(part('shop_awning' + x, box(2.8, .1, 1.1), mats.escape_rust, [x, 4.2, .6], [-.35, 0, 0]));
    }
    g.add(part('shop_neon', box(2.2, .3, .08), mats.neon_pink, [0, 4.5, .3]));
  } else if (v === 1) {                                                  // office lobby
    g.add(part('lobby_glass', box(6.4, 3.6, .2), mats.office_lit, [0, 2.1, .04]));
    for (const m of [-2.1, 0, 2.1]) g.add(part('lobby_mullion' + m, box(.16, 3.6, .3), mats.mullion, [m, 2.1, .08]));
    g.add(part('lobby_transom', box(6.4, .16, .3), mats.mullion, [0, 3.1, .08]));
    g.add(part('lobby_canopy', box(7.0, .2, 1.9), mats.mullion, [0, 4.3, .9]));
    for (const x of [-2, 0, 2]) g.add(part('lobby_downlight' + x, cyl(.16, .16, .04, 12), mats.lamp_white, [x, 4.18, 1.1]));
    g.add(part('lobby_pier', box(1.2, 4.2, .5), mats.concrete_dark, [-4.2, 2.1, .1]));
    g.add(part('lobby_pier_b', box(1.2, 4.2, .5), mats.concrete_dark, [4.2, 2.1, .1]));
  } else if (v === 2) {                                                  // blank gable, fire escape, billboard
    g.add(part('gable_face', box(10, 4.6, .2), mats.concrete_dark, [0, 2.3, .04]));
    for (let i = 0; i < 14; i++) g.add(part('gable_rung' + i, cyl(.03, .03, .5, 6), mats.escape_rust, [3.6, 1.2 + i * .42, .16], [0, 0, Math.PI / 2]));
    for (const s of [-1, 1]) g.add(part(`gable_stringer${s}`, box(.07, 6.0, .07), mats.escape_rust, [3.6 + s * .26, 3.4, .16]));
    g.add(part('gable_board_frame', box(7.2, 3.0, .16), mats.escape_steel, [-.6, 8.6, .18]));
    g.add(part('gable_board', box(6.8, 2.6, .06), mats.paint_white, [-.6, 8.6, .27]));
    g.add(part('gable_board_lamp', box(5.4, .12, .3), mats.lamp_white, [-.6, 10.4, .5]));
    g.add(part('gable_vent', box(1.2, 1.0, .3), mats.grating, [-4, 3.4, .1]));
  } else {                                                               // shuttered units
    for (const x of [-3.2, 3.2]) {
      g.add(part('shut_box' + x, box(3.0, 3.2, .26), mats.escape_steel, [x, 1.7, .04]));
      for (let i = 0; i < 10; i++) g.add(part(`shut_slat${x}_${i}`, box(2.9, .22, .06), mats.escape_rust, [x, .35 + i * .32, .17]));
      g.add(part('shut_head' + x, box(3.2, .4, .34), mats.mullion, [x, 3.6, .08]));
    }
    g.add(part('shut_door', box(1.1, 2.4, .2), mats.trim, [0, 1.2, .04]));
    g.add(part('shut_graffiti', box(2.6, 1.1, .02), mats.neon_pink, [-3.0, 1.5, .19]));
    g.add(part('shut_bin', box(1.3, 1.2, .9), mats.hvac_grey, [1.9, .76, .9]));
    g.add(part('shut_bin_lid', box(1.36, .1, .96), mats.trim, [1.9, 1.4, .9]));
  }
  /* Street furniture on the pavement: a wall lamp over the road and a stack. */
  g.add(part('block_lamp_arm', box(.08, .08, 1.5), mats.escape_steel, [-2.6, 6.4, .85]));
  g.add(part('block_lamp_head', box(.6, .16, .34), mats.trim, [-2.6, 6.3, 1.55]));
  g.add(part('block_lamp_lens', box(.5, .05, .28), mats.sodium, [-2.6, 6.2, 1.55]));
  g.add(part('block_ac', box(.9, .8, .6), mats.hvac_grey, [2.9, 10.6, .3]));
  g.add(part('block_ac_grille', new THREE.BoxGeometry(.7, .6, .06), mats.grating, [2.9, 10.6, .62]));
  return g;
}
P({ id: 'spire_wall_boundary', label: 'Street frontage (10 m)', size: '10×13.4 m', swatch: '#8a8e96', stats: { Perimeter: '110×80', Height: '13.4 m', Variants: '4' },
  note: 'The far side of the street, tiled around the 110×80 m field to enclose the plaza in a city block: carriageway, gutter, double yellows, kerb and flagged pavement in front of a three-storey block — plinth, ground-floor units, two floors of windows lit and dark, cornice, parapet, downpipe, wall lamp. Four ground floors (trading shopfronts, an office lobby, a blank gable with a fire escape and billboard, shuttered units) so the street does not repeat on a lattice. Authored facing +Z: the detailed face is the one that looks in at the Spire.',
  build(K) { return frontage(K, 'spire_wall_boundary', 0); },
  buildVariant(K, v = 0) { return frontage(K, 'spire_wall_boundary', v % 4); } });

/* Interior lane module 4 (X) × 3.4 (Z): the route, as office flooring. */
P({ id: 'spire_path_interior', label: 'Interior lane module (4 m)', size: '4×3.4 m', swatch: '#3f4550', stats: { Lane: '3.4 m', Height: '3 cm', Tile: '500 mm', Repeat: 'along route' },
  note: 'One 4 m run of the route where it crosses a floor plate — and INSIDE it is a building, not a roadway. The asphalt band, hazard-yellow edge lines, direction arrow and cat’s-eye studs are gone: a traffic arrow on an office plate read as a control rather than as where you are (§4.16), and nothing about a carriageway belongs above the lobby. What replaces them is the way a real office marks circulation: a 500 mm carpet-tile runner a couple of shades darker than the screed either side, laid in alternating rows so the pile catches light in bands, seams on the tile grid so it tiles at any 4 m join, a slim brushed-aluminium transition trim down both long edges where the carpet meets the screed, two flush floor boxes on the grid, PAIRED across the lane rather than set diagonally — this module was remade once already because two marks on opposite corners read as dots wandering side to side over a 40 m run instead of as a corridor. Contrast does the work paint used to: the runner reads as the walked line from across a 40 m plate, which is the whole reason this module exists. Three centimetres tall so it lays straight onto a slab; lay it along the flat legs at every level, the way `spire_path_ground` handles the street.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_path_interior');
    // The runner. Rows of tile alternate tone the way a quarter-turned carpet
    // tile does — eight 500 mm rows to the module, so the banding continues
    // across every join instead of restarting at it.
    for (let i = 0; i < 8; i++) g.add(part('lane_tile_row' + i, box(.5, .008, 3.4),
      i % 2 ? mats.carpet_corridor_alt : mats.carpet_corridor, [-1.75 + i * .5, .012, 0]));
    // Seams on the 500 mm grid, both ways.
    for (let i = 0; i < 8; i++) g.add(part('lane_seam_x' + i, box(.008, .002, 3.4), mats.carpet_seam, [-2 + i * .5, .017, 0]));
    for (const z of [-1.5, -1, -.5, 0, .5, 1, 1.5]) g.add(part('lane_seam_z' + z, box(4, .002, .008), mats.carpet_seam, [0, .017, z]));
    // Transition trim where the carpet meets the screed — the office answer to
    // an edge line, and the piece that actually stops the runner’s edge fraying
    // visually at this scale.
    for (const s of [-1, 1]) g.add(part(`lane_trim${s}`, box(4, .014, .06), mats.mullion, [0, .019, s * 1.67]));
    // Flush floor boxes: power and data, lids level with the pile, and PAIRED
    // across the lane on the seam cross — symmetric about the centre line, or a
    // 40 m run reads as dots wandering from side to side. The frame IS the lid:
    // a brighter metal cap at 200 mm is the cat’s-eye stud back again.
    // Local X runs ALONG the lane and Z across it, so the pair mirrors across
    // the centre at one station: paired along X tiled into a dashed centre line
    // every 1.8 m, the mark this module had deleted once already.
    for (const s of [-1, 1]) g.add(part('lane_floorbox' + s, box(.26, .012, .26), mats.trim, [0, .019, s * 1.1]));
    return g;
  } });

/* Floor bay 4 (X) × 12 (Z): slab centred on the plate, columns down to the floor below. */
P({ id: 'spire_floor', label: 'Floor bay (4 m)', size: '4×12 m', swatch: '#8a8e96', stats: { Slab: '0.4 m', Top: '+0.2', Columns: '2 × 10 m' },
  note: 'One 4 m bay of a 12 m-wide floor plate: concrete slab with a screed finish, a tape lane line, edge beams, two 0.5 m columns dropping 10 m to the floor below at the long edges, a recessed ceiling light trough and a sprinkler run on the underside. Wings run 10 bays along Z (rotate 90°); bridges run 4 bays along X squeezed to 10 m wide (scale z 0.83). Mount at the plate centre, not GroundLocal.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_floor');
    g.add(part('floor_slab', box(4, .4, 12), mats.concrete, [0, 0, 0]));
    g.add(part('floor_screed', box(4, .02, 11.6), mats.spandrel, [0, .21, 0]));
    g.add(part('floor_tape', box(4, .006, .1), mats.paint_yellow, [0, .222, 0]));
    for (const s of [-1, 1]) { g.add(part(`floor_edge_beam${s}`, box(4, .6, .3), mats.concrete_dark, [0, -.3, s * 5.85])); g.add(part(`floor_column${s}`, box(.5, 9.6, .5), mats.concrete_dark, [0, -5.0, s * 5.6])); g.add(part(`floor_column_cap${s}`, box(.7, .2, .7), mats.concrete, [0, -.3, s * 5.6])); }
    g.add(part('floor_trough', box(3.6, .1, .5), mats.trim, [0, -.25, 0])); g.add(part('floor_trough_lamp', box(3.4, .03, .3), mats.lamp_white, [0, -.31, 0]));
    g.add(part('floor_sprinkler', cyl(.05, .05, 11.4, 8), mats.pipe_oxide, [1.2, -.42, 0], [Math.PI / 2, 0, 0]));
    for (const z of [-4, 0, 4]) { g.add(part('floor_sprinkler_drop' + z, cyl(.028, .028, .1, 8), mats.pipe_oxide, [1.2, -.49, z])); g.add(part('floor_sprinkler_head' + z, cyl(.022, .04, .09, 8), mats.trim, [1.2, -.53, z])); }
    return g;
  } });

/* Facade bay 8 (X) × 40 (Y) × 1: curtain wall with an OPEN window band per
   storey — the wall is solid and the openings are void, because towers stand
   inside these and shoot out through them. */
function facade(K, id, seed, o = {}) {
  const { part, grp, box, mats } = K, g = grp(id), ap = [], D = 1.4;   // half the door width
  /* A doorway is a NOTCH in the storey's upstand rather than a hole in the
     middle of a wall: the window band above is already open, so cutting the
     1 m sill (or the 1.2 m plinth, at grade) over 2.8 m turns that storey's
     opening into something you can walk through. That is what the east face
     needs — the escape's ground leg crosses it at z 16, its re-entry at z 0 on
     floor two, and the landing gates open onto the plate at z 8 and z 0. */
  const door = o.doorAt ?? -1;
  /* Open-light patterns, 1 = void and 0 = glazed. Every row leaves at least
     one of each, so no storey is sealed and none is stripped bare. */
  const LIGHTS = [[1, 0, 0, 1], [0, 1, 1, 0], [1, 1, 0, 0], [0, 0, 1, 1],
    [1, 0, 1, 0], [0, 1, 0, 1], [1, 1, 0, 1], [1, 0, 1, 1]];
  let prevPat = -1;
  // Band tone ALTERNATES up the bay, with the starting tone hashed per bay: a
  // coin flip per storey landed on one tone four storeys running, so the
  // variation it was added for did not exist.
  const phase = h(seed, 100) < .5 ? 0 : 1;
  if (door === 0) for (const s of [-1, 1]) g.add(part(`${id}_plinth${s}`, box((8 - 2 * D) / 2, 1.2, 1.05), mats.concrete_dark, [s * (D + (8 - 2 * D) / 4), .6, 0]));
  else g.add(part(id + '_plinth', box(8, 1.2, 1.05), mats.concrete_dark, [0, .6, 0]));
  for (let f = 0; f < 4; f++) {
    /* Storeys are tied to the FLOOR PLATES (y 0 / 10 / 20 / 30), not to a
       façade pitch of its own. The old bay divided 40 m into four 9.7 m bands,
       which put storey one's glazing 1.9 m above the plate it belongs to — a
       tower standing on that floor would have been firing into masonry. */
    const y = f * 10, dr = f === door, stub = (8 - 2 * D) / 2, sx = D + stub / 2;
    /* The spandrel band runs 8.2..10.0 — it already stops exactly ON the floor
       line of the storey above, so a door bay leaves it alone: it is the wall
       UNDER the threshold. Only the 0.3 m slab edge, which stands 0.15 m proud
       of that line, is the lip, and only it gets notched. (part() chamfers a
       box inward and never inflates it, so these numbers are the box.) */
    g.add(part(id + '_spandrel' + f, box(8, 1.8, 1.0), (f + phase) % 2 ? mats.spandrel : mats.spandrel_alt, [0, y + 9.1, 0]));
    if (f + 1 === door) for (const s of [-1, 1]) g.add(part(id + `_slab_edge${f}_${s}`, box(stub, .3, 1.02), mats.concrete_dark, [s * (sx + .01), y + 10, 0]));
    else g.add(part(id + `_slab_edge${f}`, box(8.02, .3, 1.02), mats.concrete_dark, [0, y + 10, 0]));
    if (o.lobby && f === 0) { ap.push({ at: [0, 2.2, 0], size: [5.2, 4.0] }); continue; }
    // The sill is a 1 m upstand: a guard at a 30 m opening, and low enough that
    // a tower a metre inside the glass line has its muzzle over it. On the
    // ground storey the plinth is the upstand. A door storey gets two stubs.
    if (f > 0 && dr) for (const s of [-1, 1]) g.add(part(`${id}_sill${f}_${s}`, box((8 - 2 * D) / 2, .8, 1.0), mats.concrete_dark, [s * (D + (8 - 2 * D) / 4), y + .6, 0]));
    else if (f > 0) g.add(part(`${id}_sill${f}`, box(8, .8, 1.0), mats.concrete_dark, [0, y + .6, 0]));
    const sill = f === 0 ? 1.2 : y + 1.0, head = y + 7.8, hh = head - sill, cy = (sill + head) / 2;
    if (dr) for (const s of [-1, 1]) g.add(part(`${id}_cill${f}_${s}`, box((8 - 2 * D) / 2, .08, 1.12), mats.mullion, [s * (D + (8 - 2 * D) / 4), sill + .04, .04]));
    else g.add(part(`${id}_cill${f}`, box(8.02, .08, 1.12), mats.mullion, [0, sill + .04, .04]));
    g.add(part(`${id}_lintel${f}`, box(8, .4, 1.0), mats.spandrel, [0, y + 8.0, 0]));
    /* ── Which lights are open, and which are glazed solid ──────────────
       A curtain wall is not a missing wall. Every storey has four 1.84 m
       lights in the mullion grid, and only SOME of them are void: the rest
       carry fixed glazing, which is what makes this read as an office block
       from the plaza rather than as a multi-storey car park. The pattern is a
       hashed pick per storey, never the same two storeys running, and every
       pattern leaves at least one light open and at least one glazed — so a
       tower has a line out of every floor, and no floor looks stripped. */
    let pi = Math.floor(h(seed, 60 + f) * LIGHTS.length) % LIGHTS.length;
    if (pi === prevPat) pi = (pi + 3) % LIGHTS.length;
    prevPat = pi;
    const open = LIGHTS[pi].slice();
    // A doorway's own two lights are open whatever the pattern says.
    if (dr) { open[1] = 1; open[2] = 1; }
    for (let i = 0; i <= 4; i++) {
      const x = -4 + i * 2;
      // A mullion in the doorway is a post in a doorway; the door's own jambs
      // stand at its edges instead.
      if (dr && Math.abs(x) < D) continue;
      g.add(part(id + '_mullion' + f + '_' + i, box(.16, hh, .5), mats.mullion, [x, cy, .2]));
    }
    g.add(part(id + '_transom' + f, box(8, .16, .5), mats.mullion, [0, cy + 1.4, .2]));
    g.add(part(id + '_reveal' + f, box(7.68, .1, 1.0), mats.concrete_dark, [0, head - .05, 0]));
    if (dr) {
      for (const s of [-1, 1]) g.add(part(id + '_jamb' + f + '_' + s, box(.18, 3.0 + (f === 0 ? 1.2 : 1.0) - 1.0, 1.06), mats.mullion, [s * D, y + 1.5, 0]));
      // 1.9 m deep, not 1.3: the wall is 1.05 m thick and the plates stop at
      // x 19, so a threshold that only spans the wall leaves a 0.35 m hole in
      // the floor between the plate edge and the doorway. This laps both.
      g.add(part(id + '_threshold' + f, box(2 * D, .06, 1.9), mats.trim, [0, y + .03, 0]));
      g.add(part(id + '_threshold_nose' + f, box(2 * D, .05, .08), mats.paint_yellow, [0, y + .035, .92]));
    }
    const mid = cy + 1.4;   // the transom line: every light splits on it
    for (let i = 0; i < 4; i++) {
      const x = -3 + i * 2, r = h(seed, f * 4 + i);
      if (open[i]) {
        /* An OPEN light still reads as a window a building opened: some carry
           their awning pane hinged out at the transom, some have the sliding
           pane parked against a jamb. */
        if (r < .45) {
          /* A top-hung VENT, not the whole pane thrown wide. The full-height
             leaf reached z 1.11 (and 0.84 after the first trim), so on the east
             face it passed through the fire escape's inner stringer and
             handrail at x 20.72 — and the escape only moved out to x 21.8 to
             get clear of the floor plates, so it cannot give the ground back.
             A 0.9 m vent cracked 22° tops out at z 0.49, inside the wall's own
             outer face at 0.525: nothing on any face protrudes, which is also
             what a curtain wall actually opens with. */
          g.add(part(id + '_awning' + f + '_' + i, box(1.84, .9, .05), mats.curtain_glass, [x, cy + 2.1, .30], [-.38, 0, 0]));
          g.add(part(id + '_awning_stay' + f + '_' + i, box(.04, .5, .04), mats.mullion, [x + .8, cy + 1.95, .22], [-.38, 0, 0]));
        } else if (r < .62 && !(dr && Math.abs(x) < D + 1)) {
          g.add(part(id + '_slider' + f + '_' + i, box(.86, hh - .2, .06), mats.curtain_glass, [x + .49, cy, .22]));
        }
        continue;
      }
      /* A SOLID light: fixed glazing in the grid, split on the transom into an
         upper and a lower pane with a glazing bead down each jamb, so it reads
         as a glazed unit set INTO the wall rather than a panel stuck over the
         hole. What varies is what the pane is doing — sky reflection, a lit
         office, a dim one — hashed per light, so the elevation is a scatter of
         occupied and empty rooms the way a real block is at dusk. */
      const t = h(seed, 80 + f * 4 + i);
      const pane = t < .24 ? mats.office_lit : t < .5 ? mats.office_dim : mats.glass_sky;
      // The lower pane is the SHADOW BOX: opaque insulated panel behind glass,
      // which is what a curtain wall carries from floor to desk height and is
      // the half that makes the light read solid rather than dark.
      g.add(part(id + '_pane' + f + '_' + i + '_lo', box(1.84, mid - sill - .16, .06), mats.glass_spandrel, [x, (sill + mid) / 2, .2]));
      g.add(part(id + '_pane' + f + '_' + i + '_hi', box(1.84, head - mid - .16, .06), pane, [x, (mid + head) / 2, .2]));
      for (const s of [-1, 1]) g.add(part(id + '_bead' + f + '_' + i + '_' + s, box(.05, hh - .1, .1), mats.mullion, [x + s * .9, cy, .25]));
      // Blinds half-drawn on the occupied ones: the detail that says offices
      // behind the glass rather than a glass wall.
      if (t < .5) g.add(part(id + '_blind' + f + '_' + i, box(1.7, 1.3, .03), mats.paint_white, [x, head - .85, .13]));
    }
    // Occupied at night, seen THROUGH the open lights rather than reflected
    // off a pane: a strip on the reveal soffit, set back inside the wall.
    const lit = h(seed, 40 + f);
    g.add(part(id + '_soffit_lamp' + f, box(7.4, .12, .08), lit < .35 ? mats.lamp_white : lit < .6 ? mats.office_dim : mats.trim, [0, head - .3, -.38]));
    /* The sight contract: one entry per RUN of adjacent open lights, not one
       per storey. A tower behind a glazed light has no shot, and the whole
       point of listing apertures is that the sim can tell the difference. */
    for (let i = 0; i < 4; i++) {
      if (!open[i]) continue;
      let j = i; while (j + 1 < 4 && open[j + 1]) j++;
      const x0 = -3 + i * 2 - .92, x1 = -3 + j * 2 + .92, w = x1 - x0;
      // A door storey's opening runs from the floor, not from the sill.
      ap.push(dr && x0 < D && x1 > -D
        ? { at: [(x0 + x1) / 2, (y + head) / 2, 0], size: [w, head - y], door: [2 * D, 3.0] }
        : { at: [(x0 + x1) / 2, cy, 0], size: [w, hh] });
      i = j;
    }
  }
  g.add(part(id + '_parapet', box(8, .6, 1.1), mats.concrete_dark, [0, 40.3, 0]));
  /* The sight contract, declared by the art rather than guessed by the sim:
     each entry is an opening in the wall plane (local z 0, outward +Z before
     the placement yaw), centre and clear size in metres. A tower inside can
     see an enemy only through one of these, which is why the wall is solid
     everywhere else and why the openings are listed rather than implied. */
  g.userData.apertures = ap;
  g.userData.wall = { plane: 'z', thickness: 1.0, outward: [0, 0, 1] };
  return g;
}
P({ id: 'spire_facade', label: 'Facade bay (8 m)', size: '8×40 m', swatch: '#16283a', stats: { Storeys: '4', Lights: '4 × 1.84 m', Open: '1–3 per storey', Variants: 'door at storey 0–2' },
  note: 'One 8 m bay of the exterior wall, ground to parapet, and it is a WALL with windows in it. Each storey carries four 1.84 m lights in the mullion grid; a hashed pattern per storey decides which are void and which are glazed solid, never the same two storeys running, and every pattern leaves at least one of each — so a tower has a line out of every floor and no elevation looks stripped. Towers stand inside this building and fire out through the open lights, and the sim only lets a tower hit what it can see through one, so the glazed lights matter as much as the voids: a solid light is fixed glazing split on the transom into two panes with a glazing bead down each jamb, set into the reveal rather than stuck over it, and what the pane is DOING varies per light — sky reflection, a lit office, a dim one, blinds half-drawn on the occupied ones — so the block reads as a scatter of rooms at dusk the way a real office building does. Open lights keep the awning pane hinged out at the transom or the slider parked at a jamb. Storeys are pinned to the FLOOR PLATES at y 0/10/20/30, not to a facade pitch of their own: the old bay divided 40 m into four 9.7 m bands and put floor one’s glazing 1.9 m above the plate, so a tower on that floor was aiming into masonry. A 1 m sill upstand guards each opening and still clears a muzzle a metre inside the glass line. VARIANTS 1–3 notch the upstand over 2.8 m to make that storey’s opening a DOORWAY — jambs, threshold and hazard nose, no mullion in the way, and its two lights forced open — at storey 0, 1 and 2 respectively: the east (back) wall needs one at grade where the escape’s ground leg leaves the building, one at floor one where the landing gate lands, and one at floor two where the escape route walks back in. `userData.apertures` lists one entry per RUN of adjacent open lights (centre + clear size, wall plane local z 0; a door run also carries its `door` clear size) so both the collision punch and the line-of-sight test are built from the art rather than from numbers copied into C#. Five bays per face, all four faces. Lay along-Z faces with yaw 90.',
  build(K) { return facade(K, 'spire_facade', 3); },
  buildVariant(K, v = 0) { return facade(K, 'spire_facade', 3, v > 0 ? { doorAt: v - 1 } : {}); } });

P({ id: 'spire_lobby', label: 'Lobby bay (8 m)', size: '8×40 m', swatch: '#ff4f7a', stats: { Door: '5.2×4 m clear', Canopy: '2.5 m', Sign: 'neon' },
  note: 'Facade bay with the street entrance, and the doorway is a CLEAR 5.2 × 4 m opening: the revolving drums that stood in it are gone. Both ground routes walk through this bay and a tower in the lobby fires out through it, so anything in the aperture is either an obstruction to the lane or cover the enemy did not earn — the entrance doors instead stand swung open against the piers, which is also what a building under siege looks like. Piers, header, a fixed transom light over the door, a cantilevered canopy with downlights, a neon SPIRE sign, bollards. Storeys 2–4 match `spire_facade`, open windows included, and the doorway is declared in `userData.apertures` with them. Used twice on the west face: the main door at z 0 and the service door the fire-escape route uses at z 16.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = facade(K, 'spire_lobby', 9, { lobby: true });
    g.remove(g.getObjectByName('spire_lobby_plinth'));
    for (const s of [-1, 1]) g.add(part(`lobby_pier${s}`, box(1.0, 5.6, 1.05), mats.concrete_dark, [s * 3.5, 2.8, 0]));
    // Header runs from the transom light all the way to the storey's spandrel.
    // It is 2.6 m, not 1.2: the lobby branch skips the storey loop before the
    // facade's lintel is added, so a 1.2 m header left a 1 m x 8 m slot of open
    // wall above the door that was in nobody's aperture list — a second window
    // on the face both ground routes walk in through.
    g.add(part('lobby_header', box(8, 2.6, 1.05), mats.concrete_dark, [0, 6.9, 0]));
    g.add(part('lobby_glass_upper', box(6, 1.4, .2), mats.office_lit, [0, 4.9, 0]));
    // Door frame, and both leaves swung back against the piers: the 5.2 m
    // between them is void, so the lane walks through and a tower inside has
    // the street. A revolving drum in a doorway is a 2.2 m column of glass
    // standing in both.
    for (const s of [-1, 1]) g.add(part(`lobby_door_jamb${s}`, box(.14, 4.1, .6), mats.mullion, [s * 2.75, 2.05, .1]));
    g.add(part('lobby_door_head', box(5.8, .16, .6), mats.mullion, [0, 4.18, .1]));
    for (const s of [-1, 1]) { g.add(part(`lobby_door_leaf${s}`, box(1.3, 3.9, .06), mats.curtain_glass, [s * 3.05, 1.95, .62], [0, s * 1.28, 0])); g.add(part(`lobby_door_rail${s}`, box(1.3, .1, .09), mats.chrome, [s * 3.05, 1.1, .62], [0, s * 1.28, 0])); }
    g.add(part('lobby_floor', box(6, .12, 1.0), mats.spandrel, [0, .06, 0]));
    g.add(part('lobby_canopy', box(7.2, .25, 2.5), mats.mullion, [0, 5.4, 1.4]));
    for (const x of [-2.4, 0, 2.4]) g.add(part('lobby_downlight' + x, cyl(.18, .18, .04, 12), mats.lamp_white, [x, 5.26, 1.6]));
    g.add(part('lobby_sign_plate', box(3.6, .8, .1), mats.trim, [0, 7.6, .6])); g.add(part('lobby_sign_neon', box(3.0, .32, .06), mats.neon_pink, [0, 7.6, .68]));
    for (const x of [-3.2, 3.2]) g.add(part('lobby_bollard' + x, cyl(.14, .16, .9, 10), mats.escape_steel, [x, .45, 2.4]));
    return g;
  } });

/* Roof bay 8 (X) × 40 (Z) at true height (39.6–40). */
P({ id: 'spire_roof', label: 'Roof bay (8 m)', size: '8×40 m', swatch: '#35332f', stats: { Top: 'y 40.0', Parapet: '±Z edges', Bays: '5' },
  note: 'One 8 m bay of the roof at 40 m: felt slab with gravel strips, a parapet on the two Z edges, a roof drain and a service walkway. The X-end parapets are `spire_roof_parapet`. Variant 1 adds the atrium skylight — place it on ONE bay only: the skylight is a feature of the atrium, not of the module, and five bays laid end to end turned it into a 40 m glazed strip standing 0.5 m proud across the whole roof, which is the repeat-distance mistake MAP-AUTHORING §5 describes. The core stands at the centre; the fight ends here.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_roof');
    g.add(part('roof_slab', box(8, .4, 40), mats.roof_felt, [0, 39.8, 0]));
    for (const s of [-1, 1]) { g.add(part(`roof_gravel${s}`, box(8, .04, 3), mats.gravel_roof, [0, 40.02, s * 17.8])); g.add(part(`roof_parapet${s}`, box(8, 1.1, .4), mats.concrete_dark, [0, 40.55, s * 19.8])); g.add(part(`roof_coping${s}`, box(8.02, .1, .5), mats.mullion, [0, 41.12, s * 19.8])); }
    g.add(part('roof_walkway', box(1.6, .06, 34), mats.grating, [2.4, 40.03, 0]));
    if (v === 1) { g.add(part('roof_skylight_frame', box(3.6, .5, 4.2), mats.mullion, [0, 40.25, 0])); g.add(part('roof_skylight', box(3.2, .3, 3.8), mats.office_dim, [0, 40.35, 0])); }
    g.add(part('roof_drain', cyl(.25, .25, .04, 12), mats.trim, [-3, 40.02, 12])); g.add(part('roof_drain_grate', cyl(.2, .2, .03, 12), mats.grating, [-3, 40.05, 12]));
    for (const z of [-8, 8]) g.add(part('roof_vent' + z, cyl(.3, .35, .9, 10), mats.hvac_grey, [-2.6, 40.45, z]));
    return g;
  } });

P({ id: 'spire_roof_parapet', label: 'Roof parapet (8 m)', size: '8×1.3 m', swatch: '#4e5158', stats: { At: 'x ±20', Runs: '5 each' },
  note: 'End parapet for the roof\'s X edges (the roof bays only carry their own Z parapets), authored at 40 m; a warning lamp every bay.',
  build(K) {
    const { part, grp, box, mats } = K, g = grp('spire_roof_parapet');
    g.add(part('parapet_wall', box(8, 1.1, .4), mats.concrete_dark, [0, 40.55, 0])); g.add(part('parapet_coping', box(8.02, .1, .5), mats.mullion, [0, 41.12, 0]));
    g.add(part('parapet_lamp', box(.3, .12, .12), mats.beacon_red, [0, 41.25, 0]));
    return g;
  } });

/* Fire escape: landing segment 3 (Z) × 2.4 (X), and a 10 m flight. */
P({ id: 'spire_fireescape', label: 'Fire-escape landing (3 m)', size: '2.4×3 m', swatch: '#7a4a2c', stats: { Deck: 'grating', Width: '2.4 m', Rail: '1.1 m', Gate: 'variant 1', Segments: '3 per landing' },
  note: 'One 3 m segment of an 8 m landing bolted to the east wall: steel grating deck on channel stringers, tube handrail with mesh infill on both long edges, rust-streaked brackets on the building side. TWO POINT FOUR metres wide, not six: at six and mounted on the route line at x 20 the landing reached 3 m past the plate edge at x 19, so half of every landing and its whole inner handrail stood inside the office floor. Mounted at x 21.8 the deck runs 20.6–23.0 — clear of the back wall (whose outer face is 20.525) with its brackets bolting onto it, which is how a fire escape hangs. Variant 1 is the GATE segment: the inner rail is split around a 1.4 m opening with a lap plate onto the wall threshold and a grab stanchion each side, so a player steps out through the wall\u2019s doorway onto the escape instead of climbing over the rail. Place it as the middle segment of each landing, plain ones either side. Deck top at +0.2 — mount at the landing centre.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_fireescape'), W = 1.2;
    g.add(part('escape_deck', box(2.4, .08, 3), mats.grating, [0, .16, 0]));
    for (let i = 0; i < 12; i++) g.add(part('escape_bar' + i, box(2.4, .03, .04), mats.trim, [0, .21, -1.4 + i * .25]));
    for (const s of [-1, 1]) { g.add(part(`escape_stringer${s}`, box(.2, .4, 3), mats.escape_steel, [s * 1.1, -.05, 0])); }
    // Brackets bolt to the building, so they are on the inner side only.
    for (const z of [-1.1, 1.1]) g.add(part('escape_bracket' + z, box(.15, .8, .3), mats.escape_rust, [-1.1, -.45, z], [0, 0, -.5]));
    /* Handrails. The street side is continuous on every segment; the building
       side is continuous on a plain segment and split on the gate. */
    g.add(part('escape_post_o1', box(.08, 1.1, .08), mats.escape_steel, [W, .75, -1.4]));
    g.add(part('escape_post_o2', box(.08, 1.1, .08), mats.escape_steel, [W, .75, 1.4]));
    g.add(part('escape_rail_o', cyl(.03, .03, 3, 8), mats.escape_rust, [W, 1.3, 0], [Math.PI / 2, 0, 0]));
    g.add(part('escape_mesh_o', box(.02, .8, 2.8), mats.grating, [W, .7, 0]));
    if (v === 1) {
      /* The gate: a 1.4 m gap on the building side, a threshold plate over the
         joint onto the floor plate, and a grab stanchion each side of the
         opening — the thing you hold instead of the rail that used to be in
         the way. Rails and mesh stop 0.7 m short of the centre either side. */
      for (const s of [-1, 1]) {
        g.add(part(`escape_post_g${s}`, box(.08, 1.1, .08), mats.escape_steel, [-W, .75, s * 1.4]));
        g.add(part(`escape_stanchion${s}`, cyl(.045, .045, 1.14, 8), mats.escape_steel, [-W, .77, s * .7]));
        g.add(part(`escape_rail_g${s}`, cyl(.03, .03, .7, 8), mats.escape_rust, [-W, 1.3, s * 1.05], [Math.PI / 2, 0, 0]));
        g.add(part(`escape_mesh_g${s}`, box(.02, .8, .68), mats.grating, [-W, .7, s * 1.05]));
      }
      g.add(part('escape_threshold', box(.5, .05, 1.4), mats.trim, [-1.15, .19, 0]));
      g.add(part('escape_threshold_nose', box(.06, .05, 1.4), mats.paint_yellow, [-1.37, .192, 0]));
    } else {
      g.add(part('escape_post_i1', box(.08, 1.1, .08), mats.escape_steel, [-W, .75, -1.4]));
      g.add(part('escape_post_i2', box(.08, 1.1, .08), mats.escape_steel, [-W, .75, 1.4]));
      g.add(part('escape_rail_i', cyl(.03, .03, 3, 8), mats.escape_rust, [-W, 1.3, 0], [Math.PI / 2, 0, 0]));
      g.add(part('escape_mesh_i', box(.02, .8, 2.8), mats.grating, [-W, .7, 0]));
    }
    return g;
  } });

P({ id: 'spire_fireescape_flight', label: 'Fire-escape flight (10 m rise)', size: '2.2×8×10 m', swatch: '#4a4f58', stats: { Rise: '10 m', Run: '8 m (−Z)', Width: '2.2 m', Treads: '25' },
  note: 'The zigzag run between landings: two steel stringers, 25 open treads with a hazard nose, handrails both sides and an anchor bracket at each end. Origin at the bottom tread centre; climbs +10 m over 8 m toward −Z. TWO POINT TWO metres wide, matching the landing: at 2.8 m and mounted on the route line at x 20 the flight was 3.3 m over its rails, so its inner stringer, handrail and both end anchors crossed the plate edge at x 19 and stood proud through the office floor at every arrival. Mount at x 21.8 with the landings, outboard of the back wall — the whole escape then runs 20.6–23.0 and the route climbs on the treads.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_fireescape_flight'), n = 25, dz = -8 / n, dy = 10 / n, ang = Math.atan2(10, 8);
    /* The flight's own line is (0, +10, −8): it rises as it runs toward −Z, and
       every sloped part has to lie along THAT line. Rx(θ) carries a box's local
       +Z to (0, −sinθ, cosθ), so the tilt is +ang and not −ang — the negative
       laid the stringers and the rails up toward +Z, mirrored about the treads
       they carry, which is why the handrail ran diagonally across its own posts
       instead of capping them. Rx(θ) carries a cylinder's local +Y to
       (0, cosθ, sinθ), so a rail wants ang − π/2 by the same arithmetic. */
    /* Stringers hang UNDER the walking line rather than being centred on it.
       A 0.5 m box on the line (0.86 m once part() has inflated it) stands a
       third of a metre proud of the landing deck at each flight head, and at
       the head of flight one that is exactly across the gate opening a player
       steps out of the door into. Offset half a metre down the slope normal
       (0, 8, 10)/L and trimmed 0.5 m so the ends stop inside the anchors:
       treads on top, steel beneath, nothing above the deck it lands on. */
    const SL = Math.hypot(8, 10), off = .5;
    for (const s of [-1, 1]) g.add(part(`flight_stringer${s}`, box(.16, .5, SL - .5), mats.escape_steel, [s * 1.0, 5 - off * 8 / SL, -4 - off * 10 / SL], [ang, 0, 0]));
    for (let i = 0; i < n; i++) { const y = (i + .5) * dy, z = (i + .5) * dz; g.add(part('flight_tread' + i, box(2.2, .05, .34), mats.grating, [0, y, z])); g.add(part('flight_nose' + i, box(2.2, .05, .06), mats.paint_yellow, [0, y + .002, z - .16])); }
    for (const s of [-1, 1]) { g.add(part(`flight_rail${s}`, cyl(.03, .03, Math.hypot(8, 10), 8), mats.escape_rust, [s * 1.05, 6.1, -4], [ang - Math.PI / 2, 0, 0])); for (let i = 0; i < 5; i++) g.add(part(`flight_post${s}${i}`, box(.06, 1.05, .06), mats.escape_steel, [s * 1.05, (i + .5) * 2 + .5, -(i + .5) * 1.6])); }
    // Anchor plates centred ON the tread line, not 0.1 m over it: the route
    // arrives across these, and a 0.3 m bracket standing proud of the landing
    // deck is a lip in the lane at every flight head.
    for (const [y, z] of [[0, 0], [10, -8]]) g.add(part(`flight_anchor${y}`, box(2.3, .12, .4), mats.escape_rust, [0, y, z]));
    return g;
  } });

/* Interior stair flight: 10 m rise over 10 m along +X, 3.4 m wide. */
P({ id: 'spire_stairwell', label: 'Atrium stair flight (10 m)', size: '3.4×10×10 m', swatch: '#8a8e96', stats: { Rise: '10 m', Run: '10 m (+X)', Treads: '28' },
  note: 'One interior flight of the stair route: a concrete stringer slab with 28 closed treads, steel handrails on both sides, a strip lamp under each rail and a landing nose at the top. Origin at the bottom step; climbs +10 m along +X. Stretch X (scale) for the 18 m and 14 m legs — the treads stay legible.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_stairwell'), n = 28, dx = 10 / n, dy = 10 / n, L = Math.hypot(10, 10);
    g.add(part('stair_slab', box(L, .5, 3.4), mats.concrete_dark, [5, 4.7, 0], [0, 0, Math.PI / 4]));
    for (let i = 0; i < n; i++) { const x = (i + .5) * dx, y = (i + 1) * dy; g.add(part('stair_tread' + i, box(dx + .04, .08, 3.2), mats.spandrel, [x, y - .04, 0])); g.add(part('stair_riser' + i, box(.04, dy, 3.2), mats.concrete, [x - dx / 2, y - dy / 2, 0])); }
    for (const s of [-1, 1]) { g.add(part(`stair_rail${s}`, cyl(.035, .035, L, 8), mats.escape_steel, [5, 6.0, s * 1.62], [0, 0, Math.PI / 4 + Math.PI / 2])); g.add(part(`stair_rail_lamp${s}`, box(L, .03, .05), mats.lamp_white, [5, 5.05, s * 1.62], [0, 0, Math.PI / 4])); for (let i = 0; i < 5; i++) g.add(part(`stair_post${s}${i}`, box(.06, 1.0, .06), mats.escape_steel, [(i + .5) * 2, (i + .5) * 2 + .5, s * 1.62])); }
    g.add(part('stair_nose', box(.6, .1, 3.4), mats.paint_yellow, [10.2, 10.05, 0]));
    return g;
  } });

/* Skybox. */
P({ id: 'spire_skybox', label: 'Skybox', size: '1.4 km', swatch: '#2a2f4a', stats: { Type: 'deep space (shader)', Body: 'cratered moon, close', Stars: 'black-body tinted' },
  note: 'Realistic deep-space sky: a large airless moon low on the horizon lit by a cool white sun aligned to the key light, a denser Milky Way overhead since the roof fight is 40 m closer to it, faint violet nebulosity. Full sphere, unlit, fog-exempt; baked to an equirect on export.',
  build(K) { return buildSpaceSky(K.THREE, 'spire'); } });

/* Roof dressing. */
P({ id: 'spire_hvac', label: 'Dressing — HVAC unit', size: '3×1.8 m', swatch: '#9aa0a8', stats: { Roof: 'y 40', Fans: '2' },
  note: 'Rooftop air handler: ribbed cabinet on a steel frame, two fan grilles, a duct elbow into the roof, hazard-striped access panel and a condensate drip tray.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_hvac');
    g.add(part('hvac_frame', box(3.2, .3, 2.0), mats.escape_steel, [0, .15, 0]));
    g.add(part('hvac_body', box(3.0, 1.4, 1.8), mats.hvac_grey, [0, 1.0, 0]));
    for (let i = 0; i < 6; i++) g.add(part('hvac_rib' + i, box(.06, 1.3, 1.84), mats.mullion, [-1.25 + i * .5, 1.0, 0]));
    for (const x of [-.75, .75]) { g.add(part('hvac_fan_ring' + x, cyl(.55, .55, .12, 20), mats.mullion, [x, 1.76, 0])); g.add(part('hvac_fan_grille' + x, cyl(.5, .5, .04, 20), mats.grating, [x, 1.82, 0])); g.add(part('hvac_fan_hub' + x, cyl(.12, .12, .1, 12), mats.trim, [x, 1.86, 0])); }
    g.add(part('hvac_duct', cyl(.35, .35, 1.2, 12), mats.hvac_grey, [1.9, .6, -.4])); g.add(part('hvac_duct_elbow', new K.THREE.TorusGeometry(.5, .35, 10, 14, Math.PI / 2), mats.hvac_grey, [1.9, 1.2, .1], [0, Math.PI / 2, 0]));
    g.add(K.hazardStripes('hvac_panel_hazard', 1.2, .08, [-.6, .4, .93], [0, 0, 0], 6));
    g.add(part('hvac_tray', box(1.2, .06, .6), mats.escape_rust, [-1.2, .33, 1.3]));
    return g;
  } });

P({ id: 'spire_watertank', label: 'Dressing — water tank', size: '3.4×6 m', swatch: '#7f8790', stats: { Roof: 'y 40', Height: '6 m' },
  note: 'Riveted cylindrical tank on four braced legs with a conical lid, level gauge, ladder and an overflow pipe down one leg. Blocks sight — park it at a roof corner.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('spire_watertank');
    for (const [x, z] of [[-1.1, -1.1], [1.1, -1.1], [-1.1, 1.1], [1.1, 1.1]]) { g.add(part(`tank_leg${x}${z}`, box(.2, 2.6, .2), mats.escape_steel, [x, 1.3, z])); g.add(part(`tank_foot${x}${z}`, box(.5, .12, .5), mats.concrete_dark, [x, .06, z])); }
    for (const s of [-1, 1]) g.add(part(`tank_brace${s}`, box(.08, .08, 3.1), mats.escape_steel, [s * 1.1, 1.3, 0], [Math.PI / 4 * s, 0, 0]));
    g.add(part('tank_ring_beam', box(2.6, .2, 2.6), mats.escape_steel, [0, 2.7, 0]));
    g.add(part('tank_body', cyl(1.6, 1.6, 2.8, 24), mats.tank_steel, [0, 4.2, 0]));
    for (const y of [3.1, 4.2, 5.3]) g.add(part('tank_band' + y, new THREE.TorusGeometry(1.62, .04, 6, 28), mats.trim, [0, y, 0], [Math.PI / 2, 0, 0]));
    g.add(part('tank_lid', new THREE.ConeGeometry(1.7, .7, 24), mats.tank_steel, [0, 5.95, 0])); g.add(part('tank_finial', cyl(.08, .08, .5, 8), mats.chrome, [0, 6.4, 0]));
    g.add(part('tank_gauge', box(.12, 2.2, .06), mats.curtain_glass, [1.62, 4.2, 0])); g.add(part('tank_gauge_level', box(.08, 1.3, .04), mats.thruster || mats.chrome, [1.65, 3.75, 0]));
    for (let i = 0; i < 12; i++) g.add(part('tank_rung' + i, cyl(.02, .02, .5, 6), mats.chrome, [0, .5 + i * .45, -1.72], [0, 0, Math.PI / 2]));
    g.add(part('tank_overflow', cyl(.07, .07, 5.6, 8), mats.escape_rust, [-1.3, 2.8, -1.3]));
    return g;
  } });

P({ id: 'spire_dress_car', label: 'Dressing — parked car', size: '4.6×1.9 m', swatch: '#2c2e33', stats: { Length: '4.6 m', Variants: '4', Park: 'kerbside' },
  note: 'Kerbside traffic for the perimeter street: saloon, hatchback, panel van and taxi, each on four wheels with glazing, lamps, mirrors, a plate and a number-plate light. Authored along local +X (bonnet toward +X) so a row parks with one rotation. Dressing only — no collision, and never within 3 m of a route, socket or station.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('spire_dress_car');
    const paint = [0x9aa3b0, 0x38414f, 0x7a2b32, 0xd8c24a][v % 4];
    const body = new THREE.MeshStandardMaterial({ color: paint, roughness: .35, metalness: .5 }); body.name = 'car_paint';
    const L = v === 2 ? 5.4 : v === 1 ? 4.1 : 4.6, W = 1.86, van = v === 2;
    g.add(part('car_body', box(L, .8, W), body, [0, .74, 0]));
    g.add(part('car_sill', box(L - .3, .22, W - .06), mats.trim, [0, .42, 0]));
    if (van) {
      g.add(part('car_box', box(L - 1.5, 1.5, W), body, [-.5, 1.85, 0]));
      g.add(part('car_screen', box(.14, .9, W - .22), mats.curtain_glass, [L / 2 - 1.35, 1.72, 0], [0, 0, .22]));
      for (const s of [-1, 1]) g.add(part(`car_side${s}`, box(L - 2.2, .8, .04), mats.paint_white, [-.6, 1.9, s * (W / 2 + .01)]));
    } else {
      const cab = v === 1 ? L - 1.9 : L - 2.3;
      g.add(part('car_cabin', box(cab, .72, W - .14), body, [v === 1 ? -.3 : -.15, 1.44, 0]));
      g.add(part('car_roof', box(cab - .3, .06, W - .3), body, [v === 1 ? -.3 : -.15, 1.82, 0]));
      g.add(part('car_screen', box(.12, .68, W - .3), mats.curtain_glass, [cab / 2 + .1, 1.44, 0], [0, 0, .38]));
      g.add(part('car_rear_glass', box(.12, .6, W - .32), mats.curtain_glass, [-cab / 2 - .3, 1.44, 0], [0, 0, -.42]));
      for (const s of [-1, 1]) { g.add(part(`car_glass_f${s}`, box(1.1, .56, .04), mats.curtain_glass, [.5, 1.46, s * (W / 2 - .09)])); g.add(part(`car_glass_r${s}`, box(.9, .52, .04), mats.curtain_glass, [-.85, 1.46, s * (W / 2 - .09)])); }
    }
    g.add(part('car_bonnet', box(1.1, .1, W - .12), body, [L / 2 - .6, 1.13, 0]));
    g.add(part('car_grille', box(.12, .34, W - .5), mats.grating, [L / 2 - .02, .82, 0]));
    g.add(part('car_bumper_f', box(.26, .34, W + .02), mats.trim, [L / 2 - .06, .58, 0]));
    g.add(part('car_bumper_r', box(.26, .34, W + .02), mats.trim, [-L / 2 + .06, .58, 0]));
    for (const s of [-1, 1]) {
      g.add(part(`car_lamp_f${s}`, box(.1, .22, .46), mats.lamp_white, [L / 2 + .02, .96, s * (W / 2 - .38)]));
      g.add(part(`car_lamp_r${s}`, box(.1, .2, .44), mats.beacon_red, [-L / 2 - .02, .96, s * (W / 2 - .38)]));
      g.add(part(`car_mirror${s}`, box(.2, .12, .26), mats.trim, [L / 2 - 1.5, 1.34, s * (W / 2 + .12)]));
      for (const x of [L / 2 - 1.15, -L / 2 + 1.05]) {
        g.add(part(`car_tyre${s}${x}`, cyl(.33, .33, .22, 14), mats.trim, [x, .33, s * (W / 2 - .06)], [Math.PI / 2, 0, 0]));
        g.add(part(`car_hub${s}${x}`, cyl(.19, .19, .24, 12), mats.chrome, [x, .33, s * (W / 2 - .06)], [Math.PI / 2, 0, 0]));
        g.add(part(`car_arch${s}${x}`, box(.9, .1, .3), mats.trim, [x, .78, s * (W / 2 - .02)]));
      }
    }
    g.add(part('car_plate', box(.04, .16, .6), mats.paint_white, [-L / 2 - .18, .7, 0]));
    g.add(part('car_plate_lamp', box(.04, .05, .3), mats.lamp_white, [-L / 2 - .16, .92, 0]));
    if (v === 3) { g.add(part('car_sign', box(.9, .26, .42), mats.sodium, [.1, 1.95, 0])); g.add(part('car_livery', box(L - 1.2, .3, .04), mats.paint_yellow, [0, 1.02, W / 2 + .01])); g.add(part('car_livery_b', box(L - 1.2, .3, .04), mats.paint_yellow, [0, 1.02, -W / 2 - .01])); }
    return g;
  } });

P({ id: 'spire_antenna', label: 'Dressing — antenna mast', size: '12 m', swatch: '#ff2e4a', stats: { Roof: 'y 40', Beacon: 'red', Dishes: '2' },
  note: 'Lattice broadcast mast on a plinth: three chords with zigzag lacing, two microwave dishes, a whip aerial and an aviation beacon. Top at 52 m, under the 44 m air lane only if placed at a corner.',
  build(K) {
    const { part, grp, box, cyl, mats, THREE } = K, g = grp('spire_antenna');
    g.add(part('antenna_plinth', box(1.8, .4, 1.8), mats.concrete_dark, [0, .2, 0]));
    for (let i = 0; i < 3; i++) { const a = i / 3 * Math.PI * 2; g.add(part('antenna_chord' + i, cyl(.05, .07, 11, 8), mats.escape_steel, [Math.cos(a) * .5, 5.9, Math.sin(a) * .5])); }
    for (let k = 0; k < 10; k++) g.add(part('antenna_lace' + k, new THREE.TorusGeometry(.5, .025, 6, 3), mats.escape_steel, [0, 1.2 + k * 1.05, 0], [Math.PI / 2, k * .35, 0]));
    g.add(part('antenna_dish_a', new THREE.SphereGeometry(.8, 16, 10, 0, Math.PI * 2, 0, Math.PI / 3), mats.hvac_grey, [.9, 6.5, 0], [0, 0, -Math.PI / 2]));
    g.add(part('antenna_dish_b', new THREE.SphereGeometry(.6, 16, 10, 0, Math.PI * 2, 0, Math.PI / 3), mats.hvac_grey, [-.4, 9.0, .8], [Math.PI / 2, 0, 0]));
    g.add(part('antenna_whip', cyl(.02, .04, 2.5, 6), mats.chrome, [0, 12.5, 0]));
    g.add(part('antenna_beacon', new THREE.SphereGeometry(.18, 10, 8), mats.beacon_red, [0, 11.5, 0]));
    g.add(part('antenna_cabinet', box(.9, 1.2, .6), mats.hvac_grey, [1.2, 1.0, -.9]));
    return g;
  } });
