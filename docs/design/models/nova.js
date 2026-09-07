/**
 * Nova — arcing mortar. Ground only, minimum range, the clump answer.
 * Sim stats: sim/Sim.Core/Content/Towers.cs (cost 115, range 16 m, dmg 22, 0.5/s)
 *
 * Every level 2–10 on every path adds a named attachment; L4 / L7 / L10 carry
 * the breakpoint hardware. Nothing bolted on may make the barrel look like it
 * can depress onto a close target — the 22° floor is the min-range rule.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'nova',
  label: 'Nova',
  swatch: '#f0c83a',
  stats: { Cost: '115', Range: '16 m', Damage: '22', Rate: '0.5/s' },
  layers: 'Ground',
  rig: { yaw: [-180, 180], pitch: [22, 70], traverse: 45, elevate: 30 },
  note: 'Pitch floor of 22° IS the 5 m min-range rule in geometry — the barrel physically cannot depress onto a close target, so the refusal reads from outside the tower. Shell spawns at the brake; the arc is sim-side.',
  paths: [
    {
      id: 'damage', label: 'Damage / Blast', cue: 'Barrel jacket thickens with the band',
      steps: {
        2: 'Tube reinforcement bands',
        3: 'Breech pressure ring',
        4: 'Napalm canister pair on the cradle (ground burn pools)',
        5: 'Armoured brake shroud',
        6: 'Napalm feed valves',
        7: 'Cluster bomblet hopper above the breech',
        8: 'Jacket bolt collar',
        9: 'Heavy-shell rack on the cradle',
        10: 'Second tube — double-shell salvo',
      },
    },
    {
      id: 'range', label: 'Range', cue: 'Muzzle brake creeps forward as the tube extends',
      steps: {
        2: 'Elevation quadrant plate',
        3: 'Recuperator extensions',
        4: 'Tube extension + counterweight',
        5: 'Muzzle wind vane',
        6: 'Cradle spotter optic',
        7: 'Spotter mast with range plate',
        8: 'Turret ballast blocks',
        9: 'Elevation arc gear',
        10: 'Stabiliser outriggers deploy from the foot',
      },
    },
    {
      id: 'rate', label: 'Rate / Logistics', cue: 'Ready rounds fill the hopper level by level',
      steps: {
        2: 'Hoist rail over the hopper',
        3: 'Fuse-ready indicator lamps',
        4: 'Six-round hopper (up from three)',
        5: 'Breech auto-ejector chute',
        6: 'Second hoist chute',
        7: 'Powered hoist arm over the breech',
        8: 'Ammunition conveyor belt',
        9: 'Hydraulic loader ram',
        10: 'Revolver magazine drum on the turret flank',
      },
    },
  ],

  build(K, levels) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'nova', mats.energy_fuse);
    const root = grp('nova');
    root.add(K.foot('nova', mats.energy_fuse));

    const yaw = grp('nova_yaw', [0, .33, 0]);
    yaw.add(K.ringGear('nova_slew_gear', .70, 36));
    yaw.add(part('nova_turret_drum', cyl(.80, .88, .40, 24), mats.chassis, [0, .24, 0]));
    yaw.add(K.boltRing('nova_turret_bolts', .74, 14, .45));
    yaw.add(part('nova_turret_lip', cyl(.76, .82, .09, 24), mats.steel_hull, [0, .49, 0]));
    yaw.add(K.louvres('nova_turret_vent', .40, .26, 5).translateZ(-.80).translateY(.24));
    yaw.add(K.panelPlate('nova_turret_hatch', .38, .24, [0, .24, .805]));
    yaw.add(part('nova_blast_shield', box(1.06, .60, .05), mats.steel_plate, [0, 1.00, .32], [-14 * D, 0, 0]));
    yaw.add(K.hazardStripes('nova_shield_hazard', .96, .15, [0, .76, .40], [-14 * D, 0, 0], 5));
    for (const s of [-1, 1]) {
      const sfx = s < 0 ? '_l' : '_r';
      yaw.add(part('nova_cradle' + sfx, box(.13, .96, .74), mats.steel_hull, [s * .48, .97, -.16]));
      yaw.add(part('nova_cradle_rib' + sfx, box(.16, .18, .60), mats.chassis, [s * .48, .60, -.16]));
      yaw.add(part('nova_elevation_hub' + sfx, cyl(.19, .19, .10, 20), mats.brass, [s * .55, .96, -.10], [0, 0, 90 * D]));
      yaw.add(part('nova_elevation_nut' + sfx, cyl(.06, .06, .12, 6), mats.chrome, [s * .62, .96, -.10], [0, 0, 90 * D]));
    }
    const rack = grp('nova_shell_rack', [-.86, .52, .30]);
    rack.add(part('nova_hopper', box(.32, .54, .90), mats.steel_hull));
    rack.add(part('nova_hoist_chute', box(.20, .16, .62), mats.chassis, [.16, .50, -.10], [0, 0, -22 * D]));
    rack.add(K.hazardStripes('nova_hopper_hazard', .60, .14, [-.17, .10, 0], [0, -90 * D, 0], 4));
    [-.28, 0, .28].forEach((z, i) => {
      rack.add(part('nova_shell' + i, cyl(.11, .11, .40, 16), mats.brass, [0, .44, z]));
      rack.add(part('nova_shell_band' + i, tor(.113, .014, 6, 18), mats.chrome, [0, .30, z], [90 * D, 0, 0]));
      rack.add(part('nova_shell_fuse' + i, cyl(.03, .09, .16, 12), mats.energy_fuse, [0, .70, z]));
    });
    yaw.add(rack);

    const pitch = grp('nova_pitch', [0, 1.29, -.10]);
    pitch.add(part('nova_barrel', cyl(.25, .29, 1.62, 28), mats.steel_hull, [0, 0, .72], [90 * D, 0, 0]));
    pitch.add(part('nova_barrel_jacket', cyl(.31, .31, .40, 24), mats.chassis, [0, 0, .28], [90 * D, 0, 0]));
    pitch.add(K.finStack('nova_barrel_fins', 4, .07, .54, .28).translateZ(-.04));
    pitch.add(part('nova_breech_ring', cyl(.36, .36, .16, 24), mats.steel_plate, [0, 0, -.16], [90 * D, 0, 0]));
    for (let i = 0; i < 6; i++) {
      const a = (i / 6) * Math.PI * 2;
      pitch.add(part('nova_breech_lug' + i, box(.09, .07, .10), mats.chrome, [Math.cos(a) * .36, Math.sin(a) * .36, -.16], [0, 0, a]));
    }
    pitch.add(part('nova_breech_block', cyl(.34, .34, .34, 24), mats.chassis, [0, 0, -.38], [90 * D, 0, 0]));
    pitch.add(part('nova_breech_lever', box(.06, .22, .06), mats.chrome, [.30, .14, -.38], [0, 0, -24 * D]));
    pitch.add(part('nova_breech_fuse', box(.15, .15, .10), mats.energy_fuse, [0, 0, -.56]));
    for (const s of [-1, 1]) {
      const sfx = s < 0 ? '_l' : '_r';
      pitch.add(part('nova_recuperator' + sfx, cyl(.10, .10, 1.00, 16), mats.steel_plate, [s * .30, -.18, .42], [90 * D, 0, 0]));
      pitch.add(part('nova_recuperator_gland' + sfx, cyl(.11, .11, .08, 16), mats.brass, [s * .30, -.18, .94], [90 * D, 0, 0]));
    }
    pitch.add(K.hydraulic('nova_recoil_ram', [0, -.34, -.30], [0, -.30, .70], 1.2));
    pitch.add(part('nova_muzzle_brake', cyl(.35, .35, .28, 24), mats.steel_plate, [0, 0, 1.62], [90 * D, 0, 0]));
    [-.26, 0, .26].forEach((x, i) => pitch.add(part('nova_brake_vent' + i, box(.05, .36, .30), mats.trim, [x, 0, 1.62])));
    pitch.add(part('nova_brake_lip', tor(.32, .026, 8, 28), mats.chrome, [0, 0, 1.76]));
    pitch.add(part('nova_bore', cyl(.20, .20, .05, 20), mats.trim, [0, 0, 1.79], [90 * D, 0, 0]));
    pitch.add(K.muzzle('nova', [0, 0, 1.80]));

    /* ── DAMAGE ladder ──────────────────────────────────────────────── */
    up('damage', 2, pitch, [0, 0, 0], (g) => {
      for (const z of [.62, 1.02, 1.36])
        g.add(part('nova_tube_band' + z, tor(.285, .02, 8, 28), mats.chrome, [0, 0, z]));
    });
    up('damage', 3, pitch, [0, 0, -.06], (g) => {
      g.add(part('nova_pressure_ring', tor(.36, .035, 8, 32), mats.brass));
    });
    up('damage', 4, pitch, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        g.add(part('nova_napalm_can' + sfx, cyl(.16, .16, .56, 18), mats.hazard, [s * .40, -.34, .18], [90 * D, 0, 0]));
        g.add(part('nova_napalm_cap' + sfx, cyl(.17, .17, .06, 18), mats.trim, [s * .40, -.34, .48], [90 * D, 0, 0]));
        g.add(part('nova_napalm_strap' + sfx, tor(.165, .018, 6, 20), mats.chrome, [s * .40, -.34, .10]));
        g.add(part('nova_napalm_line' + sfx, cyl(.022, .022, .34, 8), mats.brass, [s * .40, -.20, .52], [50 * D, 0, 0]));
        g.add(part('nova_napalm_gauge' + sfx, box(.03, .14, .02), mats.energy_fuse, [s * .565, -.34, .18]));
      }
    });
    up('damage', 5, pitch, [0, 0, 1.62], (g) => {
      g.add(part('nova_brake_shroud', new THREE.CylinderGeometry(.40, .40, .30, 24, 1, false, -Math.PI * .75, Math.PI * 1.5), mats.chassis, [0, 0, 0], [90 * D, 0, 0]));
      g.add(K.boltRing('nova_brake_shroud_bolts', .40, 8, 0).rotateX(90 * D).translateZ(-.12));
    });
    up('damage', 6, pitch, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('nova_napalm_valve' + (s < 0 ? '_l' : '_r'), box(.08, .08, .10), mats.brass, [s * .40, -.16, .72]));
        g.add(part('nova_napalm_wheel' + (s < 0 ? '_l' : '_r'), tor(.05, .012, 6, 14), mats.chrome, [s * .46, -.16, .72], [0, 90 * D, 0]));
      }
    });
    up('damage', 7, pitch, [0, .34, -.30], (g) => {
      g.add(part('nova_cluster_hopper', box(.46, .30, .52), mats.chassis));
      g.add(K.panelPlate('nova_cluster_hatch', .30, .18, [0, .16, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 3; i++)
        g.add(part('nova_bomblet' + i, new THREE.SphereGeometry(.075, 14, 10), mats.brass, [-.13 + i * .13, .22, 0]));
      g.add(part('nova_cluster_feed', box(.14, .14, .30), mats.steel_hull, [0, -.10, .34]));
      g.add(part('nova_cluster_arm_lamp', box(.30, .02, .02), mats.energy_fuse, [0, .16, .27]));
    });
    up('damage', 8, pitch, [0, 0, .28], (g) => {
      g.add(part('nova_jacket_collar', cyl(.335, .335, .10, 24), mats.steel_plate, [0, 0, .20], [90 * D, 0, 0]));
      g.add(K.boltRing('nova_jacket_bolts', .33, 10, 0).rotateX(90 * D).translateZ(-.26));
    });
    up('damage', 9, yaw, [.78, .70, .34], (g) => {
      g.add(part('nova_heavy_rack', box(.26, .12, .62), mats.steel_hull, [0, -.10, 0]));
      for (const z of [-.16, .16]) {
        g.add(part('nova_heavy_shell' + z, cyl(.14, .14, .46, 16), mats.brass, [0, .12, z]));
        g.add(part('nova_heavy_shell_fuse' + z, cyl(.04, .11, .18, 12), mats.energy_fuse, [0, .44, z]));
      }
    });
    up('damage', 10, pitch, [0, -.44, 0], (g) => {
      g.add(part('nova_barrel_b', cyl(.22, .26, 1.50, 24), mats.steel_hull, [0, 0, .66], [90 * D, 0, 0]));
      g.add(part('nova_barrel_b_jacket', cyl(.28, .28, .34, 20), mats.chassis, [0, 0, .26], [90 * D, 0, 0]));
      g.add(part('nova_barrel_b_brake', cyl(.31, .31, .24, 20), mats.steel_plate, [0, 0, 1.50], [90 * D, 0, 0]));
      g.add(part('nova_barrel_b_bore', cyl(.17, .17, .05, 16), mats.trim, [0, 0, 1.64], [90 * D, 0, 0]));
      g.add(part('nova_barrel_b_yoke', box(.30, .40, .18), mats.steel_hull, [0, .22, -.20]));
      g.add(part('nova_barrel_b_glow', tor(.285, .016, 6, 24), mats.energy_fuse, [0, 0, .46]));
    });

    /* ── RANGE ladder ───────────────────────────────────────────────── */
    up('range', 2, yaw, [.56, 1.20, -.10], (g) => {
      g.add(part('nova_quadrant_plate', new THREE.CylinderGeometry(.26, .26, .012, 20, 1, false, 0, Math.PI * .6), mats.hazard, [0, 0, 0], [0, 0, 90 * D]));
      g.add(part('nova_quadrant_pointer', box(.03, .24, .03), mats.chrome, [.02, .12, 0]));
    });
    up('range', 3, pitch, [0, -.18, 1.02], (g) => {
      for (const s of [-1, 1])
        g.add(part('nova_recup_ext' + (s < 0 ? '_l' : '_r'), cyl(.08, .08, .34, 14), mats.chrome, [s * .30, 0, 0], [90 * D, 0, 0]));
    });
    up('range', 4, pitch, [0, 0, 0], (g) => {
      g.add(part('nova_tube_extension', cyl(.245, .25, .46, 24), mats.steel_hull, [0, 0, 1.68], [90 * D, 0, 0]));
      g.add(part('nova_tube_collar', tor(.26, .028, 8, 24), mats.chrome, [0, 0, 1.50]));
      g.add(part('nova_counterweight', cyl(.24, .24, .26, 16), mats.trim, [0, 0, -.72], [90 * D, 0, 0]));
      g.add(part('nova_tube_ext_glow', tor(.255, .014, 6, 24), mats.energy_fuse, [0, 0, 1.90]));
    });
    up('range', 5, pitch, [0, .38, 1.66], (g) => {
      g.add(part('nova_wind_vane_post', cyl(.015, .015, .16, 6), mats.chrome, [0, -.06, 0]));
      g.add(part('nova_wind_vane', box(.02, .10, .18), mats.steel_plate, [0, .04, -.04]));
    });
    up('range', 6, yaw, [.50, 1.40, .10], (g) => {
      g.add(part('nova_cradle_optic', box(.14, .12, .22), mats.chassis));
      g.add(part('nova_cradle_optic_lens', box(.09, .07, .02), mats.optic_glass, [0, 0, .12]));
    });
    up('range', 7, yaw, [-.62, 1.06, -.34], (g) => {
      g.add(part('nova_spotter_mast', cyl(.05, .06, .86, 12), mats.steel_hull, [0, .42, 0]));
      g.add(part('nova_spotter_head', box(.24, .18, .26), mats.chassis, [0, .92, 0]));
      g.add(part('nova_spotter_lens', box(.16, .11, .02), mats.optic_glass, [0, .92, .14]));
      g.add(part('nova_range_plate', box(.28, .20, .014), mats.hazard, [0, .60, .05], [0, 0, 8 * D]));
      g.add(part('nova_spotter_lamp', cyl(.025, .025, .02, 8), mats.energy_fuse, [.10, 1.02, .14], [90 * D, 0, 0]));
    });
    up('range', 8, yaw, [0, .70, 0], (g) => {
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D;
        g.add(part('nova_ballast' + i, box(.28, .20, .14), mats.trim, [Math.cos(a) * .80, 0, Math.sin(a) * .80], [0, -a + 90 * D, 0]));
      }
    });
    up('range', 9, yaw, [-.60, .96, -.10], (g) => {
      g.add(part('nova_arc_gear', new THREE.CylinderGeometry(.34, .34, .05, 24, 1, false, Math.PI * .1, Math.PI * .8), mats.brass, [0, 0, 0], [0, 0, 90 * D]));
      for (let i = 0; i < 7; i++) {
        const a = Math.PI * .1 + i * (Math.PI * .8 / 6);
        g.add(part('nova_arc_tooth' + i, box(.05, .05, .05), mats.brass, [0, Math.cos(a) * .36, Math.sin(a) * .36]));
      }
    });
    up('range', 10, root, [0, 0, 0], (g) => g.add(K.outriggers('nova_outrigger', .56)));

    /* ── RATE ladder ────────────────────────────────────────────────── */
    up('rate', 2, yaw, [-.86, 1.14, .30], (g) => {
      g.add(part('nova_hoist_rail', box(.06, .05, 1.00), mats.chrome));
      for (const z of [-.46, .46])
        g.add(part('nova_hoist_rail_post' + z, box(.05, .32, .05), mats.steel_hull, [0, -.16, z]));
    });
    up('rate', 3, yaw, [-1.03, .60, .30], (g) => {
      for (const z of [-.28, 0, .28])
        g.add(part('nova_ready_lamp' + z, cyl(.03, .03, .03, 8), mats.energy_fuse, [0, 0, z], [0, 0, 90 * D]));
    });
    up('rate', 4, yaw, [-.86, .52, .30], (g) => {
      g.add(part('nova_hopper_ext', box(.32, .30, .90), mats.steel_hull, [0, .78, 0]));
      [-.28, 0, .28].forEach((z, i) => {
        g.add(part('nova_shell_b' + i, cyl(.11, .11, .40, 14), mats.brass, [0, 1.02, z]));
        g.add(part('nova_shell_b_fuse' + i, cyl(.03, .09, .16, 10), mats.energy_fuse, [0, 1.28, z]));
      });
    });
    up('rate', 5, pitch, [.30, -.14, -.60], (g) => {
      g.add(part('nova_ejector_box', box(.16, .14, .22), mats.steel_hull));
      g.add(part('nova_ejector_chute', box(.10, .06, .30), mats.chassis, [.06, -.10, -.10], [0, 0, -30 * D]));
    });
    up('rate', 6, yaw, [-.70, 1.02, -.20], (g) => {
      g.add(part('nova_hoist_chute_b', box(.20, .16, .62), mats.chassis, [0, 0, 0], [0, 0, -22 * D]));
    });
    up('rate', 7, yaw, [-.52, 1.16, -.30], (g) => {
      g.add(part('nova_hoist_post', box(.12, .44, .12), mats.chassis));
      g.add(part('nova_hoist_arm', box(.62, .10, .12), mats.steel_hull, [.30, .26, 0]));
      g.add(part('nova_hoist_motor', box(.18, .18, .20), mats.trim, [.02, .26, 0]));
      g.add(part('nova_hoist_cable', cyl(.014, .014, .28, 6), mats.chrome, [.56, .10, 0]));
      g.add(part('nova_hoist_grab', box(.14, .10, .14), mats.brass, [.56, -.06, 0]));
      g.add(part('nova_hoist_lamp', box(.06, .03, .03), mats.energy_fuse, [.02, .37, 0]));
    });
    up('rate', 8, yaw, [0, .50, .70], (g) => {
      g.add(part('nova_conveyor', box(1.10, .06, .16), mats.brass));
      g.add(part('nova_conveyor_frame', box(1.16, .04, .20), mats.trim, [0, -.05, 0]));
      for (const x of [-.52, .52])
        g.add(part('nova_conveyor_roller' + x, cyl(.05, .05, .18, 12), mats.chrome, [x, 0, 0], [90 * D, 0, 0]));
    });
    up('rate', 9, yaw, [0, 0, 0], (g) => {
      g.add(K.hydraulic('nova_loader_ram', [-.70, .52, .62], [-.30, 1.04, .10], 1.1));
    });
    up('rate', 10, yaw, [.78, .74, -.10], (g) => {
      g.add(part('nova_mag_drum', cyl(.34, .34, .40, 24), mats.chassis, [0, 0, 0], [0, 0, 90 * D]));
      g.add(part('nova_mag_hub', cyl(.10, .10, .46, 12), mats.chrome, [0, 0, 0], [0, 0, 90 * D]));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        g.add(part('nova_mag_round' + i, cyl(.075, .075, .36, 12), mats.brass, [0, Math.cos(a) * .21, Math.sin(a) * .21], [0, 0, 90 * D]));
      }
      g.add(part('nova_mag_feed', box(.36, .14, .14), mats.steel_plate, [-.30, 0, .10]));
      g.add(K.boltRing('nova_mag_bolts', .26, 8, 0).rotateZ(90 * D).translateZ(.22));
      g.add(part('nova_mag_glow', tor(.30, .014, 6, 28), mats.energy_fuse, [.21, 0, 0], [0, 90 * D, 0]));
    });

    yaw.add(pitch);
    root.add(yaw);
    return root;
  },

  cue(model, levels) {
    const jacket = model.getObjectByName('nova_barrel_jacket');
    if (jacket) {
      const g = .95 + ((levels.damage ?? 1) - 1) / 9 * .35;
      jacket.scale.set(g, g, 1);
    }
    const creep = ((levels.range ?? 1) - 1) / 9 * .16;
    const brake = model.getObjectByName('nova_muzzle_brake');
    const lip = model.getObjectByName('nova_brake_lip');
    if (brake) brake.position.z = 1.62 + creep;
    if (lip) lip.position.z = 1.76 + creep;
    const ready = Math.min(3, Math.floor(((levels.rate ?? 1) - 1) / 3));
    for (let i = 0; i < 3; i++) {
      const on = i < Math.max(1, ready + 1);
      const shell = model.getObjectByName('nova_shell' + i);
      const fuse = model.getObjectByName('nova_shell_fuse' + i);
      if (shell) shell.visible = on;
      if (fuse) fuse.visible = on;
    }
  },
};
