/**
 * Arc — tesla chain. Instant arc that jumps between nearby targets.
 * Sim: TowerKind.Tesla is typed (ChainJumps / ChainRange / ChainFalloff on
 * TowerDef) but no Arc row exists in Towers.cs yet — M2. Stats below are the
 * plan's intent, not sim data.
 *
 * No aim rig: the chain leaves the toroid, so the tower never points. The
 * `arc_spin` group is a cosmetic slow rotation of the discharge crown.
 *
 * Path ids follow Towers.cs / DESIGN-BRIEF §3.2: damage (coil stack), range
 * (antenna array — the chain-hop hardware), rate (capacitor bank).
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'arc',
  label: 'Arc',
  swatch: '#f05ae6',
  stats: { Milestone: 'M2', Kind: 'Tesla', Applies: 'shock (L4)', Sim: 'not yet' },
  layers: 'Ground + Air',
  rig: null,
  note: 'Tesla coil, not a gun: the chain arcs from the toroid to the nearest target and hops from there, so there is nothing to aim. `arc_spin` slowly rotates the discharge crown; the client flashes the electrodes on towerFired events.',
  paths: [
    {
      id: 'damage', label: 'Damage', cue: 'Coil windings glow hotter',
      steps: {
        2: 'Grounding straps on the insulator stack',
        3: 'Second electrode ring',
        4: 'Shock capacitor bank (arc applies shock)',
        5: 'Insulator sleeves',
        6: 'Spire extension',
        7: 'Secondary toroid (jumps prefer unshocked targets)',
        8: 'Discharge vanes on the crown',
        9: 'Corona rings up the column',
        10: 'Storm overload cage (reaction amplifier)',
      },
    },
    {
      id: 'range', label: 'Range / Chain', cue: 'Relay knobs brighten per jump',
      steps: {
        2: 'Relay knob ring on the toroid',
        3: 'Chain guide arms',
        4: 'Three satellite electrodes on struts',
        5: 'Relay wiring between electrodes',
        6: 'Second relay tier',
        7: 'Ioniser emitters (extends hop range)',
        8: 'Strut bracing',
        9: 'Hop-count indicator lamps',
        10: 'Chain lattice crown',
      },
    },
    {
      id: 'rate', label: 'Rate / Conduction', cue: 'Charge meter fills faster',
      steps: {
        2: 'Coolant loop on the base drum',
        3: 'Charge meter box',
        4: 'Supercapacitor drum',
        5: 'Bus bars to the column',
        6: 'Base heat fins',
        7: 'Second coil bank',
        8: 'Generator housing',
        9: 'Flywheel',
        10: 'Charge accelerator ring',
      },
    },
  ],

  build(K) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'arc', mats.energy_arc);
    const root = grp('arc');
    root.add(K.foot('arc', mats.energy_arc));
    root.add(part('arc_base_drum', cyl(.58, .64, .50, 24), mats.chassis, [0, .62, 0]));
    root.add(K.boltRing('arc_base_bolts', .52, 12, .89));
    root.add(K.louvres('arc_base_vent', .30, .26, 4).translateZ(.62).translateY(.62));
    root.add(K.panelPlate('arc_base_hatch', .30, .28, [0, .62, -.645]));
    root.add(part('arc_base_cap', cyl(.44, .58, .10, 24), mats.steel_plate, [0, .92, 0]));

    // Insulator stack: ceramic discs alternating with steel spacers.
    for (let i = 0; i < 4; i++) {
      root.add(part('arc_insulator' + i, cyl(.34, .30, .12, 24), mats.ceramic, [0, 1.04 + i * .22, 0]));
      root.add(part('arc_spacer' + i, cyl(.20, .20, .10, 16), mats.steel_hull, [0, 1.15 + i * .22, 0]));
    }
    // Coil column with brass windings.
    root.add(part('arc_coil_core', cyl(.22, .24, 1.0, 20), mats.trim, [0, 2.36, 0]));
    for (let i = 0; i < 8; i++)
      root.add(part('arc_winding' + i, tor(.25, .028, 5, 24), mats.brass, [0, 1.92 + i * .12, 0], [90 * D, 0, 0]));
    root.add(part('arc_coil_cap', cyl(.28, .22, .10, 20), mats.steel_plate, [0, 2.90, 0]));

    // Discharge crown (cosmetic spin).
    const spin = grp('arc_spin', [0, 3.06, 0]);
    spin.userData.role = 'cosmeticSpin';
    spin.add(part('arc_toroid', tor(.42, .11, 10, 32), mats.chrome, [0, 0, 0], [90 * D, 0, 0]));
    spin.add(part('arc_toroid_hub', cyl(.16, .16, .16, 16), mats.steel_hull));
    for (let i = 0; i < 3; i++) {
      const a = (i / 3) * Math.PI * 2;
      spin.add(part('arc_electrode_arm' + i, cyl(.03, .03, .34, 8), mats.chrome, [Math.cos(a) * .55, .06, Math.sin(a) * .55], [0, -a, 90 * D]));
      spin.add(part('arc_electrode' + i, new THREE.SphereGeometry(.09, 12, 8), mats.energy_arc, [Math.cos(a) * .72, .06, Math.sin(a) * .72]));
    }
    spin.add(part('arc_spire', cyl(.02, .05, .46, 10), mats.chrome, [0, .30, 0]));
    spin.add(part('arc_spire_tip', new THREE.SphereGeometry(.06, 14, 10), mats.energy_arc, [0, .55, 0]));
    root.add(spin);
    root.add(K.cableRun('arc_feed_loom', [[.50, .70, -.30], [.40, 1.00, -.36], [.24, 1.30, -.22]], .026));

    /* ── DAMAGE ladder ──────────────────────────────────────────────── */
    up('damage', 2, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 30 * D;
        g.add(part('arc_ground_strap' + i, box(.05, 1.0, .03), mats.brass, [Math.cos(a) * .36, 1.42, Math.sin(a) * .36], [0, -a, 0]));
      }
    });
    up('damage', 3, spin, [0, -.20, 0], (g) => {
      g.add(part('arc_electrode_ring', tor(.60, .022, 8, 40), mats.chrome, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2 + 30 * D;
        g.add(part('arc_electrode_b' + i, new THREE.SphereGeometry(.045, 10, 8), mats.energy_arc, [Math.cos(a) * .60, 0, Math.sin(a) * .60]));
      }
    });
    up('damage', 4, root, [0, .74, 0], (g) => {
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D;
        g.add(part('arc_shock_cap' + i, cyl(.09, .09, .34, 14), mats.steel_hull, [Math.cos(a) * .74, 0, Math.sin(a) * .74]));
        g.add(part('arc_shock_cap_top' + i, cyl(.04, .04, .06, 8), mats.energy_arc, [Math.cos(a) * .74, .20, Math.sin(a) * .74]));
      }
    });
    up('damage', 5, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 4; i++)
        g.add(part('arc_insulator_sleeve' + i, cyl(.37, .37, .05, 24), mats.trim, [0, 1.09 + i * .22, 0]));
    });
    up('damage', 6, spin, [0, .55, 0], (g) => {
      g.add(part('arc_spire_ext', cyl(.015, .02, .40, 8), mats.chrome, [0, .20, 0]));
      g.add(part('arc_spire_ext_tip', new THREE.SphereGeometry(.05, 12, 8), mats.energy_arc, [0, .42, 0]));
    });
    up('damage', 7, spin, [0, .22, 0], (g) => {
      g.add(part('arc_toroid_b', tor(.28, .07, 12, 32), mats.chrome, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('arc_toroid_b_glow', tor(.28, .02, 6, 32), mats.energy_arc, [0, .07, 0], [90 * D, 0, 0]));
    });
    up('damage', 8, spin, [0, 0, 0], (g) => {
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2 + 22 * D;
        g.add(part('arc_vane' + i, box(.02, .14, .10), mats.steel_plate, [Math.cos(a) * .48, .02, Math.sin(a) * .48], [0, -a, 0]));
      }
    });
    up('damage', 9, root, [0, 0, 0], (g) => {
      for (const y of [2.05, 2.45, 2.80])
        g.add(part('arc_corona_ring' + y, tor(.32, .012, 4, 28), mats.energy_arc, [0, y, 0], [90 * D, 0, 0]));
    });
    up('damage', 10, spin, [0, .10, 0], (g) => {
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        g.add(part('arc_cage_rib' + i, tor(.62, .018, 4, 32), mats.brass, [0, 0, 0], [90 * D + 22 * D * Math.cos(a), a, 0]));
      }
      g.add(part('arc_cage_top', tor(.30, .02, 6, 30), mats.energy_arc, [0, .40, 0], [90 * D, 0, 0]));
    });

    /* ── RANGE (chain) ladder ───────────────────────────────────────────────── */
    up('range', 2, spin, [0, .11, 0], (g) => {
      for (let i = 0; i < 12; i++) {
        const a = (i / 12) * Math.PI * 2;
        g.add(part('arc_relay_knob' + i, new THREE.SphereGeometry(.03, 8, 6), mats.chrome, [Math.cos(a) * .42, 0, Math.sin(a) * .42]));
      }
    });
    up('range', 3, spin, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 60 * D;
        g.add(part('arc_guide_arm' + i, box(.03, .03, .50), mats.steel_hull, [Math.cos(a) * .58, -.06, Math.sin(a) * .58], [0, -a + 90 * D, 0]));
      }
    });
    up('range', 4, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 60 * D, x = Math.cos(a) * .86, z = Math.sin(a) * .86;
        g.add(part('arc_sat_strut' + i, cyl(.03, .04, 1.9, 8), mats.steel_hull, [x, 1.86, z]));
        g.add(part('arc_sat_insulator' + i, cyl(.08, .07, .10, 12), mats.ceramic, [x, 2.86, z]));
        g.add(part('arc_sat_electrode' + i, new THREE.SphereGeometry(.09, 14, 10), mats.energy_arc, [x, 2.98, z]));
        g.add(part('arc_sat_foot' + i, box(.16, .10, .16), mats.trim, [x, .92, z]));
      }
    });
    up('range', 5, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 60 * D, b = ((i + 1) / 3) * Math.PI * 2 + 60 * D;
        g.add(K.cableRun('arc_relay_wire' + i,
          [[Math.cos(a) * .86, 2.90, Math.sin(a) * .86], [Math.cos((a + b) / 2) * .78, 2.70, Math.sin((a + b) / 2) * .78], [Math.cos(b) * .86, 2.90, Math.sin(b) * .86]],
          .012, mats.chrome));
      }
    });
    up('range', 6, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 60 * D, x = Math.cos(a) * .86, z = Math.sin(a) * .86;
        g.add(part('arc_sat_insulator_b' + i, cyl(.07, .06, .10, 12), mats.ceramic, [x, 2.30, z]));
        g.add(part('arc_sat_electrode_b' + i, new THREE.SphereGeometry(.07, 12, 8), mats.energy_arc, [x, 2.40, z]));
      }
    });
    up('range', 7, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 60 * D, x = Math.cos(a) * .86, z = Math.sin(a) * .86;
        g.add(part('arc_ioniser' + i, box(.16, .12, .10), mats.chassis, [x, 1.70, z], [0, -a, 0]));
        g.add(part('arc_ioniser_pip' + i, box(.06, .04, .02), mats.energy_arc, [x * 1.08, 1.70, z * 1.08], [0, -a, 0]));
      }
    });
    up('range', 8, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 60 * D;
        g.add(part('arc_sat_brace' + i, box(.03, .03, .44), mats.steel_hull, [Math.cos(a) * .62, 1.30, Math.sin(a) * .62], [0, -a + 90 * D, 0]));
      }
    });
    up('range', 9, root, [-.40, .78, .44], (g) => {
      g.add(part('arc_hop_panel', box(.20, .12, .04), mats.trim));
      for (let i = 0; i < 4; i++)
        g.add(part('arc_hop_lamp' + i, cyl(.018, .018, .02, 8), mats.energy_arc, [-.06 + i * .04, 0, .025], [90 * D, 0, 0]));
    });
    up('range', 10, spin, [0, .28, 0], (g) => {
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        g.add(part('arc_lattice_spoke' + i, box(.02, .02, .60), mats.brass, [Math.cos(a) * .30, 0, Math.sin(a) * .30], [0, -a + 90 * D, 0]));
      }
      g.add(part('arc_lattice_ring', tor(.60, .014, 6, 40), mats.energy_arc, [0, 0, 0], [90 * D, 0, 0]));
    });

    /* ── RATE (conduction) ladder ──────────────────────────────────────────── */
    up('rate', 2, root, [0, .62, 0], (g) => {
      g.add(part('arc_coolant_loop', tor(.66, .022, 8, 40), mats.brass, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('arc_coolant_pump', box(.14, .14, .12), mats.steel_hull, [.66, 0, 0]));
    });
    up('rate', 3, root, [.50, .64, .34], (g) => {
      g.rotation.y = -34 * D;
      g.add(part('arc_meter_box', box(.18, .22, .10), mats.chassis));
      g.add(part('arc_meter_face', box(.12, .06, .015), mats.energy_arc, [0, .02, .055]));
    });
    up('rate', 4, root, [-.74, .58, -.10], (g) => {
      g.add(part('arc_supercap', cyl(.20, .20, .52, 20), mats.steel_hull));
      g.add(part('arc_supercap_cap', cyl(.08, .08, .56, 10), mats.chrome));
      for (const y of [-.18, 0, .18])
        g.add(part('arc_supercap_rib' + y, tor(.205, .014, 6, 24), mats.trim, [0, y, 0], [90 * D, 0, 0]));
      g.add(part('arc_supercap_gauge', box(.02, .34, .04), mats.energy_arc, [.205, 0, 0]));
    });
    up('rate', 5, root, [0, 0, 0], (g) => {
      for (const s of [-1, 1])
        g.add(part('arc_bus_bar' + (s < 0 ? '_l' : '_r'), box(.05, .70, .04), mats.brass, [s * .30, 1.50, .36]));
      g.add(part('arc_bus_bridge', box(.66, .05, .04), mats.brass, [0, 1.86, .36]));
    });
    up('rate', 6, root, [0, .62, 0], (g) => {
      for (let i = 0; i < 10; i++) {
        const a = (i / 10) * Math.PI * 2 + 18 * D;
        g.add(part('arc_base_fin' + i, box(.02, .30, .14), mats.steel_plate, [Math.cos(a) * .68, 0, Math.sin(a) * .68], [0, -a, 0]));
      }
    });
    up('rate', 7, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 5; i++)
        g.add(part('arc_winding_b' + i, tor(.29, .022, 5, 24), mats.brass, [0, 1.98 + i * .19, 0], [90 * D, 0, 0]));
    });
    up('rate', 8, root, [.72, .62, .28], (g) => {
      g.rotation.y = -20 * D;
      g.add(part('arc_generator', box(.30, .30, .40), mats.chassis));
      g.add(K.louvres('arc_generator_vent', .22, .20, 3).translateZ(.21));
      g.add(part('arc_generator_shaft', cyl(.04, .04, .14, 10), mats.chrome, [0, 0, -.26], [90 * D, 0, 0]));
    });
    up('rate', 9, root, [.72, .62, -.14], (g) => {
      g.rotation.y = -20 * D;
      g.add(part('arc_flywheel', cyl(.22, .22, .06, 28), mats.steel_plate, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('arc_flywheel_hub', cyl(.06, .06, .10, 12), mats.brass, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        g.add(part('arc_flywheel_spoke' + i, box(.03, .30, .02), mats.trim, [0, 0, 0], [0, 0, a]));
      }
    });
    up('rate', 10, root, [0, 1.64, 0], (g) => {
      g.add(part('arc_accel_ring', tor(.48, .05, 10, 44), mats.chassis, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('arc_accel_glow', tor(.48, .02, 8, 44), mats.energy_arc, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D;
        g.add(part('arc_accel_strut' + i, box(.04, .04, .24), mats.steel_hull, [Math.cos(a) * .36, 0, Math.sin(a) * .36], [0, -a + 90 * D, 0]));
      }
    });

    return root;
  },

  cue(model, levels) {
    for (let i = 0; i < 8; i++) {
      const w = model.getObjectByName('arc_winding' + i);
      if (!w) continue;
      if (!w.userData.own) { w.material = w.material.clone(); w.userData.own = true; }
      w.material.emissive.setHex(0xf05ae6);
      w.material.emissiveIntensity = ((levels.damage ?? 1) - 1) / 9 * .6;
    }
    for (let i = 0; i < 3; i++) {
      const e = model.getObjectByName('arc_electrode' + i);
      if (e) e.scale.setScalar(.9 + ((levels.range ?? 1) - 1) / 9 * .5);
    }
    const face = model.getObjectByName('arc_meter_face');
    if (face) face.scale.x = .3 + ((levels.rate ?? 1) - 1) / 9 * .7;
  },
};
