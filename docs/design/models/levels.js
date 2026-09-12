/**
 * Deep Field 3D — level layouts.
 *
 * Schema follows docs/MAP-AUTHORING.md (2026-09-09): routes, sockets, anchors,
 * volumes, areas, place, conditions. Foundry and Switchyard are regenerated
 * from the current sim/Sim.Core/Content/Maps.cs (44 sockets each — the old
 * mirror carried 12 and 16) and the graybox builders in GameRoot.cs. Spire is
 * authored here from its Maps.cs plan and the spire kit's own placement notes;
 * it has never been built.
 *
 * Sim space is X/Z with Y up; the Godot client converts with ToGd, we keep sim
 * coordinates directly.
 *
 *   routes    name → waypoint list. `air` is the flying layer.
 *   roads     which route ids get lane modules laid along them (flat legs only).
 *   sockets   [id, [x,y,z], tag] — sim truth, ids are permanent.
 *   volumes   { id, at, size, solid, surface?, run?, piece? } — the solid world
 *             (§2.4). Every `wall` socket must sit inside a solid one by ≥1 m;
 *             the viewer checks it.
 *   areas     { kind, at, size, to?, velocity? } — traversal (§2.5).
 *   place     [modelId, [x,y,z], rotY, scale?] — scale may be a [x,y,z] triple.
 */
export const LEVELS = {
  foundry: {
    label: 'Foundry', kit: 'foundry', field: [110, 80], totalWaves: 10,
    conditions: { 8: 'fog' },
    view: { cam: [60, 38, 72], target: [0, 2, 0] },
    routes: {
      ground: [[-40, 0, 0], [-20, 0, 0], [-20, 0, 14], [0, 0, 14], [0, 0, -8], [18, 0, -8], [18, 0, 6], [36, 0, 6]],
      // The strand now climbs to 13 m across the middle, where only the upper
      // deck reaches it — it used to fly a flat 8 m over the yard.
      air: [[-40, 9, -4], [-14, 13, -14], [8, 13, -16], [38, 9, 2]],
    },
    roads: ['ground'],
    sockets: [
      ['g7', [-34, 0, -6], 'ground'], ['g8', [-33, 0, 7], 'ground'], ['g9', [-26, 0, -6], 'ground'], ['g1', [-24, 0, 6], 'ground'], ['g11', [-26, 0, 14], 'ground'], ['g10', [-14, 0, -4], 'ground'],
      ['g2', [-14, 0, 9], 'ground'], ['g12', [-16, 0, 20], 'ground'], ['g13', [-6, 0, 20], 'ground'], ['g3', [-4, 0, 8], 'ground'],
      ['g14', [6, 0, 12], 'ground'], ['g4', [4, 0, 0], 'ground'], ['g15', [9, 0, 4], 'ground'], ['g16', [-5, 0, -5], 'ground'],
      ['g5', [12, 0, -2], 'ground'], ['g17', [0, 0, -13], 'ground'], ['g18', [22, 0, -13], 'ground'], ['g6', [24, 0, 0], 'ground'], ['g19', [30, 0, 0], 'ground'], ['g20', [30, 0, 13], 'ground'], ['g21', [14, 0, 10], 'ground'], ['g22', [36, 0, -2], 'ground'],
      // Upper deck: back row (w8/w9) trades the lane for the air strand.
      ['w1', [-6, 6, -16], 'wall'], ['w2', [2, 6, -16], 'wall'], ['w3', [10, 6, -16], 'wall'],
      ['w4', [-10, 6, -12.8], 'wall'], ['w5', [-2, 6, -12.8], 'wall'], ['w6', [6, 6, -12.8], 'wall'], ['w7', [14, 6, -12.8], 'wall'],
      ['w8', [-10, 6, -19.5], 'wall'], ['w9', [6, 6, -19.5], 'wall'],
      // Gantry bridge over the lane's elbow.
      ['w10', [9, 6, -7], 'wall'], ['w11', [9, 6, -1], 'wall'],
      ['t4', [-30, 0, 0], 'trap'], ['t1', [-20, 0, 7], 'trap'], ['t5', [-20, 0, 13], 'trap'], ['t6', [-10, 0, 14], 'trap'], ['t7', [0, 0, 10], 'trap'], ['t2', [0, 0, 3], 'trap'],
      ['t8', [0, 0, -4], 'trap'], ['t9', [9, 0, -8], 'trap'], ['t3', [18, 0, -1], 'trap'], ['t10', [18, 0, 5], 'trap'], ['t11', [28, 0, 6], 'trap'],
    ],
    heroSpawn: [0, 0, -24], armory: [-6, 0, -24],
    stations: [['spawnYard', [0, 0, -20]], ['upperDeck', [2, 6, -15]], ['midLane', [-8, 0, 5]], ['coreGate', [28, 0, 3]]],
    volumes: [
      { id: 'upperDeck', at: [2, 5.8, -16.5], size: [28, 0.4, 11], solid: true, surface: 'foundry_deck', run: 'x', piece: 4 },
      // The gantry reaches out over the elbow — 4 m wide, so w10/w11 sit 2 m
      // inside it — and it is a walkway, not a deck bay: a deck bay is 0.71 m
      // deep, so no mount height gives both a pad-flush surface at y 6 and the
      // 5.5 m of headroom §4.10 wants over the lane. Its span starts at z −11,
      // where the deck ends, so the two actually join.
      { id: 'gantry', at: [9, 6.0, -5], size: [4, 0.12, 12], top: 6.0, solid: true, surface: 'foundry_gantry_walk', run: 'z', piece: 4 },
    ],
    areas: [
      { kind: 'ladder', at: [-8, 3, -11.4], size: [1.2, 6, .8], tops: 6 },
      { kind: 'zipline', at: [14, 6.8, -17.5], size: [2, 2, 2], to: [33, 1.6, 2.5] },
      { kind: 'launcher', at: [-14, 0, -20], size: [2.4, .4, 2.4], velocity: [0, 12, 8] },
      { kind: 'armory', at: [-6, 0, -24], size: [3, 3, 3] },
    ],
    place: [
      // Upper deck: 28×11 at (2, 5.8, −16.5) → 7 bays of 4 m, one metre deeper
      // than the graybox so the front-lip row (w4–w7, z −12.8) sits a clear
      // metre inside the footprint — MAP-AUTHORING §4.9.
      ...[-10, -6, -2, 2, 6, 10, 14].map((x) => ['foundry_deck', [x, 0, -16.5], 0, [1, 1, 1.1]]),
      // Guard rail on the +Z edge (z −11.2), skipping the ladder bay (x −8) and zipline bay (x 14).
      ...[-10, -6, -2, 2, 6, 10].map((x) => ['foundry_deck_rail', [x, 6.0, -11.2], 0]),
      ['foundry_pillar', [-10, 0, -16.5], 0], ['foundry_pillar', [14, 0, -16.5], 0],
      // Gantry bridge: three 4 m walkway bays from the deck edge out over the
      // lane's elbow. The piece carries its own rails, and its walking surface
      // is at the mount, so it sits on y 6 with w10/w11 flush.
      ...[-9, -5, -1].map((z) => ['foundry_gantry_walk', [9, 6.0, z], 0]),
      // No near-end pillar: it stood 0.1 m off the lane at z −8, so the gantry
      // cantilevers off the deck edge and only its far end is propped.
      ['foundry_pillar', [9, 0, 1.4], 0],
      ['foundry_vent_tunnel', [0, 0, -6.5], 0],
      ['shared_vent_grate', [0, 0, -13.5], 0], ['shared_vent_grate', [0, 0, .5], Math.PI],
      // Traversal.
      ['shared_ladder', [-8, 0, -11.4], 0],
      ['shared_launcher_idle', [-14, 0, -20], 0],
      ['shared_zipline_anchor', [14, 6.0, -17.5], 0.62],
      ['shared_zipline_anchor', [33, 0, 2.5], -2.4, .6],
      ['shared_controlpoint_neutral', [26, 0, -6], 0],
      ['shared_armory_kiosk', [-6, 0, -24], 0],
      ['shared_spawn_portal', [-40, 0, 0], Math.PI / 2],
      ['shared_core', [40, 0, 6], -Math.PI / 2],
      // Air-lane pylons at the strand's waypoints (mast top = strand height).
      // The z −16 waypoint sits inside the deck footprint, so it gets no mast:
      // a pylon under the deck is the clearance defect MAP-AUTHORING §4.8 names.
      ['shared_airlane_pylon', [-40, 0, -4], 0, 1], ['shared_airlane_pylon', [-14, 0, -14], 0, 13 / 9], ['shared_airlane_pylon', [38, 0, 2], 0, 1],
      // Dressing — kept out of socket→route sightlines (behind sockets or beyond the lane).
      ['foundry_dress_crucible', [-32, 0, -18], .6], ['foundry_dress_gantry', [-30, 0, 24], 0], ['foundry_dress_pipes', [10, 0, 24], 0], ['foundry_dress_pipes', [-4, 0, 26], .2],
      ['foundry_dress_lightrig', [-20, 0, -26], 0], ['foundry_dress_lightrig', [30, 0, -18], 0], ['foundry_dress_lightrig', [30, 0, 22], 0],
      ['foundry_dress_steamvent', [-34, 0, 16], 0], ['foundry_dress_steamvent', [8, 0, 18], 1], ['foundry_dress_steamvent', [24, 0, 12], 2], ['foundry_dress_steamvent', [-12, 0, -30], 0],
      // Terrain scatter — corners and margins only, ≥2.5 m from any route, socket or kit footprint.
      ['foundry_terrain_scatter', [-46, 0, -30], .3], ['foundry_terrain_scatter', [-46, 0, 30], 1.2], ['foundry_terrain_scatter', [46, 0, -30], 2.1], ['foundry_terrain_scatter', [46, 0, 28], .8],
      ['foundry_terrain_scatter', [-20, 0, 32], 1.6], ['foundry_terrain_scatter', [30, 0, 32], .5], ['foundry_terrain_scatter', [12, 0, -32], 2.6], ['foundry_terrain_scatter', [-38, 0, 14], 1.0],
    ],
    zipline: [[14, 6.8, -17.5], [33, 1.6, 2.5]],
  },

  switchyard: {
    label: 'Switchyard', kit: 'switchyard', field: [110, 80], totalWaves: 12,
    conditions: { 8: 'night' },
    view: { cam: [60, 38, 72], target: [0, 2, 0] },
    routes: {
      ground: [[-45, 0, -10], [-25, 0, -10], [-25, 0, 12], [-5, 0, 12], [-5, 0, -12], [15, 0, -12], [15, 0, 10], [40, 0, 8]],
      groundShort: [[-45, 0, -10], [-20, 0, -2], [5, 0, 0], [40, 0, 8]],
      air: [[-45, 9, 4], [-13, 15, 7], [20, 15, -8], [44, 9, 2]],
    },
    roads: ['ground', 'groundShort'],
    sockets: [
      ['g8', [-38, 0, -1], 'ground'], ['g20', [-28, 0, -18], 'ground'], ['g1', [-30, 0, 2], 'ground'], ['g9', [-32, 0, 8], 'ground'],
      ['g2', [-18, 0, 8], 'ground'], ['g10', [-20, 0, 16], 'ground'], ['g11', [-10, 0, 16], 'ground'], ['g3', [-10, 0, 4], 'ground'], ['g13', [2, 0, 8], 'ground'], ['g21', [8, 0, 16], 'ground'],
      // These three go quiet the moment b1 closes the cut.
      ['g12', [-14, 0, -6], 'ground'], ['g4', [0, 0, -4], 'ground'], ['g14', [4, 0, -8], 'ground'],
      ['g5', [10, 0, 5], 'ground'], ['g15', [10, 0, -6], 'ground'], ['g6', [20, 0, -4], 'ground'], ['g16', [24, 0, 0], 'ground'], ['g17', [26, 0, -6], 'ground'], ['g7', [30, 0, 2], 'ground'], ['g18', [34, 0, -2], 'ground'], ['g19', [36, 0, 14], 'ground'],
      // Mid deck (y5) — moved over the long route's southern leg and within a
      // Nova's reach of the cut; deliberately out of range of the air strand.
      ['w1', [-16, 5, -11], 'wall'], ['w2', [-7, 5, -14.5], 'wall'], ['w5', [-19, 5, -15], 'wall'], ['w6', [-11, 5, -11.5], 'wall'], ['w7', [-2.5, 5, -12.5], 'wall'],
      // Upper catwalk (y10) — the only sockets that see the whole air strand.
      ['w3', [-4, 10, 6], 'wall'], ['w4', [10, 10, 0], 'wall'], ['w8', [-12, 10, 2], 'wall'], ['w9', [2, 10, 4], 'wall'],
      ['t5', [-35, 0, -10], 'trap'], ['t13', [-32, 0, -5.84], 'trap'], ['t1', [-25, 0, 0], 'trap'], ['t6', [-25, 0, 8], 'trap'], ['t7', [-15, 0, 12], 'trap'], ['t8', [-5, 0, 8], 'trap'], ['t2', [-5, 0, 0], 'trap'],
      ['t9', [-5, 0, -8], 'trap'], ['t4', [5, 0, 0], 'trap'], ['t10', [5, 0, -12], 'trap'], ['t3', [15, 0, -2], 'trap'], ['t11', [15, 0, 6], 'trap'], ['t12', [25, 0, 9.2], 'trap'],
      ['b1', [-8, 0, -1], 'barricade'],
    ],
    heroSpawn: [0, 0, -26], armory: [6, 0, -26],
    stations: [['yard', [0, 0, -22]], ['midDeck', [-11, 5, -13]], ['catwalk', [-1, 10, 3]], ['cutMouth', [-16, 0, -2]], ['coreGate', [32, 0, 5]]],
    cut: { route: 'groundShort', from: -18, to: -10 },
    volumes: [
      { id: 'midDeck', at: [-11, 4.8, -13], size: [20, 0.4, 8], solid: true, surface: 'switchyard_middeck', run: 'x', piece: 4 },
      { id: 'catwalk', at: [-1, 9.8, 3], size: [28, 0.4, 8], solid: true, surface: 'switchyard_catwalk', run: 'x', piece: 4 },
    ],
    areas: [
      // Tops out level with the mid deck's south edge, not under it.
      { kind: 'ladder', at: [-14, 2.5, -17.2], size: [1.2, 5, .8], tops: 5 },
      // The catwalk is 10 m up with nothing adjacent to climb: the yard hop pad
      // is the way onto it, and the zipline is the way off.
      { kind: 'launcher', at: [8, 0, -22], size: [2.4, .4, 2.4], velocity: [-3, 16, 14] },
      { kind: 'zipline', at: [12, 10.6, 4], size: [2, 2, 2], to: [35, 1.6, 2] },
      { kind: 'armory', at: [6, 0, -26], size: [3, 3, 3] },
    ],
    place: [
      // Mid deck 20×8 at (−11, 4.8, −13) → 5 bays; support column mid-run.
      ...[-19, -15, -11, -7, -3].map((x) => ['switchyard_middeck', [x, 0, -13], 0]),
      ['switchyard_column', [-11, 0, -13], 0, .49],
      // Catwalk 28×8 at (−1, 9.8, 3) → 7 bays widened to 8 m; lattice columns.
      ...[-13, -9, -5, -1, 3, 7, 11].map((x) => ['switchyard_catwalk', [x, 0, 3], 0, [1, 1, 1.6]]),
      ['switchyard_column', [-9, 0, 3], 0], ['switchyard_column', [9, 0, 6.5], 0],
      // Traversal: the yard→mid ladder is a height-matched asset, not Y-scaled.
      ['shared_ladder_500', [-14, 0, -17.2], Math.PI],
      ['shared_launcher_idle', [8, 0, -22], 0],
      ['shared_zipline_anchor', [12, 10.0, 4], 1.5], ['shared_zipline_anchor', [35, 0, 2], -1.7, .6],
      ['shared_armory_kiosk', [6, 0, -26], 0],
      ['shared_spawn_portal', [-45, 0, -10], Math.PI / 2],
      ['shared_core', [44, 0, 8], -Math.PI / 2],
      ['shared_airlane_pylon', [-45, 0, 4], 0, 1], ['shared_airlane_pylon', [-13, 0, 7], 0, 15 / 9], ['shared_airlane_pylon', [20, 0, -8], 0, 15 / 9], ['shared_airlane_pylon', [44, 0, 2], 0, 1],
      // Dressing — sidings along the ±Z margins and the far corners; nothing across a socket→route line.
      ['switchyard_dress_railcar', [-30, 0, 30], 0], ['switchyard_dress_railcar', [20, 0, 30], .05], ['switchyard_dress_railcar', [-30, 0, -30], 0],
      ['switchyard_dress_container', [30, 0, -30], 0], ['switchyard_dress_container', [30, 2.6, -30], .03], ['switchyard_dress_container', [36, 0, -27], 1.57], ['switchyard_dress_container', [-46, 0, 26], .4], ['switchyard_dress_container', [46, 0, -12], 1.57],
      ['switchyard_dress_signaltower', [-36, 0, -22], 0], ['switchyard_dress_signaltower', [-34, 0, 20], 3.14], ['switchyard_dress_signaltower', [26, 0, 16], 0],
      ['switchyard_dress_buffer', [-50, 0, 30], -Math.PI / 2], ['switchyard_dress_buffer', [-50, 0, -30], -Math.PI / 2], ['switchyard_dress_buffer', [50, 0, 30], Math.PI / 2],
      ['switchyard_terrain_scatter', [-40, 0, 20], .5], ['switchyard_terrain_scatter', [40, 0, -20], 2.0], ['switchyard_terrain_scatter', [0, 0, 34], 1.0], ['switchyard_terrain_scatter', [-8, 0, -34], .2], ['switchyard_terrain_scatter', [44, 0, 26], 2.8],
    ],
    zipline: [[12, 10.6, 4], [35, 1.6, 2]],
  },

  /**
   * Spire — sector 3, authored here for the first time (MAP-AUTHORING §7:
   * "design from scratch"). A 40×40 m block on a plaza, x ±20 / z ±20, floors
   * at 10 / 20 / 30 and the roof at 40 with the core on it.
   *
   * Every floor is a 12 m gallery ring around a 16×16 atrium, and the stair
   * route walks nearly all of it before it climbs: up the west gallery, along
   * the north, down the east, then west along the south gallery to the stair,
   * which rises ten metres through the opening in the plate above and lands
   * where the next lap starts. Four laps, one per level, then the roof. That is
   * the point of a vertical map — you give up a floor at a time, and every
   * metre of every floor is defensible ground someone has to walk.
   *
   * The last climb is external: the plate above a stair needs an opening, and
   * the roof cannot have one with the core standing on it, so the fourth flight
   * is the fire escape's and enemies come over the parapet. The escape is also
   * the whole alternative route — four exposed flights up the east face,
   * skipping every gallery, which is short and covered by everything. Long and
   * sheltered against short and open is the choice this map asks.
   *
   * The kit's own notes carry the shell placements: five 8 m facade bays per
   * face with the east face left open for the fire escape, the lobby bay used
   * twice on the west face (main door z 0, service door z 16), roof in five 8 m
   * bays with end parapets at x ±20.
   *
   * Getting down is the thing the old graybox failed hardest at, so the roof
   * has a zipline to the plaza and the escape's first landing has a hop pad
   * under it — a wipe on the roof should not cost the climb twice. The atrium
   * lift and the lobby↔roof pad pair are the client's existing traversal, now
   * written down. There are deliberately no west-face ladders: the facade
   * occupies x −20.5…−19.5 and the wing plate starts at x −20, so a ladder
   * there is narrower than the player and tops out under the floor it serves
   * (design-system README). The east side is the fire escape's.
   *
   * The field edge is a street: `spire_wall_boundary` tiles the far frontage
   * around all four sides in four ground-floor variants, and cars park at its
   * kerb, so the plaza reads as a city block rather than an open field.
   */
  spire: {
    label: 'Spire', kit: 'spire', field: [110, 80], totalWaves: 12,
    conditions: { 7: 'night', 11: 'fog' },
    // Above the 13.4 m street frontage that now rings the field and inside its
    // far side, so the enclosure reads as a street rather than a grey band.
    view: { cam: [72, 66, 88], target: [0, 10, 0] },
    routes: {
      // Interior stair: a lap of each gallery, then ten metres up. The lap runs
      // west gallery north → north gallery east → east gallery south → south
      // gallery west, and the flight sits in the last quarter, so the quarter
      // it skips is the one the stair itself occupies.
      stair: [
        [-34, 0, 0], [-20, 0, 0], [-14, 0, 0],
        [-14, 0, 14], [14, 0, 14], [14, 0, -14], [2, 0, -14],           // ground lap
        [-8, 10, -14], [-14, 10, -14],
        [-14, 10, 14], [14, 10, 14], [14, 10, -14], [2, 10, -14],       // floor one
        [-8, 20, -14], [-14, 20, -14],
        [-14, 20, 14], [14, 20, 14], [14, 20, -14], [2, 20, -14],       // floor two
        [-8, 30, -14], [-14, 30, -14],
        [-14, 30, 14], [14, 30, 14], [14, 30, -6], [22, 30, -6],        // floor three, out to the escape
        [22, 40, -14], [18, 40, -13],
        [13, 40, -13], [13, 40, 13], [-13, 40, 13], [-13, 40, -13], [0, 40, 0], // roof lap to the core
      ],
      // Fire escape: four flights up the outside of the east face, no gallery
      // walked at all. Short, and visible from everywhere.
      escape: [
        [-34, 0, 16], [-24, 0, 24], [26, 0, 24], [26, 0, 18], [21, 0, 18], // plaza, round the corner to the foot of the escape
        [22, 10, 10], [22, 20, 2], [22, 30, -6], [22, 40, -14], [18, 40, -13],
        [13, 40, -13], [13, 40, 13], [-13, 40, 13], [-13, 40, -13], [0, 40, 0],
      ],
      // Flyers spiral the outside and land on the roof, skipping every floor.
      air: [[-40, 8, 0], [-26, 20, -26], [26, 30, -26], [26, 40, 20], [0, 44, 0]],
    },
    // Only the legs outside the shell get asphalt; inside the footprint the
    // same route reads as a taped corridor at every level, grade included.
    roads: ['stair', 'escape'],
    indoor: { x: [-20, 20], z: [-20, 20] },
    sockets: [
      // Grade. Street and plaza pads cover the approach and the escape's plaza
      // leg from a few metres off it — a tower pad in the lane is a tower pad
      // enemies walk through; four more stand inside the lobby, where the
      // ground lap runs.
      ['g1', [-29, 0, 12], 'ground'], ['g2', [-27, 0, -6], 'ground'], ['g3', [-22, 0, 8], 'ground'], ['g4', [31, 0, 17], 'ground'],
      ['g5', [-30, 0, 26], 'ground'], ['g6', [-22, 0, 28], 'ground'], ['g7', [-6, 0, 28], 'ground'], ['g8', [18, 0, 28], 'ground'], ['g9', [28, 0, 22], 'ground'],
      ['g10', [0, 0, 8], 'ground'], ['g11', [-8, 0, 8], 'ground'], ['g12', [8, 0, 4], 'ground'], ['g13', [4, 0, -8], 'ground'],
      // Gallery pads — twelve per level, alternating either side of the lap
      // line so every leg is answered from both flanks and nothing stands more
      // than 4 m off the lane. Tagged ground: on a gallery the floor is the
      // ground, and the sim builds the same towers on ground and wall alike.
      ...[10, 20, 30].flatMap((y, n) => {
        const L = 'l' + (n + 1);
        return [
          [L + 'a', [-18, y, -14], 'ground'], [L + 'b', [-10, y, -6], 'ground'], [L + 'c', [-18, y, 2], 'ground'],
          [L + 'd', [-10, y, 10], 'ground'], [L + 'e', [-18, y, 18], 'ground'],
          [L + 'f', [-4, y, 10], 'ground'], [L + 'g', [4, y, 18], 'ground'],
          [L + 'h', [18, y, 14], 'ground'], [L + 'i', [10, y, 6], 'ground'],
          [L + 'j', [18, y, -2], 'ground'], [L + 'k', [10, y, -10], 'ground'], [L + 'l', [4, y, -18], 'ground'],
        ];
      }),
      // Roof: the last stand, and the only tier the air lane comes near. Eight
      // pads — the parapet where the escape arrives, all four sides of the roof
      // lap and the diagonal in to the core each answered from three.
      ['r1', [-17, 40, -10], 'ground'], ['r2', [-17, 40, 4], 'ground'], ['r3', [17, 40, -10], 'ground'], ['r4', [17, 40, -5], 'ground'],
      ['r5', [-6, 40, 17], 'ground'], ['r6', [11, 40, -17], 'ground'], ['r7', [8, 40, 16], 'ground'], ['r8', [-10, 40, -4], 'ground'],
      // Path plates, one or two per level, laid on the lane itself.
      ['t1', [-24, 0, 0], 'trap'], ['t2', [-14, 0, 8], 'trap'], ['t3', [6, 0, 14], 'trap'], ['t4', [14, 0, -6], 'trap'], ['t5', [2, 0, 24], 'trap'],
      ['t6', [-14, 10, 4], 'trap'], ['t7', [10, 10, 14], 'trap'], ['t8', [14, 20, 6], 'trap'], ['t9', [-14, 30, -6], 'trap'], ['t10', [13, 40, 4], 'trap'],
    ],
    heroSpawn: [-30, 0, 8], armory: [-26, 0, 10],
    stations: [['lobby', [-18, 0, 6]], ['mezzanine', [-4, 10, -14]], ['midFloor', [14, 20, 6]], ['upperFloor', [-4, 30, 14]], ['roof', [0, 40, -8]]],
    volumes: [
      // Each floor is a closed 12 m gallery ring: west and east galleries run
      // the full 40 m, north and south close the ends across the atrium. The
      // south gallery is short two bays at every level above grade — that gap
      // is the stair opening the flight below rises through.
      ...[10, 20, 30].flatMap((y) => [
        { id: `f${y}_west`, at: [-14, y - .2, 0], size: [12, .4, 40], top: y, solid: true, surface: 'spire_floor', run: 'z', piece: 4 },
        { id: `f${y}_east`, at: [14, y - .2, 0], size: [12, .4, 40], top: y, solid: true, surface: 'spire_floor', run: 'z', piece: 4 },
        { id: `f${y}_north`, at: [0, y - .2, 14], size: [16, .4, 12], top: y, solid: true, surface: 'spire_floor', run: 'x', piece: 4 },
        { id: `f${y}_south`, at: [4, y - .2, -14], size: [8, .4, 12], top: y, solid: true, surface: 'spire_floor', run: 'x', piece: 4 },
      ]),
      // The roof bay is authored at true height: you stand on 40.07, not 40.2.
      { id: 'roof', at: [0, 40, 0], size: [40, .4, 40], top: 40.07, solid: true, surface: 'spire_roof', run: 'x', piece: 8 },
      // Fire escape hangs off the east face: a landing at every floor and one
      // at roof level, 8 m of grating each.
      ...[[10, 10], [20, 2], [30, -6], [40, -14]].map(([y, z]) => (
        { id: `escape_l${y}`, at: [23.5, y - .2, z], size: [6, .2, 8], top: y, solid: true, surface: 'spire_fireescape', run: 'z', piece: 3 }
      )),
      // Stair flights, as the walkable ramps they are: three inside on the
      // south gallery, four outside zigzagging up the east face.
      ...[0, 10, 20].map((y) => ({ id: `flight_${y}`, at: [-3, y + 5, -14], size: [10, .5, 3.4], solid: true })),
      ...[0, 10, 20, 30].map((y, i) => ({ id: `escape_f${y}`, at: [22, y + 5, 14 - i * 8], size: [3, .5, 8], solid: true })),
    ],
    areas: [
      { kind: 'zipline', at: [18, 40.6, 18], size: [2, 2, 2], to: [36, 1.6, 26] },
      { kind: 'launcher', at: [28, 0, 12], size: [2.4, .4, 2.4], velocity: [-5, 15, -1.5] },
      // Cargo lift in the atrium, serving grade and all three floors; it stops
      // level with each plate, which the west ladders never did.
      { kind: 'elevator', at: [0, 20, -4], size: [3.4, 40, 3.4], stops: [0, 10, 20, 30, 40] },
      { kind: 'teleporter', at: [-16, 0, -6], size: [2, 2.4, 2], padId: 'lobbyRoof' },
      { kind: 'teleporter', at: [6, 40, -14], size: [2, 2.4, 2], padId: 'lobbyRoof' },
      { kind: 'armory', at: [-26, 0, 10], size: [3, 3, 3] },
    ],
    place: [
      /* ── Shell: facade bays, 8 m each, authored ground-to-parapet ────────── */
      // West face (x −20): main lobby at z 0, service lobby at z 16.
      ...[-16, -8, 8].map((z) => ['spire_facade', [-20, 0, z], -Math.PI / 2]),
      ['spire_lobby', [-20, 0, 0], -Math.PI / 2], ['spire_lobby', [-20, 0, 16], -Math.PI / 2],
      // South and north faces. The east face is left open for the fire escape.
      ...[-16, -8, 0, 8, 16].map((x) => ['spire_facade', [x, 0, -20], Math.PI]),
      ...[-16, -8, 0, 8, 16].map((x) => ['spire_facade', [x, 0, 20], 0]),
      /* ── Floors: a 12 m gallery ring per level around a 16×16 atrium ────── */
      ...[10, 20, 30].flatMap((y) => [
        // West and east galleries, 10 bays each along Z.
        // Mounted 0.2 m low: the bay's slab top is +0.2 from its mount, and the
        // pads and the route are on the level's own height, so mounting at the
        // height put the walking surface 20 cm above both of them.
        ...[-18, -14, -10, -6, -2, 2, 6, 10, 14, 18].flatMap((z) => [
          ['spire_floor', [-14, y - .2, z], Math.PI / 2],
          ['spire_floor', [14, y - .2, z], Math.PI / 2],
        ]),
        // The north gallery closes the ring; the south gallery keeps only its
        // two eastern bays — x −8…0 is the opening the stair rises through.
        ...[-6, -2, 2, 6].map((x) => ['spire_floor', [x, y - .2, 14], 0]),
        ...[2, 6].map((x) => ['spire_floor', [x, y - .2, -14], 0]),
      ]),
      /* ── Roof: 5 bays of 8 m, end parapets on the X edges ───────────────── */
      ...[-16, -8, 0, 8, 16].map((x) => ['spire_roof#0', [x, 0, 0], 0]),
      ...[-16, -8, 0, 8, 16].flatMap((z) => [
        ['spire_roof_parapet', [-20, 0, z], Math.PI / 2],
        ['spire_roof_parapet', [20, 0, z], Math.PI / 2],
      ]),
      /* ── Stair: one flight per level, stacked on the south gallery ──────── */
      ...[0, 10, 20].map((y) => ['spire_stairwell', [2, y, -14], Math.PI]),
      /* ── Fire escape: four flights up the east face, a landing at each ───── */
      ...[[0, 18], [10, 10], [20, 2], [30, -6]].map(([y, z]) => ['spire_fireescape_flight', [y === 0 ? 21 : 22, y, z], 0]),
      ...[[10, 10], [20, 2], [30, -6], [40, -14]].flatMap(([y, z]) => [-2.6, 0, 2.6].map((d) => ['spire_fireescape', [23.5, y - .2, z + d], 0])),
      /* ── Fixtures ────────────────────────────────────────────────────────── */
      ['shared_core', [0, 40.02, 0], -Math.PI / 2],
      ['shared_spawn_portal', [-34, 0, 0], Math.PI / 2], ['shared_spawn_portal', [-34, 0, 16], Math.PI / 2],
      ['shared_armory_kiosk', [-26, 0, 10], Math.PI / 2],
      ['shared_launcher_idle', [28, 0, 12], -Math.PI / 2],
      ['shared_zipline_anchor', [18, 40.02, 18], 2.2], ['shared_zipline_anchor', [36, 0, 26], -0.9, .6],
      // Only the strand's first waypoint is inside mast height; the rest of the
      // air lane is above the block, which is the point of it.
      ['shared_airlane_pylon', [-40, 0, 0], 0, 8 / 9],
      /* ── Service plant at grade. The tank is not on the roof: the lap runs
         the full ±13 ring, leaving a 6.8 m diagonal pocket in each corner, and
         a 3.4 × 6 m tank cannot sit 3 m clear of the lap and inside the parapet
         at once. Its own note says it blocks sight, so it stands against the
         block's south-east corner where that is an asset. ── */
      ['spire_watertank', [26, 0, -26], .4],
      /* ── Roof dressing: corners only, clear of the core, lap and air lane ─ */
      ['spire_antenna', [-17.5, 40.02, -17.5], 0],
      ['spire_hvac', [-18, 40.02, 16], Math.PI / 2], ['spire_hvac', [18, 40.02, 8], -Math.PI / 2],
      /* ── Plaza dressing — margins only, ≥3 m off every route and socket ── */
      ['spire_terrain_scatter', [-44, 0, -22], .3], ['spire_terrain_scatter', [-44, 0, 26], 1.4], ['spire_terrain_scatter', [44, 0, -24], 2.2], ['spire_terrain_scatter', [42, 0, 24], .7],
      ['spire_terrain_scatter', [-12, 0, 30], 1.1], ['spire_terrain_scatter', [27, 0, 33], .2], ['spire_terrain_scatter', [-2, 0, -30], 2.7], ['spire_terrain_scatter', [30, 0, -12], 1.8],
      /* ── Kerbside traffic on the perimeter street ─────────────────────── */
      // The frontage piece carries the carriageway; cars park just off its kerb
      // — z ±37.6 on the long edges, x ±52.6 on the short ones, wheels at y .1 so
      // they stand on the carriageway rather than 10 cm into it. Nose direction
      // alternates the way a real street parks, and every car is ≥10 m from the
      // nearest socket, so nothing here reads as gameplay.
      ['spire_dress_car#0', [-40, .1, 37.6], Math.PI], ['spire_dress_car#2', [-33, .1, 37.6], Math.PI], ['spire_dress_car#1', [-19, .1, 37.6], 0],
      ['spire_dress_car#3', [-7, .1, 37.6], Math.PI], ['spire_dress_car#0', [7, .1, 37.6], 0], ['spire_dress_car#2', [21, .1, 37.6], Math.PI], ['spire_dress_car#1', [34, .1, 37.6], 0],
      ['spire_dress_car#1', [-44, .1, -37.6], 0], ['spire_dress_car#3', [-27, .1, -37.6], 0], ['spire_dress_car#0', [-14, .1, -37.6], Math.PI],
      ['spire_dress_car#2', [11, .1, -37.6], 0], ['spire_dress_car#1', [26, .1, -37.6], Math.PI], ['spire_dress_car#0', [40, .1, -37.6], 0],
      ['spire_dress_car#3', [52.6, .1, -20], -Math.PI / 2], ['spire_dress_car#0', [52.6, .1, -5], Math.PI / 2], ['spire_dress_car#2', [52.6, .1, 14], -Math.PI / 2],
      ['spire_dress_car#1', [-52.6, .1, -12], Math.PI / 2], ['spire_dress_car#0', [-52.6, .1, 7], -Math.PI / 2], ['spire_dress_car#3', [-52.6, .1, 23], Math.PI / 2],
    ],
    zipline: [[18, 40.6, 18], [36, 1.6, 26]],
  },

  /**
   * M4 — The Toaster. Mirrored from `Maps.Toaster` and `ToasterLayout`: the
   * routes, sockets, hero stations and vehicle spawns are sim truth and are
   * copied verbatim, coordinate for coordinate. Volumes, areas and `place` are
   * design's.
   *
   * 320 × 160 — six times the area of any map before it, so two things here
   * are laid by RULE rather than listed: the treeline (about 780 trees on a
   * hashed 3.5 m lattice, `belt`) and the made roads (`vroads`, ninety-odd 4 m
   * modules on the authored polylines). A `place` array of 870 entries is not
   * something anyone can read or diff, and upstream generates both from the
   * same rules — keeping the rule in the kit means the client and the art
   * cannot drift apart.
   *
   * `roads: []` is deliberate and is the one place this map breaks with the
   * other three. Everywhere else the enemy lane IS the road and gets paved.
   * Here the made roads are the VEHICLE network — handling is read off those
   * centre lines — and the enemy routes mostly go cross-country over grass.
   * Paving the lanes would put tarmac through the middle of the fields and,
   * worse, tell a driver the grip changes where it does not.
   */
  toaster: {
    label: 'The Toaster', kit: 'toaster', field: [320, 160], totalWaves: 12,
    conditions: { 8: 'night' },
    view: { cam: [150, 96, 190], target: [-10, 2, 0] },
    sky: 'toaster_skybox',
    routes: {
      // The long way: out of the north-east gate, down the pond's west side,
      // east along the south road, then through the south pad to the west pad
      // and back across the whole property. Legs 11 and 20 are warps.
      long: [
        [72, 0, 24], [69, 0, -4], [57, 0, -9], [17, 0, -13], [5, 0, -16], [-4, 0, -31],
        [-7, 0, -50], [0, 0, -57], [17, 0, -58], [53, 0, -57], [74, 0, -52], [72, 0, -40],
        [-137, 0, 11], [-131, 0, 13], [-110, 0, 20], [-98, 0, 38], [-77, 0, 45], [-61, 0, 49],
        [-54, 0, 54], [-50, 0, 60], [-44, 0, 60], [52, 0, 29], [56, 0, 22], [56, 0, 19],
        [53, 0, 13], [39, 0, 12], [10, 0, 10], [-7, 0, -3], [-25, 0, -10], [-45, 0, -10], [-57, 0, -10],
      ],
      // No warps: gate to core down the drive, 139 m. The first two waves are
      // this and nothing else, so the core is learned before the pads are.
      direct: [
        [72, 0, 24], [56, 0, 19], [53, 0, 13], [39, 0, 12], [10, 0, 10], [-7, 0, -3],
        [-25, 0, -10], [-45, 0, -10], [-57, 0, -10],
      ],
      // One warp: the south loop, out at the west pad, back along the core's
      // blind side — everything the drive's kill-box points the wrong way for.
      west: [
        [72, 0, 24], [69, 0, -4], [57, 0, -9], [17, 0, -13], [5, 0, -16], [-4, 0, -31],
        [-7, 0, -50], [0, 0, -57], [17, 0, -58], [53, 0, -57], [74, 0, -52], [72, 0, -40],
        [-137, 0, 11], [-124, 0, 14], [-108, 0, 4], [-80, 0, -8], [-57, 0, -10],
      ],
      // Nine metres over the drive, on the walkers' own line. A farm has no
      // deck, so a strand anywhere else would be a lane nothing in the game
      // can reach — the §4.5 failure rather than a second front.
      air: [
        [72, 9, 24], [56, 9, 19], [53, 9, 13], [39, 9, 12], [10, 9, 10], [-7, 9, -3],
        [-25, 9, -10], [-45, 9, -10], [-57, 9, -10],
      ],
    },
    teleportLegs: { long: [11, 20], west: [11] },
    roads: [],
    /* The made roads. Vehicle handling reads grip, acceleration and top speed
       off these lines, so where a road runs is gameplay and what it is drawn
       with is not.

       Upstream's table gives four fragments and they do not join: the barn
       drive stops 19 m short of the barn and its far end dies 8 m off the
       county road; the county road ends at (35, 13) in open field; the south
       road begins and ends in open field 100 m inside each boundary; and the
       circular drive sat 22 m clear of the house it serves. Four stubs is not
       a network — nothing drives from anywhere to anywhere — and “each
       vehicle drives, turns, stops at the edge and lets you out” cannot be
       checked on a road that goes nowhere.

       So every authored vertex is KEPT and the gaps are closed by extension:
       the county road now runs from off the north boundary to a junction with
       the south road, the south road crosses the full 320 m and leaves the
       map at both ends, the barn drive runs up the barn's east flank onto the
       apron in front of its big +Z door and meets the county road at its
       (−54, 45) vertex, and two gravel spurs reach the two houses that had no
       road at all. The whole thing is one connected graph. */
    vroads: [
      // County road: north boundary → past the Vehickle drive → south road.
      /* (16, 12) and the southern tail are ADDED vertices, not moved ones:
         every point upstream authored is still on the line. The bow at (16,
         12) exists because the straight from (10, 22) to (35, 13) runs
         through the Vehickle house's north-west corner — 2 m of centreline
         inside the footprint, 5 m once the 6 m carriageway is counted — and
         a road through a building is worse than a road that ends nowhere.
         The tail turns at (64, −42) and (70, −58) for the same reason at the
         Grnmchn house: the direct line to the south road clipped its
         south-west corner by 4.5 m. */
      { kind: 'asphalt', width: 6, pts: [
        [-54, 0, 80], [-54, 0, 45], [-36, 0, 54], [0, 0, 54], [10, 0, 22], [16, 0, 12], [35, 0, 13],
        [52, 0, -14], [64, 0, -42], [70, 0, -58], [72, 0, -66]] },
      // South road: a through road, so it runs off both edges (x ±160).
      { kind: 'asphalt', width: 6, pts: [[-160, 0, -66], [-45, 0, -66], [97, 0, -66], [160, 0, -66]] },
      /* Barn drive. The barn's vehicle door is on +Z, on the far side from
         the authored drive, so the gravel runs up the east flank and turns
         onto the apron — which is why the Gator can be driven out of the shed
         rather than parked facing a wall. */
      { kind: 'gravel', width: 4, pts: [
        [-108, 0, 66], [-95, 0, 66], [-90, 0, 64], [-88, 0, 58], [-89, 0, 45], [-72, 0, 47], [-61, 0, 49], [-54, 0, 45]] },
      // Buggy house spur: the only way to the west house, off the barn drive.
      { kind: 'gravel', width: 4, pts: [[-124, 0, -1], [-120, 0, 2], [-112, 0, 14], [-100, 0, 30], [-89, 0, 45]] },
      // Grnmchn spur: off the county road's last bend to the −X front door,
      // approaching from the west so it never crosses the footprint.
      { kind: 'gravel', width: 4, pts: [[70, 0, -58], [75, 0, -52], [78, 0, -50]] },
    ],
    /* Circular drive, at the Vehickle house's front rather than 22 m north of
       it. Centre (24, 4) with the kit's 12 m centreline radius puts the ring
       at z −8…12 and x 12…36: two metres clear of the house's north face,
       three from the front door, and near enough tangent to the county road
       where it passes that the two read as one junction. */
    drive: [24, 0, 4],
    belt: { depth: 12, step: 3.5 },
    pond: { at: [19, 0, -36], radius: 17 },
    sockets: [
      ['g1', [61.9, 0, 5.3], 'ground'], ['g2', [66.1, 0, 16.1], 'ground'], ['g3', [56.4, 0, 0.2], 'ground'],
      ['g4', [18.8, 0, -1.8], 'ground'], ['g5', [24.5, 0, -1.2], 'ground'], ['g6', [30.2, 0, -0.6], 'ground'],
      ['g7', [36.1, 0, -1.5], 'ground'], ['g8', [5.8, 0, -10.6], 'ground'], ['g9', [-10.8, 0, -42.1], 'ground'],
      ['g10', [-9.3, 0, -32.6], 'ground'], ['g11', [-0.2, 0, -48.6], 'ground'], ['g12', [3.1, 0, -52.2], 'ground'],
      ['g13', [13.9, 0, -62.8], 'ground'], ['g14', [30.0, 0, -62.6], 'ground'], ['g15', [35.1, 0, -62.5], 'ground'],
      ['g16', [40.0, 0, -52.4], 'ground'], ['g17', [59.4, 0, -48.8], 'ground'], ['g18', [61.5, 0, -43.8], 'ground'],
      ['g19', [65.0, 0, -49.0], 'ground'], ['g20', [-137.1, 0, 16.2], 'ground'], ['g21', [-119.5, 0, 22.1], 'ground'],
      ['g22', [-111.0, 0, 14.4], 'ground'], ['g23', [-109.7, 0, 29.5], 'ground'], ['g24', [-99.3, 0, 16.1], 'ground'],
      ['g25', [-93.8, 0, 34.1], 'ground'], ['g26', [-88.5, 0, 35.9], 'ground'], ['g27', [-78.0, 0, 39.4], 'ground'],
      ['g28', [-64.9, 0, 53.2], 'ground'], ['g29', [-64.8, 0, 58.0], 'ground'], ['g30', [-58.7, 0, 56.8], 'ground'],
      ['g31', [-52.8, 0, 48.7], 'ground'], ['g32', [-46.3, 0, 51.1], 'ground'], ['g33', [48.7, 0, 24.8], 'ground'],
      ['g34', [57.0, 0, 5.8], 'ground'], ['g35', [61.1, 0, 11.0], 'ground'], ['g36', [47.1, 0, 3.1], 'ground'],
      ['g37', [48.1, 0, 19.8], 'ground'], ['g38', [12.6, 0, 15.2], 'ground'], ['g39', [18.4, 0, 15.6], 'ground'],
      /* Two pads that close the only §4.5 gaps on the map, both on the
         approach out of the spawn gate where the lane is still in open field
         and the buildings that carry the other pads have not started yet.
         g70 sits 7.1 m south of the long/west lane's second leg; g71 stands
         under the air lane's turn toward the Vehickle house, which the roof
         pads reach along but not across. */
      ['g70', [63.5, 0, -14.0], 'ground'], ['g71', [58.0, 0, 12.0], 'ground'],
      ['g40', [30.0, 0, 16.4], 'ground'], ['g41', [35.8, 0, 16.8], 'ground'], ['g42', [-3.7, 0, 5.8], 'ground'],
      ['g43', [-0.9, 0, -6.5], 'ground'], ['g44', [4.3, 0, -4.8], 'ground'], ['g45', [4.8, 0, 12.3], 'ground'],
      ['g46', [11.4, 0, 1.1], 'ground'], ['g47', [-24.6, 0, -4.5], 'ground'], ['g48', [-20.1, 0, -2.7], 'ground'],
      ['g49', [-15.6, 0, -1.0], 'ground'], ['g50', [-15.4, 0, -20.8], 'ground'], ['g51', [-10.6, 0, -16.8], 'ground'],
      ['g52', [-42.5, 0, -15.0], 'ground'], ['g53', [-37.5, 0, -15.0], 'ground'], ['g54', [-29.9, 0, -14.3], 'ground'],
      ['g55', [-51.0, 0, -5.0], 'ground'], ['g56', [-51.0, 0, -15.0], 'ground'], ['g57', [60.6, 0, 27.0], 'ground'],
      ['g58', [66.3, 0, 30.2], 'ground'], ['g59', [-119.6, 0, 5.2], 'ground'], ['g60', [-94.4, 0, 3.6], 'ground'],
      ['g61', [-85.0, 0, -0.4], 'ground'], ['g62', [-80.4, 0, -2.4], 'ground'], ['g63', [-70.9, 0, -3.8], 'ground'],
      ['g64', [-65.2, 0, -4.3], 'ground'], ['g65', [31.0, 0, 6.0], 'ground'],
      // The only height on the map: one pad per roof, which is why each
      // building's ladder is on the wall its roof socket is nearest.
      ['w_barn', [-95.0, 6.4, 45.5], 'wall'], ['w_buggy', [-141.5, 3.8, 5.0], 'wall'],
      ['w_vehickle', [45.0, 3.8, 20.0], 'wall'], ['w_grnmchn', [80.0, 3.8, -37.0], 'wall'],
      ['t1', [71.1, 0, 16.0], 'trap'], ['t2', [53.0, 0, -9.4], 'trap'], ['t3', [21.0, 0, -12.6], 'trap'],
      ['t4', [-4.0, 0, -31.0], 'trap'], ['t5', [12.8, 0, -57.8], 'trap'], ['t6', [45.0, 0, -57.2], 'trap'],
      ['t7', [72.7, 0, -44.0], 'trap'], ['t8', [-126.8, 0, 14.4], 'trap'], ['t9', [-100.4, 0, 34.4], 'trap'],
      ['t10', [-69.0, 0, 47.0], 'trap'], ['t11', [-29.0, 0, -10.0], 'trap'], ['t12', [-100.0, 0, 0.6], 'trap'],
      ['t13', [-70.8, 0, -8.8], 'trap'],
    ],
    spawn: [72, 0, 24], core: [-57, 0, -10],
    heroSpawn: [-60, 0, -22], armory: [-66, 0, -22],
    stations: [
      ['coreYard', [-60, 0, -18]], ['coreGate', [-45, 0, -4]], ['driveBend', [12, 0, -4]],
      ['gateWatch', [66, 0, 10]], ['housePad', [56, 0, 32]], ['pondWest', [-2, 0, -28]],
      ['southRoad', [30, 0, -52]], ['southPad', [78, 0, -38]], ['westPad', [-128, 0, 4]],
      ['westRun', [-92, 0, -4]], ['barnDrive', [-84, 0, 40]], ['northRoad', [-50, 0, 56]],
    ],
    /* Four roofs, and nothing else has a top. Each volume's top is its roof
       socket's own height rather than the roof surface: the pad check wants
       the socket within 0.25 m of the volume top, and upstream authors the
       pads 100 mm over the surface they stand on. */
    volumes: [
      { id: 'barn', at: [-108, 6.2, 54], size: [30, .4, 21], top: 6.4, solid: true },
      { id: 'buggyHouse', at: [-134, 3.6, -7], size: [19, .4, 28], top: 3.8, solid: true },
      { id: 'vehickleHouse', at: [30, 3.6, 31], size: [34, .4, 26], top: 3.8, solid: true },
      { id: 'grnmchnHouse', at: [90, 3.6, -50], size: [24, .4, 30], top: 3.8, solid: true },
    ],
    /* One ladder per building, on the wall its roof socket is nearest. A climb
       that tops out a building's width from the pad it serves leaves the
       player standing on rungs with nothing to step onto, which is the mistake
       this project has now made on three maps and the traversal probe
       measures. */
    areas: [
      { kind: 'ladder', at: [-93.2, 3.15, 45.5], size: [.8, 6.3, 1.2], tops: 6.3 },
      { kind: 'ladder', at: [-143.7, 1.85, 5], size: [.8, 3.7, 1.2], tops: 3.7 },
      { kind: 'ladder', at: [47.2, 1.85, 20], size: [.8, 3.7, 1.2], tops: 3.7 },
      { kind: 'ladder', at: [77.8, 1.85, -37], size: [.8, 3.7, 1.2], tops: 3.7 },
      { kind: 'armory', at: [-66, 0, -22], size: [3, 3, 3] },
    ],
    place: [
      /* ── The four buildings ─────────────────────────────────────────────
         Shell and roof are separate files at the same centre: the roof is a
         surface players stand on and the code puts a collider under it. */
      ['toaster_barn_shell', [-108, 0, 54], 0], ['toaster_barn_roof', [-108, 0, 54], 0],
      ['toaster_house_buggy_shell', [-134, 0, -7], 0], ['toaster_house_buggy_roof', [-134, 0, -7], 0],
      ['toaster_house_vehickle_shell', [30, 0, 31], 0], ['toaster_house_vehickle_roof', [30, 0, 31], 0],
      ['toaster_house_grnmchn_shell', [90, 0, -50], 0], ['toaster_house_grnmchn_roof', [90, 0, -50], 0],
      // Ladders: `shared_ladder` reused, four times. It is 14,056 tris, which
      // is fine at four and would not be at forty.
      ['shared_ladder', [-93.2, 0, 45.5], -Math.PI / 2, [1, 6.3 / 5, 1]],
      ['shared_ladder', [-143.7, 0, 5], Math.PI / 2, [1, 3.7 / 5, 1]],
      ['shared_ladder', [47.2, 0, 20], -Math.PI / 2, [1, 3.7 / 5, 1]],
      ['shared_ladder', [77.8, 0, -37], Math.PI / 2, [1, 3.7 / 5, 1]],

      /* ── Interiors ──────────────────────────────────────────────────────
         Placed here rather than baked into the shells, so each room arranges
         around the teleport pad and the roof socket rather than around
         nothing. The pads are `shared_teleporter_pad_idle`, reused. */
      ['shared_teleporter_pad_idle', [-52, 0, -14], 0],
      ['shared_teleporter_pad_idle', [-134, .15, -12], 0],
      ['shared_teleporter_pad_idle', [23, .15, 31], 0],
      ['shared_teleporter_pad_idle', [86, .15, -48], 0],
      ['toaster_dress_workbench', [-116, .15, 62], Math.PI], ['toaster_dress_shelving', [-100, .15, 62.8], Math.PI],
      ['toaster_dress_workbench', [16, .15, 31], Math.PI / 2],
      ['toaster_dress_furniture_living', [-132, .15, 2], 0], ['toaster_dress_furniture_kitchen', [-134, .15, -18.6], 0],
      ['toaster_dress_furniture_living', [34, .15, 38], Math.PI], ['toaster_dress_furniture_kitchen', [38, .15, 42.6], Math.PI],
      ['toaster_dress_furniture_living', [93, .15, -44], 0], ['toaster_dress_furniture_kitchen', [90, .15, -63.6], 0],

      /* ── Pond ───────────────────────────────────────────────────────────
         Thirty-four metres across rather than the forty-odd it is on the
         ground: three routes pass within twenty-two metres of its centre. */
      ['toaster_pond', [19, 0, -36], 0], ['toaster_dock', [8, 0, -30], -1.1],

      /* ── Warp gates ─────────────────────────────────────────────────────
         Two pairs, at the two teleport legs the routes actually declare:
         (72, −40) ⇒ (−137, 11) and (−44, 60) ⇒ (52, 29). Yawed along the
         route so the opening faces the way the wave is going. */
      ['shared_warp_gate_active', [72, 0, -40], 0.35],
      ['shared_warp_gate_active', [-137, 0, 11], 0.30],
      ['shared_warp_gate_active', [-44, 0, 60], 1.60],
      ['shared_warp_gate_active', [52, 0, 29], -1.05],

      /* ── Vehicles ───────────────────────────────────────────────────────
         Parked outside the building each is named for, clear of every lane,
         and pointed at OPEN GROUND: a vehicle nosed at the wall it is parked
         against is a vehicle whose first press of W is a crash. Yaws are the
         sim's, converted to radians. */
      ['vehicle_buggy', [-120, 0, -14], -Math.PI / 2],
      ['vehicle_dagator', [-110, 0, 40], 0],
      ['vehicle_grnmchn', [76, 0, -30], Math.PI],
      ['vehicle_vehickle', [58, 0, 38], -Math.PI / 2],

      /* ── Dressing ───────────────────────────────────────────────────────
         Nothing here sits within 3 m of a route, a socket, a pad, the spawn,
         the armory or a hero station — the code refuses those placements and
         logs them, so a bad coordinate is a line in the output rather than a
         girder in the roadway. */
      ['toaster_propane_tank', [-122, 0, 66], 0], ['toaster_propane_tank', [-126, 0, -20], 1.57], ['toaster_propane_tank', [100, 0, -58], 1.57],
      ['toaster_woodpile', [-118, 0, 44], .2], ['toaster_woodpile', [98, 0, -38], -1.3],
      ['toaster_wreck_pickup', [-116, 0, 70], .45],
      // Everything else — mailboxes, the fence runs, the bale field, the belt
      // understory and the leaf drift — is generated and filtered below,
      // against the same clearance rule the client refuses placements on.
    ],
  },
};

/**
 * Is (x, z) in a made road?
 *
 * Roads are a SEPARATE network from the enemy lanes, and everything that
 * places scenery only ever knew about the lanes. That was survivable while
 * the roads were four short stubs well inside the field; the moment they
 * were extended to the boundaries it put 36 tree trunks in the carriageway
 * — the county road runs out to z 80 through the north belt, the south road
 * out to x ±160 through both side belts, and at z −66 it lies inside the
 * 12 m belt band along its whole length.
 *
 * The roads are authored data; anything scattering over the field can read
 * them. Exported rather than written twice, because a belt and a dressing
 * pass that disagree about where the road is are worse than neither.
 */
export function roadKeepOut(L, margin = 2) {
  const segs = [];
  for (const r of L.vroads || []) {
    const hw = (r.width ?? 6) / 2 + margin;
    for (let i = 0; i < r.pts.length - 1; i++) segs.push([r.pts[i], r.pts[i + 1], hw * hw]);
  }
  // The circular drive is an annulus, not a disc: the middle of it is grass.
  const d = L.drive, RIN = 12 - 3 - margin, ROUT = 12 + 3 + margin;
  return (x, z) => {
    for (const [a, b, r2] of segs) {
      const abx = b[0] - a[0], abz = b[2] - a[2], l2 = abx * abx + abz * abz;
      const t = l2 ? Math.max(0, Math.min(1, ((x - a[0]) * abx + (z - a[2]) * abz) / l2)) : 0;
      const dx = x - (a[0] + abx * t), dz = z - (a[2] + abz * t);
      if (dx * dx + dz * dz < r2) return true;
    }
    if (d) { const rr = Math.hypot(x - d[0], z - d[2]); if (rr > RIN && rr < ROUT) return true; }
    return false;
  };
}
/* ══ generated dressing ═══════════════════════════════════════════════════
 *
 * The Toaster's fence runs, bale field, understory and leaf drift are rows
 * rather than placements — six times the area of any map before it, and a
 * hand-listed row is a row that is wrong the next time a route moves. So they
 * are generated here and passed through the SAME rule the client enforces
 * when it refuses a placement and logs it: nothing within 3 m of a walked
 * lane, a socket, the spawn, the core, the armory or a hero station.
 *
 * Measured from the piece's own FOOTPRINT, not its origin: a 4 m fence module
 * placed 3.1 m from a lane still has two metres of itself in the lane. Each
 * id carries its plan radius, and the test is 3 m plus that.
 *
 * The first pass hand-placed these and put eleven of them inside the margin —
 * a fence across the barn drive's lane, two mailboxes in it, four bales on
 * the drive. Generating them and filtering is not a convenience; it is the
 * only version of this that stays true.
 */
{
  const T = LEVELS.toaster;
  /* Plan radius per id, and these are the BUILT footprints rather than the
     nominal sizes: a "4 m clump" whose parts are a rock one side and a rotted
     post the other reaches 2.4 m from its origin. Using the nominal 2.0 put
     one scatter clump 2 m from a build pad — inside the margin by 200 mm,
     which is exactly how these get missed. */
  const RADIUS = {
    toaster_mailbox: .4, toaster_fence_wood: 2.1, toaster_hay_bale: .8,
    toaster_understory: 2.4, toaster_leaf_pile: 1.5, toaster_terrain_scatter: 2.4,
  };
  // Walked lanes only — a teleport leg is a line nothing ever stands on.
  const walked = [];
  for (const [name, pts] of Object.entries(T.routes)) {
    if (name === 'air') continue;
    const tl = new Set((T.teleportLegs || {})[name] || []);
    for (let i = 0; i < pts.length - 1; i++) if (!tl.has(i)) walked.push([pts[i], pts[i + 1]]);
  }
  const keepOut = [...T.sockets.map(([, p]) => p), T.spawn, T.core, T.heroSpawn, T.armory, ...T.stations.map(([, p]) => p)];
  const segDist = (x, z, [a, b]) => {
    const abx = b[0] - a[0], abz = b[2] - a[2], l2 = abx * abx + abz * abz;
    const t = l2 ? Math.max(0, Math.min(1, ((x - a[0]) * abx + (z - a[2]) * abz) / l2)) : 0;
    return Math.hypot(x - (a[0] + abx * t), z - (a[2] + abz * t));
  };
  /* Roadside furniture may stand at the kerb; scenery may not stand ON the
     road. A fence line and a mailbox are the point of a verge, so they get
     the road test at the carriageway edge; understory, leaf drift, bales and
     ground scatter get the same 2 m margin as the trees, because a shrub on
     asphalt is the same mistake as an oak in it. */
  const onRoad = roadKeepOut(T, 2);
  const atKerb = roadKeepOut(T, 0);
  const VERGE = new Set(['toaster_mailbox', 'toaster_fence_wood']);
  const add = (id, x, z, ry) => {
    const m = 3 + (RADIUS[id] || 1) + .2;
    for (const s of walked) if (segDist(x, z, s) < m) return;
    for (const p of keepOut) if (Math.hypot(p[0] - x, p[2] - z) < m) return;
    if ((VERGE.has(id) ? atKerb : onRoad)(x, z)) return;
    T.place.push([id, [x, 0, z], ry]);
  };

  // Mailboxes at the three road heads.
  add('toaster_mailbox', -52.5, 44, 1.57);
  add('toaster_mailbox', -86, 42.4, 0.14);
  add('toaster_mailbox', 33, 9.6, -0.34);
  // Post-and-rail: both sides of the barn drive, and the east field's frontage.
  for (let i = 0; i < 8; i++) add('toaster_fence_wood', -88 + i * 4, 41.5, 0.14);
  for (let i = 0; i < 9; i++) add('toaster_fence_wood', -88 + i * 4, 53, 0.14);
  for (let i = 0; i < 10; i++) add('toaster_fence_wood', 44 + i * 4, -22, 0);
  // Round bales, scattered through the east field on a spiral so they do not
  // read as a grid.
  for (let i = 0; i < 24; i++) {
    const r = i * 2.399;
    add('toaster_hay_bale', 96 + Math.cos(r) * (8 + (i % 5) * 7), -6 + Math.sin(r) * (7 + (i % 4) * 6), r);
  }
  // Understory and leaf drift along the inner edge of the treeline.
  for (let i = 0; i < 18; i++) {
    const s = i % 2 ? 1 : -1, t = (i * 0.137) % 1;
    add('toaster_understory', -148 + t * 296, s * (66 - (i % 3) * 3), i * .7);
  }
  for (let i = 0; i < 12; i++) {
    const s = i % 2 ? 1 : -1, t = ((i * 0.211) + .07) % 1;
    add('toaster_leaf_pile', -146 + t * 292, s * (64 - (i % 4) * 4), i * 1.1);
  }
  // Ground scatter: the detail the terrain tile is forbidden from carrying.
  for (const [x, z, ry] of [[-70, 24, .4], [-30, 34, 1.9], [22, -6, 2.7], [-16, -48, .8],
    [62, -24, 2.1], [-96, -26, 1.2], [112, 30, .6], [-40, 30, 2.4]]) {
    add('toaster_terrain_scatter', x, z, ry);
  }
}
