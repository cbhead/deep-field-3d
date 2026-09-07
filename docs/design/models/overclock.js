/**
 * Overclock — buff pylon. No damage; boosts nearby towers' fire rate. M4.
 * Not yet typed in the sim; stats are plan intent.
 *
 * No aim rig. `overclock_spin` is the resonance ring; the client scales its
 * speed to the number of towers currently linked.
 */
import { makeUp } from './tower-kit.js';

export const TOWER = {
  id: 'overclock',
  label: 'Overclock',
  swatch: '#ff6f1a',
  stats: { Milestone: 'M4', Kind: 'Buff aura', Applies: 'rate buff', Sim: 'not yet' },
  layers: 'Towers (+ traps L4)',
  rig: null,
  note: 'A pylon with nothing to shoot: a tall prism core under a slow resonance ring. Ember orange is the buff colour here and on Forge\'s Overdrive, so a buffed tower, the pylon, and the faction ability all read as one system.',
  paths: [
    {
      id: 'potency', label: 'Potency', cue: 'Core prism glows hotter',
      steps: {
        2: 'Conductor rings on the core',
        3: 'Capacitor cans on the base',
        4: 'Trap coupler (also buffs traps)',
        5: 'Core stabiliser vanes',
        6: 'Second conductor tier',
        7: 'Resonance aura emitters (+scrap yield in radius)',
        8: 'Emitter braces',
        9: 'Overdrive lamp cluster',
        10: 'Forge-sync crown (syncs with Overdrive)',
      },
    },
    {
      id: 'network', label: 'Network', cue: 'Link nodes light up per linked tower',
      steps: {
        2: 'Relay nodes on the base',
        3: 'Relay cabling',
        4: 'Three link antennae',
        5: 'Antenna insulators',
        6: 'Node indicator lamps',
        7: 'Link dishes on the antennae',
        8: 'Antenna guy-wires',
        9: 'Second node tier',
        10: 'Network lattice',
      },
    },
    {
      id: 'efficiency', label: 'Efficiency', cue: 'Base fins multiply',
      steps: {
        2: 'Heat fins on the base drum',
        3: 'Regulator box',
        4: 'Battery bank (longer buff uptime)',
        5: 'Coolant loop',
        6: 'Second battery bank',
        7: 'Flux governor',
        8: 'Governor linkage',
        9: 'Reserve cell',
        10: 'Efficiency core shroud',
      },
    },
  ],

  build(K) {
    const { D, mats, part, box, cyl, tor, grp, THREE } = K;
    const up = makeUp(K, 'overclock', mats.energy_buff);
    const root = grp('overclock');
    root.add(K.foot('overclock', mats.energy_buff));
    root.add(part('overclock_base_drum', cyl(.52, .60, .46, 24), mats.chassis, [0, .60, 0]));
    root.add(K.boltRing('overclock_base_bolts', .48, 12, .85));
    root.add(K.louvres('overclock_base_vent', .28, .26, 4).translateZ(.56).translateY(.60));
    root.add(K.panelPlate('overclock_base_hatch', .28, .28, [0, .60, -.585]));
    root.add(part('overclock_base_cap', cyl(.36, .52, .10, 24), mats.steel_plate, [0, .88, 0]));
    // Prism core: a tall hex prism, ember-lit, in a three-post cage.
    root.add(part('overclock_core', cyl(.16, .20, 1.60, 6), mats.energy_buff, [0, 1.74, 0]));
    root.add(part('overclock_core_base', cyl(.26, .30, .14, 6), mats.steel_hull, [0, .98, 0]));
    root.add(part('overclock_core_cap', cyl(.20, .24, .10, 6), mats.steel_hull, [0, 2.58, 0]));
    for (let i = 0; i < 3; i++) {
      const a = (i / 3) * Math.PI * 2 + 30 * D, x = Math.cos(a) * .34, z = Math.sin(a) * .34;
      root.add(part('overclock_cage_post' + i, box(.06, 1.70, .06), mats.chassis, [x, 1.78, z], [0, -a, 0]));
      root.add(part('overclock_cage_foot' + i, box(.12, .08, .12), mats.trim, [x, .96, z], [0, -a, 0]));
    }
    root.add(part('overclock_cage_ring_a', tor(.36, .02, 8, 32), mats.trim, [0, 1.30, 0], [90 * D, 0, 0]));
    root.add(part('overclock_cage_ring_b', tor(.36, .02, 8, 32), mats.trim, [0, 2.20, 0], [90 * D, 0, 0]));
    root.add(K.cableRun('overclock_loom', [[.48, .66, -.24], [.40, .84, -.30], [.26, 1.00, -.16]], .024));

    const spin = grp('overclock_spin', [0, 2.72, 0]);
    spin.userData.role = 'cosmeticSpin';
    spin.add(part('overclock_ring', tor(.42, .05, 10, 44), mats.chrome, [0, 0, 0], [90 * D, 0, 0]));
    spin.add(part('overclock_ring_glow', tor(.42, .02, 8, 44), mats.energy_buff, [0, 0, 0], [90 * D, 0, 0]));
    for (let i = 0; i < 3; i++) {
      const a = (i / 3) * Math.PI * 2;
      spin.add(part('overclock_ring_spoke' + i, box(.03, .03, .40), mats.steel_hull, [Math.cos(a) * .21, 0, Math.sin(a) * .21], [0, -a + 90 * D, 0]));
      spin.add(part('overclock_ring_node' + i, box(.10, .08, .08), mats.chassis, [Math.cos(a) * .44, 0, Math.sin(a) * .44], [0, -a, 0]));
    }
    spin.add(part('overclock_ring_hub', cyl(.10, .10, .10, 12), mats.brass));
    spin.add(part('overclock_spire', cyl(.02, .04, .30, 8), mats.chrome, [0, .20, 0]));
    spin.add(part('overclock_spire_tip', new THREE.SphereGeometry(.05, 12, 8), mats.energy_buff, [0, .38, 0]));
    root.add(spin);

    /* ── POTENCY ladder ─────────────────────────────────────────────── */
    up('potency', 2, root, [0, 0, 0], (g) => {
      for (const y of [1.50, 1.98])
        g.add(part('overclock_conductor' + y, tor(.22, .02, 8, 24), mats.brass, [0, y, 0], [90 * D, 0, 0]));
    });
    up('potency', 3, root, [0, .62, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 90 * D;
        g.add(part('overclock_cap' + i, cyl(.09, .09, .30, 14), mats.steel_hull, [Math.cos(a) * .66, 0, Math.sin(a) * .66]));
        g.add(part('overclock_cap_top' + i, cyl(.04, .04, .05, 8), mats.energy_buff, [Math.cos(a) * .66, .17, Math.sin(a) * .66]));
      }
    });
    up('potency', 4, root, [-.62, .74, .22], (g) => {
      g.add(part('overclock_trap_coupler', box(.22, .20, .16), mats.chassis));
      g.add(part('overclock_trap_coupler_face', box(.14, .04, .02), mats.energy_buff, [0, 0, .09]));
      g.add(part('overclock_trap_coupler_pipe', cyl(.02, .02, .22, 8), mats.brass, [.14, -.04, 0], [0, 0, 90 * D]));
    });
    up('potency', 5, root, [0, 1.74, 0], (g) => {
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2;
        g.add(part('overclock_vane' + i, box(.02, .60, .06), mats.steel_plate, [Math.cos(a) * .24, 0, Math.sin(a) * .24], [0, -a, 0]));
      }
    });
    up('potency', 6, root, [0, 0, 0], (g) => {
      for (const y of [1.14, 2.36])
        g.add(part('overclock_conductor_b' + y, tor(.24, .018, 8, 24), mats.brass, [0, y, 0], [90 * D, 0, 0]));
    });
    up('potency', 7, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 30 * D, x = Math.cos(a) * .56, z = Math.sin(a) * .56;
        g.add(part('overclock_aura_emitter' + i, box(.14, .30, .12), mats.steel_hull, [x, 1.60, z], [0, -a, 0]));
        g.add(part('overclock_aura_lens' + i, cyl(.05, .05, .02, 12), mats.energy_buff, [x * 1.12, 1.60, z * 1.12], [0, 0, 90 * D]));
        g.add(part('overclock_aura_arm' + i, box(.20, .04, .04), mats.chassis, [x * .7, 1.60, z * .7], [0, -a, 0]));
      }
    });
    up('potency', 8, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 30 * D, x = Math.cos(a) * .50, z = Math.sin(a) * .50;
        g.add(part('overclock_emitter_brace' + i, box(.03, .40, .03), mats.chrome, [x, 1.30, z], [0, -a, 18 * D]));
      }
    });
    up('potency', 9, spin, [0, .12, 0], (g) => {
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2 + 30 * D;
        g.add(part('overclock_od_lamp' + i, new THREE.SphereGeometry(.03, 8, 6), mats.energy_buff, [Math.cos(a) * .42, 0, Math.sin(a) * .42]));
      }
    });
    up('potency', 10, spin, [0, .40, 0], (g) => {
      g.add(part('overclock_crown_ring', tor(.24, .022, 8, 32), mats.brass, [0, 0, 0], [90 * D, 0, 0]));
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2;
        g.add(part('overclock_crown_flame' + i, new THREE.ConeGeometry(.04, .16, 6), mats.energy_buff, [Math.cos(a) * .24, .08, Math.sin(a) * .24]));
      }
    });

    /* ── NETWORK ladder ─────────────────────────────────────────────── */
    up('network', 2, root, [0, .90, 0], (g) => {
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D;
        g.add(part('overclock_relay' + i, box(.10, .08, .08), mats.chassis, [Math.cos(a) * .46, 0, Math.sin(a) * .46], [0, -a, 0]));
      }
    });
    up('network', 3, root, [0, .90, 0], (g) => {
      g.add(K.cableRun('overclock_relay_cable', [[.33, 0, .33], [0, .04, .48], [-.33, 0, .33], [-.48, .04, 0], [-.33, 0, -.33]], .012));
    });
    up('network', 4, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 90 * D, x = Math.cos(a) * .72, z = Math.sin(a) * .72;
        g.add(part('overclock_antenna' + i, cyl(.02, .03, 1.60, 8), mats.steel_hull, [x, 1.66, z]));
        g.add(part('overclock_antenna_foot' + i, box(.12, .10, .12), mats.trim, [x, .90, z]));
        g.add(part('overclock_antenna_tip' + i, new THREE.SphereGeometry(.04, 10, 8), mats.energy_buff, [x, 2.48, z]));
      }
    });
    up('network', 5, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 90 * D, x = Math.cos(a) * .72, z = Math.sin(a) * .72;
        for (const y of [1.30, 1.90])
          g.add(part('overclock_antenna_insulator' + i + y, cyl(.05, .04, .06, 10), mats.ceramic, [x, y, z]));
      }
    });
    up('network', 6, root, [0, .98, 0], (g) => {
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D;
        g.add(part('overclock_relay_lamp' + i, cyl(.02, .02, .02, 8), mats.energy_buff, [Math.cos(a) * .50, 0, Math.sin(a) * .50], [0, 0, 90 * D]));
      }
    });
    up('network', 7, root, [0, 0, 0], (g) => {
      const pts = [];
      for (let i = 0; i <= 8; i++) { const t = i / 8; pts.push(new THREE.Vector2(t * .16, t * t * .06)); }
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 90 * D, x = Math.cos(a) * .72, z = Math.sin(a) * .72;
        g.add(part('overclock_link_dish' + i, new THREE.LatheGeometry(pts, 18), mats.steel_plate, [x, 2.20, z], [-90 * D, 0, -a + 90 * D]));
        g.add(part('overclock_link_feed' + i, new THREE.SphereGeometry(.03, 10, 8), mats.energy_buff, [x * .92, 2.20, z * .92]));
      }
    });
    up('network', 8, root, [0, 0, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 90 * D, x = Math.cos(a) * .72, z = Math.sin(a) * .72;
        g.add(K.cableRun('overclock_guy' + i, [[x, 2.40, z], [x * .8, 2.20, z * .8], [x * .55, 2.00, z * .55]], .008, mats.chrome));
      }
    });
    up('network', 9, root, [0, 1.10, 0], (g) => {
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2;
        g.add(part('overclock_relay_b' + i, box(.08, .08, .06), mats.chassis, [Math.cos(a) * .40, 0, Math.sin(a) * .40], [0, -a, 0]));
      }
    });
    up('network', 10, root, [0, 2.48, 0], (g) => {
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + 90 * D, b = ((i + 1) / 3) * Math.PI * 2 + 90 * D;
        g.add(K.cableRun('overclock_lattice' + i,
          [[Math.cos(a) * .72, 0, Math.sin(a) * .72], [Math.cos((a + b) / 2) * .60, .10, Math.sin((a + b) / 2) * .60], [Math.cos(b) * .72, 0, Math.sin(b) * .72]],
          .01, mats.brass));
        g.add(K.cableRun('overclock_lattice_in' + i, [[Math.cos(a) * .72, 0, Math.sin(a) * .72], [Math.cos(a) * .36, .16, Math.sin(a) * .36], [0, .24, 0]], .01, mats.brass));
      }
      g.add(part('overclock_lattice_apex', new THREE.SphereGeometry(.05, 12, 8), mats.energy_buff, [0, .24, 0]));
      {
      }
    });

    /* ── EFFICIENCY ladder ──────────────────────────────────────────── */
    up('efficiency', 2, root, [0, .60, 0], (g) => {
      for (let i = 0; i < 8; i++) {
        const a = (i / 8) * Math.PI * 2 + 22 * D;
        g.add(part('overclock_base_fin' + i, box(.02, .28, .12), mats.steel_plate, [Math.cos(a) * .62, 0, Math.sin(a) * .62], [0, -a, 0]));
      }
    });
    up('efficiency', 3, root, [.50, .62, .36], (g) => {
      g.rotation.y = -36 * D;
      g.add(part('overclock_regulator', box(.18, .22, .10), mats.chassis));
      g.add(part('overclock_regulator_dial', cyl(.04, .04, .02, 12), mats.brass, [0, .03, .055], [90 * D, 0, 0]));
    });
    up('efficiency', 4, root, [-.62, .58, -.30], (g) => {
      g.rotation.y = 30 * D;
      g.add(part('overclock_battery', box(.34, .30, .22), mats.steel_hull));
      for (const x of [-.10, 0, .10])
        g.add(part('overclock_battery_cell' + x, box(.06, .20, .02), mats.energy_buff, [x, 0, .115]));
    });
    up('efficiency', 5, root, [0, .74, 0], (g) => {
      g.add(part('overclock_coolant_loop', tor(.58, .02, 8, 40), mats.brass, [0, 0, 0], [90 * D, 0, 0]));
    });
    up('efficiency', 6, root, [.62, .58, -.30], (g) => {
      g.rotation.y = -30 * D;
      g.add(part('overclock_battery_b', box(.34, .30, .22), mats.steel_hull));
      for (const x of [-.10, 0, .10])
        g.add(part('overclock_battery_b_cell' + x, box(.06, .20, .02), mats.energy_buff, [x, 0, .115]));
    });
    up('efficiency', 7, root, [0, .98, -.50], (g) => {
      g.add(part('overclock_governor', cyl(.10, .10, .22, 16), mats.chassis, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('overclock_governor_wheel', cyl(.14, .14, .03, 20), mats.brass, [0, 0, .12], [90 * D, 0, 0]));
      g.add(part('overclock_governor_mount', box(.12, .10, .14), mats.steel_hull, [0, -.10, 0]));
      g.add(part('overclock_governor_glow', tor(.145, .01, 6, 20), mats.energy_buff, [0, 0, .135]));
    });
    up('efficiency', 8, root, [0, .98, -.34], (g) => {
      g.add(part('overclock_linkage', box(.03, .03, .30), mats.chrome, [0, .08, 0], [20 * D, 0, 0]));
      g.add(part('overclock_linkage_pin', cyl(.02, .02, .06, 8), mats.brass, [0, .14, .14], [0, 0, 90 * D]));
    });
    up('efficiency', 9, root, [0, .58, .62], (g) => {
      g.add(part('overclock_reserve_cell', cyl(.10, .10, .34, 14), mats.steel_plate, [0, 0, 0], [90 * D, 0, 0]));
      g.add(part('overclock_reserve_glow', tor(.105, .01, 6, 20), mats.energy_buff, [0, 0, .06]));
    });
    up('efficiency', 10, root, [0, 1.74, 0], (g) => {
      g.add(part('overclock_core_shroud', new THREE.CylinderGeometry(.30, .32, 1.0, 6, 1, true), mats.trim));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2 + 30 * D;
        g.add(part('overclock_shroud_slot' + i, box(.02, .70, .06), mats.energy_buff, [Math.cos(a) * .30, 0, Math.sin(a) * .30], [0, -a, 0]));
      }
    });

    return root;
  },

  cue(model, levels) {
    const core = model.getObjectByName('overclock_core');
    if (core) {
      if (!core.userData.own) { core.material = core.material.clone(); core.userData.own = true; }
      core.material.emissiveIntensity = .5 + ((levels.potency ?? 1) - 1) / 9 * 1.5;
    }
    for (let i = 0; i < 3; i++) {
      const n = model.getObjectByName('overclock_ring_node' + i);
      if (n) n.scale.setScalar(1 + ((levels.network ?? 1) - 1) / 9 * .6);
    }
    const ring = model.getObjectByName('overclock_ring_glow');
    if (ring) {
      if (!ring.userData.own) { ring.material = ring.material.clone(); ring.userData.own = true; }
      ring.material.emissiveIntensity = .6 + ((levels.efficiency ?? 1) - 1) / 9 * 1.2;
    }
  },
};
