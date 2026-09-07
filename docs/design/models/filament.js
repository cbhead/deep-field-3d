/**
 * Filament — ramp beam. Sustained-fire tower whose damage climbs the longer
 * it holds one target. Not yet typed in the sim (M3); stats are plan intent.
 *
 * The only tower in this set with an aim rig: a beam must physically track,
 * so this rig is the fastest — beam towers that lag their target look broken.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'filament',
  label: 'Filament',
  swatch: '#ff2e4a',
  stats: { Milestone: 'M3', Kind: 'Beam (ramp)', Applies: 'shred (L7)', Sim: 'not yet' },
  layers: 'Ground + Air',
  rig: { yaw: [-180, 180], pitch: [-12, 48], traverse: 220, elevate: 160 },
  note: 'A beam has no travel time, so the rig IS the weapon: 220°/s yaw so it never loses a strafing target once acquired. Heat ramp is shown as the radiator stack glowing; the client drives emissive from the sim\'s ramp multiplier. Beam origin is the lens — `filament_muzzle`.',
  paths: [
    {
      id: 'ramp', label: 'Ramp rate', cue: 'Radiator stack glows hotter',
      steps: {
        2: 'Heat pipes from lens to breech',
        3: 'Coil sleeve on the emitter',
        4: 'Heat-retention shroud (heat kept on target switch)',
        5: 'Second radiator bank',
        6: 'Thermal fins on the breech',
        7: 'Heat accumulator cell',
        8: 'Pipe manifold',
        9: 'Hot-core indicator',
        10: 'Ramp turbine on the breech',
      },
    },
    {
      id: 'peak', label: 'Peak', cue: 'Lens ring brightens',
      steps: {
        2: 'Lens brace',
        3: 'Aperture ring',
        4: 'Power coupler',
        5: 'Second lens element',
        6: 'Dielectric plate',
        7: 'Shred emitter under the lens (beam applies shred)',
        8: 'Capacitor bank on the cradle',
        9: 'Focus gimbal ring',
        10: 'Split-beam prism head (two apertures)',
      },
    },
    {
      id: 'optics', label: 'Optics', cue: 'Emitter tube lengthens',
      steps: {
        2: 'Sight glass',
        3: 'Lens hood',
        4: 'Telescope tube extension',
        5: 'Rangefinder pod',
        6: 'Stabiliser fin',
        7: 'Optics mast',
        8: 'Gimbal counterweight',
        9: 'Corrector lens array',
        10: 'Stabiliser outriggers deploy from the foot',
      },
    },
  ],

  build(K) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'filament', mats.energy_beam);
    const root = grp('filament');
    root.add(K.foot('filament', mats.energy_beam));

    const yaw = grp('filament_yaw', [0, .33, 0]);
    yaw.add(K.ringGear('filament_slew_gear', .54, 30));
    yaw.add(part('filament_yaw_drum', cyl(.50, .56, .40, 24), mats.steel_hull, [0, .25, 0]));
    yaw.add(K.boltRing('filament_yaw_bolts', .44, 10, .46));
    yaw.add(part('filament_mast', cyl(.16, .22, .90, 16), mats.chassis, [0, .90, 0]));
    yaw.add(part('filament_mast_collar', cyl(.24, .24, .08, 16), mats.steel_plate, [0, 1.36, 0]));
    for (const s of [-1, 1]) {
      const sfx = s < 0 ? '_l' : '_r';
      yaw.add(part('filament_yoke' + sfx, box(.10, .46, .30), mats.chassis, [s * .30, 1.56, -.04]));
      yaw.add(part('filament_yoke_hub' + sfx, cyl(.11, .11, .08, 16), mats.brass, [s * .36, 1.70, 0], [0, 0, 90 * D]));
    }
    yaw.add(K.cableRun('filament_mast_loom', [[.20, .46, -.30], [.24, .90, -.24], [.14, 1.36, -.14]], .024));
    yaw.add(part('filament_power_box', box(.30, .26, .22), mats.chassis, [-.40, .60, -.22]));
    yaw.add(part('filament_power_pip', box(.02, .10, .04), mats.energy_beam, [-.56, .60, -.22]));

    const pitch = grp('filament_pitch', [0, 1.70, 0]);
    pitch.add(part('filament_body', box(.40, .34, .80), mats.steel_plate, [0, 0, -.14]));
    pitch.add(part('filament_emitter_tube', cyl(.11, .13, 1.30, 20), mats.steel_hull, [0, 0, .80], [90 * D, 0, 0]));
    for (let i = 0; i < 4; i++)
      pitch.add(part('filament_focus_ring' + i, tor(.135, .022, 8, 24), mats.trim, [0, 0, .40 + i * .26]));
    pitch.add(part('filament_lens_housing', cyl(.18, .15, .18, 20), mats.steel_plate, [0, 0, 1.50], [90 * D, 0, 0]));
    pitch.add(part('filament_lens', cyl(.11, .11, .03, 20), mats.optic_glass, [0, 0, 1.60], [90 * D, 0, 0]));
    pitch.add(part('filament_lens_glow', tor(.12, .012, 6, 24), mats.energy_beam, [0, 0, 1.60]));
    // Radiator stack: the heat ramp readout.
    pitch.add(K.finStack('filament_radiator', 6, .07, .46, .28, .016).translateZ(-.50).translateY(.22));
    pitch.add(part('filament_radiator_core', box(.10, .06, .44), mats.energy_beam, [0, .18, -.32]));
    pitch.add(part('filament_breech', cyl(.16, .18, .30, 20), mats.chassis, [0, 0, -.68], [90 * D, 0, 0]));
    pitch.add(part('filament_breech_cap', cyl(.10, .10, .06, 16), mats.brass, [0, 0, -.86], [90 * D, 0, 0]));
    pitch.add(K.hydraulic('filament_elev_ram', [.22, -.24, -.44], [.22, -.20, .30], .9));
    pitch.add(part('filament_charge_strip', box(.02, .16, .50), mats.energy_beam, [.21, 0, -.10]));
    pitch.add(K.muzzle('filament', [0, 0, 1.63]));

    /* ── RAMP ladder ────────────────────────────────────────────────── */
    up('ramp', 2, pitch, [0, 0, 0], (g) => {
      for (const s of [-1, 1])
        g.add(K.cableRun('filament_heat_pipe' + (s < 0 ? '_l' : '_r'), [[s * .12, .10, 1.40], [s * .18, .14, .60], [s * .16, .22, -.30]], .018, mats.brass));
    });
    up('ramp', 3, pitch, [0, 0, .30], (g) => {
      for (let i = 0; i < 6; i++)
        g.add(part('filament_coil_sleeve' + i, tor(.14, .016, 6, 24), mats.brass, [0, 0, i * .05]));
    });
    up('ramp', 4, pitch, [0, 0, .90], (g) => {
      g.add(part('filament_heat_shroud', new THREE.CylinderGeometry(.19, .19, .70, 24, 1, false, -Math.PI * .8, Math.PI * 1.6), mats.chassis, [0, 0, 0], [90 * D, 0, 0]));
      g.add(K.boltRing('filament_shroud_bolts', .19, 8, 0).rotateX(90 * D).translateZ(-.30));
      g.add(part('filament_shroud_glow', box(.02, .06, .60), mats.energy_beam, [0, .19, 0]));
    });
    up('ramp', 5, pitch, [0, -.22, -.50], (g) => {
      g.add(K.finStack('filament_radiator_b', 6, .07, .46, .22, .016));
    });
    up('ramp', 6, pitch, [0, 0, -.68], (g) => {
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2;
        g.add(part('filament_breech_fin' + i, box(.02, .10, .26), mats.steel_plate, [Math.cos(a) * .20, Math.sin(a) * .20, 0], [0, 0, a]));
      }
    });
    up('ramp', 7, pitch, [-.30, .06, -.30], (g) => {
      g.add(part('filament_accumulator', cyl(.10, .10, .40, 16), mats.steel_hull, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('filament_accumulator_glow', tor(.105, .012, 6, 20), mats.energy_beam, [0, 0, .10]));
      g.add(part('filament_accumulator_cap', cyl(.05, .05, .08, 10), mats.brass, [0, 0, .24], [90 * D, 0, 0]));
    });
    up('ramp', 8, pitch, [0, .22, -.10], (g) => {
      g.add(part('filament_manifold', box(.34, .05, .06), mats.brass));
      for (const x of [-.12, 0, .12])
        g.add(part('filament_manifold_stub' + x, cyl(.018, .018, .10, 8), mats.brass, [x, 0, .06], [90 * D, 0, 0]));
    });
    up('ramp', 9, pitch, [0, .12, -.86], (g) => {
      g.add(part('filament_hot_core', new THREE.SphereGeometry(.05, 12, 8), mats.energy_beam));
      g.add(part('filament_hot_core_cage', tor(.06, .008, 6, 16), mats.chrome, [0, 0, 0], [90 * D, 0, 0]));
    });
    up('ramp', 10, pitch, [0, 0, -1.0], (g) => {
      g.add(part('filament_turbine_housing', cyl(.20, .18, .20, 20), mats.chassis, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2;
        g.add(part('filament_turbine_blade' + i, box(.03, .14, .04), mats.chrome, [Math.cos(a) * .10, Math.sin(a) * .10, -.02], [0, 0, a + 30 * D]));
      }
      g.add(part('filament_turbine_hub', cyl(.04, .04, .06, 10), mats.brass, [0, 0, -.06], [90 * D, 0, 0]));
      g.add(part('filament_turbine_glow', tor(.19, .012, 6, 24), mats.energy_beam, [0, 0, .10]));
    });

    /* ── PEAK ladder ────────────────────────────────────────────────── */
    up('peak', 2, pitch, [0, 0, 1.40], (g) => {
      for (const s of [-1, 1])
        g.add(part('filament_lens_brace' + (s < 0 ? '_l' : '_r'), box(.03, .03, .34), mats.steel_hull, [s * .17, -.04, 0]));
    });
    up('peak', 3, pitch, [0, 0, 1.62], (g) => {
      g.add(part('filament_aperture_ring', tor(.16, .02, 8, 28), mats.chrome));
    });
    up('peak', 4, pitch, [0, -.22, .10], (g) => {
      g.add(part('filament_coupler', box(.20, .12, .30), mats.chassis));
      g.add(part('filament_coupler_glow', box(.14, .02, .20), mats.energy_beam, [0, -.07, 0]));
    });
    up('peak', 5, pitch, [0, 0, 1.72], (g) => {
      g.add(part('filament_lens_b', cyl(.09, .09, .03, 20), mats.optic_glass, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('filament_lens_b_ring', tor(.10, .012, 6, 24), mats.energy_beam));
    });
    up('peak', 6, pitch, [0, .20, -.12], (g) => {
      g.add(part('filament_dielectric', box(.36, .02, .40), mats.ceramic));
    });
    up('peak', 7, pitch, [0, -.20, 1.30], (g) => {
      g.add(part('filament_shred_emitter', box(.12, .10, .30), mats.chassis));
      g.add(part('filament_shred_lens', cyl(.035, .035, .02, 12), mats.energy_field, [0, 0, .16], [90 * D, 0, 0]));
      g.add(part('filament_shred_mount', box(.04, .10, .08), mats.steel_hull, [0, .08, -.06]));
    });
    up('peak', 8, yaw, [.36, 1.20, -.16], (g) => {
      for (const y of [-.12, .12]) {
        g.add(part('filament_cap' + y, cyl(.07, .07, .30, 14), mats.steel_hull, [0, y, 0], [0, 0, 90 * D]));
        g.add(part('filament_cap_rib' + y, tor(.072, .01, 6, 16), mats.trim, [.06, y, 0], [0, 90 * D, 0]));
      }
    });
    up('peak', 9, pitch, [0, 0, 1.50], (g) => {
      g.add(part('filament_gimbal_ring', tor(.24, .018, 8, 32), mats.brass, [0, 0, 0], [0, 0, 0]));
      for (const a of [0, 90, 180, 270])
        g.add(part('filament_gimbal_pin' + a, box(.03, .03, .06), mats.chrome, [Math.cos(a * D) * .21, Math.sin(a * D) * .21, 0]));
    });
    up('peak', 10, pitch, [0, 0, 1.84], (g) => {
      g.add(part('filament_prism', box(.34, .16, .16), mats.chassis));
      for (const s of [-1, 1]) {
        g.add(part('filament_prism_lens' + (s < 0 ? '_l' : '_r'), cyl(.055, .055, .03, 16), mats.optic_glass, [s * .10, 0, .085], [90 * D, 0, 0]));
        g.add(part('filament_prism_glow' + (s < 0 ? '_l' : '_r'), tor(.06, .008, 6, 18), mats.energy_beam, [s * .10, 0, .10]));
      }
    });

    /* ── OPTICS ladder ──────────────────────────────────────────────── */
    up('optics', 2, pitch, [.14, .20, .40], (g) => {
      g.add(part('filament_sight_glass', cyl(.03, .03, .16, 10), mats.optic_glass, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('filament_sight_mount', box(.04, .06, .06), mats.steel_hull, [0, -.05, 0]));
    });
    up('optics', 3, pitch, [0, 0, 1.68], (g) => {
      g.add(part('filament_lens_hood', new THREE.CylinderGeometry(.17, .14, .16, 20, 1, true), mats.trim, [0, 0, 0], [90 * D, 0, 0]));
    });
    up('optics', 4, pitch, [0, 0, 1.30], (g) => {
      g.add(part('filament_tube_ext', cyl(.10, .11, .50, 18), mats.steel_hull, [0, 0, .25], [90 * D, 0, 0]));
      g.add(part('filament_tube_ext_ring', tor(.115, .014, 6, 22), mats.energy_beam, [0, 0, .50]));
    });
    up('optics', 5, pitch, [-.18, .14, .70], (g) => {
      g.add(part('filament_rangefinder', box(.08, .08, .26), mats.chassis));
      g.add(part('filament_rangefinder_lens', cyl(.025, .025, .02, 10), mats.energy_beam, [0, 0, .14], [90 * D, 0, 0]));
    });
    up('optics', 6, pitch, [0, .24, 1.20], (g) => {
      g.add(part('filament_stab_fin', box(.02, .16, .40), mats.steel_plate));
    });
    up('optics', 7, pitch, [0, .34, -.20], (g) => {
      g.add(part('filament_optics_mast', box(.06, .24, .06), mats.steel_hull, [0, .10, 0]));
      g.add(part('filament_optics_head', box(.18, .12, .30), mats.steel_plate, [0, .28, .10]));
      g.add(part('filament_optics_lens', box(.10, .07, .02), mats.optic_glass, [0, .28, .26]));
      g.add(part('filament_optics_reticle', box(.03, .03, .01), mats.energy_beam, [0, .28, .275]));
    });
    up('optics', 8, pitch, [0, -.14, -.98], (g) => {
      g.add(part('filament_counterweight', box(.30, .18, .16), mats.trim));
      g.add(part('filament_counterweight_bar', cyl(.02, .02, .34, 8), mats.chrome, [0, .12, 0], [0, 0, 90 * D]));
    });
    up('optics', 9, pitch, [0, 0, 1.10], (g) => {
      for (const z of [0, .10, .20])
        g.add(part('filament_corrector' + z, tor(.15, .01, 6, 24), mats.chrome, [0, 0, z]));
    });
    up('optics', 10, root, [0, 0, 0], (g) => g.add(K.outriggers('filament_outrigger', .50)));

    yaw.add(pitch);
    root.add(yaw);
    return root;
  },

  cue(model, levels) {
    const core = model.getObjectByName('filament_radiator_core');
    if (core) {
      if (!core.userData.own) { core.material = core.material.clone(); core.userData.own = true; }
      core.material.emissiveIntensity = .5 + ((levels.ramp ?? 1) - 1) / 9 * 1.6;
    }
    const glow = model.getObjectByName('filament_lens_glow');
    if (glow) {
      if (!glow.userData.own) { glow.material = glow.material.clone(); glow.userData.own = true; }
      glow.material.emissiveIntensity = .6 + ((levels.peak ?? 1) - 1) / 9 * 1.4;
    }
    const tube = model.getObjectByName('filament_emitter_tube');
    if (tube) tube.scale.y = .9 + ((levels.optics ?? 1) - 1) / 9 * .25;
  },
};
