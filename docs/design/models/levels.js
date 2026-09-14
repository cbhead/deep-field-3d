/**
 * Deep Field 3D — level layouts.
 *
 * Schema follows docs/MAP-AUTHORING.md (2026-09-09): routes, sockets, anchors,
 * volumes, areas, place, conditions. Foundry and Switchyard are regenerated
 * from the current sim/Sim.Core/Content/Maps.cs (44 sockets each — the old
 * mirror carried 12 and 16) and the graybox builders in GameRoot.cs. Spire is
 * regenerated from the M5 redesign upstream — Maps.Spire plus GameRoot's Spire
 * section — which replaced every route, every socket and the whole building:
 * plates with the atrium and a stair well punched out of them, four flights,
 * two escape landings, a lift that runs, six staggered atrium ladders.
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
/**
 * Rectangle subtraction, as Punch() in GameRoot.cs — a floor is a rectangle
 * with holes in it, and this is the primitive the Spire's plates are cut with.
 * Slabs are [a0, b0, a1, b1] in the level's own X/Z; up to four pieces come
 * back per rect, and slivers are dropped rather than emitted as plate too thin
 * to stand on. Kept identical to the client's version on purpose: the plates
 * design walks in the viewer and the colliders the game builds are then the
 * same set of rectangles rather than two hand-matched lists.
 */
function punch(rects, holes) {
  let kept = rects.slice();
  for (const h of [].concat(holes)) {
    const out = [];
    for (const r of kept) {
      if (h[2] <= r[0] || h[0] >= r[2] || h[3] <= r[1] || h[1] >= r[3]) { out.push(r); continue; }
      if (r[1] < h[1]) out.push([r[0], r[1], r[2], h[1]]);
      if (r[3] > h[3]) out.push([r[0], h[3], r[2], r[3]]);
      const b0 = Math.max(r[1], h[1]), b1 = Math.min(r[3], h[3]);
      if (r[0] < h[0]) out.push([r[0], b0, h[0], b1]);
      if (r[2] > h[2]) out.push([h[2], b0, r[2], b1]);
    }
    kept = out.filter((s) => s[2] - s[0] >= .05 && s[3] - s[1] >= .05);
  }
  return kept;
}
/** A punched plate as a volume: walking surface at `top`, with its module. */
const plateVol = (id, s, top, surface, piece) => ({
  id, at: [(s[0] + s[2]) / 2, top - .2, (s[1] + s[3]) / 2],
  size: [s[2] - s[0], .4, s[3] - s[1]], top, solid: true, surface,
  run: (s[2] - s[0]) >= (s[3] - s[1]) ? 'x' : 'z', piece,
});
/**
 * A punched plate as placements: the module tiled across it the way SpireDeck
 * does. The bay is 4 m along its run and 12 m across, and a plate cut round a
 * well is not 12 m across anywhere in particular, so the across axis is scaled
 * to the piece and the 4 m repeat is left alone — the difference between
 * tiling a shape and stretching one bay over it.
 */
function plateBays(id, s, y, across = 12) {
  const cx = (s[0] + s[2]) / 2, cz = (s[1] + s[3]) / 2, sx = s[2] - s[0], sz = s[3] - s[1];
  const alongX = sx >= sz, run = alongX ? sx : sz, wide = alongX ? sz : sx;
  const n = Math.max(1, Math.round(run / 4)), step = run / n, out = [];
  for (let i = 0; i < n; i++) {
    const o = -run / 2 + step * (i + .5);
    out.push([id, alongX ? [cx + o, y, cz] : [cx, y, cz + o], alongX ? 0 : Math.PI / 2,
      [step / 4, 1, wide / across]]);
  }
  return out;
}

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
      /* The ride line hangs 0.8 m under the cable at BOTH ends — that is the
         trolley and the T-bar. The cable itself runs sheave to sheave: 7.60 m
         on the deck post down to 1.60 m on the grade post, 6 m of drop over
         27.6 m of run. `to` used to be written at 1.60, which is the CABLE's
         bottom end, not the rider's, and left the line ending above the wire
         it hangs from. */
      { kind: 'zipline', at: [14, 6.8, -17.5], size: [2, 2, 2], to: [34.36, 0.8, 2.06] },
      { kind: 'launcher', at: [-14, 0, -20], size: [2.4, .4, 2.4], velocity: [0, 12, 8] },
      { kind: 'armory', at: [-6, 0, -24], size: [3, 3, 3] },
    ],
    place: [
      // Upper deck: 28×11 at (2, 5.8, −16.5) → 7 bays of 4 m, one metre deeper
      // than the graybox so the front-lip row (w4–w7, z −12.8) sits a clear
      // metre inside the footprint — MAP-AUTHORING §4.9.
      ...[-10, -6, -2, 2, 6, 10, 14].map((x) => ['foundry_deck', [x, 0, -16.5], 0, [1, 1, 1.1]]),
      /* Guard rail around the WHOLE deck, 0.2 m inside each edge (the deck is
         x −12…16, z −22…−11). Every run is placed with its toe board facing
         the drop, which fixes the rotation per edge: front ry π, back 0, left
         +π/2, right −π/2. Sides are 4 m + 4 m + a 2.6 m closer (#1) so they
         end ON the front and back lines instead of overhanging them.
         Three openings, all deliberate: the ladder head at x −8, flanked by the
         mirrored gap runs (#3/#2) that leave a 1.4 m gap with a grab stanchion
         each side; the same pair again at x 6/10, because the gantry bridge
         lands on this edge (its walkway spans x 7.05…11.0 at z −11) and a
         completed perimeter would otherwise wall the deck off from it; and the
         right-edge bay z −13.8…−17.8, which is where the zipline leaves — the
         cable crosses that line at z −15.8 and y 7.11, one centimetre over a
         rail top, so that bay stays open. */
      ...[-10, -6, -2, 2, 6, 10, 14].map((x) => ['foundry_deck_rail', [x, 6.0, -21.8], 0]),
      ['foundry_deck_rail#3', [-10, 6.0, -11.2], Math.PI], ['foundry_deck_rail#2', [-6, 6.0, -11.2], Math.PI],
      ...[-2, 2, 14].map((x) => ['foundry_deck_rail', [x, 6.0, -11.2], Math.PI]),
      ['foundry_deck_rail#3', [6, 6.0, -11.2], Math.PI], ['foundry_deck_rail#2', [10, 6.0, -11.2], Math.PI],
      ...[-19.8, -15.8].map((z) => ['foundry_deck_rail', [-11.8, 6.0, z], Math.PI / 2]),
      ['foundry_deck_rail#1', [-11.8, 6.0, -12.5], Math.PI / 2],
      ['foundry_deck_rail', [15.8, 6.0, -19.8], -Math.PI / 2],
      ['foundry_deck_rail#1', [15.8, 6.0, -12.5], -Math.PI / 2],
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
      /* Both anchors are the same full-size post, and each is yawed to FACE
         the other: the sheave sits 0.2 m out along the anchor's local +Z, so
         an anchor pointing anywhere else hangs its cable off the side of its
         own wheel. Top post stands on the deck (surface y 6.0, sheave 7.60),
         bottom post on grade (sheave 1.60). The bottom one used to be scaled
         to 0.6 — a 1.1 m post whose sheave sat at 0.96, below the height the
         cable was drawn to. At full size its base plate is 0.9 m across, so
         the landing post moved clear of both the lane and the g19 pad (to
         34.5, 2.2) to keep the §4.8 three-metre clearance it used to get by
         being small. */
      ['shared_zipline_anchor', [14, 6.0, -17.5], 0.805],
      ['shared_zipline_anchor', [34.5, 0, 2.2], -2.336],
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
    /* No `zipline:` endpoint pair here: the cable is strung from the two
       anchors' own sheave nodes (`userData.zip`), so there is exactly one
       description of where it is attached. */
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
   * Spire — sector 3, REGENERATED from the M5 redesign upstream rather than
   * from the version authored here. A 40 × 40 m block on a plaza with floors
   * at 10 / 20 / 30 and the core on the roof at 40; everything else about it
   * changed. What this mirrors, piece by piece, is Maps.Spire (routes,
   * sockets, anchors, stations) and GameRoot's Spire section (the building).
   *
   * The old design here was a 12 m gallery ring around a 16 × 16 atrium with
   * a lap of every floor before each climb, 67 sockets and a four-flight fire
   * escape. None of it survives. Upstream's building is a full 38 × 40 plate
   * per level with the atrium and ONE stair well cut out of it, four interior
   * flights that spiral the void, a two-flight escape that rejoins the stair
   * at floor two, and 68 sockets placed on measured floor.
   *
   * Three things worth carrying into any refinement, because each is a fix
   * for a defect rather than a preference:
   *
   *   - The plates have holes. The atrium is void plaza-to-roof (it is also
   *     the players' way down, free and instant), each level has a well where
   *     the flight below comes within 3 m of the plate, and the roof has one
   *     at the stair head plus the lift's. Without them the flights rose into
   *     the underside of the floor they serve.
   *   - The doorways are cut out of the collision, not only out of the art:
   *     both ground routes used to walk through solid curtain wall, and the
   *     escape's ground leg moved from z 18 to z 16 because a 3.4 m lane a
   *     metre and a half off the south facade was eleven §4.8 violations.
   *   - Circulation is the map's idea: the lift is an express lobby-to-roof,
   *     six ladders staggered round the atrium are the local stops it does
   *     not make, the atrium is the ride down and the fare is the climb back.
   *     The defence is mobile and the attack is not.
   *
   * Design's own, unchanged from our side: the perimeter street frontage, the
   * painted interior lane on every flat leg above grade, the plaza dressing.
   */
  spire: {
    label: 'Spire', kit: 'spire', field: [110, 80], totalWaves: 12,
    conditions: { 7: 'night', 11: 'fog' },
    view: { cam: [72, 66, 88], target: [0, 10, 0] },
    routes: {
      // Interior stair: in at the main door, then four flights round the
      // atrium, arriving on the roof through the stair head. Every leg that
      // climbs is a flight — 10, 10, 18 and 14 m of run for 10 m of rise,
      // which is what `spire_stairwell` was authored to stretch to.
      stair: [
        [-34, 0, 0], [-14, 0, 0], [-14, 0, -14],
        [-4, 10, -14], [14, 10, -14],
        [14, 20, -4], [14, 20, 14],
        [-4, 30, 14], [-14, 30, 14],
        [-14, 40, 0], [0, 40, 0],
      ],
      // Fire escape: in at the service door at z 16, out through the BACK WALL
      // at grade, then two flights on the east face to floor two, where it
      // walks back in through that wall's own doorway and joins the stair. The
      // climbing legs are at x 21.8 — the escape hangs off the outside of the
      // wall, so its treads are outboard of the wall plane at x 20.
      escape: [
        [-34, 0, 16], [-18, 0, 16], [21.8, 0, 16],
        [21.8, 10, 8], [21.8, 20, 0], [19, 20, 0],
        [14, 20, 14], [-4, 30, 14], [-14, 30, 14],
        [-14, 40, 0], [0, 40, 0],
      ],
      // Flyers climb the outside and come over the parapet. Every leg stays
      // clear of the building's own volume — the old spiral cut through floor
      // three and again through the top storey, which reads in a match as
      // flyers inside the walls.
      air: [[-40, 6, 0], [-30, 12, -16], [-23, 20, -24], [6, 27, -26], [26, 32, -22], [26, 41, 10], [0, 43, 0]],
    },
    // Only the legs outside the shell get asphalt; inside the footprint the
    // same route reads as a taped corridor at every level, grade included.
    roads: ['stair', 'escape'],
    indoor: { x: [-20, 20], z: [-20, 20] },
    sockets: [
      // Plaza pads. The two western clusters answer the air lane's approach as
      // well as the ground lanes' — a flyer at 12 m over the forecourt is the
      // one stretch of that strand a tower on the floor can reach.
      ['g1', [-42, 0, -16], 'ground'], ['g2', [-42, 0, -8], 'ground'], ['g3', [-38, 0, -12], 'ground'],
      ['g4', [-28, 0, -16], 'ground'], ['g5', [-24, 0, -12], 'ground'], ['g6', [-24, 0, -6], 'ground'],
      ['g7', [-24, 0, 10], 'ground'], ['g8', [-20, 0, 20], 'ground'], ['g9', [-16, 0, 6], 'ground'],
      ['g10', [-16, 0, 12], 'ground'], ['g11', [-14, 0, 20], 'ground'], ['g12', [-10, 0, 6], 'ground'],
      ['g13', [2, 0, -14], 'ground'], ['g14', [8, 0, 8], 'ground'], ['g15', [12, 0, 4], 'ground'],
      // Deck pads, numbered UP the building: w1 is the lowest and w39 the
      // highest, because on this map the floor a pad is on is the first thing
      // you need to know about it. Every one stands on a plate, a fire-escape
      // landing or the roof, a clear metre from the edge.
      ['w1', [-18, 10, -8], 'wall'], ['w2', [-16, 10, -16], 'wall'], ['w3', [8, 10, -2], 'wall'],
      ['w4', [8, 10, 18], 'wall'], ['w5', [10, 10, 6], 'wall'], ['w6', [10, 10, 12], 'wall'],
      ['w7', [12, 10, -8], 'wall'], ['w8', [14, 10, 16], 'wall'], ['w9', [18, 10, 4], 'wall'],
      ['w10', [-18, 20, -14], 'wall'], ['w11', [-14, 20, -18], 'wall'], ['w12', [-8, 20, -18], 'wall'],
      ['w13', [-2, 20, -18], 'wall'], ['w14', [4, 20, -18], 'wall'], ['w15', [10, 20, -18], 'wall'],
      ['w16', [18, 20, -18], 'wall'],
      ['w17', [-16, 30, -16], 'wall'], ['w18', [-16, 30, 0], 'wall'], ['w19', [-12, 30, 12], 'wall'],
      ['w20', [-10, 30, 6], 'wall'], ['w21', [-6, 30, 12], 'wall'], ['w22', [-4, 30, -16], 'wall'],
      ['w23', [8, 30, -18], 'wall'], ['w24', [8, 30, 6], 'wall'], ['w25', [14, 30, -16], 'wall'],
      ['w26', [16, 30, 2], 'wall'], ['w27', [16, 30, 12], 'wall'], ['w28', [18, 30, -12], 'wall'],
      ['w29', [18, 30, -6], 'wall'],
      ['w30', [-8, 40, 8], 'wall'], ['w31', [-4, 40, 12], 'wall'], ['w32', [-2, 40, 4], 'wall'],
      ['w33', [12, 40, -8], 'wall'], ['w34', [12, 40, 6], 'wall'], ['w35', [12, 40, 12], 'wall'],
      ['w36', [14, 40, -14], 'wall'], ['w37', [16, 40, 2], 'wall'], ['w38', [18, 40, -4], 'wall'],
      ['w39', [18, 40, 8], 'wall'],
      // Trap plates on the flat legs only, where a plate is a plate and not a
      // step: they are contact triggers, so the flights get none.
      ['t1', [-27.3, 0, 0], 'trap'], ['t2', [-20.7, 0, 0], 'trap'], ['t3', [-14, 0, -7], 'trap'],
      ['t4', [2, 10, -14], 'trap'], ['t5', [8, 10, -14], 'trap'], ['t6', [14, 20, 2], 'trap'],
      ['t7', [14, 20, 8], 'trap'], ['t8', [-7, 40, 0], 'trap'], ['t9', [-26, 0, 16], 'trap'],
      ['t10', [-11.7, 0, 16], 'trap'], ['t11', [-5.3, 0, 16], 'trap'], ['t12', [1, 0, 16], 'trap'],
      ['t13', [7.3, 0, 16], 'trap'], ['t14', [13.7, 0, 16], 'trap'],
    ],
    heroSpawn: [-30, 0, 8], armory: [-26, 0, 10],
    // One per level, each on floor that exists. The roof station is where the
    // match ends; the lobby one is where you come back to down the atrium.
    stations: [
      ['lobby', [-16, 0, 6]], ['floorOne', [-14, 10, 6]], ['floorTwo', [14, 20, 8]],
      ['floorThree', [-14, 30, -6]], ['roof', [0, 40, -8]],
    ],
    volumes: [
      /* The three office floors: a 38 × 40 plate each, with the atrium and one
         stair well cut out. The well is the stretch where the flight below
         comes within three metres of the plate — measured off the lane — and
         the plate keeps every other metre, because every metre it keeps is
         somewhere a pad can stand. */
      ...[[10, [-11, -18, -4, -10]], [20, [11, -13, 19, -4]], [30, [-4, 10, 5, 18]]].flatMap(
        ([y, well]) => punch([[-19, -20, 19, 20]], [[-7, -10, 7, 10], well])
          .map((s, i) => plateVol('f' + y + '_' + (i + 1), s, y, 'spire_floor', 4))),
      /* The roof: the core's ground, and the only floor here that is a
         destination rather than a landing. Two wells — the stair head, which
         stops short of z 0 because the roof lap walks that line and an opening
         under it is a hole in the lane, and the lift's. */
      ...punch([[-20, -20, 20, 20]], [[-18, .5, -10, 7], [-7, -10, -3, -6]])
        .map((s, i) => plateVol('roof_' + (i + 1), s, 40, 'spire_roof', 8)),
      /* Fire escape: two landings, not the three that were here before — the
         third stood at y 30 on a route that re-enters the building at y 20, so
         it served nothing and never had. 2.4 m deep and outboard of the back
         wall (outer face 20.525): upstream's 6 m slab (x 17–23) put half of
         every landing inside the office floor. */
      ...[[10, 4, 12], [20, -4, 4]].map(([y, z0, z1]) =>
        plateVol('escape_l' + y, [20.6, z0, 23, z1], y, 'spire_fireescape', 3)),
      /* The flights, as the walkable ramps they are: four inside spiralling
         the atrium, two outside on the east face. */
      { id: 'flight_1', at: [-9, 5, -14], size: [10, .5, 3.4], solid: true },
      { id: 'flight_2', at: [14, 15, -9], size: [3.4, .5, 10], solid: true },
      { id: 'flight_3', at: [5, 25, 14], size: [18, .5, 3.4], solid: true },
      { id: 'flight_4', at: [-14, 35, 7], size: [3.4, .5, 14], solid: true },
      { id: 'escape_f1', at: [21.8, 5, 12], size: [2.4, .5, 8], solid: true },
      { id: 'escape_f2', at: [21.8, 15, 4], size: [2.4, .5, 8], solid: true },
      // The lift's shaft, in the atrium's north-west corner: the atrium is
      // void plaza-to-roof, so the car pierces no plate on its way up and the
      // only opening it needs is the one in the roof.
      { id: 'liftShaft', at: [-5, 20, -8], size: [4, 40, 4], solid: true },
    ],
    areas: [
      // The lift, running the building's full height: an express, lobby to
      // roof, and slow enough that boarding it is a decision.
      { kind: 'elevator', at: [-5, 20, -8], size: [4, 40, 4], stops: [0, 40] },
      /* The local stops the express does not make, on opposite corners of the
         atrium and STAGGERED by storey rather than stacked: two grip volumes
         at the same spot on consecutive storeys overlap and chain, so a hold
         at the plaza carried you past floor one to wherever you let go. Six
         metres of offset on the middle pair turns the climb into a spiral
         round the void — which is what the enemies' stair is doing on the
         other side of it. tops is the plate you step onto; the rungs run
         1.5 m past it, in the void, so there is time to step across. */
      ...[[6, -8, -2], [-6, 8, 2]].flatMap(([x, near, far]) => [0, 1, 2].map((f) => (
        { kind: 'ladder', at: [x, f * 10 + 5, f === 1 ? far : near], size: [1.6, 12, 1.6], tops: (f + 1) * 10 }
      ))),
      // The rotation the lift is too slow for. The roof end doubles as the
      // traversal exit that keeps the roof's south-east corner in reach.
      { kind: 'teleporter', at: [-14, .2, 6], size: [2, 2.4, 2], padId: 'padGround', label: 'LOBBY' },
      { kind: 'teleporter', at: [6, 40.2, 6], size: [2, 2.4, 2], padId: 'padRoof', label: 'ROOF' },
      // One way down that is not the atrium, out over the OPEN east face — the
      // three walled faces would put the rider through a curtain wall.
      { kind: 'zipline', at: [14, 40.7, -2], size: [2.4, 2.6, 2.4], to: [32, 1.2, 0] },
      { kind: 'armory', at: [-26, 0, 10], size: [3, 3, 3] },
    ],
    place: [
      /* ── Shell: five 8 m facade bays per face, ground to parapet, on ALL FOUR
         faces — the east side is a real back wall now, not an open flank. Its
         bays carry the escape's three doorways (variant N notches storey N−1):
         grade at z 16 where the ground leg leaves the building, floor one at
         z 8 where the landing gate lands, floor two at z 0 where the route
         walks back in. Both lobby bays are on the west face, where the kit puts
         them: the main door at z 0 and the service door at z 16. ── */
      ...[-16, -8, 8].map((z) => ['spire_facade', [-20, 0, z], -Math.PI / 2]),
      ['spire_lobby', [-20, 0, 0], -Math.PI / 2], ['spire_lobby', [-20, 0, 16], -Math.PI / 2],
      ...[-16, -8, 0, 8, 16].map((x) => ['spire_facade', [x, 0, -20], Math.PI]),
      ...[-16, -8, 0, 8, 16].map((x) => ['spire_facade', [x, 0, 20], 0]),
      ...[-16, -8].map((z) => ['spire_facade', [20, 0, z], Math.PI / 2]),
      ['spire_facade#3', [20, 0, 0], Math.PI / 2], ['spire_facade#2', [20, 0, 8], Math.PI / 2],
      ['spire_facade#1', [20, 0, 16], Math.PI / 2],
      /* ── Floors: the punched plates, tiled with the 4 m bay. Bays mount
         0.2 m low so the walking surface lands ON the level's own height. ── */
      ...[[10, [-11, -18, -4, -10]], [20, [11, -13, 19, -4]], [30, [-4, 10, 5, 18]]].flatMap(
        ([y, well]) => punch([[-19, -20, 19, 20]], [[-7, -10, 7, 10], well])
          .flatMap((s) => plateBays('spire_floor', s, y - .2))),
      /* ── Roof: 5 bays of 8 m, the skylight on the ONE bay over the atrium
         (five bays of it is a 40 m glazed strip — §5's repeat-distance
         mistake), the plain bays cycled so five bays are not one bay five
         times, and end parapets on the X edges. ── */
      // Every bay is laid PLAIN. The skylight variant belongs over the atrium
      // and the atrium's centre line is the roof lap: variant 1 puts its frame
      // at 40.25-40.5 across z 0, x -1.8..1.8, so the lane paint rode up onto
      // the glass (0.62 m of it) on the surface the match ends on. The
      // skylight wants shipping as its own piece, placed once, clear of the
      // lap - the same forward ask that took it off five bays last pass.
      ['spire_roof#0', [-16, 0, 0], 0], ['spire_roof#2', [-8, 0, 0], 0], ['spire_roof#3', [0, 0, 0], 0],
      ['spire_roof#0', [8, 0, 0], 0], ['spire_roof#2', [16, 0, 0], 0],
      ...[-16, -8, 0, 8, 16].flatMap((z) => [
        ['spire_roof_parapet', [-20, 0, z], Math.PI / 2],
        ['spire_roof_parapet', [20, 0, z], Math.PI / 2],
      ]),
      /* ── The stair: four flights spiralling the atrium, 10 m of rise each.
         The art climbs +X from its bottom step, so the yaw turns it onto the
         leg and the X scale stretches it to the 18 and the 14 m run. ── */
      ['spire_stairwell', [-14, 0, -14], 0],
      ['spire_stairwell', [14, 10, -14], -Math.PI / 2],
      ['spire_stairwell', [14, 20, 14], Math.PI, [1.8, 1, 1]],
      ['spire_stairwell', [-14, 30, 14], Math.PI / 2, [1.4, 1, 1]],
      /* ── Fire escape: two flights on the open east face, a landing at each
         floor they serve. The flight art climbs toward −Z, which is the way
         both legs run, so neither is yawed. Flights and landings alike mount at
         x 21.8 so the whole escape runs 20.6–23.0 — outboard of the back wall,
         brackets bolted onto it — while the route climbs on the treads. The
         MIDDLE segment of each landing is the gate variant, opposite that
         storey's doorway, so you step through the wall rather than over the
         rail. ── */
      ['spire_fireescape_flight', [21.8, 0, 16], 0], ['spire_fireescape_flight', [21.8, 10, 8], 0],
      ...[[10, 8], [20, 0]].flatMap(([y, cz]) => [[-8 / 3, 0], [0, 1], [8 / 3, 0]].map(
        ([d, v]) => ['spire_fireescape#' + v, [21.8, y - .2, cz + d], 0])),
      /* ── Fixtures ────────────────────────────────────────────────────────── */
      ['shared_core', [0, 40.02, 0], -Math.PI / 2],
      ['shared_spawn_portal', [-34, 0, 0], Math.PI / 2], ['shared_spawn_portal', [-34, 0, 16], Math.PI / 2],
      ['shared_armory_kiosk', [-26, 0, 10], Math.PI / 2],
      // Traversal. The lift runs the full 40 m for the first time; the six
      // atrium ladders are 10 m climbs, so they use the 10 m piece — the set
      // holds a true 30 cm rung pitch at every height, so a placement picks
      // the height rather than scaling one.
      // The shaft ships as one 5 m bay, so the 40 m rise is eight of them.
      ...[0, 5, 10, 15, 20, 25, 30, 35].map((y) => ['shared_elevator_shaft', [-5, y, -8], 0]),
      ['shared_elevator', [-5, 0, -8], 0],
      ...[[6, -8, -2], [-6, 8, 2]].flatMap(([x, near, far]) => [0, 1, 2].map(
        (f) => ['shared_ladder_1000', [x, f * 10, f === 1 ? far : near], x > 0 ? 0 : Math.PI])),
      ['shared_teleporter_pad_idle', [-14, 0, 6], 0], ['shared_teleporter_pad_idle', [6, 40.02, 6], 0],
      // 2 m off w37 where upstream puts it; two metres south clears the pad
      // without moving the ride off the open east face.
      ['shared_zipline_anchor', [14, 40.02, -2], -Math.PI / 2], ['shared_zipline_anchor', [32, 0, 0], Math.PI / 2, .6],
      // Only the strand's first waypoint is inside mast height; the rest of
      // the air lane is above the block, which is the point of it.
      ['shared_airlane_pylon', [-40, 0, 0], 0, 6 / 9],
      /* ── Roof plant. The mast tops out at 52 m, so it stands in the corner
         the air lane does not come over; the tank is back on the roof now that
         the lap no longer rings the parapet. ── */
      ['spire_hvac', [-15, 40, -16], .31], ['spire_hvac', [-15, 40, -10], -.21],
      // Upstream stands the tank at (15, -16), which is 2.24 m from w36 and
      // leaves 0.3 m between a 3.4 m tank and a build pad. Hard into the
      // corner instead — and measured off the FOOTPRINT, not the origin: the
      // lid cone is the widest part at r 1.7, so 3.28 m clear of w36, 14 m of
      // w38, and 19.68 against the parapet's 19.8 inner face.
      ['spire_watertank', [18, 40, -18], 0],
      ['spire_antenna', [-16, 40, 16], 0],
      /* ── Plaza dressing — margins only, ≥3 m off every route and socket ── */
      ['spire_terrain_scatter', [-44, 0, -22], .3], ['spire_terrain_scatter', [-44, 0, 26], 1.4], ['spire_terrain_scatter', [44, 0, -24], 2.2], ['spire_terrain_scatter', [42, 0, 24], .7],
      ['spire_terrain_scatter', [-12, 0, 30], 1.1], ['spire_terrain_scatter', [27, 0, 33], .2], ['spire_terrain_scatter', [-2, 0, -30], 2.7], ['spire_terrain_scatter', [30, 0, -12], 1.8],
      /* ── Kerbside traffic on the perimeter street, variants cycled so a row
         of parked cars is four models rather than one model eight times. ── */
      ['spire_dress_car#0', [-44, 0, -26], Math.PI / 2], ['spire_dress_car#1', [-44, 0, -34], Math.PI / 2],
      ['spire_dress_car#2', [-44, 0, 26], Math.PI / 2], ['spire_dress_car#3', [44, 0, -14], -Math.PI / 2],
      ['spire_dress_car#0', [30, 0, -31], 0], ['spire_dress_car#1', [-12, 0, -31], 0],
      ['spire_dress_car#2', [12, 0, 31], 0], ['spire_dress_car#3', [-30, 0, 31], 0],
    ],
    zipline: [[14, 40.7, -2], [32, 1.2, 0]],
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
   * `roads` here names every walked route, as on the other maps — but the
   * module it lays is `toaster_path_ground`, a worn two-rut TRACK in the
   * tile's own grass, not tarmac. The made roads (`vroads`) are the VEHICLE
   * network — handling is read off those centre lines — and the enemy routes
   * mostly go cross-country over the fields; paving them would put asphalt
   * through a hayfield and tell a driver the grip changes where it does not.
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
    /* Every WALKED route is dressed with `toaster_path_ground` — the worn
       two-rut track, not tarmac (FORWARD-MANIFEST-hero §E). The lanes still go
       cross-country; a farm marks a line walked twelve waves a night exactly
       this way, and the module is the tile's own grass with the wear in
       vertex colour, so the fields stay fields. Teleport legs are skipped by
       the builder — nothing walks them. */
    roads: ['direct', 'long', 'west'],
    /* Which tiles are which ground. Mown lawn round the houses and along the
       roads; beyond the fences the tile grid carries the FIELD variants —
       rough grazed pasture south-west, cut hay stubble east. Tile-aligned
       (20 m), because a variant is a whole tile. */
    fields: [
      { variant: 5, x: [80, 140], z: [-20, 60] },
      { variant: 4, x: [-140, -20], z: [-60, -20] },
    ],
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
      /* Shed apron: 8 m of gravel across the whole +Z front, under the drive's
         first leg. Design-only (the client draws the drive strip); it sits 15
         mm under the drive so the two gravels do not fight. */
      { kind: 'apron', width: 8, pts: [[-122, 0, 68.5], [-90, 0, 68.5]] },
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
      /* Each tank and woodpile 3 m off the wall it serves, OUTSIDE the
         footprint: (−126, −20), (100, −58), (−118, 44) and (98, −38) were all
         inside a building. */
      ['toaster_propane_tank', [44, 0, 47.5], 0], ['toaster_propane_tank', [-146.4, 0, -12], 1.57], ['toaster_propane_tank', [105, 0, -50], 1.57],
      ['toaster_woodpile', [-120, 0, 40], .2], ['toaster_woodpile', [106, 0, -42], -1.3],
      ['toaster_wreck_pickup', [-128, 0, 63], 1.2],

      /* ── The working farm ───────────────────────────────────────────────
         Bins and fuel by the shed; the mill, its tank and two feeders in the
         pasture; a hen house and a garden shed; gardens and washing behind
         the two houses that are lived in; the wagon where the baler stopped. */
      ['toaster_grain_bin#1', [-137, 0, 50], .4], ['toaster_grain_bin', [-137, 0, 59.5], 1.1],
      ['toaster_fuel_tank', [-127, 0, 38], 0],
      ['toaster_windmill', [-85, 0, -46], 0], ['toaster_stock_tank', [-81, 0, -46], 0],
      ['toaster_bale_feeder', [-60, 0, -50], 0], ['toaster_bale_feeder', [-100, 0, -36], .8],
      ['toaster_shed_small', [110, 0, -58], Math.PI], ['toaster_shed_small', [36, 0, 52], Math.PI],
      ['toaster_garden_plot', [26, 0, 53], 0], ['toaster_garden_plot', [-134, 0, -30], 0],
      ['toaster_clothesline', [14, 0, 50], 0], ['toaster_clothesline', [112, 0, -48], Math.PI / 2],
      ['toaster_hay_wagon', [100, 0, 44], .3],
      // Everything else — mailboxes, the fence runs, the bale field, the belt
      // understory and the cut-grass drift — is generated and filtered below,
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
 * The Toaster's fence runs, bale field, understory and grass drift are rows
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
    toaster_mailbox: .4, toaster_fence_wood: 2.1, toaster_fence_wire: 2.1, toaster_gate_farm: 2.2, toaster_hay_bale: .8,
    toaster_understory: 2.4, toaster_leaf_pile: 1.5, toaster_terrain_scatter: 2.4, toaster_hedgerow: 2.2,
    toaster_tree_oak_open: 7.6, toaster_tree_apple: 2.5, toaster_crop_rows: 7.1, toaster_reeds: 1.1, toaster_utility_pole: 1.2,
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
     road. Fence lines, gates, poles, a hedgerow and a mailbox are the point
     of a verge, so they get the road test at the carriageway edge; the rest
     get the same 2 m margin as the trees. Nothing generated may land inside
     a building's footprint either — the first drop stood two tanks and two
     woodpiles inside walls because nothing checked. */
  const onRoad = roadKeepOut(T, 2);
  const atKerb = roadKeepOut(T, 0);
  const VERGE = new Set(['toaster_mailbox', 'toaster_fence_wood', 'toaster_fence_wire', 'toaster_gate_farm', 'toaster_utility_pole', 'toaster_hedgerow']);
  const inBuilding = (x, z, m) => T.volumes.some((v) => Math.abs(x - v.at[0]) < v.size[0] / 2 + m && Math.abs(z - v.at[2]) < v.size[2] / 2 + m);
  const add = (id, x, z, ry) => {
    const base = id.split('#')[0], rad = RADIUS[base] || 1, m = 3 + rad + .2;
    for (const s of walked) if (segDist(x, z, s) < m) return false;
    for (const p of keepOut) if (Math.hypot(p[0] - x, p[2] - z) < m) return false;
    if ((VERGE.has(base) ? atKerb : onRoad)(x, z)) return false;
    if (inBuilding(x, z, rad + 1)) return false;
    T.place.push([id, [x, 0, z], ry]);
    return true;
  };
  /* A fence run from a to b in 4 m modules, posts landing on module ends, a
     gate in place of the modules `gates` names. Runs are the field lines —
     where a fence goes IS the design of a farm. */
  const fence = (id, a, b, gates = []) => {
    const dx = b[0] - a[0], dz = b[1] - a[1], len = Math.hypot(dx, dz), n = Math.max(1, Math.round(len / 4)), ry = Math.atan2(-dz, dx);
    for (let i = 0; i < n; i++) { const t = (i + .5) / n; add(gates.includes(i) ? 'toaster_gate_farm' : id, a[0] + dx * t, a[1] + dz * t, ry); }
  };

  // Mailboxes at the three road heads.
  add('toaster_mailbox', -52.5, 44, 1.57);
  add('toaster_mailbox', -86, 42.4, 0.14);
  add('toaster_mailbox', 33, 9.6, -0.34);
  // Post-and-rail both sides of the shed drive — the one dressed fence.
  for (let i = 0; i < 8; i++) add('toaster_fence_wood', -88 + i * 4, 41.5, 0.14);
  for (let i = 0; i < 9; i++) add('toaster_fence_wood', -88 + i * 4, 53, 0.14);

  /* ── Field lines ───────────────────────────────────────────────────────
     Wire on T-posts. The south-west pasture (mill, tank, feeders), the
     orchard plot beside it, the crop field north of the core yard, and the
     hayfield east of the spawn — four enclosures that turn 320 m of lawn into
     a property. Gates face the yard each field is worked from. */
  fence('toaster_fence_wire', [-118, -60], [-24, -60]);
  fence('toaster_fence_wire', [-24, -60], [-24, -28], [4]);
  fence('toaster_fence_wire', [-24, -28], [-118, -28], [12]);
  fence('toaster_fence_wire', [-118, -28], [-118, -60]);
  fence('toaster_fence_wire', [-146, -36], [-122, -36], [3]);
  fence('toaster_fence_wire', [-122, -36], [-122, -60]);
  fence('toaster_fence_wire', [-41, 7], [-9, 7], [4]);
  fence('toaster_fence_wire', [-9, 7], [-9, 49]);
  fence('toaster_fence_wire', [-9, 49], [-41, 49]);
  fence('toaster_fence_wire', [-41, 49], [-41, 7]);
  fence('toaster_fence_wire', [85, -16], [85, 62], [9]);
  fence('toaster_fence_wire', [85, 62], [146, 62]);
  fence('toaster_fence_wire', [146, 62], [146, -16]);
  fence('toaster_fence_wire', [146, -16], [85, -16], [7]);

  // Hedgerow along the hayfield frontage, with open-grown oaks standing in it.
  for (let z = -12; z <= 58; z += 4) add('toaster_hedgerow', 82, z, Math.PI / 2 + ((z / 4) % 2 ? .05 : -.05));
  for (const [x, z, ry] of [[86, -10, .3], [82, 40, 2.2], [82, 58, .8]]) add('toaster_tree_oak_open', x, z, ry);
  // Specimen oaks: in the pasture, over the pond, at the road, by the shed.
  for (const [x, z, ry] of [[-30, -35, .4], [-48, -52, 1.7], [-100, -44, 2.6], [-72, -57, .9], [-20, 61, 1.3], [-75, 60, 2.1], [112, -30, .5], [38, -40, 2.9]]) add('toaster_tree_oak_open', x, z, ry);
  // Orchard: four rows of four on 6 m centres, south of the Buggy house.
  for (let c = 0; c < 4; c++) for (let r = 0; r < 4; r++) add('toaster_tree_apple', -143 + c * 6, -57 + r * 6, c * 1.3 + r * .7);
  // Row crop: twelve 10 m modules inside the wire north of the core yard.
  for (let c = 0; c < 3; c++) for (let r = 0; r < 4; r++) add('toaster_crop_rows', -35 + c * 10, 13 + r * 10, 0);
  // Round bales through the hayfield on a spiral, so they do not read as a grid.
  for (let i = 0; i < 26; i++) {
    const r = i * 2.399;
    add('toaster_hay_bale', 110 + Math.cos(r) * (6 + (i % 5) * 5.5), 16 + Math.sin(r) * (6 + (i % 4) * 5), r);
  }
  // Reeds on the pond's wet shelf — not on the dock side, not where the lane passes.
  for (const a of [.35, .95, 1.6, 3.55, 4.25, 4.95, 5.6, 6.05]) add('toaster_reeds', 19 + Math.cos(a) * 16.2, -36 + Math.sin(a) * 16.2, a);

  // Understory and cut-grass drift along the inner edge of the treeline.
  for (let i = 0; i < 18; i++) {
    const s = i % 2 ? 1 : -1, t = (i * 0.137) % 1;
    add('toaster_understory', -148 + t * 296, s * (66 - (i % 3) * 3), i * .7);
  }
  for (let i = 0; i < 12; i++) {
    const s = i % 2 ? 1 : -1, t = ((i * 0.211) + .07) % 1;
    add('toaster_leaf_pile', -146 + t * 292, s * (64 - (i % 4) * 4), i * 1.1);
  }
  // Ground scatter: the detail the terrain tile is forbidden from carrying,
  // on a hashed lattice across the whole field and filtered like the rest.
  for (let i = 0; i < 64; i++) {
    const x = -140 + ((i * 53) % 280) + ((i * 7) % 11) - 5, z = -62 + ((i * 29) % 124) + ((i * 3) % 7) - 3;
    add('toaster_terrain_scatter', x, z, i * .7);
  }

  /* ── Power lines ───────────────────────────────────────────────────────
     Poles every 36 m down the county road and the south road, 5.5 m off the
     centreline on one consistent side, every third one carrying a
     transformer. The conductors are built by the viewer from the poles that
     PASSED the clearance test (`powerlines`), so a refused pole shortens a
     span rather than leaving wire over nothing. */
  T.powerlines = [];
  const poleLine = (pts, spacing, off) => {
    const line = []; let carry = spacing * .5, k = 0;
    for (let i = 0; i < pts.length - 1; i++) {
      const [ax, , az] = pts[i], [bx, , bz] = pts[i + 1], dx = bx - ax, dz = bz - az, len = Math.hypot(dx, dz);
      const nx = dz / len, nz = -dx / len, ry = Math.atan2(-nz, nx);
      let s = carry;
      for (; s < len; s += spacing, k++) {
        const t = s / len, x = ax + dx * t + nx * off, z = az + dz * t + nz * off;
        if (Math.abs(x) > 152 || Math.abs(z) > 76) continue;
        if (add(k % 3 === 1 ? 'toaster_utility_pole#1' : 'toaster_utility_pole', x, z, ry)) line.push([x, z, ry]);
      }
      carry = s - len;
    }
    if (line.length > 1) T.powerlines.push(line);
  };
  poleLine(T.vroads[0].pts, 36, 5.5);
  poleLine([[-152, 0, -66], [152, 0, -66]], 40, 5.5);
}
