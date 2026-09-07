/**
 * Skywatch — anti-air flak. Air layer only; the vertical-coverage answer.
 * Sim stats: sim/Sim.Core/Content/Towers.cs (cost 90, range 15 m, dmg 4, 3.0/s)
 *
 * Every level 2–10 on every path adds a named attachment; L4 / L7 / L10 carry
 * the breakpoint hardware. Nothing may lower the gun line or hide the dish —
 * this is the one M1 tower that points up.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'skywatch',
  label: 'Skywatch',
  swatch: '#22d3ee',
  stats: { Cost: '90', Range: '15 m', Damage: '4', Rate: '3.0/s' },
  layers: 'Air',
  rig: { yaw: [-180, 180], pitch: [6, 82], traverse: 200, elevate: 140 },
  note: 'Built to hold a strafing Skiff: 200°/s yaw and an 82° ceiling. The radar group spins independently of aim, so the tower reads as "searching" even while the guns are parked. Flak burst spawns at the flash ring.',
  paths: [
    {
      id: 'damage', label: 'Damage', cue: 'Charge strip gains through the band',
      steps: {
        2: 'Muzzle crowns on every tube',
        3: 'Gun-housing armour plate',
        4: 'Proximity-fuse ring at the muzzle (flak burst)',
        5: 'Second barrel clamp',
        6: 'Ammunition-type indicator strip',
        7: 'Second barrel pair — four to six tubes',
        8: 'Recoil dampers on the housing',
        9: 'Fuse programmer box',
        10: 'Volley pods flanking the cradle (full salvo mode)',
      },
    },
    {
      id: 'range', label: 'Range / Tracking', cue: 'Dish grows with acquisition range',
      steps: {
        2: 'IFF beacon lamp on the dish',
        3: 'Upgraded feed horn',
        4: 'Wide search dish',
        5: 'Mast sensor cluster',
        6: 'Dish counterweight',
        7: 'Secondary whip antenna + IFF blade',
        8: 'Mast cable ladder',
        9: 'Elevation encoder disc',
        10: 'Mast riser — the whole gun line lifts a tier',
      },
    },
    {
      id: 'rate', label: 'Rate', cue: 'Barrel cooling rings multiply as heat rises',
      steps: {
        2: 'Belt guides on the drums',
        3: 'Case ejection chute',
        4: 'Oversized ammo drums',
        5: 'Feed motor',
        6: 'Spare belt boxes',
        7: 'Twin feed chutes with belt covers',
        8: 'Barrel shroud vents',
        9: 'Coolant lines to the tubes',
        10: 'Liquid cooling jackets and header tank',
      },
    },
  ],

  build(K, levels) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'skywatch', mats.energy_flak);
    const root = grp('skywatch');
    root.add(K.foot('skywatch', mats.energy_flak));

    const yaw = grp('skywatch_yaw', [0, .33, 0]);
    yaw.add(K.ringGear('skywatch_slew_gear', .56, 30));
    yaw.add(part('skywatch_yaw_drum', cyl(.54, .58, .42, 24), mats.steel_hull, [0, .26, 0]));
    yaw.add(K.boltRing('skywatch_yaw_bolts', .46, 10, .47));
    yaw.add(part('skywatch_mast', cyl(.32, .40, .86, 16), mats.chassis, [0, .91, 0]));
    yaw.add(K.panelPlate('skywatch_mast_hatch', .26, .34, [0, .90, .335]));
    for (let i = 0; i < 4; i++)
      yaw.add(part('skywatch_mast_rung' + i, cyl(.018, .018, .30, 8), mats.chrome, [0, .60 + i * .18, -.36], [0, 0, 90 * D]));
    yaw.add(part('skywatch_cradle', box(.88, .38, .70), mats.chassis, [0, 1.45, -.02]));
    yaw.add(part('skywatch_cradle_deck', box(.92, .05, .74), mats.steel_plate, [0, 1.66, -.02]));
    for (const s of [-1, 1]) {
      const sfx = s < 0 ? '_l' : '_r';
      yaw.add(part('skywatch_ammo_drum' + sfx, cyl(.28, .28, .34, 24), mats.steel_hull, [s * .62, 1.45, -.02], [0, 0, 90 * D]));
      for (let i = 0; i < 3; i++)
        yaw.add(part('skywatch_drum_rib' + sfx + i, tor(.285, .016, 6, 24), mats.trim, [s * (.50 + i * .12), 1.45, -.02], [0, 90 * D, 0]));
      yaw.add(part('skywatch_drum_cap' + sfx, cyl(.13, .13, .38, 12), mats.chrome, [s * .62, 1.45, -.02], [0, 0, 90 * D]));
      yaw.add(part('skywatch_feed_chute' + sfx, box(.30, .13, .16), mats.steel_plate, [s * .38, 1.63, .06], [0, 0, s * 16 * D]));
      yaw.add(part('skywatch_outrigger' + sfx, box(.14, .12, .34), mats.steel_hull, [s * .48, .18, .22]));
    }

    const radar = grp('skywatch_radar', [0, 1.77, -.52]);
    radar.userData.role = 'cosmeticSpin';
    const dishPts = [];
    for (let i = 0; i <= 12; i++) { const t = i / 12; dishPts.push(new THREE.Vector2(t * .52, t * t * .20)); }
    radar.add(part('skywatch_radar_post', cyl(.075, .09, .70, 14), mats.trim, [0, 0, 0]));
    radar.add(part('skywatch_radar_yoke', box(.14, .10, .22), mats.steel_hull, [0, .32, -.02]));
    radar.add(part('skywatch_radar_dish', new THREE.LatheGeometry(dishPts, 36), mats.steel_plate, [0, .38, -.04], [-58 * D, 0, 0]));
    radar.add(part('skywatch_radar_rim', tor(.52, .016, 6, 40), mats.chrome, [0, .38, -.04], [32 * D, 0, 0]));
    radar.add(part('skywatch_radar_spine_a', box(.014, .96, .014), mats.chrome, [0, .38, -.035], [32 * D, 0, 0]));
    radar.add(part('skywatch_radar_spine_b', box(.96, .014, .014), mats.chrome, [0, .38, -.035], [32 * D, 0, 0]));
    for (const s of [-1, 1])
      radar.add(part('skywatch_radar_strut' + (s < 0 ? '_l' : '_r'), cyl(.018, .018, .40, 8), mats.chrome, [s * .12, .22, .06], [-38 * D, 0, s * 14 * D]));
    radar.add(part('skywatch_radar_feed', cyl(.05, .05, .26, 12), mats.energy_flak, [0, .46, .08], [-58 * D, 0, 0]));
    yaw.add(radar);

    const pitch = grp('skywatch_pitch', [0, 1.53, .04]);
    pitch.add(part('skywatch_gun_housing', box(.46, .40, .52), mats.steel_plate, [0, -.10, 0]));
    pitch.add(part('skywatch_gun_saddle', cyl(.22, .22, .44, 20), mats.chassis, [0, .02, .06], [90 * D, 0, 0]));
    const tubes = [];
    let n = 0;
    for (const x of [-.14, .14]) for (const y of [-.14, .14]) {
      const i = n++;
      tubes.push([x, y]);
      pitch.add(part('skywatch_barrel' + i, cyl(.062, .072, 1.30, 14), mats.trim, [x, y, .78], [90 * D, 0, 0]));
      pitch.add(part('skywatch_barrel_jacket' + i, cyl(.085, .085, .34, 14), mats.steel_hull, [x, y, .34], [90 * D, 0, 0]));
      for (let k = 0; k < 3; k++)
        pitch.add(part('skywatch_barrel_ring' + i + '_' + k, tor(.075, .012, 6, 16), mats.chrome, [x, y, .60 + k * .22]));
    }
    pitch.add(part('skywatch_barrel_clamp', box(.42, .42, .09), mats.steel_hull, [0, 0, .95]));
    pitch.add(part('skywatch_flash_ring', tor(.26, .048, 10, 28), mats.steel_plate, [0, 0, 1.40]));
    pitch.add(part('skywatch_flash_vane_a', box(.44, .03, .10), mats.chrome, [0, 0, 1.40]));
    pitch.add(part('skywatch_flash_vane_b', box(.03, .44, .10), mats.chrome, [0, 0, 1.40]));
    pitch.add(part('skywatch_charge_strip', box(.05, .32, .38), mats.energy_flak, [.24, 0, .30]));
    pitch.add(part('skywatch_ammo_feed', box(.16, .12, .30), mats.steel_hull, [-.26, -.06, .10]));
    pitch.add(K.hydraulic('skywatch_elev_ram', [.28, -.30, -.14], [.28, -.24, .46]));
    pitch.add(K.muzzle('skywatch', [0, 0, 1.48]));

    /* ── DAMAGE ladder ──────────────────────────────────────────────── */
    up('damage', 2, pitch, [0, 0, 1.44], (g) => {
      tubes.forEach(([x, y], i) => g.add(part('skywatch_crown' + i, tor(.07, .016, 6, 14), mats.chrome, [x, y, 0])));
    });
    up('damage', 3, pitch, [0, -.10, 0], (g) => {
      g.add(part('skywatch_housing_armour', box(.52, .12, .40), mats.chassis, [0, .22, -.02]));
      g.add(K.boltRing('skywatch_armour_bolts', .14, 6, .285));
    });
    up('damage', 4, pitch, [0, 0, 1.56], (g) => {
      g.add(part('skywatch_fuse_ring', tor(.30, .055, 10, 32), mats.chassis));
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2;
        g.add(part('skywatch_fuse_node' + i, box(.05, .05, .09), mats.energy_flak, [Math.cos(a) * .30, Math.sin(a) * .30, .05], [0, 0, a]));
      }
      g.add(part('skywatch_fuse_collar', cyl(.22, .24, .10, 20), mats.steel_hull, [0, 0, -.06], [90 * D, 0, 0]));
    });
    up('damage', 5, pitch, [0, 0, .56], (g) => {
      g.add(part('skywatch_barrel_clamp_b', box(.40, .40, .08), mats.steel_hull));
      g.add(K.boltRing('skywatch_clamp_b_bolts', .17, 4, 0).rotateX(90 * D).translateZ(-.05));
    });
    up('damage', 6, pitch, [-.24, .06, .18], (g) => {
      g.add(part('skywatch_ammo_indicator', box(.03, .10, .30), mats.trim));
      for (const z of [-.09, 0, .09])
        g.add(part('skywatch_ammo_pip' + z, box(.02, .05, .05), mats.energy_flak, [-.02, 0, z]));
    });
    up('damage', 7, pitch, [0, 0, 0], (g) => {
      for (const x of [-.30, .30]) {
        const tag = x < 0 ? '_l' : '_r';
        g.add(part('skywatch_barrel_b' + tag, cyl(.058, .068, 1.22, 12), mats.trim, [x, 0, .74], [90 * D, 0, 0]));
        g.add(part('skywatch_barrel_b_jacket' + tag, cyl(.08, .08, .30, 12), mats.steel_hull, [x, 0, .32], [90 * D, 0, 0]));
        for (let k = 0; k < 2; k++)
          g.add(part('skywatch_barrel_b_ring' + tag + k, tor(.07, .011, 6, 14), mats.chrome, [x, 0, .66 + k * .26]));
      }
      g.add(part('skywatch_barrel_b_clamp', box(.74, .12, .08), mats.steel_hull, [0, 0, .95]));
      for (const x of [-.30, .30]) g.add(part('skywatch_barrel_b_crown' + x, tor(.065, .012, 6, 14), mats.energy_flak, [x, 0, 1.36]));
    });
    up('damage', 8, pitch, [0, 0, 0], (g) => {
      for (const s of [-1, 1])
        g.add(K.hydraulic('skywatch_damper' + (s < 0 ? '_l' : '_r'), [s * .26, -.26, -.22], [s * .26, .18, .16], .7));
    });
    up('damage', 9, pitch, [.30, -.06, -.16], (g) => {
      g.add(part('skywatch_fuse_programmer', box(.14, .18, .20), mats.chassis));
      g.add(part('skywatch_fuse_dial', cyl(.05, .05, .02, 12), mats.brass, [.075, .02, 0], [0, 0, 90 * D]));
      g.add(part('skywatch_fuse_lead', cyl(.014, .014, .26, 6), mats.rubber, [0, .12, .12], [40 * D, 0, 0]));
    });
    up('damage', 10, pitch, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        const tag = s < 0 ? '_l' : '_r', pod = grp('skywatch_volley_pod' + tag, [s * .46, .18, .18]);
        pod.add(part('skywatch_volley_shell' + tag, box(.26, .30, .60), mats.chassis));
        pod.add(part('skywatch_volley_face' + tag, box(.22, .26, .03), mats.trim, [0, 0, .31]));
        for (let i = 0; i < 4; i++) {
          const cx = (i % 2 ? .055 : -.055), cy = (i < 2 ? .07 : -.07);
          pod.add(part('skywatch_volley_tube' + tag + i, cyl(.042, .042, .10, 10), mats.energy_flak, [cx, cy, .33], [90 * D, 0, 0]));
        }
        pod.add(part('skywatch_volley_mount' + tag, box(.14, .12, .18), mats.steel_hull, [-s * .18, -.06, 0]));
        g.add(pod);
      }
    });

    /* ── RANGE ladder ───────────────────────────────────────────────── */
    up('range', 2, radar, [.40, .62, -.30], (g) => {
      g.add(part('skywatch_iff_lamp', new THREE.SphereGeometry(.035, 10, 8), mats.energy_flak));
      g.add(part('skywatch_iff_lamp_stalk', cyl(.012, .012, .10, 6), mats.chrome, [0, -.06, 0]));
    });
    up('range', 3, radar, [0, .50, .12], (g) => {
      g.add(part('skywatch_feed_horn', cyl(.09, .05, .14, 12), mats.steel_plate, [0, 0, 0], [-58 * D, 0, 0]));
      g.add(part('skywatch_feed_horn_ring', tor(.085, .012, 6, 16), mats.chrome, [0, .04, .06], [32 * D, 0, 0]));
    });
    up('range', 4, radar, [0, .40, -.06], (g) => {
      const widePts = [];
      for (let i = 0; i <= 14; i++) { const t = i / 14; widePts.push(new THREE.Vector2(t * .74, t * t * .24)); }
      g.add(part('skywatch_wide_dish', new THREE.LatheGeometry(widePts, 40), mats.steel_plate, [0, 0, -.02], [-58 * D, 0, 0]));
      g.add(part('skywatch_wide_rim', tor(.74, .018, 6, 44), mats.chrome, [0, 0, -.02], [32 * D, 0, 0]));
      g.add(part('skywatch_wide_feed_arm', cyl(.016, .016, .46, 8), mats.chrome, [0, .10, .16], [-58 * D, 0, 0]));
      g.add(part('skywatch_wide_feed_tip', new THREE.SphereGeometry(.035, 10, 8), mats.energy_flak, [0, .22, .35]));
    });
    up('range', 5, yaw, [.30, 1.12, .22], (g) => {
      g.add(part('skywatch_sensor_cluster', box(.16, .22, .14), mats.chassis));
      g.add(part('skywatch_sensor_eye_a', cyl(.03, .03, .02, 10), mats.optic_glass, [.0, .06, .075], [90 * D, 0, 0]));
      g.add(part('skywatch_sensor_eye_b', cyl(.03, .03, .02, 10), mats.optic_glass, [.0, -.04, .075], [90 * D, 0, 0]));
      g.add(part('skywatch_sensor_mount', box(.08, .08, .12), mats.steel_hull, [0, 0, -.12]));
    });
    up('range', 6, radar, [0, .18, -.30], (g) => {
      g.add(part('skywatch_dish_counterweight', cyl(.09, .09, .22, 14), mats.trim, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('skywatch_dish_cw_arm', box(.05, .05, .24), mats.steel_hull, [0, .08, .12]));
    });
    up('range', 7, radar, [.22, .30, .10], (g) => {
      g.add(part('skywatch_whip', cyl(.012, .016, .84, 8), mats.chrome, [0, .42, 0]));
      g.add(part('skywatch_whip_base', cyl(.05, .06, .07, 10), mats.trim, [0, .04, 0]));
      g.add(part('skywatch_iff_blade', box(.03, .30, .16), mats.steel_plate, [-.44, .34, -.04]));
      g.add(part('skywatch_whip_tip', new THREE.SphereGeometry(.025, 8, 6), mats.energy_flak, [0, .85, 0]));
    });
    up('range', 8, yaw, [.36, .90, -.18], (g) => {
      g.add(part('skywatch_cable_ladder', box(.06, .78, .10), mats.trim));
      for (let i = 0; i < 5; i++)
        g.add(part('skywatch_cable_ladder_rung' + i, box(.08, .02, .12), mats.chrome, [0, -.32 + i * .16, 0]));
      g.add(K.cableRun('skywatch_ladder_loom', [[0, -.40, 0], [.04, -.10, .02], [0, .40, 0]], .02));
    });
    up('range', 9, yaw, [-.48, 1.53, .04], (g) => {
      g.add(part('skywatch_encoder_disc', cyl(.15, .15, .04, 24), mats.brass, [0, 0, 0], [0, 0, 90 * D]));
      for (let i = 0; i < 12; i++) {
        const a = (i / 12) * Math.PI * 2;
        g.add(part('skywatch_encoder_tick' + i, box(.045, .02, .04), mats.trim, [0, Math.cos(a) * .12, Math.sin(a) * .12], [-a, 0, 0]));
      }
      g.add(part('skywatch_encoder_head', box(.06, .08, .08), mats.chassis, [-.04, .16, 0]));
    });
    up('range', 10, yaw, [0, 0, 0], (g) => {
      g.add(part('skywatch_mast_riser', cyl(.30, .34, .46, 16), mats.chassis, [0, 1.55, 0]));
      g.add(K.boltRing('skywatch_riser_bolts', .30, 10, 1.79));
      g.add(part('skywatch_riser_ladder', box(.20, .44, .03), mats.chrome, [0, 1.55, -.34]));
      g.add(part('skywatch_riser_band', tor(.33, .014, 6, 32), mats.energy_flak, [0, 1.55, 0], [90 * D, 0, 0]));
    });

    /* ── RATE ladder ────────────────────────────────────────────────── */
    up('rate', 2, yaw, [0, 1.45, -.02], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('skywatch_belt_guide' + (s < 0 ? '_l' : '_r'), box(.22, .05, .20), mats.steel_plate, [s * .46, .30, .02]));
        g.add(part('skywatch_belt_guide_roller' + (s < 0 ? '_l' : '_r'), cyl(.03, .03, .20, 10), mats.chrome, [s * .36, .27, .02], [90 * D, 0, 0]));
      }
    });
    up('rate', 3, pitch, [.30, -.20, -.02], (g) => {
      g.add(part('skywatch_eject_chute', box(.10, .08, .26), mats.chassis, [0, 0, 0], [0, 0, -28 * D]));
      g.add(part('skywatch_eject_lip', box(.12, .02, .10), mats.chrome, [.08, -.08, .10]));
    });
    up('rate', 4, yaw, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        const tag = s < 0 ? '_l' : '_r';
        g.add(part('skywatch_big_drum' + tag, cyl(.36, .36, .40, 24), mats.steel_hull, [s * .74, 1.45, -.02], [0, 0, 90 * D]));
        g.add(part('skywatch_big_drum_cap' + tag, cyl(.15, .15, .44, 12), mats.chrome, [s * .74, 1.45, -.02], [0, 0, 90 * D]));
        g.add(part('skywatch_big_drum_rib' + tag, tor(.365, .018, 6, 26), mats.trim, [s * .74, 1.45, -.02], [0, 90 * D, 0]));
        g.add(part('skywatch_big_drum_gauge' + tag, box(.02, .16, .04), mats.energy_flak, [s * .945, 1.45, -.02]));
      }
    });
    up('rate', 5, yaw, [0, 1.74, -.34], (g) => {
      g.add(part('skywatch_feed_motor', cyl(.10, .10, .22, 16), mats.chassis, [0, 0, 0], [0, 0, 90 * D]));
      g.add(part('skywatch_feed_motor_cap', cyl(.06, .06, .26, 10), mats.brass, [0, 0, 0], [0, 0, 90 * D]));
      g.add(part('skywatch_feed_motor_mount', box(.14, .08, .16), mats.steel_hull, [0, -.10, .04]));
    });
    up('rate', 6, yaw, [0, 1.24, .30], (g) => {
      for (const x of [-.24, .24]) {
        g.add(part('skywatch_belt_box' + x, box(.26, .16, .12), mats.chassis, [x, 0, 0]));
        g.add(part('skywatch_belt_box_latch' + x, box(.06, .04, .02), mats.chrome, [x, 0, .07]));
      }
    });
    up('rate', 7, yaw, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        const tag = s < 0 ? '_l' : '_r';
        g.add(part('skywatch_feed_cover' + tag, box(.34, .16, .20), mats.chassis, [s * .40, 1.74, .08], [0, 0, s * 16 * D]));
        g.add(part('skywatch_feed_belt' + tag, box(.30, .05, .13), mats.brass, [s * .40, 1.74, .16], [0, 0, s * 16 * D]));
        g.add(part('skywatch_feed_sprocket' + tag, cyl(.08, .08, .14, 10), mats.chrome, [s * .22, 1.70, .08], [0, 0, 90 * D]));
        g.add(part('skywatch_feed_lamp' + tag, box(.06, .02, .03), mats.energy_flak, [s * .40, 1.83, .08], [0, 0, s * 16 * D]));
      }
    });
    up('rate', 8, pitch, [0, 0, .34], (g) => {
      tubes.forEach(([x, y], i) => {
        for (const k of [-.08, .08])
          g.add(part('skywatch_shroud_vent' + i + k, box(.03, .06, .05), mats.trim, [x + Math.sign(x) * .085, y, k]));
      });
    });
    up('rate', 9, pitch, [0, 0, 0], (g) => {
      tubes.forEach(([x, y], i) =>
        g.add(part('skywatch_cool_line' + i, cyl(.018, .018, .30, 8), mats.brass, [x * 1.5, y * 1.5, .30], [90 * D, 0, 0])));
      g.add(part('skywatch_cool_manifold', tor(.24, .02, 6, 24), mats.brass, [0, 0, .16]));
    });
    up('rate', 10, pitch, [0, 0, 0], (g) => {
      tubes.forEach(([x, y], i) =>
        g.add(part('skywatch_cool_jacket' + i, cyl(.098, .098, .62, 14), mats.steel_plate, [x, y, .74], [90 * D, 0, 0])));
      g.add(part('skywatch_cool_tank', cyl(.13, .13, .40, 16), mats.brass, [-.30, .06, -.02], [90 * D, 0, 0]));
      g.add(part('skywatch_cool_pump', box(.16, .14, .16), mats.trim, [-.30, -.14, .16]));
      g.add(part('skywatch_cool_tank_glow', tor(.135, .012, 6, 20), mats.energy_flak, [-.30, .06, .10]));
    });

    yaw.add(pitch);
    root.add(yaw);
    return root;
  },

  cue(model, levels) {
    const strip = model.getObjectByName('skywatch_charge_strip');
    if (strip) {
      if (!strip.userData.own) { strip.material = strip.material.clone(); strip.userData.own = true; }
      strip.material.emissiveIntensity = .5 + ((levels.damage ?? 1) - 1) / 9 * 1.5;
    }
    const dish = model.getObjectByName('skywatch_radar_dish');
    if (dish) {
      const s = .9 + ((levels.range ?? 1) - 1) / 9 * .35;
      dish.scale.set(s, 1, s);
    }
    const rings = Math.min(3, 1 + Math.floor(((levels.rate ?? 1) - 1) / 4));
    for (let i = 0; i < 4; i++)
      for (let k = 0; k < 3; k++) {
        const r = model.getObjectByName('skywatch_barrel_ring' + i + '_' + k);
        if (r) r.visible = k < rings;
      }
  },
};
