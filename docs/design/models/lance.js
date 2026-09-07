/**
 * Lance — railgun corridor pierce. Ground layer, lane weapon.
 * Sim stats: sim/Sim.Core/Content/Towers.cs (cost 75, range 12 m, dmg 8, 1.6/s)
 *
 * Upgrade display: every level 2–10 on every path adds a named attachment.
 * L4 / L7 / L10 are the breakpoint rungs and carry the big hardware (they add
 * a mechanic); the other six are smaller additive increments — straps, plates,
 * lines, sinks — so progress reads at every purchase without changing the
 * tower's silhouette class.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'lance',
  label: 'Lance',
  swatch: '#2b5cff',
  stats: { Cost: '75', Range: '12 m', Damage: '8', Rate: '1.6/s' },
  layers: 'Ground',
  rig: { yaw: [-180, 180], pitch: [-8, 26], traverse: 90, elevate: 60 },
  note: 'Long rail = a lane weapon. Yaw is unlimited but slow-ish; a Leaper crossing at 8 m/s inside 6 m will out-slew it — that miss is the design, not a bug. Bolt leaves the aperture, not the socket origin.',
  paths: [
    {
      id: 'damage', label: 'Damage', cue: 'Accelerator glow gains with each level',
      steps: {
        2: 'Rail tension straps',
        3: 'Breech reinforcement cheeks',
        4: 'Twin heavy rails + second accelerator strip (wider pierce corridor)',
        5: 'Coil armour caps on the rear stations',
        6: 'Fifth coil station at the muzzle',
        7: 'Weak-point sensor pod above the breech (bonus damage on marks)',
        8: 'Rail bridge clamps tying the heavy rails',
        9: 'Capacitor terminal cage',
        10: 'Flux-breaker muzzle extension (rail ignores flat armour)',
      },
    },
    {
      id: 'range', label: 'Range / Precision', cue: 'Rail shrouds lengthen through the band',
      steps: {
        2: 'Deep optic sunshade',
        3: 'Muzzle stabiliser fins',
        4: 'Extended optic mast (longer sight base)',
        5: 'Laser rangefinder on the optic',
        6: 'Forward shroud segments',
        7: 'Designator boom off the left trunnion',
        8: 'Trunnion bracing gussets',
        9: 'Elevation counter-balance weights',
        10: 'Stabiliser outriggers deploy from the foot',
      },
    },
    {
      id: 'rate', label: 'Rate', cue: 'Coil windings brighten as charge time drops',
      steps: {
        2: 'Coolant line, capacitors to breech',
        3: 'Second bus-bar tap',
        4: 'Third capacitor can + heavier bus bar',
        5: 'Breech heat-sink fins',
        6: 'Capacitor rib covers',
        7: 'Breech radiator stack (heat dump)',
        8: 'Coolant reservoir on the yaw drum',
        9: 'Fast-charge relay box',
        10: 'Autoloader drum on the breech flank',
      },
    },
  ],

  build(K, levels) {
    const { D, mats, part, box, cyl, tor, grp } = K;
    const up = makeUp(K, 'lance', mats.energy_rail);
    const root = grp('lance');
    root.add(K.foot('lance', mats.energy_rail));

    const yaw = grp('lance_yaw', [0, .33, 0]);
    yaw.add(K.ringGear('lance_slew_gear', .58, 32));
    yaw.add(part('lance_yaw_drum', cyl(.56, .60, .46, 24), mats.steel_hull, [0, .28, 0]));
    yaw.add(K.panelPlate('lance_yaw_hatch', .32, .26, [0, .30, .585]));
    yaw.add(K.boltRing('lance_yaw_bolts', .50, 10, .50));
    yaw.add(K.louvres('lance_yaw_vent', .34, .24, 4).translateZ(-.58).translateY(.30));
    yaw.add(part('lance_yaw_cap', cyl(.52, .56, .07, 24), mats.steel_plate, [0, .54, 0]));
    for (const s of [-1, 1]) {
      const sfx = s < 0 ? '_l' : '_r';
      yaw.add(part('lance_trunnion' + sfx, box(.15, .80, .44), mats.chassis, [s * .46, .90, -.08]));
      yaw.add(part('lance_trunnion_gusset' + sfx, box(.13, .26, .26), mats.steel_hull, [s * .46, .62, .10]));
      yaw.add(part('lance_trunnion_hub' + sfx, cyl(.14, .14, .10, 20), mats.brass, [s * .50, .86, 0], [0, 0, 90 * D]));
      yaw.add(part('lance_trunnion_nut' + sfx, cyl(.05, .05, .12, 6), mats.chrome, [s * .56, .86, 0], [0, 0, 90 * D]));
    }
    yaw.add(K.cableRun('lance_yaw_loom', [[-.40, .18, -.30], [-.46, .34, -.10], [-.40, .62, .04]], .028));

    const pitch = grp('lance_pitch', [0, 1.19, 0]);
    for (const s of [-1, 1]) {
      const sfx = s < 0 ? '_l' : '_r';
      pitch.add(part('lance_rail' + sfx, box(.13, .17, 3.0), mats.chrome, [s * .24, 0, .25]));
      pitch.add(part('lance_rail_shroud' + sfx, box(.09, .22, 2.6), mats.steel_hull, [s * .33, 0, .35]));
    }
    pitch.add(part('lance_accelerator', box(.28, .05, 2.8), mats.energy_rail, [0, 0, .25]));
    [-.35, .25, .85, 1.45].forEach((z, i) => {
      pitch.add(part('lance_coil' + i, tor(.33, .08, 10, 28), mats.trim, [0, 0, z]));
      pitch.add(part('lance_coil_winding' + i, tor(.33, .038, 8, 28), mats.brass, [0, 0, z + .06]));
      pitch.add(part('lance_insulator' + i, box(.50, .09, .10), mats.steel_plate, [0, -.20, z]));
    });
    pitch.add(part('lance_breech', box(.80, .72, .86), mats.chassis, [0, -.02, -1.32]));
    pitch.add(part('lance_breech_cap', cyl(.30, .30, .10, 24), mats.steel_plate, [0, -.02, -1.80], [90 * D, 0, 0]));
    pitch.add(K.panelPlate('lance_breech_hatch', .34, .30, [0, .31, -1.32], [90 * D, 0, 0]));
    pitch.add(K.hazardStripes('lance_breech_hazard', .42, .22, [.41, -.20, -1.32], [0, 90 * D, 0]));
    for (const s of [-1, 1]) {
      const sfx = s < 0 ? '_l' : '_r', x = s * .28;
      pitch.add(part('lance_capacitor' + sfx, cyl(.15, .15, .74, 20), mats.steel_hull, [x, .48, -1.30], [90 * D, 0, 0]));
      for (let i = 0; i < 4; i++)
        pitch.add(part('lance_cap_rib' + sfx + i, tor(.155, .016, 6, 20), mats.trim, [x, .48, -1.60 + i * .19]));
      pitch.add(part('lance_cap_terminal' + sfx, cyl(.05, .05, .07, 10), mats.brass, [x, .48, -.90], [90 * D, 0, 0]));
    }
    pitch.add(part('lance_bus_bar', box(.62, .05, .09), mats.brass, [0, .68, -1.30]));
    pitch.add(K.cableRun('lance_cap_loom_l', [[-.28, .48, -.88], [-.14, .30, -.72], [0, .18, -.90]], .025));
    pitch.add(K.cableRun('lance_cap_loom_r', [[.28, .48, -.88], [.14, .30, -.72], [0, .18, -.90]], .025));
    pitch.add(K.hydraulic('lance_elev_ram_r', [.30, -.30, -1.00], [.30, -.36, .10], 1.1));
    pitch.add(K.hydraulic('lance_elev_ram_l', [-.30, -.30, -1.00], [-.30, -.36, .10], 1.1));
    pitch.add(part('lance_muzzle_shroud', cyl(.29, .25, .34, 24), mats.steel_plate, [0, 0, 1.78], [90 * D, 0, 0]));
    pitch.add(K.finStack('lance_muzzle_fins', 5, .055, .50, .34).translateZ(1.50));
    pitch.add(part('lance_aperture', cyl(.16, .16, .05, 20), mats.energy_rail, [0, 0, 1.96], [90 * D, 0, 0]));
    pitch.add(part('lance_muzzle_lip', tor(.26, .026, 8, 28), mats.chrome, [0, 0, 1.95]));
    pitch.add(part('lance_optic', box(.28, .21, .50), mats.steel_plate, [0, .47, -.42]));
    pitch.add(part('lance_optic_shade', box(.24, .16, .12), mats.trim, [0, .47, -.13]));
    pitch.add(part('lance_optic_lens', box(.17, .12, .02), mats.optic_glass, [0, .47, -.06]));
    pitch.add(part('lance_optic_mount', box(.10, .18, .18), mats.steel_hull, [0, .32, -.42]));
    pitch.add(K.muzzle('lance', [0, 0, 2.02]));

    /* ── DAMAGE ladder ──────────────────────────────────────────────── */
    up('damage', 2, pitch, [0, 0, 0], (g) => {
      for (const z of [-.05, .55, 1.15])
        g.add(part('lance_rail_strap' + z, box(.60, .06, .05), mats.trim, [0, .12, z]));
      for (const z of [-.05, .55, 1.15])
        g.add(part('lance_rail_strap_glow' + z, box(.30, .02, .02), mats.energy_rail, [0, .155, z]));
    });
    up('damage', 3, pitch, [0, -.02, -1.32], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('lance_breech_cheek' + (s < 0 ? '_l' : '_r'), box(.06, .56, .70), mats.steel_plate, [s * .43, 0, 0]));
        g.add(K.boltRing('lance_cheek_bolts' + (s < 0 ? '_l' : '_r'), .20, 6, 0).rotateZ(90 * D).translateY(s * .47));
      }
    });
    up('damage', 4, pitch, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        g.add(part('lance_heavy_rail' + sfx, box(.11, .24, 2.7), mats.chrome, [s * .42, 0, .35]));
        g.add(part('lance_heavy_rail_brace' + sfx, box(.20, .07, .30), mats.steel_hull, [s * .33, .16, -.20]));
        g.add(part('lance_heavy_rail_brace2' + sfx, box(.20, .07, .30), mats.steel_hull, [s * .33, .16, 1.10]));
      }
      g.add(part('lance_accelerator_b', box(.62, .04, 2.6), mats.energy_rail, [0, .16, .35]));
    });
    up('damage', 5, pitch, [0, 0, 0], (g) => {
      for (const [i, z] of [[0, -.35], [1, .25]])
        g.add(part('lance_coil_cap' + i, new K.THREE.CylinderGeometry(.40, .40, .20, 16, 1, false, -Math.PI / 2, Math.PI), mats.chassis, [0, 0, z], [90 * D, 0, 0]));
    });
    up('damage', 6, pitch, [0, 0, 2.0], (g) => {
      g.add(part('lance_coil4', tor(.30, .07, 10, 28), mats.trim));
      g.add(part('lance_coil4_glow', tor(.30, .02, 6, 28), mats.energy_rail, [0, 0, -.06]));
      g.add(part('lance_coil_winding4', tor(.30, .034, 8, 28), mats.brass, [0, 0, .06]));
      g.add(part('lance_insulator4', box(.46, .08, .09), mats.steel_plate, [0, -.18, 0]));
    });
    up('damage', 7, pitch, [0, .74, -.30], (g) => {
      g.add(part('lance_sensor_pod', box(.34, .20, .40), mats.chassis));
      g.add(part('lance_sensor_face', box(.24, .13, .02), mats.optic_glass, [0, 0, .21]));
      g.add(part('lance_sensor_mast', box(.08, .22, .08), mats.steel_hull, [0, -.20, -.06]));
      g.add(part('lance_sensor_pip', cyl(.03, .03, .06, 8), mats.energy_rail, [.10, .11, 0]));
    });
    up('damage', 8, pitch, [0, 0, 0], (g) => {
      for (const z of [-.50, .40, 1.30])
        g.add(part('lance_bridge_clamp' + z, box(.96, .10, .08), mats.steel_hull, [0, -.08, z]));
    });
    up('damage', 9, pitch, [0, .68, -1.30], (g) => {
      g.add(part('lance_terminal_cage_top', box(.70, .03, .16), mats.brass, [0, .14, 0]));
      for (const x of [-.33, 0, .33])
        g.add(part('lance_terminal_cage_post' + x, box(.03, .16, .03), mats.brass, [x, .06, .06]));
      g.add(part('lance_terminal_cage_post2', box(.03, .16, .03), mats.brass, [-.33, .06, -.06]));
      g.add(part('lance_terminal_cage_post3', box(.03, .16, .03), mats.brass, [.33, .06, -.06]));
    });
    up('damage', 10, pitch, [0, 0, 2.18], (g) => {
      g.add(part('lance_breaker_body', cyl(.24, .28, .52, 24), mats.steel_hull, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        g.add(part('lance_breaker_flute' + i, box(.05, .05, .48), mats.chrome, [Math.cos(a) * .26, Math.sin(a) * .26, 0], [0, 0, a]));
      }
      g.add(part('lance_breaker_ring', tor(.27, .03, 8, 28), mats.brass, [0, 0, .26]));
      g.add(part('lance_breaker_core', cyl(.13, .13, .06, 16), mats.energy_rail, [0, 0, .28], [90 * D, 0, 0]));
    });

    /* ── RANGE ladder ───────────────────────────────────────────────── */
    up('range', 2, pitch, [0, .47, 0], (g) => {
      g.add(part('lance_optic_shade_deep', box(.25, .17, .16), mats.trim, [0, 0, .02]));
    });
    up('range', 3, pitch, [0, 0, 1.70], (g) => {
      for (const s of [-1, 1])
        g.add(part('lance_stab_fin' + (s < 0 ? '_l' : '_r'), box(.02, .18, .30), mats.steel_plate, [s * .30, .18, 0], [0, 0, s * -20 * D]));
    });
    up('range', 4, pitch, [0, .68, -.42], (g) => {
      g.add(part('lance_optic_riser', box(.14, .30, .16), mats.steel_hull, [0, .04, 0]));
      g.add(part('lance_optic_long', box(.22, .17, .74), mats.steel_plate, [0, .27, .12]));
      g.add(part('lance_optic_long_lens', box(.14, .11, .02), mats.optic_glass, [0, .27, .50]));
      g.add(part('lance_optic_long_reticle', box(.04, .04, .01), mats.energy_rail, [0, .27, .515]));
      g.add(part('lance_optic_long_hood', box(.20, .15, .10), mats.trim, [0, .27, .44]));
    });
    up('range', 5, pitch, [.18, .95, -.30], (g) => {
      g.add(part('lance_rangefinder', box(.08, .08, .22), mats.chassis));
      g.add(part('lance_rangefinder_lens', cyl(.025, .025, .02, 10), mats.energy_rail, [0, 0, .12], [90 * D, 0, 0]));
    });
    up('range', 6, pitch, [0, 0, 1.78], (g) => {
      for (const s of [-1, 1])
        g.add(part('lance_shroud_ext' + (s < 0 ? '_l' : '_r'), box(.09, .20, .44), mats.steel_hull, [s * .33, 0, 0]));
    });
    up('range', 7, pitch, [-.52, .30, .10], (g) => {
      g.add(part('lance_designator_boom', cyl(.035, .035, .90, 10), mats.chrome, [0, 0, .30], [90 * D, 0, 0]));
      g.add(part('lance_designator_head', box(.14, .14, .18), mats.chassis, [0, 0, .78]));
      g.add(part('lance_designator_lens', cyl(.05, .05, .03, 12), mats.energy_rail, [0, 0, .88], [90 * D, 0, 0]));
      g.add(part('lance_designator_mount', box(.10, .16, .12), mats.steel_hull, [0, 0, -.14]));
    });
    up('range', 8, yaw, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) {
        g.add(part('lance_brace_gusset' + (s < 0 ? '_l' : '_r'), box(.10, .46, .10), mats.steel_plate, [s * .56, .70, -.22], [0, 0, s * 18 * D]));
        g.add(part('lance_brace_foot' + (s < 0 ? '_l' : '_r'), box(.16, .06, .18), mats.trim, [s * .50, .55, -.30]));
      }
    });
    up('range', 9, pitch, [0, -.30, -1.72], (g) => {
      for (const s of [-1, 1])
        g.add(part('lance_counterweight' + (s < 0 ? '_l' : '_r'), box(.18, .26, .24), mats.trim, [s * .24, 0, 0]));
      g.add(part('lance_counterweight_bar', box(.66, .05, .05), mats.chrome, [0, .16, 0]));
    });
    up('range', 10, root, [0, 0, 0], (g) => g.add(K.outriggers('lance_outrigger')));

    /* ── RATE ladder ────────────────────────────────────────────────── */
    up('rate', 2, pitch, [0, 0, 0], (g) => {
      g.add(K.cableRun('lance_coolant_line', [[.28, .30, -1.62], [.42, .10, -1.40], [.36, -.20, -1.00]], .02, mats.brass));
    });
    up('rate', 3, pitch, [.24, .68, -1.10], (g) => {
      g.add(part('lance_bus_tap', box(.10, .10, .12), mats.brass));
      g.add(part('lance_bus_tap_line', cyl(.018, .018, .30, 8), mats.rubber, [0, -.12, .10], [30 * D, 0, 0]));
    });
    up('rate', 4, pitch, [0, 0, 0], (g) => {
      g.add(part('lance_capacitor_c', cyl(.15, .15, .74, 20), mats.steel_hull, [0, .70, -1.30], [90 * D, 0, 0]));
      for (let i = 0; i < 4; i++)
        g.add(part('lance_cap_rib_c' + i, tor(.155, .016, 6, 20), mats.trim, [0, .70, -1.60 + i * .19]));
      g.add(part('lance_bus_bar_heavy', box(.70, .08, .13), mats.brass, [0, .90, -1.30]));
      g.add(part('lance_bus_bar_glow', box(.60, .02, .03), mats.energy_rail, [0, .95, -1.30]));
    });
    up('rate', 5, pitch, [0, .36, -1.62], (g) => {
      g.add(K.finStack('lance_breech_sink', 6, .05, .40, .14, .012).translateZ(-.12));
    });
    up('rate', 6, pitch, [0, 0, 0], (g) => {
      for (const s of [-1, 1]) for (let i = 0; i < 3; i++)
        g.add(part('lance_cap_cover' + (s < 0 ? '_l' : '_r') + i, tor(.16, .022, 6, 20), mats.steel_plate, [s * .28, .48, -1.50 + i * .19]));
    });
    up('rate', 7, pitch, [-.52, .10, -1.20], (g) => {
      g.add(part('lance_radiator_frame', box(.10, .52, .70), mats.chassis));
      g.add(K.finStack('lance_radiator_fins', 7, .085, .18, .48, .016).translateZ(-.26).translateX(-.10));
      g.add(part('lance_radiator_pipe', cyl(.035, .035, .40, 10), mats.brass, [.06, .28, .04], [0, 0, 30 * D]));
      g.add(part('lance_radiator_glow', box(.02, .40, .60), mats.energy_rail, [-.06, 0, 0]));
    });
    up('rate', 8, yaw, [.40, .30, -.40], (g) => {
      g.add(part('lance_reservoir', cyl(.12, .12, .40, 16), mats.steel_plate, [0, 0, 0], [0, 0, 20 * D]));
      g.add(part('lance_reservoir_cap', cyl(.06, .06, .06, 10), mats.brass, [-.07, .21, 0]));
      g.add(part('lance_reservoir_strap', tor(.125, .014, 6, 18), mats.trim, [0, 0, 0], [90 * D, 0, 20 * D]));
    });
    up('rate', 9, yaw, [-.46, .22, .30], (g) => {
      g.add(part('lance_relay_box', box(.22, .26, .18), mats.chassis));
      g.add(part('lance_relay_strip', box(.02, .16, .04), mats.energy_rail, [.12, 0, 0]));
      g.add(part('lance_relay_conduit', cyl(.02, .02, .22, 8), mats.rubber, [0, .20, 0]));
    });
    up('rate', 10, pitch, [.56, .06, -1.28], (g) => {
      g.add(part('lance_autoloader_drum', cyl(.30, .30, .34, 20), mats.steel_hull, [0, 0, 0], [0, 0, 90 * D]));
      g.add(part('lance_autoloader_cap', cyl(.13, .13, .38, 12), mats.chrome, [0, 0, 0], [0, 0, 90 * D]));
      for (let i = 0; i < 5; i++) {
        const a = (i / 5) * Math.PI * 2;
        g.add(part('lance_autoloader_slug' + i, cyl(.055, .055, .30, 10), mats.brass, [0, Math.cos(a) * .19, Math.sin(a) * .19], [0, 0, 90 * D]));
      }
      g.add(part('lance_autoloader_arm', box(.34, .10, .10), mats.steel_plate, [-.20, 0, .22]));
      g.add(part('lance_autoloader_glow', tor(.31, .014, 6, 28), mats.energy_rail, [.18, 0, 0], [0, 90 * D, 0]));
    });

    yaw.add(pitch);
    root.add(yaw);
    return root;
  },

  cue(model, levels) {
    const acc = model.getObjectByName('lance_accelerator');
    if (acc) {
      if (!acc.userData.own) { acc.material = acc.material.clone(); acc.userData.own = true; }
      acc.material.emissiveIntensity = .5 + (levels.damage ?? 1) / 10 * 1.4;
    }
    const shroudScale = .8 + (levels.range ?? 1) / 10 * .5;
    for (const s of ['_l', '_r']) {
      const sh = model.getObjectByName('lance_rail_shroud' + s);
      if (sh) sh.scale.z = shroudScale;
    }
    for (let i = 0; i < 5; i++) {
      const w = model.getObjectByName('lance_coil_winding' + i);
      if (!w) continue;
      if (!w.userData.own) { w.material = w.material.clone(); w.userData.own = true; }
      w.material.emissive.setHex(0x2b5cff);
      w.material.emissiveIntensity = ((levels.rate ?? 1) - 1) / 9 * .55;
    }
  },
};
