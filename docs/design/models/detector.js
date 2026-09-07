/**
 * Detector — reveals stealth (Shade) and, at L4, burrowed Moles. Unlocks with
 * Shade at M3. Not yet typed in the sim; stats are plan intent.
 *
 * No aim rig. `detector_spin` is the rotating sensor head — the client speeds
 * it up while a reveal is active.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'detector',
  label: 'Detector',
  swatch: '#7fe65a',
  stats: { Milestone: 'M3', Kind: 'Detection aura', Applies: 'reveal · mark (L7)', Sim: 'not yet' },
  layers: 'Ground + Air (+ burrow L4)',
  rig: null,
  note: 'Support silhouette: a rotating sensor head on a plain column, no barrel anywhere. Green is reserved for detection across the roster so a revealed enemy and the thing revealing it share a colour. Fog and Night both exempt this tower — it must read at range in both.',
  paths: [
    {
      id: 'field', label: 'Field', cue: 'Sensor slits glow wider',
      steps: {
        2: 'Antenna stubs on the head',
        3: 'Base sensor ring',
        4: 'Ground seismic spikes (reveals burrowed Moles)',
        5: 'Relay box on the column',
        6: 'Second antenna tier',
        7: 'Wide array wings',
        8: 'Wing struts',
        9: 'Field coils on the column',
        10: 'Pulse ring (map-wide pulse on wave start)',
      },
    },
    {
      id: 'analysis', label: 'Analysis', cue: 'Analysis lens brightens',
      steps: {
        2: 'Data recorder box',
        3: 'Analysis lens on the head',
        4: 'Mark projector (revealed enemies take vulnerability)',
        5: 'Cooling fins on the recorder',
        6: 'Second mark projector',
        7: 'Analysis sensor cluster',
        8: 'Cabling harness',
        9: 'Uplink dish',
        10: 'Analysis crown',
      },
    },
  ],

  build(K) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'detector', mats.energy_scan);
    const root = grp('detector');
    root.add(K.foot('detector', mats.energy_scan));
    root.add(part('detector_column', cyl(.30, .40, 1.40, 20), mats.chassis, [0, 1.06, 0]));
    root.add(K.panelPlate('detector_column_hatch', .22, .40, [0, .90, .34]));
    root.add(K.boltRing('detector_column_bolts', .34, 10, .40));
    root.add(part('detector_column_collar', cyl(.36, .36, .08, 20), mats.steel_plate, [0, 1.78, 0]));
    root.add(part('detector_bearing', cyl(.26, .30, .12, 24), mats.brass, [0, 1.86, 0]));
    root.add(K.cableRun('detector_loom', [[.26, .30, -.24], [.34, .80, -.26], [.28, 1.40, -.16]], .024));
    root.add(part('detector_junction_b', box(.20, .24, .14), mats.steel_hull, [.36, .60, -.20]));

    const spin = grp('detector_spin', [0, 2.06, 0]);
    spin.userData.role = 'cosmeticSpin';
    spin.add(part('detector_head', cyl(.48, .44, .30, 24), mats.steel_hull));
    spin.add(part('detector_head_cap', cyl(.40, .48, .06, 24), mats.steel_plate, [0, .18, 0]));
    spin.add(part('detector_head_base', cyl(.44, .36, .06, 24), mats.steel_plate, [0, -.18, 0]));
    for (let i = 0; i < 8; i++) {
      const a = (i / 8) * Math.PI * 2;
      spin.add(part('detector_slit' + i, box(.02, .16, .10), mats.energy_scan, [Math.cos(a) * .47, 0, Math.sin(a) * .47], [0, -a, 0]));
    }
    spin.add(part('detector_eye_housing', box(.26, .18, .16), mats.chassis, [0, 0, .50]));
    spin.add(part('detector_eye', cyl(.06, .06, .03, 16), mats.optic_glass, [0, 0, .59], [90 * D, 0, 0]));
    spin.add(part('detector_eye_ring', tor(.07, .01, 6, 20), mats.energy_scan, [0, 0, .59]));
    spin.add(part('detector_mast', cyl(.03, .04, .40, 10), mats.chrome, [0, .40, 0]));
    spin.add(part('detector_mast_tip', new THREE.SphereGeometry(.05, 12, 8), mats.energy_scan, [0, .62, 0]));
    root.add(spin);

    /* ── FIELD ladder ───────────────────────────────────────────────── */
    up('field', 2, spin, [0, .22, 0], (g) => {
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D;
        g.add(part('detector_stub' + i, cyl(.012, .012, .24, 6), mats.chrome, [Math.cos(a) * .34, .12, Math.sin(a) * .34]));
      }
    });
    up('field', 3, root, [0, .42, 0], (g) => {
      g.add(part('detector_base_ring', tor(.56, .03, 8, 40), mats.steel_hull, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2;
        g.add(part('detector_base_node' + i, box(.08, .06, .08), mats.energy_scan, [Math.cos(a) * .56, 0, Math.sin(a) * .56]));
      }
    });
    up('field', 4, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2 + 15 * D, x = Math.cos(a) * 1.0, z = Math.sin(a) * 1.0;
        g.add(part('detector_seismic_spike' + i, new THREE.ConeGeometry(.05, .30, 8), mats.chrome, [x, .15, z], [Math.PI, 0, 0]));
        g.add(part('detector_seismic_cap' + i, cyl(.06, .06, .04, 10), mats.energy_scan, [x, .32, z]));
        g.add(K.cableRun('detector_seismic_line' + i, [[x * .9, .20, z * .9], [x * .75, .06, z * .75], [x * .6, .10, z * .6]], .012));
      }
    });
    up('field', 5, root, [-.38, 1.20, .12], (g) => {
      g.add(part('detector_relay', box(.16, .22, .14), mats.chassis));
      g.add(part('detector_relay_pip', box(.02, .10, .03), mats.energy_scan, [-.09, 0, 0]));
    });
    up('field', 6, spin, [0, .22, 0], (g) => {
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2;
        g.add(part('detector_stub_b' + i, cyl(.012, .012, .34, 6), mats.chrome, [Math.cos(a) * .26, .17, Math.sin(a) * .26]));
        g.add(part('detector_stub_b_tip' + i, new THREE.SphereGeometry(.02, 8, 6), mats.energy_scan, [Math.cos(a) * .26, .35, Math.sin(a) * .26]));
      }
    });
    up('field', 7, spin, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('detector_wing' + (s < 0 ? '_l' : '_r'), box(.60, .22, .04), mats.steel_plate, [s * .78, 0, -.10]));
        g.add(part('detector_wing_glow' + (s < 0 ? '_l' : '_r'), box(.50, .03, .01), mats.energy_scan, [s * .78, 0, -.075]));
      }
    });
    up('field', 8, spin, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('detector_wing_strut' + (s < 0 ? '_l' : '_r'), box(.34, .03, .03), mats.chrome, [s * .62, .12, -.02], [0, 0, s * -14 * D]));
        g.add(part('detector_wing_strut_b' + (s < 0 ? '_l' : '_r'), box(.34, .03, .03), mats.chrome, [s * .62, -.12, -.02], [0, 0, s * 14 * D]));
      }
    });
    up('field', 9, root, [0, 0, 0], (g) => {
      for (const y of [.70, 1.00, 1.30])
        g.add(part('detector_field_coil' + y, tor(.38, .022, 8, 32), mats.brass, [0, y, 0], [90 * D, 0, 0]));
      g.add(part('detector_field_coil_glow', tor(.40, .01, 6, 32), mats.energy_scan, [0, 1.00, 0], [90 * D, 0, 0]));
    });
    up('field', 10, spin, [0, -.30, 0], (g) => {
      g.add(part('detector_pulse_ring', tor(.66, .04, 10, 48), mats.chassis, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('detector_pulse_glow', tor(.66, .016, 8, 48), mats.energy_scan, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D;
        g.add(part('detector_pulse_strut' + i, box(.04, .04, .28), mats.steel_hull, [Math.cos(a) * .52, .06, Math.sin(a) * .52], [0, -a + 90 * D, 0]));
      }
    });

    /* ── ANALYSIS ladder ────────────────────────────────────────────── */
    up('analysis', 2, root, [.40, 1.44, .08], (g) => {
      g.add(part('detector_recorder', box(.22, .26, .18), mats.steel_hull));
      g.add(part('detector_recorder_face', box(.12, .06, .015), mats.energy_scan, [0, .04, .095]));
    });
    up('analysis', 3, spin, [0, .08, .50], (g) => {
      g.add(part('detector_analysis_lens', cyl(.045, .045, .03, 14), mats.optic_glass, [0, .10, .08], [90 * D, 0, 0]));
      g.add(part('detector_analysis_lens_ring', tor(.05, .008, 6, 16), mats.chrome, [0, .10, .085]));
    });
    up('analysis', 4, spin, [-.30, -.02, .38], (g) => {
      g.add(part('detector_mark_projector', box(.12, .12, .22), mats.chassis));
      g.add(part('detector_mark_lens', cyl(.035, .035, .02, 12), mats.energy_fuse, [0, 0, .12], [90 * D, 0, 0]));
    });
    up('analysis', 5, root, [.40, 1.44, .08], (g) => {
      g.add(K.finStack('detector_recorder_fins', 5, .04, .04, .22, .01).translateZ(-.08).translateX(.14));
    });
    up('analysis', 6, spin, [.30, -.02, .38], (g) => {
      g.add(part('detector_mark_projector_b', box(.12, .12, .22), mats.chassis));
      g.add(part('detector_mark_lens_b', cyl(.035, .035, .02, 12), mats.energy_fuse, [0, 0, .12], [90 * D, 0, 0]));
    });
    up('analysis', 7, spin, [0, .26, -.30], (g) => {
      g.add(part('detector_cluster', box(.30, .14, .20), mats.steel_plate));
      for (const x of [-.09, 0, .09])
        g.add(part('detector_cluster_eye' + x, cyl(.03, .03, .02, 10), mats.optic_glass, [x, 0, .11], [90 * D, 0, 0]));
      g.add(part('detector_cluster_bar', box(.26, .015, .01), mats.energy_scan, [0, .06, .105]));
    });
    up('analysis', 8, root, [0, 0, 0], (g) => {
      g.add(K.cableRun('detector_harness_a', [[.40, 1.30, .08], [.20, 1.60, .20], [0, 1.80, .10]], .016));
      g.add(K.cableRun('detector_harness_b', [[.50, 1.44, .08], [.46, 1.10, .10], [.36, .72, -.14]], .016));
    });
    up('analysis', 9, spin, [-.30, .30, -.20], (g) => {
      const pts = [];
      for (let i = 0; i <= 8; i++) { const t = i / 8; pts.push(new THREE.Vector2(t * .18, t * t * .07)); }
      g.add(part('detector_uplink_dish', new THREE.LatheGeometry(pts, 20), mats.steel_plate, [0, .10, 0], [-40 * D, 0, 0]));
      g.add(part('detector_uplink_post', cyl(.02, .02, .16, 8), mats.chrome));
      g.add(part('detector_uplink_feed', cyl(.012, .012, .10, 6), mats.energy_scan, [0, .14, .06], [-40 * D, 0, 0]));
    });
    up('analysis', 10, spin, [0, .30, 0], (g) => {
      g.add(part('detector_crown_ring', tor(.30, .018, 8, 36), mats.brass, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        g.add(part('detector_crown_pin' + i, cyl(.012, .012, .14, 6), mats.brass, [Math.cos(a) * .30, .07, Math.sin(a) * .30]));
        g.add(part('detector_crown_tip' + i, new THREE.SphereGeometry(.02, 8, 6), mats.energy_scan, [Math.cos(a) * .30, .15, Math.sin(a) * .30]));
      }
    });

    return root;
  },

  cue(model, levels) {
    for (let i = 0; i < 8; i++) {
      const s = model.getObjectByName('detector_slit' + i);
      if (s) s.scale.y = .8 + ((levels.field ?? 1) - 1) / 9 * .8;
    }
    const eye = model.getObjectByName('detector_eye_ring');
    if (eye) {
      if (!eye.userData.own) { eye.material = eye.material.clone(); eye.userData.own = true; }
      eye.material.emissiveIntensity = .5 + ((levels.analysis ?? 1) - 1) / 9 * 1.5;
    }
  },
};
