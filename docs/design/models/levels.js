/**
 * Deep Field 3D — level layouts lifted from sim/Sim.Core/Content/Maps.cs and
 * game/scripts/GameRoot.cs (graybox builders). Sim space is X/Z with Y up; the
 * Godot client converts with ToGd, we keep sim coordinates directly.
 * `place` lists kit instances: [modelId, [x,y,z], rotY, scale?].
 */
export const LEVELS = {
  foundry: {
    label: 'Foundry', kit: 'foundry', field: [110, 80],
    routes: {
      ground: [[-40, 0, 0], [-20, 0, 0], [-20, 0, 14], [0, 0, 14], [0, 0, -8], [18, 0, -8], [18, 0, 6], [36, 0, 6]],
      air: [[-40, 8, -6], [-10, 9, -2], [14, 8, 4], [36, 8, 5]],
    },
    sockets: [
      ['g1', [-24, 0, 6], 'ground'], ['g2', [-14, 0, 9], 'ground'], ['g3', [-4, 0, 8], 'ground'], ['g4', [4, 0, 0], 'ground'], ['g5', [12, 0, -2], 'ground'], ['g6', [24, 0, 0], 'ground'],
      ['w1', [-6, 6, -16], 'wall'], ['w2', [2, 6, -16], 'wall'], ['w3', [10, 6, -16], 'wall'],
      ['t1', [-20, 0, 7], 'trap'], ['t2', [0, 0, 3], 'trap'], ['t3', [18, 0, -1], 'trap'],
    ],
    heroSpawn: [0, 0, -24], armory: [-6, 0, -24],
    stations: [['spawnYard', [0, 0, -20]], ['upperDeck', [2, 6, -15]], ['midLane', [-8, 0, 5]], ['coreGate', [28, 0, 3]]],
    place: [
      // Upper deck: 28×10 at (2, 5.8, -17) → 7 bays of 4 m.
      ...[-10, -6, -2, 2, 6, 10, 14].map((x) => ['foundry_deck', [x, 0, -17], 0]),
      // Guard rail on the +Z edge (z −12.2), skipping the ladder bay (x −8) and zipline bay (x 14).
      ...[-10, -2, 2, 6, 10].map((x) => ['foundry_deck_rail', [x, 6.0, -12.2], 0]),
      ['foundry_deck_rail', [-6, 6.0, -12.2], 0],
      ['foundry_pillar', [-10, 0, -17], 0], ['foundry_pillar', [14, 0, -17], 0],
      ['foundry_vent_tunnel', [0, 0, -6.5], 0],
      ['shared_vent_grate', [0, 0, -13.5], 0], ['shared_vent_grate', [0, 0, .5], Math.PI],
      // Traversal.
      ['shared_ladder', [-8, 0, -12.4], 0],
      ['shared_launcher_idle', [-14, 0, -20], 0],
      ['shared_zipline_anchor', [14, 6.0, -14], 0.85],
      ['shared_zipline_anchor', [28, 0, 2], -2.3, .6],
      ['shared_controlpoint_neutral', [26, 0, -6], 0],
      ['shared_armory_kiosk', [-6, 0, -24], 0],
      ['shared_spawn_portal', [-40, 0, 0], Math.PI / 2],
      ['shared_core', [40, 0, 6], -Math.PI / 2],
      // Air-lane pylons at each air waypoint (mast top = strand height).
      ['shared_airlane_pylon', [-40, 0, -6], 0, 8 / 9], ['shared_airlane_pylon', [-10, 0, -2], 0, 1], ['shared_airlane_pylon', [14, 0, 4], 0, 8 / 9], ['shared_airlane_pylon', [36, 0, 5], 0, 8 / 9],
      // Dressing — kept out of socket→route sightlines (behind sockets or beyond the lane).
      ['foundry_dress_crucible', [-32, 0, -18], .6], ['foundry_dress_gantry', [-30, 0, 24], 0], ['foundry_dress_pipes', [10, 0, 24], 0], ['foundry_dress_pipes', [-4, 0, 26], .2],
      ['foundry_dress_lightrig', [-20, 0, -26], 0], ['foundry_dress_lightrig', [30, 0, -18], 0], ['foundry_dress_lightrig', [30, 0, 22], 0],
      ['foundry_dress_steamvent', [-28, 0, 12], 0], ['foundry_dress_steamvent', [8, 0, 18], 1], ['foundry_dress_steamvent', [24, 0, 12], 2], ['foundry_dress_steamvent', [-12, 0, -30], 0],
      // Terrain scatter — corners and margins only, ≥2.5 m from any route, socket or kit footprint.
      ['foundry_terrain_scatter', [-46, 0, -30], .3], ['foundry_terrain_scatter', [-46, 0, 30], 1.2], ['foundry_terrain_scatter', [46, 0, -30], 2.1], ['foundry_terrain_scatter', [46, 0, 28], .8],
      ['foundry_terrain_scatter', [-20, 0, 32], 1.6], ['foundry_terrain_scatter', [30, 0, 32], .5], ['foundry_terrain_scatter', [12, 0, -32], 2.6], ['foundry_terrain_scatter', [-38, 0, 14], 1.0],
    ],
    zipline: [[14, 6.8, -14], [28, 1.6, 2]],
  },
  switchyard: {
    label: 'Switchyard', kit: 'switchyard', field: [110, 80],
    routes: {
      ground: [[-45, 0, -10], [-25, 0, -10], [-25, 0, 12], [-5, 0, 12], [-5, 0, -12], [15, 0, -12], [15, 0, 10], [40, 0, 8]],
      groundShort: [[-45, 0, -10], [-20, 0, -2], [5, 0, 0], [40, 0, 8]],
      air: [[-45, 9, 4], [-12, 10, 0], [16, 9, -4], [40, 9, 6]],
    },
    sockets: [
      ['g1', [-30, 0, 2], 'ground'], ['g2', [-18, 0, 8], 'ground'], ['g3', [-10, 0, 4], 'ground'], ['g4', [0, 0, -4], 'ground'], ['g5', [10, 0, 2], 'ground'], ['g6', [20, 0, -4], 'ground'], ['g7', [30, 0, 2], 'ground'],
      ['w1', [-12, 5, -18], 'wall'], ['w2', [0, 5, -18], 'wall'], ['w3', [-4, 10, 6], 'wall'], ['w4', [10, 10, 0], 'wall'],
      ['t1', [-25, 0, 0], 'trap'], ['t2', [-5, 0, 0], 'trap'], ['t3', [15, 0, -2], 'trap'], ['t4', [5, 0, 0], 'trap'],
      ['b1', [-8, 0, -1], 'barricade'],
    ],
    heroSpawn: [0, 0, -26], armory: [6, 0, -26],
    stations: [['yard', [0, 0, -22]], ['midDeck', [-6, 5, -17]], ['catwalk', [2, 10, 3]], ['cutMouth', [-16, 0, -2]], ['coreGate', [32, 0, 5]]],
    cut: { route: 'groundShort', from: -18, to: -10 },
    place: [
      // Mid deck 24×8 at (−6, 4.8, −18) → 6 bays; support pillar at x −6 (half-height column).
      ...[-16, -12, -8, -4, 0, 4].map((x) => ['switchyard_middeck', [x, 0, -18], 0]),
      ['switchyard_column', [-6, 0, -18], 0, .49],
      // Catwalk 22×5 at (3, 9.8, 3) → 6 bays (−7…13), lattice columns at x −6 and 12.
      ...[-6, -2, 2, 6, 10, 13].map((x) => ['switchyard_catwalk', [x, 0, 3], 0]),
      ['switchyard_column', [-6, 0, 3], 0], ['switchyard_column', [12, 0, 3], 0],
      // Traversal: ladders yard→mid (5 m) and mid→catwalk (5.4 m), launcher, zipline, armory, core, portal.
      ['shared_ladder', [-14, 0, -14.2], 0, 5 / 6], ['shared_ladder', [-4, 4.8, 1.2], 0, 5.4 / 6],
      ['shared_launcher_idle', [8, 0, -22], 0],
      ['shared_zipline_anchor', [12, 10.0, 4], 1.5], ['shared_zipline_anchor', [32, 0, 5], -1.6, .6],
      ['shared_armory_kiosk', [6, 0, -26], 0],
      ['shared_spawn_portal', [-45, 0, -10], Math.PI / 2],
      ['shared_core', [44, 0, 8], -Math.PI / 2],
      ['shared_airlane_pylon', [-45, 0, 4], 0, 1], ['shared_airlane_pylon', [-12, 0, 0], 0, 10 / 9], ['shared_airlane_pylon', [16, 0, -4], 0, 1], ['shared_airlane_pylon', [40, 0, 6], 0, 1],
      // Dressing — sidings along the ±Z margins and the far corners; nothing across a socket→route line.
      ['switchyard_dress_railcar', [-30, 0, 30], 0], ['switchyard_dress_railcar', [20, 0, 30], .05], ['switchyard_dress_railcar', [-30, 0, -30], 0],
      ['switchyard_dress_container', [30, 0, -30], 0], ['switchyard_dress_container', [30, 2.6, -30], .03], ['switchyard_dress_container', [36, 0, -27], 1.57], ['switchyard_dress_container', [-46, 0, 26], .4], ['switchyard_dress_container', [46, 0, -12], 1.57],
      ['switchyard_dress_signaltower', [-36, 0, -22], 0], ['switchyard_dress_signaltower', [-34, 0, 20], 3.14], ['switchyard_dress_signaltower', [26, 0, 16], 0],
      ['switchyard_dress_buffer', [-50, 0, 30], -Math.PI / 2], ['switchyard_dress_buffer', [-50, 0, -30], -Math.PI / 2], ['switchyard_dress_buffer', [50, 0, 30], Math.PI / 2],
      ['switchyard_terrain_scatter', [-40, 0, 20], .5], ['switchyard_terrain_scatter', [40, 0, -20], 2.0], ['switchyard_terrain_scatter', [0, 0, 34], 1.0], ['switchyard_terrain_scatter', [-8, 0, -34], .2], ['switchyard_terrain_scatter', [44, 0, 26], 2.8],
    ],
    zipline: [[12, 10.6, 4], [32, 1.6, 5]],
  },
};
