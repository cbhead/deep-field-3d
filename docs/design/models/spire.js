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
  mat('office_lit', 0xffe2b0, { roughness: .3, emissive: new THREE.Color(0xffd79a), emissiveIntensity: .75 });
  mat('office_dim', 0x2a3a4a, { roughness: .3, emissive: new THREE.Color(0x3a5570), emissiveIntensity: .25 });
  mat('mullion', 0x3a3f4a, { roughness: .45, metalness: .7 });
  mat('spandrel', 0x8a8e96, { roughness: .8, metalness: .05 });
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

/* Interior lane module 4 (X) × 3.4 (Z): the route, painted on a floor plate. */
P({ id: 'spire_path_interior', label: 'Interior lane module (4 m)', size: '4×3.4 m', swatch: '#d8b04a', stats: { Lane: '3.4 m', Height: '3 cm', Repeat: 'along route' },
  note: 'One 4 m run of the route where it crosses a floor plate: a dark traffic band worn through the screed, hazard-yellow edge lines flush with the band, one direction arrow and four recessed floor studs. Every mark is symmetric about the lane centre and tiles on the 4 m repeat, with the studs 2 m apart in pairs. No centre line — that would imply two-way traffic on a one-way enemy lane. The arrow (1.4 m shaft, barbs swept back from the tip) points local −X because the layer rotates each module by atan2(d.x,d.z)+90°, which maps local +X against the direction of travel. The band is what makes the lane read at a distance — white paint alone disappears on pale concrete. Three centimetres tall so it lays straight onto a slab; lay it along the flat legs at every level, the way `spire_path_ground` handles the street.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_path_interior');
    g.add(part('lane_band', box(4, .008, 3.4), mats.asphalt, [0, .012, 0]));
    for (const s of [-1, 1]) g.add(part(`lane_edge${s}`, box(4, .014, .16), mats.paint_yellow, [0, .019, s * 1.62]));
    // No centre line: it would imply two-way traffic, and its dashes ran into
    // the arrow shaft to read as one long barbed line. Edge lines, one arrow and
    // the studs carry the lane.
    g.add(part('lane_arrow_shaft', box(1.4, .016, .14), mats.paint_white, [0, .022, 0]));
    for (const s of [-1, 1]) g.add(part(`lane_arrow_barb${s}`, box(.64, .016, .14), mats.paint_white, [-.45, .022, s * .2], [0, -s * .675, 0]));
    // Studs in pairs across the lane, 2 m apart along it.
    for (const x of [-1, 1]) for (const s of [-1, 1]) g.add(part(`lane_stud${x}_${s}`, cyl(.09, .09, .024, 10), mats.lamp_white, [x, .022, s * 1.35]));
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
    g.add(part('floor_sprinkler', cyl(.05, .05, 11.4, 8), mats.beacon_red, [1.2, -.3, 0], [Math.PI / 2, 0, 0]));
    for (const z of [-4, 0, 4]) g.add(part('floor_sprinkler_head' + z, cyl(.03, .05, .12, 8), mats.chrome, [1.2, -.42, z]));
    return g;
  } });

/* Facade bay 8 (X) × 40 (Y) × 1: curtain wall over four spandrel bands, lit windows per bay. */
function facade(K, id, seed, o = {}) {
  const { part, grp, box, mats } = K, g = grp(id);
  g.add(part(id + '_plinth', box(8, 1.2, 1.05), mats.concrete_dark, [0, .6, 0]));
  for (let f = 0; f < 4; f++) {
    const y0 = 1.2 + f * 9.7 + (f === 0 ? 0 : 0);
    g.add(part(`${id}_spandrel${f}`, box(8, 1.4, 1.0), mats.spandrel, [0, y0 + 9.0, 0]));
    g.add(part(`${id}_slab_edge${f}`, box(8.02, .3, 1.02), mats.concrete_dark, [0, y0 + 8.35, 0]));
    if (o.lobby && f === 0) continue;
    for (let i = 0; i < 4; i++) { const lit = h(seed, f * 4 + i); g.add(part(`${id}_glass${f}_${i}`, box(1.84, 7.0, .3), lit < .3 ? mats.office_lit : lit < .55 ? mats.office_dim : mats.curtain_glass, [-3 + i * 2, y0 + 4.6, 0])); }
    for (let i = 0; i <= 4; i++) g.add(part(`${id}_mullion${f}_${i}`, box(.16, 7.2, .5), mats.mullion, [-4 + i * 2, y0 + 4.6, .2]));
    g.add(part(`${id}_transom${f}`, box(8, .16, .5), mats.mullion, [0, y0 + 2.4, .2]));
  }
  g.add(part(id + '_parapet', box(8, .6, 1.1), mats.concrete_dark, [0, 40.3, 0]));
  return g;
}
P({ id: 'spire_facade', label: 'Facade bay (8 m)', size: '8×40 m', swatch: '#16283a', stats: { Storeys: '4', Glass: 'curtain wall', Lit: 'per bay' },
  note: 'One 8 m bay of the exterior curtain wall, ground to parapet: stone plinth, four storeys of dark glazing in steel mullions over concrete spandrel bands, a scatter of lit and dimmed offices so the block reads occupied at night. Five bays per face; the east face is left open for the fire escape. Lay along-Z faces with yaw 90.',
  build(K) { return facade(K, 'spire_facade', 3); } });

P({ id: 'spire_lobby', label: 'Lobby bay (8 m)', size: '8×40 m', swatch: '#ff4f7a', stats: { Door: '6×4 m', Canopy: '2.5 m', Sign: 'neon' },
  note: 'Facade bay with the street entrance: a 6 m glazed opening with revolving-door drums, a cantilevered canopy with downlights, a neon SPIRE sign and a pair of bollards. Storeys 2–4 match `spire_facade`. Used twice on the west face: the main door at z 0 and the service door the fire-escape route uses at z 16.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = facade(K, 'spire_lobby', 9, { lobby: true });
    g.remove(g.getObjectByName('spire_lobby_plinth'));
    for (const s of [-1, 1]) g.add(part(`lobby_pier${s}`, box(1.0, 5.6, 1.05), mats.concrete_dark, [s * 3.5, 2.8, 0]));
    g.add(part('lobby_header', box(8, 1.2, 1.05), mats.concrete_dark, [0, 6.2, 0]));
    g.add(part('lobby_glass_upper', box(6, 1.4, .2), mats.office_lit, [0, 4.9, 0]));
    for (const x of [-1.5, 1.5]) { g.add(part('lobby_drum' + x, cyl(1.1, 1.1, 3.9, 20), mats.curtain_glass, [x, 1.95, 0])); g.add(part('lobby_drum_cap' + x, cyl(1.15, 1.15, .15, 20), mats.mullion, [x, 3.95, 0])); g.add(part('lobby_drum_axis' + x, cyl(.06, .06, 3.9, 8), mats.chrome, [x, 1.95, 0])); }
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

/* Fire escape: landing segment 3 (Z) × 6 (X) and a 10 m flight. */
P({ id: 'spire_fireescape', label: 'Fire-escape landing (3 m)', size: '6×3 m', swatch: '#7a4a2c', stats: { Deck: 'grating', Rail: '1.1 m', Segments: '3 per landing' },
  note: 'One 3 m segment of an 8 m landing on the east face: steel grating deck on channel stringers, tube handrail with mesh infill on both long edges, rust-streaked wall brackets. Deck top at +0.2 — mount at the landing centre.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_fireescape');
    g.add(part('escape_deck', box(6, .08, 3), mats.grating, [0, .16, 0]));
    for (let i = 0; i < 12; i++) g.add(part('escape_bar' + i, box(6, .03, .04), mats.trim, [0, .21, -1.4 + i * .25]));
    for (const s of [-1, 1]) { g.add(part(`escape_stringer${s}`, box(.2, .4, 3), mats.escape_steel, [s * 2.9, -.05, 0])); g.add(part(`escape_bracket${s}`, box(.15, .8, .3), mats.escape_rust, [s * 2.9, -.45, 0], [0, 0, s * .5])); }
    for (const s of [-1, 1]) { for (const z of [-1.4, 1.4]) g.add(part(`escape_post${s}${z}`, box(.08, 1.1, .08), mats.escape_steel, [s * 2.95, .75, z])); g.add(part(`escape_rail${s}`, cyl(.03, .03, 3, 8), mats.escape_rust, [s * 2.95, 1.3, 0], [Math.PI / 2, 0, 0])); g.add(part(`escape_mesh${s}`, box(.02, .8, 2.8), mats.grating, [s * 2.95, .7, 0])); }
    return g;
  } });

P({ id: 'spire_fireescape_flight', label: 'Fire-escape flight (10 m rise)', size: '3×8×10 m', swatch: '#4a4f58', stats: { Rise: '10 m', Run: '8 m (−Z)', Treads: '25' },
  note: 'The zigzag run between landings: two steel stringers, 25 open treads with a hazard nose, handrails both sides and an anchor bracket at each end. Origin at the bottom tread centre; climbs +10 m over 8 m toward −Z. Place at (19, 0, 18) and (20, 10, 10) for the escape route legs.',
  build(K) {
    const { part, grp, box, cyl, mats } = K, g = grp('spire_fireescape_flight'), n = 25, dz = -8 / n, dy = 10 / n, ang = Math.atan2(10, 8);
    for (const s of [-1, 1]) g.add(part(`flight_stringer${s}`, box(.16, .5, Math.hypot(8, 10)), mats.escape_steel, [s * 1.45, 5, -4], [-ang, 0, 0]));
    for (let i = 0; i < n; i++) { const y = (i + .5) * dy, z = (i + .5) * dz; g.add(part('flight_tread' + i, box(2.8, .05, .34), mats.grating, [0, y, z])); g.add(part('flight_nose' + i, box(2.8, .05, .06), mats.paint_yellow, [0, y + .002, z - .16])); }
    for (const s of [-1, 1]) { g.add(part(`flight_rail${s}`, cyl(.03, .03, Math.hypot(8, 10), 8), mats.escape_rust, [s * 1.5, 6.1, -4], [-ang + Math.PI / 2, 0, 0])); for (let i = 0; i < 5; i++) g.add(part(`flight_post${s}${i}`, box(.06, 1.05, .06), mats.escape_steel, [s * 1.5, (i + .5) * 2 + .5, -(i + .5) * 1.6])); }
    for (const [y, z] of [[0, 0], [10, -8]]) g.add(part(`flight_anchor${y}`, box(3.2, .3, .4), mats.escape_rust, [0, y + .1, z]));
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
