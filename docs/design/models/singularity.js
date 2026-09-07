/**
 * Singularity — gravity well. Chill aura, no projectile, no aim rig.
 * Sim stats: sim/Sim.Core/Content/Towers.cs (cost 110, range 9 m, chill aura)
 *
 * Two upgrade paths (range, rate) matching the sim; every level 2–10 adds a
 * named attachment. Nothing bolted on may look like a weapon.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'singularity',
  label: 'Singularity',
  swatch: '#9b5be8',
  stats: { Cost: '110', Range: '9 m', Damage: '—', Rate: 'aura' },
  layers: 'Ground + Air',
  rig: null,
  note: 'No aim rig at all. `singularity_spin` is a cosmetic group the client rotates at chill potency; nothing about it is authoritative. The field applies in UpdateStatuses, so there is no muzzle and no projectile.',
  paths: [
    {
      id: 'range', label: 'Range / Field', cue: 'Halo widens with field radius',
      steps: {
        2: 'Pylon foot plates',
        3: 'Lens focus rings',
        4: 'Ground projector ring around the foot (field footprint made visible)',
        5: 'Projector conduit from the foot junction',
        6: 'Pylon-top reflector plates',
        7: 'Outer emitter collar between the pylons',
        8: 'Collar-to-pylon struts',
        9: 'Halo secondary coils',
        10: 'Wide-field lens array — a second lens on every pylon',
      },
    },
    {
      id: 'rate', label: 'Rate / Persistence', cue: 'Core brightens as charge cycle shortens',
      steps: {
        2: 'Stem capacitor cans',
        3: 'Pump manifold',
        4: 'Coil bank on the stem (faster field cycling)',
        5: 'Stem cooling fins',
        6: 'Second core cage ring',
        7: 'Second halo returns on a crossed axis',
        8: 'Coolant tank on the stem',
        9: 'Field regulator boxes on each pylon',
        10: 'Event-horizon shroud — dark torus and inner cage',
      },
    },
  ],

  build(K, levels) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'singularity', mats.energy_field);
    const root = grp('singularity');
    root.add(K.foot('singularity', mats.energy_field));
    root.add(part('singularity_stem', cyl(.40, .54, .58, 20), mats.chassis, [0, .62, 0]));
    root.add(part('singularity_stem_flange', cyl(.46, .46, .07, 24), mats.steel_plate, [0, .90, 0]));
    root.add(K.boltRing('singularity_stem_bolts', .38, 9, .945));
    root.add(part('singularity_pump', box(.30, .26, .22), mats.steel_hull, [.44, .50, .30], [0, -30 * D, 0]));
    root.add(K.cableRun('singularity_coolant_feed', [[.44, .62, .30], [.30, .78, .18], [.12, .92, .06]], .03, mats.brass));

    const arms = [];
    for (let i = 0; i < 3; i++) {
      const arm = grp('singularity_arm' + i);
      arm.rotation.y = (i / 3) * Math.PI * 2;
      arm.add(part('singularity_pylon' + i, box(.13, 1.90, .16), mats.chassis, [.52, 1.78, 0], [0, 0, 9 * D]));
      arm.add(part('singularity_pylon_inner' + i, box(.09, 1.80, .10), mats.chassis, [.36, 1.82, 0], [0, 0, 9 * D]));
      for (let k = 0; k < 5; k++)
        arm.add(part('singularity_truss' + i + '_' + k, box(.065, .34, .065), mats.trim,
          [.45 - k * .014, 1.06 + k * .34, 0], [0, 0, (k % 2 ? 54 : -54) * D]));
      arm.add(part('singularity_pylon_foot' + i, box(.24, .16, .26), mats.chassis, [.60, .90, 0]));
      arm.add(part('singularity_coolant_pipe' + i, cyl(.032, .032, 1.70, 10), mats.brass, [.60, 1.80, .10], [0, 0, 9 * D]));
      arm.add(part('singularity_emitter' + i, box(.22, .28, .24), mats.steel_plate, [.40, 2.68, 0], [0, 0, -32 * D]));
      arm.add(part('singularity_emitter_cowl' + i, box(.15, .17, .19), mats.trim, [.27, 2.60, 0], [0, 0, -32 * D]));
      arm.add(part('singularity_lens' + i, cyl(.075, .075, .06, 14), mats.energy_field, [.22, 2.56, 0], [0, 0, 58 * D]));
      root.add(arm);
      arms.push(arm);
    }

    root.add(part('singularity_containment_ring', tor(.62, .045, 10, 44), mats.steel_plate, [0, 1.92, 0], [90 * D, 0, 0]));
    for (let i = 0; i < 6; i++) {
      const a = (i / 6) * Math.PI * 2;
      root.add(part('singularity_ring_flange' + i, box(.10, .12, .16), mats.chrome, [Math.cos(a) * .62, 1.92, Math.sin(a) * .62], [0, -a, 0]));
    }

    const spin = grp('singularity_spin', [0, 2.30, 0]);
    spin.userData.role = 'cosmeticSpin';
    spin.add(part('singularity_core', new THREE.SphereGeometry(.30, 32, 24), mats.energy_field));
    spin.add(part('singularity_core_cage', tor(.34, .022, 8, 32), mats.trim, [0, 0, 0], [90 * D, 0, 0]));
    const halo = grp('singularity_halo_a');
    halo.rotation.x = 74 * D;
    halo.add(part('singularity_halo_a_ring', tor(.56, .026, 8, 48), mats.energy_field));
    for (let k = 0; k < 6; k++) {
      const a = (k / 6) * Math.PI * 2;
      halo.add(part('singularity_halo_a_coil' + k, tor(.04, .012, 6, 14), mats.brass, [Math.cos(a) * .56, Math.sin(a) * .56, 0], [0, 90 * D, a]));
    }
    spin.add(halo);
    root.add(spin);

    // Per-arm increments share one visibility toggle via a proxy group; the
    // cue() below mirrors that toggle onto each arm's copy.
    const perArm = (path, lvl, fill) => {
      up(path, lvl, root, [0, 0, 0], () => {});
      arms.forEach((arm, i) => {
        const g = grp(`singularity_up_${path}_l${lvl}_arm${i}`);
        fill(g, i);
        arm.add(g);
      });
    };

    /* ── RANGE ladder ───────────────────────────────────────────────── */
    perArm('range', 2, (g, i) => {
      g.add(part('singularity_foot_plate' + i, box(.34, .04, .36), mats.steel_plate, [.60, .84, 0]));
      g.add(K.boltRing('singularity_foot_plate_bolts' + i, .12, 4, .865).translateX(.60));
    });
    perArm('range', 3, (g, i) => {
      g.add(part('singularity_focus_ring' + i, tor(.10, .014, 6, 20), mats.chrome, [.20, 2.55, 0], [0, 0, 58 * D]));
    });
    up('range', 4, root, [0, 0, 0], (g) => {
      g.add(part('singularity_projector_ring', tor(1.02, .035, 8, 56), mats.steel_plate, [0, .05, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2 + 15 * D;
        g.add(part('singularity_projector_node' + i, cyl(.07, .09, .10, 10), mats.energy_field, [Math.cos(a) * 1.02, .09, Math.sin(a) * 1.02]));
        g.add(part('singularity_projector_shoe' + i, box(.16, .06, .18), mats.trim, [Math.cos(a) * 1.02, .03, Math.sin(a) * 1.02], [0, -a, 0]));
      }
    });
    up('range', 5, root, [0, 0, 0], (g) => {
      g.add(K.cableRun('singularity_projector_conduit', [[.60, .26, -.44], [.84, .18, -.62], [1.02, .08, -.30]], .022, mats.brass));
      g.add(part('singularity_projector_gland', cyl(.04, .05, .05, 10), mats.brass, [1.02, .10, -.30]));
    });
    perArm('range', 6, (g, i) => {
      g.add(part('singularity_reflector' + i, box(.30, .02, .30), mats.steel_plate, [.46, 2.88, 0], [0, 0, -20 * D]));
      g.add(part('singularity_reflector_post' + i, cyl(.02, .02, .10, 8), mats.chrome, [.42, 2.82, 0]));
    });
    up('range', 7, root, [0, 1.30, 0], (g) => {
      g.add(part('singularity_collar_ring', tor(.72, .04, 8, 48), mats.steel_hull, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 60 * D;
        g.add(part('singularity_collar_pod' + i, box(.22, .16, .18), mats.steel_hull, [Math.cos(a) * .72, .05, Math.sin(a) * .72], [0, -a, 0]));
        g.add(part('singularity_collar_lens' + i, cyl(.055, .055, .05, 12), mats.energy_field, [Math.cos(a) * .84, .05, Math.sin(a) * .84], [0, 0, 90 * D]));
      }
    });
    perArm('range', 8, (g, i) => {
      g.add(part('singularity_collar_strut' + i, box(.04, .04, .30), mats.chrome, [.60, 1.30, 0], [0, 90 * D, 0]));
      g.add(part('singularity_collar_strut_b' + i, box(.04, .30, .04), mats.chrome, [.66, 1.16, 0], [0, 0, 24 * D]));
      g.add(part('singularity_collar_strut_pip' + i, new THREE.SphereGeometry(.025, 8, 6), mats.energy_field, [.76, 1.30, 0]));
    });
    up('range', 9, spin, [0, 0, 0], (g) => {
      g.rotation.x = 74 * D;
      for (let k = 0; k < 6; k++) {
        const a = (k / 6) * Math.PI * 2 + 30 * D;
        g.add(part('singularity_halo_a_coil_b' + k, tor(.034, .011, 6, 14), mats.brass, [Math.cos(a) * .56, Math.sin(a) * .56, 0], [0, 90 * D, a]));
      }
    });
    perArm('range', 10, (g, i) => {
      g.add(part('singularity_lens_b' + i, cyl(.06, .06, .05, 12), mats.energy_field, [.30, 2.28, 0], [0, 0, 58 * D]));
      g.add(part('singularity_lens_b_cowl' + i, box(.13, .14, .16), mats.trim, [.36, 2.28, 0], [0, 0, -32 * D]));
    });

    /* ── RATE ladder ────────────────────────────────────────────────── */
    up('rate', 2, root, [0, .58, 0], (g) => {
      for (const a of [110 * D, 250 * D]) {
        g.add(part('singularity_stem_cap' + a, cyl(.07, .07, .30, 12), mats.steel_hull, [Math.cos(a) * .52, 0, Math.sin(a) * .52]));
        g.add(part('singularity_stem_cap_rib' + a, tor(.072, .012, 6, 16), mats.trim, [Math.cos(a) * .52, .08, Math.sin(a) * .52], [90 * D, 0, 0]));
      }
    });
    up('rate', 3, root, [.44, .50, .30], (g) => {
      g.rotation.y = -30 * D;
      for (const x of [-.08, 0, .08])
        g.add(part('singularity_manifold_pipe' + x, cyl(.02, .02, .24, 8), mats.brass, [x, .22, 0]));
      g.add(part('singularity_manifold_header', box(.28, .05, .08), mats.brass, [0, .34, 0]));
    });
    up('rate', 4, root, [0, .66, 0], (g) => {
      for (let i = 0; i < 3; i++)
        g.add(part('singularity_stem_coil' + i, tor(.50, .035, 8, 32), mats.brass, [0, i * .12 - .12, 0], [90 * D, 0, 0]));
      g.add(part('singularity_stem_junction', box(.20, .26, .18), mats.steel_hull, [-.50, 0, .16]));
      g.add(part('singularity_stem_junction_pip', box(.02, .12, .04), mats.energy_field, [-.61, 0, .16]));
    });
    up('rate', 5, root, [0, .40, 0], (g) => {
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2 + 22 * D;
        g.add(part('singularity_stem_fin' + i, box(.02, .22, .14), mats.steel_plate, [Math.cos(a) * .56, 0, Math.sin(a) * .56], [0, -a, 0]));
      }
    });
    up('rate', 6, spin, [0, 0, 0], (g) => {
      g.add(part('singularity_core_cage_b', tor(.36, .02, 8, 32), mats.trim, [0, 0, 0], [0, 0, 0]));
      g.add(part('singularity_core_cage_c', tor(.36, .02, 8, 32), mats.trim, [0, 0, 0], [0, 90 * D, 0]));
    });
    up('rate', 7, spin, [0, 0, 0], (g) => {
      g.rotation.set(-64 * D, 34 * D, 0);
      g.add(part('singularity_halo_b_ring', tor(.48, .024, 8, 44), mats.energy_field));
      for (let k = 0; k < 6; k++) {
        const a = (k / 6) * Math.PI * 2;
        g.add(part('singularity_halo_b_coil' + k, tor(.038, .011, 6, 14), mats.brass, [Math.cos(a) * .48, Math.sin(a) * .48, 0], [0, 90 * D, a]));
      }
    });
    up('rate', 8, root, [-.48, .60, -.30], (g) => {
      g.add(part('singularity_coolant_tank', cyl(.12, .12, .42, 16), mats.steel_plate));
      g.add(part('singularity_coolant_tank_cap', cyl(.06, .06, .06, 10), mats.brass, [0, .24, 0]));
      g.add(part('singularity_coolant_tank_strap', tor(.125, .014, 6, 18), mats.trim, [0, -.06, 0], [90 * D, 0, 0]));
    });
    perArm('rate', 9, (g, i) => {
      g.add(part('singularity_regulator' + i, box(.16, .20, .14), mats.chassis, [.66, 1.62, .12], [0, 0, 9 * D]));
      g.add(part('singularity_regulator_pip' + i, box(.02, .10, .03), mats.energy_field, [.75, 1.62, .12], [0, 0, 9 * D]));
    });
    up('rate', 10, spin, [0, 0, 0], (g) => {
      g.add(part('singularity_horizon_torus', tor(.68, .075, 10, 44), mats.trim, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('singularity_horizon_inner', tor(.68, .03, 8, 44), mats.energy_field, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2;
        g.add(part('singularity_horizon_vane' + i, box(.045, .16, .045), mats.chrome, [Math.cos(a) * .68, 0, Math.sin(a) * .68], [0, -a, 0]));
      }
    });

    return root;
  },

  cue(model, levels) {
    const halo = model.getObjectByName('singularity_halo_a');
    if (halo) {
      const s = .85 + ((levels.range ?? 1) - 1) / 9 * .45;
      halo.scale.set(s, s, 1);
    }
    const ring = model.getObjectByName('singularity_projector_ring');
    if (ring) {
      const s = .92 + ((levels.range ?? 1) - 1) / 9 * .30;
      ring.scale.set(s, s, 1);
    }
    const core = model.getObjectByName('singularity_core');
    if (core) {
      if (!core.userData.own) { core.material = core.material.clone(); core.userData.own = true; }
      core.material.emissiveIntensity = .55 + ((levels.rate ?? 1) - 1) / 9 * 1.3;
      core.scale.setScalar(.92 + ((levels.rate ?? 1) - 1) / 9 * .22);
    }
    // Mirror per-arm increments off their proxy toggles.
    for (const path of ['range', 'rate']) {
      for (let lv = 2; lv <= 10; lv++) {
        const on = (levels[path] ?? 1) >= lv;
        for (let i = 0; i < 3; i++) {
          const g = model.getObjectByName(`singularity_up_${path}_l${lv}_arm${i}`);
          if (g) g.visible = on;
        }
      }
    }
  },
};
