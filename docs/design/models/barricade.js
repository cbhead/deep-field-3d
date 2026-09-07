/**
 * Barricade — HP blocker on `barricade` sockets. Closes a shortcut route while
 * alive; enemies pick the long way at spawn. Sim: TowerKind.Barricade and
 * SocketTag.Barricade are typed (StructureHp on TowerDef); no row yet — M2.
 *
 * Form factor differs from every other tower: a 4.5 × 2.2 m wall segment
 * (DESIGN-BRIEF §3.2), not a 2.1 m pad. Authored at 3.0 m and scaled ×1.5 on
 * `barricade_body` so every attachment keeps its proportions. Origin at the slot centre, wall runs along X, enemies approach
 * from +Z (the "kill face"). No rig, no spin.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'barricade',
  label: 'Barricade',
  swatch: 'var(--brass-400)',
  stats: { Milestone: 'M2', Kind: 'Barricade', Socket: 'barricade', Sim: 'not yet' },
  layers: 'Blocks ground route',
  rig: null,
  headline: 'structure',
  note: 'A wall, not a weapon: it exists so the route gate reads from across the map. +Z is the enemy face — every spite attachment lives there, every recovery attachment lives on the safe side. Damage states (HP 100 / 66 / 33 %) are the next pass: they should chip the concrete, not the frame, so the silhouette survives to the end.',
  paths: [
    {
      id: 'bulk', label: 'Bulk', cue: 'Frame posts thicken',
      steps: {
        2: 'Corner plates',
        3: 'Top rail',
        4: 'Armour slabs on the enemy face (+HP)',
        5: 'Buttress feet',
        6: 'Second cross beam',
        7: 'Heavy top cap',
        8: 'Diagonal cross-bracing',
        9: 'Rear buttresses',
        10: 'Full blast plate',
      },
    },
    {
      id: 'spite', label: 'Spite', cue: 'Spike tips brighten',
      steps: {
        2: 'Rivet studs on the face',
        3: 'Razor strip along the top',
        4: 'Spike row (contact damage applies shred)',
        5: 'Second spike row',
        6: 'Spike guards',
        7: 'Knockback charge pads (knockback on break)',
        8: 'Pad cabling',
        9: 'Arc emitters at the corners',
        10: 'Kill-face spike wall',
      },
    },
    {
      id: 'recovery', label: 'Recovery', cue: 'Nanite lines glow with regen rate',
      steps: {
        2: 'Patch-kit box',
        3: 'Nanite feed line',
        4: 'Folded repair arm (regen)',
        5: 'Reserve plate rack',
        6: 'Second nanite line',
        7: 'Rebuild drone dock (self-rebuild once per wave)',
        8: 'Docked drone',
        9: 'Supply tanks',
        10: 'Rebuild scaffold frame',
      },
    },
  ],

  build(K) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'barricade', mats.energy_fuse);
    const root = grp('barricade');
    const S = 1.5; // 3.0 m authoring width → 4.5 m brief width (height lands at 2.2 m)
    const body = grp('barricade_body');
    body.scale.setScalar(S);
    root.add(body);
    const W = 3.0, H = 1.3, T = .56;

    // Footing + two concrete blocks with a steel frame.
    body.add(part('barricade_footing', box(W + .3, .14, T + .5), mats.chassis, [0, .07, 0]));
    body.add(K.hazardStripes('barricade_footing_hazard', W + .2, .08, [0, .08, (T + .5) / 2 + .005], [0, 0, 0], 10));
    for (const s of [-1, 1]) {
      body.add(part('barricade_block' + (s < 0 ? '_l' : '_r'), box(W / 2 - .12, H - .30, T), mats.concrete, [s * (W / 4 + .02), .14 + (H - .30) / 2, 0]));
      body.add(part('barricade_block_cap' + (s < 0 ? '_l' : '_r'), box(W / 2 - .06, .08, T + .06), mats.steel_hull, [s * (W / 4 + .02), H - .12, 0]));
    }
    for (const x of [-W / 2, -W / 6 * .6, W / 6 * .6, W / 2]) {
      body.add(part('barricade_post' + x, box(.14, H, .16), mats.chassis, [x * .98, H / 2 + .07, 0]));
      body.add(part('barricade_post_cap' + x, box(.18, .06, .20), mats.steel_plate, [x * .98, H + .10, 0]));
    }
    body.add(part('barricade_beam', box(W, .10, .10), mats.steel_hull, [0, .40, T / 2 + .06]));
    body.add(part('barricade_beam_rear', box(W, .10, .10), mats.steel_hull, [0, .40, -T / 2 - .06]));
    body.add(part('barricade_centre_plate', box(.44, .50, .03), mats.steel_plate, [0, .70, T / 2 + .02]));
    body.add(K.boltRing('barricade_centre_bolts', .17, 6, 0).rotateX(90 * D).translateZ(-.72).translateY(T / 2 + .04));
    body.add(part('barricade_status_lamp', box(.20, .04, .02), mats.energy_fuse, [0, .96, T / 2 + .035]));
    body.add(K.cableRun('barricade_loom', [[-.9, .16, -T / 2 - .12], [-.4, .30, -T / 2 - .16], [0, .46, -T / 2 - .12]], .024));
    body.add(part('barricade_junction', box(.22, .18, .14), mats.chassis, [-1.0, .18, -T / 2 - .16]));

    /* ── BULK ladder ────────────────────────────────────────────────── */
    up('bulk', 2, body, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) for (const z of [-1, 1])
        g.add(part(`barricade_corner_plate${s}${z}`, box(.22, .40, .04), mats.steel_plate, [s * (W / 2 - .04), .40, z * (T / 2 + .03)]));
    });
    up('bulk', 3, body, [0, H + .14, 0], (g) => {
      g.add(part('barricade_top_rail', box(W + .1, .06, .12), mats.steel_hull));
      g.add(part('barricade_top_rail_strip', box(W - .4, .015, .03), mats.energy_fuse, [0, .035, .05]));
    });
    up('bulk', 4, body, [0, 0, T / 2 + .06], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('barricade_armour_slab' + (s < 0 ? '_l' : '_r'), box(W / 2 - .30, H - .50, .08), mats.steel_plate, [s * (W / 4 + .06), .14 + (H - .30) / 2, 0]));
        g.add(K.boltRing('barricade_armour_bolts' + (s < 0 ? '_l' : '_r'), .28, 8, 0).rotateX(90 * D).translateZ(-(.14 + (H - .30) / 2)).translateY(.05).translateX(s * (W / 4 + .06)));
        g.add(part('barricade_armour_lamp' + (s < 0 ? '_l' : '_r'), box(.16, .03, .02), mats.energy_fuse, [s * (W / 4 + .06), .30, .05]));
      }
    });
    up('bulk', 5, body, [0, 0, 0], (g) => {
      for (const s of [-1, 1])
        g.add(part('barricade_buttress_foot' + (s < 0 ? '_l' : '_r'), box(.30, .18, T + .70), mats.concrete, [s * (W / 2 - .08), .22, 0]));
    });
    up('bulk', 6, body, [0, .92, 0], (g) => {
      g.add(part('barricade_beam_b', box(W, .08, .08), mats.steel_hull, [0, 0, T / 2 + .06]));
      g.add(part('barricade_beam_b_rear', box(W, .08, .08), mats.steel_hull, [0, 0, -T / 2 - .06]));
    });
    up('bulk', 7, body, [0, H + .22, 0], (g) => {
      g.add(part('barricade_heavy_cap', box(W + .16, .12, T + .16), mats.chassis));
      g.add(K.hazardStripes('barricade_cap_hazard', W, .08, [0, 0, (T + .16) / 2 + .005], [0, 0, 0], 8));
      for (const x of [-1.2, 0, 1.2]) g.add(part('barricade_cap_lamp' + x, cyl(.03, .03, .02, 8), mats.energy_fuse, [x, .07, 0]));
    });
    up('bulk', 8, body, [0, 0, -T / 2 - .12], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('barricade_brace_a' + (s < 0 ? '_l' : '_r'), box(.06, 1.40, .05), mats.steel_hull, [s * .70, .74, 0], [0, 0, s * 42 * D]));
        g.add(part('barricade_brace_b' + (s < 0 ? '_l' : '_r'), box(.06, 1.40, .05), mats.steel_hull, [s * .70, .74, .0], [0, 0, -s * 42 * D]));
      }
    });
    up('bulk', 9, body, [0, 0, -T / 2 - .34], (g) => {
      for (const x of [-1.0, 0, 1.0]) {
        g.add(part('barricade_rear_buttress' + x, box(.16, .90, .40), mats.concrete, [x, .52, 0], [-22 * D, 0, 0]));
        g.add(part('barricade_rear_buttress_foot' + x, box(.22, .10, .50), mats.chassis, [x, .12, -.06]));
      }
    });
    up('bulk', 10, body, [0, 0, T / 2 + .16], (g) => {
      g.add(part('barricade_blast_plate', box(W + .10, H + .06, .06), mats.chassis, [0, H / 2 + .10, 0]));
      g.add(K.hazardStripes('barricade_blast_hazard', W - .2, .12, [0, .20, .035], [0, 0, 0], 10));
      g.add(K.boltRing('barricade_blast_bolts', .14, 6, 0).rotateX(90 * D).translateZ(-(H / 2 + .10)).translateY(.04));
      g.add(part('barricade_blast_strip', box(W - .5, .03, .015), mats.energy_fuse, [0, H - .02, .04]));
    });

    /* ── SPITE ladder ───────────────────────────────────────────────── */
    up('spite', 2, body, [0, 0, T / 2 + .02], (g) => {
      for (let i = 0; i < 12; i++)
        g.add(part('barricade_rivet' + i, cyl(.03, .03, .03, 8), mats.chrome, [-1.1 + i * .2, .30, 0], [90 * D, 0, 0]));
    });
    up('spite', 3, body, [0, H + .06, T / 2 - .04], (g) => {
      g.add(part('barricade_razor_strip', box(W - .2, .02, .08), mats.chrome));
      for (let i = 0; i < 14; i++)
        g.add(part('barricade_razor_tooth' + i, new THREE.ConeGeometry(.03, .08, 4), mats.chrome, [-1.3 + i * .2, .05, 0]));
    });
    up('spite', 4, body, [0, .74, T / 2 + .06], (g) => {
      for (let i = 0; i < 7; i++)
        g.add(part('barricade_spike' + i, new THREE.ConeGeometry(.05, .34, 8), mats.chrome, [-1.2 + i * .4, 0, .17], [90 * D, 0, 0]));
      for (let i = 0; i < 7; i++) g.add(part('barricade_spike_base' + i, tor(.055, .01, 6, 12), mats.energy_fuse, [-1.2 + i * .4, 0, .01]));
    });
    up('spite', 5, body, [0, .42, T / 2 + .06], (g) => {
      for (let i = 0; i < 6; i++)
        g.add(part('barricade_spike_b' + i, new THREE.ConeGeometry(.045, .30, 8), mats.chrome, [-1.0 + i * .4, 0, .15], [90 * D, 0, 0]));
    });
    up('spite', 6, body, [0, .74, T / 2 + .06], (g) => {
      for (let i = 0; i < 7; i++)
        g.add(part('barricade_spike_guard' + i, tor(.075, .014, 6, 14), mats.trim, [-1.2 + i * .4, 0, .04]));
    });
    up('spite', 7, body, [0, 1.02, T / 2 + .05], (g) => {
      for (const x of [-1.0, -.35, .35, 1.0]) {
        g.add(part('barricade_kb_pad' + x, box(.40, .16, .05), mats.chassis, [x, 0, 0]));
        g.add(part('barricade_kb_pad_glow' + x, box(.30, .04, .015), mats.energy_fuse, [x, 0, .03]));
      }
    });
    up('spite', 8, body, [0, 1.12, T / 2 + .06], (g) => {
      g.add(K.cableRun('barricade_pad_cable', [[-1.0, 0, 0], [-.35, .04, .03], [.35, .04, .03], [1.0, 0, 0]], .016));
    });
    up('spite', 9, body, [0, H + .16, T / 2], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('barricade_arc_post' + (s < 0 ? '_l' : '_r'), cyl(.03, .03, .26, 8), mats.chrome, [s * (W / 2 - .06), .08, 0]));
        g.add(part('barricade_arc_tip' + (s < 0 ? '_l' : '_r'), new THREE.SphereGeometry(.05, 10, 8), mats.energy_arc, [s * (W / 2 - .06), .24, 0]));
      }
    });
    up('spite', 10, body, [0, 0, T / 2 + .30], (g) => {
      for (let r = 0; r < 3; r++) for (let i = 0; i < 8; i++)
        g.add(part(`barricade_wall_spike${r}_${i}`, new THREE.ConeGeometry(.04, .40, 6), mats.chrome, [-1.3 + i * .37 + (r % 2) * .18, .30 + r * .34, .20], [90 * D, 0, 0]));
      g.add(part('barricade_spike_frame', box(W, 1.0, .05), mats.trim, [0, .64, 0]));
      g.add(part('barricade_spike_frame_glow', box(W - .2, .02, .015), mats.energy_fuse, [0, 1.16, .03]));
    });

    /* ── RECOVERY ladder ────────────────────────────────────────────── */
    up('recovery', 2, body, [.9, .34, -T / 2 - .16], (g) => {
      g.add(part('barricade_patch_kit', box(.30, .22, .16), mats.steel_hull));
      g.add(K.hazardStripes('barricade_patch_hazard', .26, .06, [0, .06, -.085], [0, 180 * D, 0], 3));
    });
    up('recovery', 3, body, [0, 0, -T / 2 - .10], (g) => {
      g.add(K.cableRun('barricade_nanite_line', [[-1.0, .28, 0], [-.3, .62, -.06], [.6, .90, -.02], [1.3, .70, 0]], .018, mats.energy_scan));
    });
    up('recovery', 4, body, [-.6, H + .14, -T / 2 - .08], (g) => {
      g.add(part('barricade_arm_base', box(.16, .12, .16), mats.chassis));
      g.add(part('barricade_arm_seg_a', box(.08, .08, .60), mats.steel_hull, [0, .10, -.26], [0, 0, 0]));
      g.add(part('barricade_arm_seg_b', box(.07, .07, .50), mats.steel_hull, [0, .16, -.06], [0, 0, 0]));
      g.add(part('barricade_arm_tool', cyl(.04, .06, .12, 10), mats.brass, [0, .16, .20], [90 * D, 0, 0]));
      g.add(part('barricade_arm_tool_tip', cyl(.02, .02, .03, 8), mats.energy_scan, [0, .16, .27], [90 * D, 0, 0]));
    });
    up('recovery', 5, body, [0, .70, -T / 2 - .16], (g) => {
      for (let i = 0; i < 3; i++)
        g.add(part('barricade_reserve_plate' + i, box(.70, .40, .03), mats.steel_plate, [.2, 0, -i * .05]));
      g.add(part('barricade_reserve_rack', box(.80, .06, .20), mats.trim, [.2, -.23, -.06]));
    });
    up('recovery', 6, body, [0, 0, -T / 2 - .12], (g) => {
      g.add(K.cableRun('barricade_nanite_line_b', [[1.2, .24, 0], [.3, .50, -.05], [-.7, .80, -.02], [-1.3, .60, 0]], .016, mats.energy_scan));
    });
    up('recovery', 7, body, [.8, H + .14, -T / 2 - .30], (g) => {
      g.add(part('barricade_dock_pad', box(.50, .06, .40), mats.chassis));
      g.add(part('barricade_dock_ring', tor(.16, .014, 6, 24), mats.energy_scan, [0, .04, 0], [90 * D, 0, 0]));
      g.add(part('barricade_dock_post', box(.06, .20, .06), mats.steel_hull, [-.22, .12, -.16]));
    });
    up('recovery', 8, body, [.8, H + .28, -T / 2 - .30], (g) => {
      g.add(part('barricade_drone_body', box(.24, .10, .24), mats.steel_plate));
      for (const s of [-1, 1]) for (const z of [-1, 1])
        g.add(part(`barricade_drone_rotor${s}${z}`, cyl(.09, .09, .012, 14), mats.trim, [s * .16, .06, z * .16]));
      g.add(part('barricade_drone_eye', box(.06, .03, .02), mats.energy_scan, [0, 0, .125]));
    });
    up('recovery', 9, body, [-1.0, .40, -T / 2 - .34], (g) => {
      for (const x of [-.14, .14]) {
        g.add(part('barricade_tank' + x, cyl(.11, .11, .56, 16), mats.steel_plate, [x, 0, 0]));
        g.add(part('barricade_tank_cap' + x, cyl(.05, .05, .06, 10), mats.brass, [x, .31, 0]));
        g.add(part('barricade_tank_strap' + x, tor(.115, .012, 6, 18), mats.trim, [x, .10, 0], [90 * D, 0, 0]));
      }
    });
    up('recovery', 10, body, [0, 0, -T / 2 - .22], (g) => {
      for (const x of [-W / 2 + .1, 0, W / 2 - .1])
        g.add(part('barricade_scaffold_post' + x, cyl(.03, .03, H + .5, 8), mats.brass, [x, (H + .5) / 2 + .07, 0]));
      for (const y of [.60, 1.20, H + .5])
        g.add(part('barricade_scaffold_rail' + y, cyl(.02, .02, W, 8), mats.brass, [0, y, 0], [0, 0, 90 * D]));
      g.add(part('barricade_scaffold_glow', box(W - .3, .02, .02), mats.energy_scan, [0, 1.20, .04]));
    });

    return root;
  },

  /** Brief §3.2 state meshes. Each mutates a freshly built model in place. */
  variants: {
    intact: null,
    damaged(model, K) {
      const { mats, part, box, D } = K;
      const body = model.getObjectByName('barricade_body');
      const hide = (n) => { const o = model.getObjectByName(n); if (o) o.visible = false; };
      hide('barricade_block_cap_l'); hide('barricade_centre_plate'); hide('barricade_centre_bolts');
      const br = model.getObjectByName('barricade_block_r');
      if (br) { br.rotation.z = -5 * D; br.position.y -= .04; }
      [[-1.05, .55, .28, 30], [-.55, .80, .22, -55], [.40, .45, .34, 70], [.95, .75, .18, 15]].forEach(([x, y, l, a], i) =>
        body.add(part('barricade_crack' + i, box(.035, l, .02), mats.trim, [x, y, .29], [0, 0, a * D])));
      body.add(part('barricade_scorch', box(.90, .60, .008), mats.rubber, [-.50, .60, .285]));
      body.add(part('barricade_chip', box(.30, .16, .60), mats.rubber, [-.62, 1.10, 0]));
      const lamp = model.getObjectByName('barricade_status_lamp');
      if (lamp) { lamp.material = lamp.material.clone(); lamp.material.emissiveIntensity = .25; }
    },
    broken(model, K) {
      const { mats, part, box, D, THREE } = K;
      const body = model.getObjectByName('barricade_body');
      const hide = (n) => { const o = model.getObjectByName(n); if (o) o.visible = false; };
      for (const n of ['barricade_block_r', 'barricade_block_cap_r', 'barricade_block_cap_l', 'barricade_post0.3', 'barricade_post1.5',
        'barricade_post_cap0.3', 'barricade_post_cap1.5', 'barricade_centre_plate', 'barricade_centre_bolts', 'barricade_beam']) hide(n);
      const bl = model.getObjectByName('barricade_block_l');
      if (bl) { bl.rotation.z = 9 * D; bl.rotation.x = -4 * D; bl.position.y -= .10; bl.position.x -= .06; }
      const rear = model.getObjectByName('barricade_beam_rear');
      if (rear) { rear.rotation.z = -16 * D; rear.position.y -= .18; rear.position.x += .30; }
      const pl = model.getObjectByName('barricade_post-0.3');
      if (pl) { pl.rotation.z = -28 * D; pl.position.x += .18; pl.position.y -= .12; }
      // Rubble field on the fallen (+X) half, deterministic scatter.
      [[.35, .18, .14, .40, .22, .30, 12], [.80, .10, -.20, .55, .20, .36, -30], [1.20, .12, .18, .34, .24, .28, 48],
       [.62, .38, .05, .30, .18, .26, 70], [1.05, .30, -.10, .26, .16, .22, -12], [1.45, .08, .30, .22, .16, .30, 25],
       [.20, .40, -.16, .20, .14, .18, 40], [.90, .52, .12, .18, .12, .16, -60]].forEach(([x, y, z, w, h, d, a], i) =>
        body.add(part('barricade_rubble' + i, box(w, h, d), mats.concrete, [x, y, z], [0, a * D, (i % 3) * 7 * D])));
      body.add(part('barricade_rebar_a', new THREE.CylinderGeometry(.018, .018, .70, 6), mats.rubber, [.55, .40, .08], [0, 0, 62 * D]));
      body.add(part('barricade_rebar_b', new THREE.CylinderGeometry(.018, .018, .55, 6), mats.rubber, [1.10, .32, -.06], [20 * D, 0, -40 * D]));
      body.add(part('barricade_scorch', box(1.10, .60, .008), mats.rubber, [-.60, .55, .285]));
      const lamp = model.getObjectByName('barricade_status_lamp');
      if (lamp) lamp.material = mats.trim;
    },
  },

  cue(model, levels) {
    for (const x of [-1.5, -.3, .3, 1.5]) {
      const p = model.getObjectByName('barricade_post' + x);
      if (p) p.scale.x = p.scale.z = 1 + ((levels.bulk ?? 1) - 1) / 9 * .5;
    }
    for (const n of ['barricade_nanite_line', 'barricade_nanite_line_b', 'barricade_scaffold_glow', 'barricade_dock_ring']) {
      const m = model.getObjectByName(n);
      if (!m) continue;
      if (!m.userData.own) { m.material = m.material.clone(); m.userData.own = true; }
      m.material.emissiveIntensity = .4 + ((levels.recovery ?? 1) - 1) / 9 * 1.2;
    }
    const lamp = model.getObjectByName('barricade_status_lamp');
    if (lamp) {
      if (!lamp.userData.own) { lamp.material = lamp.material.clone(); lamp.userData.own = true; }
      lamp.material.emissiveIntensity = .6 + ((levels.spite ?? 1) - 1) / 9 * 1.2;
    }
  },
};
