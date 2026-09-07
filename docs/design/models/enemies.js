/**
 * Deep Field 3D — enemy models (DESIGN-BRIEF §3.1), close-up pass.
 * Metres, Y-up, feet at y = 0, **facing −Z** (Godot forward). Static meshes with
 * simple pivot groups where the brief asks (`skiff_wing_l/r`, `mole_drill`);
 * motion is procedural in code. Albedo is flat `carapace` steel so runtime tint
 * (hp / status / elite) has headroom. Weak points and eyes carry the emissive.
 *
 * Detail vocabulary (shared so the roster reads as one species of machine):
 * two-segment limbs with joint knuckles and clawed feet, layered armour plates
 * with rivet rows, louvred vents, hydraulic rams, cable looms, dorsal spine fins.
 */
import { makeKit } from './tower-kit.js';

export function makeEnemyKit(THREE) {
  const K = makeKit(THREE);
  const { mats } = K;
  const mat = (name, color, o = {}) => { const m = new THREE.MeshStandardMaterial({ color, roughness: .7, metalness: .2, flatShading: false, ...o }); m.name = name; mats[name] = m; return m; };
  mat('carapace', 0x7d8ba3);
  mat('carapace_dark', 0x3e4a63, { roughness: .8 });
  mat('carapace_pale', 0xa6b2c6, { roughness: .6 });
  mat('joint', 0x242e43, { roughness: .9, metalness: .1, flatShading: false });
  mat('dirt', 0x5a4a3a, { roughness: .95, metalness: 0 });
  mat('weak', 0xe9614c, { roughness: .3, metalness: 0, emissive: new THREE.Color(0xe9614c), emissiveIntensity: 1.1, flatShading: false });
  mat('eye', 0xf4dca4, { roughness: .3, emissive: new THREE.Color(0xf4dca4), emissiveIntensity: 1.0, flatShading: false });
  mat('thruster', 0x22d3ee, { roughness: .3, emissive: new THREE.Color(0x22d3ee), emissiveIntensity: 1.0 });
  mat('shield_bubble', 0x2fb4be, { roughness: .1, metalness: 0, transparent: true, opacity: .32, emissive: new THREE.Color(0x2fb4be), emissiveIntensity: .5, flatShading: false, side: THREE.DoubleSide, depthWrite: false });
  mat('shield_glass', 0x2fb4be, { roughness: .15, metalness: 0, transparent: true, opacity: .55, emissive: new THREE.Color(0x2fb4be), emissiveIntensity: .35, flatShading: false });
  mat('mote_sac', 0xe3bc66, { roughness: .4, emissive: new THREE.Color(0xe3bc66), emissiveIntensity: .35, flatShading: false });
  // M3/M4 roster
  mat('shade_shell', 0x1a2030, { roughness: .35, metalness: .4, transparent: true, opacity: .55, flatShading: false });
  mat('shade_shimmer', 0x7fa6ff, { roughness: .2, metalness: 0, transparent: true, opacity: .22, emissive: new THREE.Color(0x7fa6ff), emissiveIntensity: .6, flatShading: false, side: THREE.DoubleSide, depthWrite: false });
  mat('shade_eye', 0xb8c8ff, { roughness: .3, emissive: new THREE.Color(0xb8c8ff), emissiveIntensity: 1.3, flatShading: false });
  mat('mender_glass', 0x7fe65a, { roughness: .15, metalness: 0, transparent: true, opacity: .6, emissive: new THREE.Color(0x7fe65a), emissiveIntensity: .7, flatShading: false });
  mat('mender_beam', 0xb6ff8a, { roughness: .3, metalness: 0, transparent: true, opacity: .35, emissive: new THREE.Color(0x7fe65a), emissiveIntensity: 1.0, flatShading: false, side: THREE.DoubleSide, depthWrite: false });
  mat('ram_plate', 0x5a5f6a, { roughness: .55, metalness: .7 });
  mat('ram_rust', 0x6b3f2a, { roughness: .9, metalness: .2 });
  mat('engine_glow', 0xff6f1a, { roughness: .3, emissive: new THREE.Color(0xff6f1a), emissiveIntensity: 1.2, flatShading: false });
  mat('brood_sac', 0xe3bc66, { roughness: .35, transparent: true, opacity: .8, emissive: new THREE.Color(0xe3bc66), emissiveIntensity: .5, flatShading: false });
  mat('brood_membrane', 0x8a6a3a, { roughness: .6, transparent: true, opacity: .7, flatShading: false, side: THREE.DoubleSide });
  mat('gild', 0xe0b34a, { roughness: .2, metalness: 1.0 });
  mat('gild_eye', 0xffe08a, { roughness: .3, emissive: new THREE.Color(0xffd54a), emissiveIntensity: 1.3, flatShading: false });
  mat('jugg_plate', 0x4a5160, { roughness: .6, metalness: .7 });
  mat('volt_node', 0xf05ae6, { roughness: .3, emissive: new THREE.Color(0xf05ae6), emissiveIntensity: 1.4, flatShading: false });
  mat('rage_glow', 0xff2e4a, { roughness: .3, emissive: new THREE.Color(0xff2e4a), emissiveIntensity: 1.6, flatShading: false });
  mat('leaper_skin', 0x6a7a5a, { roughness: .6, metalness: .25 });
  mat('leaper_pad', 0x2a3226, { roughness: .95 });
  mat('shell_plate', 0x8d99ad, { roughness: .5, metalness: .55 });
  mat('shell_seam', 0xff6f1a, { roughness: .3, emissive: new THREE.Color(0xff6f1a), emissiveIntensity: 1.1, flatShading: false });
  mat('swift_fin', 0x7fe65a, { roughness: .3, metalness: .2, transparent: true, opacity: .75, emissive: new THREE.Color(0x7fe65a), emissiveIntensity: .5, flatShading: false, side: THREE.DoubleSide });
  return K;
}

/* ── shared enemy greebles ────────────────────────────────────────────── */
const V = (THREE, p) => new THREE.Vector3(...p);
function seg(K, name, from, to, r0, r1, material) {
  const { THREE, mats } = K;
  const a = V(THREE, from), b = V(THREE, to), len = a.distanceTo(b);
  const m = new THREE.Mesh(new THREE.CylinderGeometry(r1, r0, len, 7), material || mats.carapace_dark);
  m.name = name; m.position.copy(a).lerp(b, .5);
  m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize());
  return m;
}
/** Two-segment limb: hip → knee → ankle, knuckle spheres, armour shin plate, clawed foot. */
function limb(K, name, hip, knee, ankle, r = .06, toes = 3) {
  const { THREE, mats, grp, part } = K;
  const g = grp(name);
  g.add(seg(K, name + '_thigh', hip, knee, r * 1.15, r * .9));
  g.add(seg(K, name + '_shin', knee, ankle, r * .85, r * .7));
  g.add(part(name + '_hip_joint', new THREE.SphereGeometry(r * 1.35, 10, 8), mats.joint, hip));
  g.add(part(name + '_knee', new THREE.SphereGeometry(r * 1.2, 10, 8), mats.joint, knee));
  const kneeCap = part(name + '_knee_cap', new THREE.BoxGeometry(r * 2.4, r * 2.2, r * 1.2), mats.carapace, knee);
  const dir = V(THREE, ankle).sub(V(THREE, knee)).normalize();
  kneeCap.position.add(new THREE.Vector3(0, 0, -r * 1.1));
  kneeCap.quaternion.setFromUnitVectors(new THREE.Vector3(0, -1, 0), dir);
  g.add(kneeCap);
  const shin = seg(K, name + '_shin_plate', knee, ankle, r * 1.05, r * .9, mats.carapace);
  shin.scale.set(.6, .55, .6); shin.position.z -= r * .6;
  g.add(shin);
  g.add(part(name + '_ankle', new THREE.SphereGeometry(r * 1.05, 10, 8), mats.joint, ankle));
  const foot = grp(name + '_foot', [ankle[0], 0, ankle[2]]);
  foot.add(part(name + '_sole', new THREE.BoxGeometry(r * 2.8, r * .9, r * 3.4), mats.carapace_dark, [0, r * .5, -r * .3]));
  for (let i = 0; i < toes; i++) {
    const x = (i - (toes - 1) / 2) * r * 1.4;
    const toe = part(name + '_toe' + i, new THREE.ConeGeometry(r * .5, r * 2.2, 5), mats.chrome, [x, r * .45, -r * 2.6]);
    toe.rotation.x = -Math.PI / 2; foot.add(toe);
  }
  foot.add(part(name + '_heel_spur', new THREE.ConeGeometry(r * .4, r * 1.4, 5), mats.chrome, [0, r * .5, r * 1.6], [Math.PI / 2, 0, 0]));
  g.add(foot);
  return g;
}
/** Layered armour plate: pale face on a dark backing, riveted corners. */
function plate(K, name, w, h, pos, rot = [0, 0, 0], face = null) {
  const { THREE, mats, grp, part } = K;
  const g = grp(name, pos); g.rotation.set(...rot);
  g.add(part(name + '_back', new THREE.BoxGeometry(w + .04, h + .04, .03), mats.carapace_dark, [0, 0, .015]));
  g.add(part(name + '_face', new THREE.BoxGeometry(w, h, .035), face || mats.carapace_pale, [0, 0, -.01]));
  const rivet = new THREE.CylinderGeometry(.014, .014, .012, 6);
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) {
    const m = new THREE.Mesh(rivet, mats.chrome); m.name = `${name}_rivet${sx}${sy}`;
    m.position.set(sx * (w / 2 - .035), sy * (h / 2 - .035), -.03); m.rotation.x = Math.PI / 2; g.add(m);
  }
  return g;
}
/** Dorsal spine: a row of shrinking fins along +Z from `from`. */
function spine(K, name, from, count, step, h, material) {
  const { THREE, mats, grp } = K;
  const g = grp(name, from);
  for (let i = 0; i < count; i++) {
    const s = 1 - i / (count + 1);
    const m = new THREE.Mesh(new THREE.ConeGeometry(.035 * s + .01, h * s, 4), material || mats.carapace_dark);
    m.name = name + '_' + i; m.position.set(0, h * s / 2, i * step); m.rotation.y = Math.PI / 4; g.add(m);
  }
  return g;
}
const eyeSlit = (K, name, pos, w = .18) => K.part(name, new K.THREE.BoxGeometry(w, .03, .02), K.mats.eye, pos);
const lens = (K, name, pos, r = .06) => {
  const { THREE, mats, grp, part } = K;
  const g = grp(name, pos);
  g.add(part(name + '_housing', new THREE.CylinderGeometry(r * 1.4, r * 1.5, r * .8, 10), mats.joint, [0, 0, 0], [Math.PI / 2, 0, 0]));
  g.add(part(name + '_glass', new THREE.SphereGeometry(r, 12, 8), mats.eye, [0, 0, -r * .5]));
  return g;
};

export const ENEMIES = [
  {
    id: 'drifter', file: 'enemy_drifter.glb', label: 'Drifter', size: '1.6 m', swatch: '#7d8ba3',
    stats: { Hp: '20', Speed: '2.5 m/s', Bounty: '6', Layer: 'Ground' },
    note: 'The reference grunt. Hunched slab torso on two reverse-jointed stilt legs, backpack core pod, forearm blades, one visor slit. Every other enemy is read against this silhouette.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('drifter');
      g.add(limb(K, 'drifter_leg_l', [-.20, .76, .02], [-.28, .42, -.16], [-.22, .08, .06], .055));
      g.add(limb(K, 'drifter_leg_r', [.20, .76, .02], [.28, .42, -.16], [.22, .08, .06], .055));
      g.add(part('drifter_pelvis', box(.46, .16, .28), mats.carapace_dark, [0, .80, 0]));
      g.add(plate(K, 'drifter_pelvis_plate', .30, .12, [0, .80, -.145]));
      g.add(K.hydraulic('drifter_spine_ram_l', [-.14, .86, .10], [-.12, 1.06, .18], .8));
      g.add(K.hydraulic('drifter_spine_ram_r', [.14, .86, .10], [.12, 1.06, .18], .8));
      g.add(part('drifter_torso', box(.50, .58, .30), mats.carapace, [0, 1.18, .02], [8 * D, 0, 0]));
      g.add(plate(K, 'drifter_chest_upper', .40, .22, [0, 1.32, -.15], [8 * D, 0, 0]));
      g.add(plate(K, 'drifter_chest_lower', .34, .18, [0, 1.06, -.11], [8 * D, 0, 0]));
      g.add(K.louvres('drifter_rib_vent_l', .10, .18, 3).translateX(-.26).translateY(1.14).rotateY(-90 * D));
      g.add(K.louvres('drifter_rib_vent_r', .10, .18, 3).translateX(.26).translateY(1.14).rotateY(90 * D));
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        g.add(part('drifter_pauldron' + sfx, new THREE.CylinderGeometry(.14, .17, .12, 6), mats.carapace_pale, [s * .34, 1.44, .02], [0, 0, s * 14 * D]));
        g.add(part('drifter_shoulder' + sfx, new THREE.SphereGeometry(.08, 10, 8), mats.joint, [s * .34, 1.36, .02]));
        g.add(seg(K, 'drifter_upper_arm' + sfx, [s * .36, 1.34, .02], [s * .42, 1.02, .10], .05, .045));
        g.add(part('drifter_elbow' + sfx, new THREE.SphereGeometry(.06, 10, 8), mats.joint, [s * .42, 1.02, .10]));
        g.add(seg(K, 'drifter_forearm' + sfx, [s * .42, 1.02, .10], [s * .40, .74, -.06], .045, .04));
        g.add(part('drifter_blade' + sfx, box(.02, .34, .10), mats.chrome, [s * .40, .80, -.10], [10 * D, 0, 0]));
        g.add(part('drifter_blade_edge' + sfx, box(.008, .30, .03), mats.steel_plate, [s * .40, .80, -.16], [10 * D, 0, 0]));
      }
      g.add(part('drifter_neck', cyl(.07, .09, .10, 8), mats.joint, [0, 1.50, -.08]));
      g.add(part('drifter_head', box(.30, .20, .30), mats.carapace, [0, 1.60, -.10]));
      g.add(plate(K, 'drifter_visor', .24, .10, [0, 1.60, -.26], [0, 0, 0], mats.carapace_dark));
      g.add(eyeSlit(K, 'drifter_eye', [0, 1.60, -.28], .18));
      g.add(part('drifter_antenna', cyl(.008, .012, .22, 6), mats.chrome, [.12, 1.80, -.02]));
      g.add(part('drifter_antenna_tip', new THREE.SphereGeometry(.018, 8, 6), mats.weak, [.12, 1.92, -.02]));
      // Backpack core pod — the exposed weak point.
      g.add(part('drifter_pack', box(.34, .36, .18), mats.carapace_dark, [0, 1.20, .24]));
      g.add(part('drifter_core_housing', cyl(.09, .09, .06, 10), mats.joint, [0, 1.22, .34], [90 * D, 0, 0]));
      g.add(part('drifter_core', new THREE.SphereGeometry(.065, 12, 10), mats.weak, [0, 1.22, .35]));
      for (const s of [-1, 1]) g.add(part('drifter_exhaust' + (s < 0 ? '_l' : '_r'), cyl(.035, .04, .14, 8), mats.trim, [s * .12, 1.42, .30], [-30 * D, 0, 0]));
      g.add(K.cableRun('drifter_loom_l', [[-.14, 1.04, .30], [-.22, .92, .22], [-.20, .82, .12]], .016));
      g.add(K.cableRun('drifter_loom_r', [[.14, 1.04, .30], [.22, .92, .22], [.20, .82, .12]], .016));
      g.add(spine(K, 'drifter_spine', [0, 1.46, .06], 4, .07, .10));
      g.scale.setScalar(.88); // authored tall; 1.6 m to the head top, antenna above
      return g;
    },
  },
  {
    id: 'mote', file: 'enemy_mote.glb', label: 'Mote', size: '0.9 m', swatch: '#e3bc66',
    stats: { Hp: '7', Speed: '3.4 m/s', Bounty: '2', Layer: 'Ground' },
    note: 'Swarm chaff — instantly "the small one". A faceted pod on three jointed legs, tri-petal jaw around a single lens, dorsal fins, amber sac underneath. Scatters 2.8 m across the path so it reads as spray, not a line.',
    build(K) {
      const { part, grp, mats, THREE, D, cyl } = K;
      const g = grp('mote');
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + Math.PI / 2, c = Math.cos(a), s = Math.sin(a);
        g.add(limb(K, 'mote_leg' + i, [c * .16, .52, s * .16], [c * .40, .40, s * .40], [c * .36, .06, s * .36], .032, 2));
      }
      g.add(part('mote_body', new THREE.IcosahedronGeometry(.30, 1), mats.carapace, [0, .58, 0]));
      g.add(part('mote_body_band', new THREE.TorusGeometry(.29, .02, 6, 12), mats.carapace_dark, [0, .58, 0], [90 * D, 0, 0]));
      g.add(part('mote_body_band_b', new THREE.TorusGeometry(.27, .015, 6, 12), mats.carapace_dark, [0, .58, 0], [0, 0, 90 * D]));
      g.add(part('mote_collar', cyl(.16, .20, .10, 8), mats.carapace_dark, [0, .58, -.24], [90 * D, 0, 0]));
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + Math.PI / 2;
        g.add(part('mote_jaw' + i, new THREE.ConeGeometry(.05, .18, 4), mats.chrome, [Math.cos(a) * .14, .58 + Math.sin(a) * .14, -.36], [-100 * D, 0, -a]));
      }
      g.add(lens(K, 'mote_eye', [0, .58, -.30], .07));
      g.add(spine(K, 'mote_spine', [0, .84, -.10], 3, .10, .10));
      for (const s of [-1, 1]) g.add(part('mote_vent' + (s < 0 ? '_l' : '_r'), new THREE.BoxGeometry(.03, .10, .04), mats.trim, [s * .28, .62, .06]));
      g.add(part('mote_sac', new THREE.SphereGeometry(.10, 12, 10), mats.mote_sac, [0, .38, .14]));
      g.add(part('mote_sac_clamp', new THREE.TorusGeometry(.10, .012, 6, 12), mats.trim, [0, .40, .14], [70 * D, 0, 0]));
      return g;
    },
  },
  {
    id: 'monolith', file: 'enemy_monolith.glb', label: 'Monolith', size: '3.8 m', swatch: '#3e4a63',
    stats: { Hp: '150', Speed: '1.1 m/s', Bounty: '34', Sight: 'blocks' },
    note: 'A walking wall. The 2.4 m slab is the mechanic — it physically occludes sightlines — so the front is a tiled plate field and everything mechanical (spine, rams, core) hangs off the back.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('monolith');
      for (const s of [-1, 1]) for (const z of [-1, 1]) {
        const sfx = `${s < 0 ? '_l' : '_r'}${z < 0 ? '_f' : '_b'}`;
        g.add(part('monolith_leg' + sfx, cyl(.16, .20, .70, 8), mats.carapace_dark, [s * .80, .50, z * .40]));
        g.add(part('monolith_knee' + sfx, new THREE.SphereGeometry(.22, 10, 8), mats.joint, [s * .80, .86, z * .40]));
        g.add(part('monolith_knee_guard' + sfx, cyl(.24, .28, .18, 6), mats.carapace, [s * .80, .92, z * .40]));
        g.add(part('monolith_foot' + sfx, cyl(.28, .34, .16, 6), mats.trim, [s * .80, .08, z * .40]));
        for (let i = 0; i < 3; i++) {
          const a = (i / 3) * Math.PI * 2 + (z < 0 ? -90 : 90) * D;
          g.add(part(`monolith_toe${sfx}_${i}`, new THREE.ConeGeometry(.07, .22, 5), mats.chrome, [s * .80 + Math.cos(a) * .34, .08, z * .40 + Math.sin(a) * .34], [Math.PI / 2, 0, -a - Math.PI / 2]));
        }
        g.add(K.hydraulic(`monolith_leg_ram${sfx}`, [s * .62, 1.14, z * .30], [s * .78, .70, z * .48], 1.1));
      }
      g.add(part('monolith_pelvis', box(2.0, .40, .96), mats.carapace_dark, [0, 1.10, 0]));
      g.add(K.boltRing('monolith_pelvis_bolts', .40, 8, 1.31));
      g.add(K.hazardStripes('monolith_pelvis_hazard', 1.6, .10, [0, 1.10, -.485], [0, 0, 0], 8));
      // The slab: dark core, 3×3 tiled front, grooves, corner armour.
      g.add(part('monolith_slab', box(2.40, 2.40, .56), mats.carapace, [0, 2.44, 0]));
      for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++)
        g.add(plate(K, `monolith_tile_${i}${j}`, .66, .66, [(i - 1) * .74, 1.70 + j * .74, -.30], [0, 0, 0], j === 1 && i === 1 ? mats.carapace : mats.carapace_pale));
      for (const s of [-1, 1]) for (const y of [1.32, 3.56]) g.add(part(`monolith_corner${s}${y}`, box(.30, .30, .64), mats.carapace_dark, [s * 1.15, y, 0]));
      for (const s of [-1, 1]) g.add(part('monolith_edge_rail' + (s < 0 ? '_l' : '_r'), box(.08, 2.4, .62), mats.trim, [s * 1.22, 2.44, 0]));
      g.add(part('monolith_crown', box(2.60, .26, .84), mats.carapace_dark, [0, 3.77, 0]));
      g.add(K.louvres('monolith_crown_vent', 1.2, .16, 4).translateY(3.77).translateZ(-.43));
      for (const x of [-.9, 0, .9]) g.add(part('monolith_crown_lamp' + x, cyl(.04, .04, .03, 8), mats.weak, [x, 3.92, -.20]));
      g.add(part('monolith_eye', box(.70, .05, .02), mats.eye, [0, 3.50, -.34]));
      g.add(part('monolith_eye_hood', box(.80, .06, .10), mats.trim, [0, 3.56, -.34]));
      // Back: exposed spine, cooling stacks, the core.
      g.add(part('monolith_spine', cyl(.14, .16, 2.2, 8), mats.joint, [0, 2.44, .36]));
      for (let i = 0; i < 6; i++) g.add(part('monolith_vertebra' + i, cyl(.20, .20, .08, 8), mats.carapace_dark, [0, 1.56 + i * .36, .36]));
      g.add(part('monolith_core_housing', cyl(.26, .26, .14, 12), mats.trim, [0, 2.30, .44], [90 * D, 0, 0]));
      g.add(part('monolith_core', new THREE.SphereGeometry(.18, 16, 12), mats.weak, [0, 2.30, .48]));
      for (const s of [-1, 1]) {
        g.add(K.finStack('monolith_radiator' + (s < 0 ? '_l' : '_r'), 6, .06, .40, .60).translateX(s * .70).translateY(2.80).translateZ(.30));
        g.add(K.hydraulic('monolith_slab_ram' + (s < 0 ? '_l' : '_r'), [s * .50, 1.32, .40], [s * .40, 1.90, .40], 1.3));
        g.add(K.cableRun('monolith_loom' + (s < 0 ? '_l' : '_r'), [[s * .20, 2.30, .50], [s * .50, 1.90, .52], [s * .60, 1.32, .46]], .03));
      }
      return g;
    },
  },
  {
    id: 'skiff', file: 'enemy_skiff.glb', label: 'Skiff', size: '1.5 m wingspan', swatch: '#22d3ee',
    stats: { Hp: '26', Speed: '3.0 m/s', Bounty: '10', Layer: 'Air' },
    note: 'Must read airborne at distance: a flat delta hull with glass canopy, twin belly thrusters, V-tail, and wings on `skiff_wing_l/r` pivots with hinge pods and split ailerons. Origin is the hull underside; the sim supplies altitude.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('skiff');
      g.add(part('skiff_hull', box(.34, .18, .90), mats.carapace, [0, .18, 0]));
      g.add(part('skiff_hull_keel', box(.22, .06, .80), mats.carapace_dark, [0, .08, .02]));
      g.add(part('skiff_nose', new THREE.ConeGeometry(.16, .40, 4), mats.carapace_pale, [0, .18, -.64], [-90 * D, 45 * D, 0]));
      g.add(part('skiff_intake', new THREE.TorusGeometry(.10, .025, 6, 8), mats.trim, [0, .18, -.46]));
      g.add(part('skiff_canopy_frame', box(.22, .12, .34), mats.carapace_dark, [0, .32, -.10]));
      g.add(part('skiff_canopy', box(.18, .09, .30), mats.shield_glass, [0, .34, -.10]));
      g.add(eyeSlit(K, 'skiff_eye', [0, .34, -.26], .12));
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        const w = grp('skiff_wing' + sfx, [s * .17, .20, .05]);
        w.userData.role = 'flapPivot';
        w.add(part('skiff_hinge_pod' + sfx, cyl(.06, .06, .18, 8), mats.joint, [s * .04, 0, 0], [90 * D, 0, 0]));
        w.add(part('skiff_wing_panel' + sfx, box(.56, .035, .48), mats.carapace, [s * .30, 0, .04], [0, s * -12 * D, 0]));
        w.add(plate(K, 'skiff_wing_plate' + sfx, .30, .26, [s * .30, .03, -.02], [-90 * D, 0, s * -12 * D]));
        w.add(part('skiff_aileron' + sfx, box(.40, .025, .12), mats.carapace_dark, [s * .30, -.005, .30], [0, s * -12 * D, 0]));
        w.add(part('skiff_wing_spar' + sfx, box(.54, .05, .04), mats.trim, [s * .30, .0, -.14], [0, s * -12 * D, 0]));
        w.add(part('skiff_wing_tip' + sfx, box(.08, .10, .30), mats.carapace_dark, [s * .59, .02, .10], [0, s * -12 * D, 0]));
        w.add(part('skiff_wing_lamp' + sfx, box(.03, .03, .10), mats.thruster, [s * .625, .02, .10], [0, s * -12 * D, 0]));
        w.add(part('skiff_wing_fin' + sfx, box(.02, .14, .18), mats.carapace_pale, [s * .58, .12, .04], [0, s * -12 * D, 0]));
        g.add(w);
        g.add(part('skiff_thruster_pod' + sfx, cyl(.09, .11, .32, 10), mats.carapace_dark, [s * .12, .06, .18], [90 * D, 0, 0]));
        g.add(part('skiff_nozzle' + sfx, cyl(.07, .10, .08, 10), mats.trim, [s * .12, .06, .36], [90 * D, 0, 0]));
        g.add(part('skiff_exhaust' + sfx, cyl(.06, .03, .06, 10), mats.thruster, [s * .12, .06, .40], [90 * D, 0, 0]));
        g.add(part('skiff_tail_fin' + sfx, box(.03, .26, .26), mats.carapace, [s * .10, .34, .40], [0, 0, s * -30 * D]));
        g.add(part('skiff_tail_lamp' + sfx, box(.02, .04, .04), mats.weak, [s * .17, .45, .44], [0, 0, s * -30 * D]));
      }
      g.add(part('skiff_sensor_ball', new THREE.SphereGeometry(.06, 10, 8), mats.joint, [0, .03, -.30]));
      g.add(part('skiff_sensor_lens', new THREE.SphereGeometry(.03, 8, 6), mats.weak, [0, .0, -.32]));
      g.add(part('skiff_antenna', cyl(.006, .01, .24, 6), mats.chrome, [0, .40, .30], [30 * D, 0, 0]));
      g.add(spine(K, 'skiff_spine', [0, .27, -.02], 4, .08, .06));
      return g;
    },
  },
  {
    id: 'aegis', file: 'enemy_aegis.glb', label: 'Aegis', size: '2.2 m', swatch: '#9aa6b7',
    stats: { Hp: '55', Front: '140° · ×0.25', Rear: '×1.5', Bounty: '18' },
    note: 'Directional armour you answer by moving. Three banded 140° shield plates on struts across the front; the rear is an open cage of coolant pipes around a glowing weak core. Four jointed legs, low and braced.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('aegis');
      for (const s of [-1, 1]) for (const z of [-1, 1]) {
        const sfx = `${s < 0 ? '_l' : '_r'}${z < 0 ? '_f' : '_b'}`;
        g.add(limb(K, 'aegis_leg' + sfx, [s * .34, .84, z * .26], [s * .62, .56, z * .50], [s * .54, .08, z * .44], .065));
        g.add(K.hydraulic('aegis_leg_ram' + sfx, [s * .30, 1.00, z * .30], [s * .58, .60, z * .48], .9));
      }
      g.add(part('aegis_body', box(.82, .66, 1.02), mats.carapace, [0, 1.16, .06]));
      g.add(part('aegis_body_lower', box(.66, .30, .90), mats.carapace_dark, [0, .82, .06]));
      g.add(plate(K, 'aegis_back_plate_a', .60, .40, [0, 1.50, .06], [90 * D, 0, 0]));
      g.add(plate(K, 'aegis_back_plate_b', .60, .30, [0, 1.50, .46], [90 * D, 0, 0], mats.carapace));
      g.add(K.louvres('aegis_flank_vent_l', .30, .18, 4).translateX(-.42).translateY(1.20).translateZ(.20).rotateY(-90 * D));
      g.add(K.louvres('aegis_flank_vent_r', .30, .18, 4).translateX(.42).translateY(1.20).translateZ(.20).rotateY(90 * D));
      g.add(part('aegis_neck', cyl(.10, .12, .16, 8), mats.joint, [0, 1.36, -.44], [60 * D, 0, 0]));
      g.add(part('aegis_head', box(.38, .28, .36), mats.carapace_dark, [0, 1.44, -.54]));
      g.add(plate(K, 'aegis_head_plate', .28, .14, [0, 1.50, -.725], [0, 0, 0], mats.carapace));
      g.add(eyeSlit(K, 'aegis_eye', [0, 1.40, -.73], .22));
      // Front shield: 3 concentric bands, riveted, on struts.
      const arc = 140 * D, wedge = (rt, rb, h, open = true) => new THREE.CylinderGeometry(rt, rb, h, 28, 1, open, Math.PI - arc / 2, arc);
      g.add(part('aegis_shield', wedge(.92, .98, 1.50), mats.steel_plate, [0, 1.30, .20]));
      g.add(part('aegis_shield_band_top', wedge(1.0, 1.0, .40), mats.carapace_pale, [0, 1.86, .20]));
      g.add(part('aegis_shield_band_mid', wedge(1.0, 1.0, .34), mats.carapace_pale, [0, 1.30, .20]));
      g.add(part('aegis_shield_band_low', wedge(1.0, 1.0, .40), mats.carapace_pale, [0, .74, .20]));
      for (const y of [.58, 1.10, 1.50, 2.02]) g.add(part('aegis_shield_rim' + y, wedge(1.03, 1.03, .06), mats.trim, [0, y, .20]));
      for (let i = 0; i < 7; i++) for (const y of [.90, 1.30, 1.70]) {
        const a = Math.PI - arc / 2 + (i + .5) * (arc / 7);
        g.add(part(`aegis_shield_rivet${i}_${y}`, cyl(.03, .03, .03, 6), mats.chrome, [Math.sin(a) * 1.02, y, .20 + Math.cos(a) * 1.02], [Math.PI / 2, 0, -a]));
      }
      g.add(part('aegis_shield_boss', cyl(.20, .26, .16, 8), mats.trim, [0, 1.30, -.84], [90 * D, 0, 0]));
      g.add(part('aegis_shield_boss_lamp', cyl(.06, .06, .03, 8), mats.eye, [0, 1.30, -.93], [90 * D, 0, 0]));
      for (const s of [-1, 1]) for (const y of [.92, 1.68]) g.add(seg(K, `aegis_strut${s}${y}`, [s * .40, y, -.44], [s * .60, y, -.86], .04, .035, mats.carapace_dark));
      g.add(K.hazardStripes('aegis_shield_hazard', 1.1, .08, [0, .46, -.78], [0, 0, 0], 6));
      // Rear: weak core in a pipe cage.
      g.add(part('aegis_core_housing', cyl(.26, .26, .12, 12), mats.trim, [0, 1.16, .60], [90 * D, 0, 0]));
      g.add(part('aegis_weak_core', new THREE.SphereGeometry(.22, 16, 12), mats.weak, [0, 1.16, .70]));
      for (let i = 0; i < 5; i++) {
        const a = (i / 5) * Math.PI * 2 + 18 * D;
        g.add(part('aegis_cage' + i, cyl(.022, .022, .60, 6), mats.chrome, [Math.cos(a) * .32, 1.16 + Math.sin(a) * .32, .74], [Math.PI / 2, 0, 0]));
      }
      g.add(part('aegis_cage_ring', new THREE.TorusGeometry(.32, .02, 6, 20), mats.trim, [0, 1.16, 1.02]));
      g.add(K.cableRun('aegis_coolant_l', [[-.30, 1.42, .58], [-.40, 1.30, .84], [-.28, 1.16, .96]], .022, mats.thruster));
      g.add(K.cableRun('aegis_coolant_r', [[.30, 1.42, .58], [.40, 1.30, .84], [.28, 1.16, .96]], .022, mats.thruster));
      for (const s of [-1, 1]) g.add(part('aegis_exhaust' + (s < 0 ? '_l' : '_r'), cyl(.05, .06, .20, 8), mats.trim, [s * .30, 1.56, .50], [-40 * D, 0, 0]));
      return g;
    },
  },
  {
    id: 'warden', file: 'enemy_warden.glb', label: 'Warden', size: '1.8 m', swatch: '#2fb4be',
    stats: { Hp: '60', Shield: '25 (regen)', Bounty: '16', Layer: 'Ground' },
    note: 'Body only — the bubble is `enemy_warden_shield.glb`, a separate mesh that pops and regrows independently. A finned shield generator on the back feeds three coiled emitter masts on the shoulder ring, so the shield has an obvious source.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('warden');
      g.add(limb(K, 'warden_leg_l', [-.22, .84, .02], [-.30, .46, -.14], [-.24, .08, .08], .06));
      g.add(limb(K, 'warden_leg_r', [.22, .84, .02], [.30, .46, -.14], [.24, .08, .08], .06));
      g.add(part('warden_pelvis', new THREE.CylinderGeometry(.30, .24, .18, 6), mats.carapace_dark, [0, .88, 0]));
      g.add(part('warden_torso', new THREE.CylinderGeometry(.34, .26, .62, 6), mats.carapace, [0, 1.26, 0]));
      g.add(plate(K, 'warden_chest', .34, .30, [0, 1.28, -.30], [0, 0, 0]));
      g.add(plate(K, 'warden_abdomen', .26, .14, [0, 1.02, -.26], [0, 0, 0], mats.carapace_dark));
      for (const s of [-1, 1]) g.add(K.louvres('warden_vent' + (s < 0 ? '_l' : '_r'), .10, .16, 3).translateX(s * .30).translateY(1.20).rotateY(s * 90 * D));
      g.add(part('warden_shoulders', new THREE.CylinderGeometry(.46, .38, .16, 6), mats.carapace_pale, [0, 1.64, 0]));
      g.add(K.boltRing('warden_shoulder_bolts', .40, 6, 1.73, mats.chrome));
      g.add(part('warden_collar', new THREE.TorusGeometry(.20, .03, 6, 6), mats.joint, [0, 1.74, -.04], [90 * D, 0, 0]));
      g.add(part('warden_head', new THREE.CylinderGeometry(.14, .18, .24, 6), mats.carapace_dark, [0, 1.84, -.06]));
      g.add(plate(K, 'warden_visor', .20, .09, [0, 1.84, -.22], [0, 0, 0], mats.carapace));
      g.add(eyeSlit(K, 'warden_eye', [0, 1.84, -.245], .16));
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        g.add(part('warden_shoulder_joint' + sfx, new THREE.SphereGeometry(.08, 10, 8), mats.joint, [s * .40, 1.56, .02]));
        g.add(seg(K, 'warden_upper_arm' + sfx, [s * .42, 1.54, .02], [s * .50, 1.20, .10], .05, .045));
        g.add(part('warden_elbow' + sfx, new THREE.SphereGeometry(.06, 10, 8), mats.joint, [s * .50, 1.20, .10]));
        g.add(seg(K, 'warden_forearm' + sfx, [s * .50, 1.20, .10], [s * .46, .90, -.08], .045, .04));
        g.add(part('warden_fist' + sfx, box(.10, .12, .12), mats.carapace_dark, [s * .46, .86, -.10]));
      }
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + Math.PI / 2, x = Math.cos(a) * .38, z = Math.sin(a) * .38;
        g.add(part('warden_emitter_base' + i, cyl(.05, .06, .08, 6), mats.joint, [x, 1.76, z]));
        g.add(part('warden_emitter' + i, cyl(.025, .035, .30, 6), mats.chrome, [x, 1.94, z]));
        for (let k = 0; k < 3; k++) g.add(part(`warden_emitter_coil${i}_${k}`, new THREE.TorusGeometry(.045, .01, 6, 10), mats.brass, [x, 1.86 + k * .05, z], [90 * D, 0, 0]));
        g.add(part('warden_emitter_tip' + i, new THREE.SphereGeometry(.05, 10, 8), mats.shield_glass, [x, 2.12, z]));
      }
      // Shield generator backpack.
      g.add(part('warden_generator', cyl(.16, .18, .40, 10), mats.carapace_dark, [0, 1.30, .36]));
      g.add(K.finStack('warden_gen_fins', 5, .05, .30, .26).translateY(1.30).translateZ(.42).rotateX(90 * D));
      g.add(part('warden_gen_ring', new THREE.TorusGeometry(.17, .02, 6, 20), mats.shield_glass, [0, 1.46, .36], [90 * D, 0, 0]));
      g.add(part('warden_gen_core', cyl(.08, .08, .05, 10), mats.weak, [0, 1.16, .52], [90 * D, 0, 0]));
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + Math.PI / 2;
        g.add(K.cableRun('warden_feed' + i, [[0, 1.50, .36], [Math.cos(a) * .22, 1.66, .20 + Math.sin(a) * .16], [Math.cos(a) * .38, 1.76, Math.sin(a) * .38]], .014, mats.thruster));
      }
      g.scale.setScalar(.9); // 1.8 m to the head; emitter masts above
      return g;
    },
  },
  {
    id: 'warden_shield', file: 'enemy_warden_shield.glb', label: 'Warden shield', size: 'r 1.15 m', swatch: '#65dce4',
    stats: { Shield: '25', Regen: 'after lull', Blocks: 'burn' },
    note: 'Separate bubble mesh, origin at the Warden\'s feet so it drops in at the same transform. Faceted translucent field with three seam rings and hex contact nodes where the emitter masts touch it; scale 1→0 on pop.',
    build(K) {
      const { part, grp, mats, THREE, D } = K;
      const g = grp('warden_shield');
      g.add(part('warden_shield_bubble', new THREE.IcosahedronGeometry(1.15, 2), mats.shield_bubble, [0, 1.20, 0]));
      g.add(part('warden_shield_seam_h', new THREE.TorusGeometry(1.15, .012, 6, 48), mats.thruster, [0, 1.20, 0], [90 * D, 0, 0]));
      g.add(part('warden_shield_seam_a', new THREE.TorusGeometry(1.15, .008, 6, 48), mats.thruster, [0, 1.20, 0], [30 * D, 0, 0]));
      g.add(part('warden_shield_seam_b', new THREE.TorusGeometry(1.15, .008, 6, 48), mats.thruster, [0, 1.20, 0], [30 * D, 120 * D, 0]));
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + Math.PI / 2;
        g.add(part('warden_shield_node' + i, new THREE.CylinderGeometry(.10, .10, .02, 6), mats.shield_glass, [Math.cos(a) * .38, 2.30, Math.sin(a) * .38]));
      }
      g.add(part('warden_shield_apex', new THREE.CylinderGeometry(.14, .14, .02, 6), mats.shield_glass, [0, 2.35, 0]));
      return g;
    },
  },
  {
    id: 'mole_surfaced', file: 'enemy_mole_surfaced.glb', label: 'Mole (surfaced)', size: '1.4 m', swatch: '#5a4a3a',
    stats: { Hp: '34', Speed: '2.2 m/s', Bounty: '12', Burrows: 'yes' },
    note: 'Low, long, obviously the one that goes under: three overlapping shell segments, a fluted drill on `mole_drill` (spin it), triple-claw digging arms, flank vents, and soil still packed in the plates.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('mole_surfaced');
      // Segmented shell, each ring overlapping the next toward the tail.
      [[-.20, .74, .52], [.18, .70, .48], [.52, .60, .40]].forEach(([z, w, h], i) => {
        g.add(part('mole_segment' + i, new THREE.CylinderGeometry(w / 2, w / 2 * .92, .38, 8), mats.carapace, [0, .06 + h / 2, z], [90 * D, 22.5 * D, 0]));
        g.add(part('mole_segment_ridge' + i, new THREE.TorusGeometry(w / 2 + .01, .02, 6, 8), mats.carapace_dark, [0, .06 + h / 2, z - .18], [0, 22.5 * D, 0]));
        g.add(plate(K, 'mole_back_plate' + i, .28, .22, [0, .06 + h + .02, z], [-90 * D, 0, 0]));
      });
      for (const s of [-1, 1]) for (const z of [-.20, .18]) g.add(K.louvres(`mole_vent${s}${z}`, .12, .10, 2).translateX(s * .32).translateY(.36).translateZ(z).rotateY(s * 90 * D));
      g.add(part('mole_tail', new THREE.ConeGeometry(.16, .40, 8), mats.carapace_dark, [0, .30, .90], [90 * D, 0, 0]));
      g.add(part('mole_tail_lamp', new THREE.SphereGeometry(.04, 8, 6), mats.weak, [0, .30, 1.06]));
      // Head + drill.
      g.add(part('mole_head', box(.54, .40, .36), mats.carapace_pale, [0, .38, -.58]));
      g.add(plate(K, 'mole_brow', .40, .12, [0, .54, -.76], [-20 * D, 0, 0], mats.carapace_dark));
      for (const s of [-1, 1]) g.add(lens(K, 'mole_eye' + (s < 0 ? '_l' : '_r'), [s * .18, .48, -.76], .035));
      const drill = grp('mole_drill', [0, .34, -.78]);
      drill.userData.role = 'spin';
      drill.add(part('mole_drill_hub', cyl(.22, .24, .10, 10), mats.joint, [0, 0, 0], [90 * D, 0, 0]));
      drill.add(part('mole_drill_cone', new THREE.ConeGeometry(.20, .62, 10), mats.chrome, [0, 0, -.36], [-90 * D, 0, 0]));
      for (let i = 0; i < 5; i++) {
        const t = i / 5, r = .19 * (1 - t) + .02;
        drill.add(part('mole_drill_flute' + i, new THREE.TorusGeometry(r, .022, 6, 12), mats.trim, [0, 0, -.12 - t * .52], [0, 0, 0]));
      }
      drill.add(part('mole_drill_tip', new THREE.ConeGeometry(.04, .14, 6), mats.weak, [0, 0, -.72], [-90 * D, 0, 0]));
      g.add(drill);
      // Digging arms with three claws each.
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        g.add(part('mole_shoulder' + sfx, new THREE.SphereGeometry(.09, 10, 8), mats.joint, [s * .34, .30, -.34]));
        g.add(seg(K, 'mole_arm' + sfx, [s * .34, .30, -.34], [s * .52, .16, -.60], .07, .06));
        g.add(part('mole_wrist' + sfx, new THREE.SphereGeometry(.07, 10, 8), mats.joint, [s * .52, .16, -.60]));
        g.add(K.hydraulic('mole_arm_ram' + sfx, [s * .26, .48, -.40], [s * .48, .24, -.58], .8));
        for (let i = 0; i < 3; i++) g.add(part(`mole_claw${sfx}_${i}`, new THREE.ConeGeometry(.045, .26, 4), mats.chrome, [s * (.46 + i * .06), .12 - i * .02, -.76], [-80 * D, 0, s * (i - 1) * 14 * D]));
      }
      g.add(part('mole_core', cyl(.06, .06, .04, 8), mats.weak, [0, .50, .72], [90 * D, 0, 0]));
      // Soil clumps still packed in the plates.
      [[.22, .74, -.10], [-.20, .70, .24], [.10, .60, .50]].forEach((p, i) => g.add(part('mole_soil' + i, new THREE.DodecahedronGeometry(.07 - i * .01, 0), mats.dirt, p)));
      return g;
    },
  },
  {
    id: 'mole_burrowed', file: 'enemy_mole_burrowed.glb', label: 'Mole (burrowed)', size: 'mound 1.2 m', swatch: '#8a6f55',
    stats: { State: 'untargetable', Tell: 'tremor trail' },
    note: 'Dirt mound with the drill tip just breaking through, cracked ground plates lifted around it, and the tremor trail behind (+Z, since it travels −Z) as ridged soil with thrown clods. The enemy is invisible; the ground is not.',
    build(K) {
      const { part, grp, mats, THREE, D } = K;
      const g = grp('mole_burrowed');
      g.add(part('mole_mound', new THREE.ConeGeometry(.62, .34, 9), mats.dirt, [0, .17, 0]));
      g.add(part('mole_mound_crest', new THREE.ConeGeometry(.30, .18, 7), mats.dirt, [0, .40, 0]));
      g.add(part('mole_drill_break', new THREE.ConeGeometry(.08, .22, 8), mats.chrome, [0, .56, 0]));
      g.add(part('mole_drill_break_tip', new THREE.ConeGeometry(.03, .08, 6), mats.weak, [0, .70, 0]));
      for (let i = 0; i < 6; i++) {
        const a = (i / 6) * Math.PI * 2 + 15 * D;
        g.add(part('mole_ground_plate' + i, new THREE.BoxGeometry(.30, .03, .22), mats.concrete, [Math.cos(a) * .58, .10, Math.sin(a) * .58], [Math.sin(a) * -22 * D, -a, Math.cos(a) * 22 * D]));
        g.add(part('mole_clod' + i, new THREE.DodecahedronGeometry(.05 + (i % 3) * .015, 0), mats.dirt, [Math.cos(a) * .80, .04, Math.sin(a) * .80]));
      }
      for (let i = 0; i < 4; i++) {
        const z = .72 + i * .56, s = 1 - i * .18, x = (i % 2 ? .12 : -.10) * s;
        g.add(part('mole_tremor' + i, new THREE.ConeGeometry(.40 * s, .16 * s, 7), mats.dirt, [x, .08 * s, z]));
        g.add(part('mole_tremor_ridge' + i, new THREE.BoxGeometry(.06 * s, .04 * s, .50 * s), mats.dirt, [x, .10 * s, z + .20]));
        g.add(part('mole_tremor_crack' + i, new THREE.BoxGeometry(.04, .01, .44 * s), mats.trim, [x, .005, z + .26]));
        g.add(part('mole_tremor_clod' + i, new THREE.DodecahedronGeometry(.04 * s + .02, 0), mats.dirt, [x + .30 * s, .03, z + .1]));
      }
      g.add(part('mole_fissure_glow', new THREE.RingGeometry(.44, .50, 9), mats.weak, [0, .01, 0], [-90 * D, 0, 0]));
      return g;
    },
  },
  {
    id: 'cluster', file: 'enemy_cluster.glb', label: 'Cluster', size: '1.8 m', swatch: '#e3bc66',
    stats: { Hp: '40', Splits: '5 Motes', Bounty: '10', Layer: 'Ground' },
    note: 'Visibly gravid: five Mote sacs bulge through the carapace behind clamp rings, a dorsal ridge runs over the belly, and a mandibled head hangs low. Four spindly jointed legs keep the belly the silhouette so the split is telegraphed.',
    build(K) {
      const { part, box, grp, mats, THREE, D, cyl } = K;
      const g = grp('cluster');
      for (let i = 0; i < 4; i++) {
        const a = (i / 4) * Math.PI * 2 + 45 * D, c = Math.cos(a), s = Math.sin(a);
        g.add(limb(K, 'cluster_leg' + i, [c * .30, 1.00, s * .30], [c * .70, 1.02, s * .70], [c * .62, .08, s * .62], .045, 2));
        g.add(part('cluster_hip_socket' + i, new THREE.CylinderGeometry(.08, .10, .10, 6), mats.carapace_dark, [c * .34, 1.00, s * .34], [0, -a, 90 * D]));
      }
      g.add(part('cluster_belly', new THREE.IcosahedronGeometry(.62, 1), mats.carapace, [0, 1.10, 0]));
      g.add(part('cluster_belly_under', new THREE.IcosahedronGeometry(.50, 1), mats.carapace_dark, [0, .92, .06]));
      g.add(part('cluster_seam', new THREE.TorusGeometry(.62, .02, 6, 32), mats.trim, [0, 1.10, 0], [90 * D, 0, 0]));
      g.add(part('cluster_seam_b', new THREE.TorusGeometry(.60, .015, 6, 32), mats.trim, [0, 1.10, 0], [0, 0, 90 * D]));
      g.add(spine(K, 'cluster_ridge', [0, 1.66, -.30], 6, .12, .14));
      g.add(part('cluster_neck', cyl(.10, .12, .20, 8), mats.joint, [0, 1.08, -.64], [70 * D, 0, 0]));
      g.add(part('cluster_head', box(.32, .26, .32), mats.carapace_dark, [0, 1.00, -.78]));
      g.add(plate(K, 'cluster_head_plate', .24, .12, [0, 1.06, -.945], [0, 0, 0], mats.carapace));
      g.add(eyeSlit(K, 'cluster_eye', [0, 1.00, -.95], .16));
      for (const s of [-1, 1]) g.add(part('cluster_mandible' + (s < 0 ? '_l' : '_r'), new THREE.ConeGeometry(.04, .24, 4), mats.chrome, [s * .14, .86, -.96], [-70 * D, 0, s * 20 * D]));
      const sacs = [[.44, 1.32, -.30], [-.46, 1.24, -.22], [.34, .80, .40], [-.36, 1.02, .46], [.06, 1.60, .22]];
      sacs.forEach((p, i) => {
        const dir = new THREE.Vector3(...p).sub(new THREE.Vector3(0, 1.10, 0)).normalize();
        g.add(part('cluster_sac' + i, new THREE.SphereGeometry(.20, 14, 10), mats.mote_sac, p));
        const ring = part('cluster_sac_clamp' + i, new THREE.TorusGeometry(.19, .022, 6, 14), mats.trim, new THREE.Vector3(...p).addScaledVector(dir, -.06).toArray());
        ring.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), dir); g.add(ring);
        const eye = part('cluster_sac_eye' + i, new THREE.SphereGeometry(.035, 8, 6), mats.eye, new THREE.Vector3(...p).addScaledVector(dir, .19).toArray());
        g.add(eye);
      });
      for (const s of [-1, 1]) g.add(K.louvres('cluster_vent' + (s < 0 ? '_l' : '_r'), .16, .12, 3).translateX(s * .58).translateY(.98).translateZ(.10).rotateY(s * 90 * D));
      g.add(part('cluster_core', cyl(.06, .06, .04, 8), mats.weak, [0, 1.16, .64], [90 * D, 0, 0]));
      g.add(K.cableRun('cluster_umbilical_a', [[.30, 1.40, -.22], [.10, 1.62, .16], [-.30, 1.30, .38]], .016, mats.mote_sac));
      g.add(K.cableRun('cluster_umbilical_b', [[-.40, 1.14, -.16], [-.10, .82, .10], [.28, .74, .34]], .016, mats.mote_sac));
      return g;
    },
  },
  {
    id: 'shade', file: 'enemy_shade.glb', label: 'Shade', size: '1.7 m', swatch: '#7fa6ff', ms: 'M3',
    stats: { Hp: '34', Speed: '2.6 m/s (×1.45 unseen)', Bounty: '11', Stealth: 'towers blind' },
    note: 'Invisible to towers; only heroes see the shimmer. Tall and thin — a Drifter frame stretched and hollowed: translucent smoked shell over a dark endoskeleton, long forward-raked head with a single pale eye, no backpack (nothing to emit), and a cloak of vertical cloaking vanes that trail behind (`shade_vanes`, sway them). `enemy_shade_shimmer.glb` is the reveal overlay. Detector turns the shell opaque via material tint.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('shade');
      g.add(limb(K, 'shade_leg_l', [-.14, .96, .02], [-.20, .52, -.20], [-.16, .08, .10], .045, 2));
      g.add(limb(K, 'shade_leg_r', [.14, .96, .02], [.20, .52, -.20], [.16, .08, .10], .045, 2));
      g.add(part('shade_pelvis', new THREE.CylinderGeometry(.16, .12, .14, 6), mats.joint, [0, 1.0, 0]));
      g.add(part('shade_spine', cyl(.05, .07, .70, 6), mats.joint, [0, 1.38, .02]));
      for (let i = 0; i < 5; i++) g.add(part('shade_rib' + i, new THREE.TorusGeometry(.14 - i * .012, .012, 5, 10), mats.joint, [0, 1.12 + i * .13, .0], [0, 0, 0]));
      g.add(part('shade_shell', new THREE.CylinderGeometry(.20, .14, .74, 7), mats.shade_shell, [0, 1.40, 0]));
      g.add(part('shade_shell_collar', new THREE.CylinderGeometry(.24, .20, .10, 7), mats.shade_shell, [0, 1.78, 0]));
      g.add(part('shade_core', new THREE.OctahedronGeometry(.06, 0), mats.shade_eye, [0, 1.42, 0]));
      // Long raked head: a blade pointing forward, one eye underneath.
      g.add(part('shade_neck', cyl(.04, .05, .14, 6), mats.joint, [0, 1.90, -.04], [.4, 0, 0]));
      g.add(part('shade_head', box(.12, .10, .46), mats.carapace_dark, [0, 1.98, -.26], [.12, 0, 0]));
      g.add(part('shade_head_shell', box(.15, .06, .40), mats.shade_shell, [0, 2.04, -.24], [.12, 0, 0]));
      g.add(part('shade_eye', new THREE.SphereGeometry(.035, 10, 8), mats.shade_eye, [0, 1.94, -.44]));
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        g.add(seg(K, 'shade_upper_arm' + sfx, [s * .22, 1.72, .02], [s * .30, 1.34, .12], .035, .03));
        g.add(seg(K, 'shade_forearm' + sfx, [s * .30, 1.34, .12], [s * .22, .98, -.16], .03, .02));
        g.add(part('shade_claw' + sfx, new THREE.ConeGeometry(.02, .22, 5), mats.chrome, [s * .21, .84, -.20], [Math.PI, 0, 0]));
      }
      // Cloaking vanes: eight long thin blades hanging from the collar and trailing +Z.
      const vanes = grp('shade_vanes', [0, 1.78, .06]);
      for (let i = 0; i < 8; i++) { const a = (i / 8) * Math.PI * 1.2 - Math.PI * .6 + Math.PI / 2; const x = Math.cos(a) * .22, z = Math.abs(Math.sin(a)) * .10 + .04; vanes.add(part('shade_vane' + i, box(.05, .9 + (i % 3) * .15, .012), mats.shade_shell, [x, -.5 - (i % 3) * .07, z], [.18 + (i % 2) * .06, 0, x * .3])); }
      g.add(vanes);
      g.scale.setScalar(.85);
      return g;
    },
  },
  {
    id: 'shade_shimmer', file: 'enemy_shade_shimmer.glb', label: 'Shade shimmer', size: '1.9 m', swatch: '#b8c8ff', ms: 'M3',
    stats: { Shows: 'to heroes always', Reveal: 'Detector / damage', Fades: 'with speed' },
    note: 'Overlay at the Shade\'s transform: a heat-haze column of stacked translucent rings (`shimmer_spin`), a faint ground disc and a broken outline of the silhouette. Brighter while revealed, near-invisible while it sprints unseen. Towers never draw it.',
    build(K) {
      const { part, grp, mats, THREE } = K;
      const g = grp('shade_shimmer');
      const spin = grp('shimmer_spin');
      for (let i = 0; i < 9; i++) spin.add(part('shimmer_ring' + i, new THREE.TorusGeometry(.26 + Math.sin(i * 1.3) * .06, .012, 6, 24), mats.shade_shimmer, [0, .25 + i * .19, 0], [Math.PI / 2, 0, 0]));
      g.add(spin);
      g.add(part('shimmer_column', new THREE.CylinderGeometry(.22, .30, 1.8, 12, 1, true), mats.shade_shimmer, [0, 1.0, 0]));
      g.add(part('shimmer_disc', new THREE.RingGeometry(.30, .42, 24), mats.shade_shimmer, [0, .02, 0], [-Math.PI / 2, 0, 0]));
      g.add(part('shimmer_head', new THREE.BoxGeometry(.16, .08, .44), mats.shade_shimmer, [0, 1.74, -.22], [.12, 0, 0]));
      return g;
    },
  },
  {
    id: 'mender', file: 'enemy_mender.glb', label: 'Mender', size: '1.9 m', swatch: '#7fe65a', ms: 'M3',
    stats: { Hp: '46', Speed: '2.0 m/s', Heals: '7/s · r 6 m', Bounty: '14' },
    note: 'Kill-it-first silhouette: a hunched carrier on four legs with a tall relay mast and a green reservoir dome — the heal source is the biggest, brightest thing on it. Three articulated emitter arms end in dish heads that aim at allies (`mender_arm0..2`); the dome (`mender_dome`) pulses with the heal tick. Coolant lines run dome → arms so the flow reads.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('mender');
      for (const [sx, sz, n] of [[-1, -1, 'fl'], [1, -1, 'fr'], [-1, 1, 'bl'], [1, 1, 'br']]) g.add(limb(K, 'mender_leg_' + n, [sx * .30, .78, sz * .22], [sx * .50, .50, sz * .38], [sx * .44, .08, sz * .46], .05, 2));
      g.add(part('mender_body', new THREE.CylinderGeometry(.42, .36, .40, 8), mats.carapace, [0, .86, 0]));
      g.add(part('mender_body_skirt', new THREE.CylinderGeometry(.48, .42, .10, 8), mats.carapace_dark, [0, .68, 0]));
      g.add(plate(K, 'mender_front', .30, .18, [0, .88, -.42]));
      g.add(eyeSlit(K, 'mender_eye', [0, .90, -.455], .14));
      g.add(K.hazardStripes('mender_hazard', .9, .06, [0, .70, -.47], [0, 0, 0], 6));
      // Reservoir dome + relay mast.
      g.add(part('mender_dome_ring', new THREE.TorusGeometry(.30, .04, 8, 24), mats.chrome, [0, 1.06, 0], [90 * D, 0, 0]));
      g.add(part('mender_dome', new THREE.SphereGeometry(.30, 16, 12, 0, Math.PI * 2, 0, Math.PI / 2), mats.mender_glass, [0, 1.06, 0]));
      g.add(part('mender_dome_core', new THREE.IcosahedronGeometry(.12, 1), mats.energy_scan, [0, 1.18, 0]));
      g.add(part('mender_mast', cyl(.03, .04, .60, 6), mats.chrome, [0, 1.62, .10]));
      for (let k = 0; k < 4; k++) g.add(part('mender_mast_ring' + k, new THREE.TorusGeometry(.06, .01, 6, 12), mats.brass, [0, 1.40 + k * .12, .10], [90 * D, 0, 0]));
      g.add(part('mender_mast_lamp', new THREE.SphereGeometry(.05, 10, 8), mats.energy_scan, [0, 1.94, .10]));
      // Emitter arms with dish heads.
      for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2 + Math.PI / 6, ax = Math.cos(a) * .36, az = Math.sin(a) * .36;
        const arm = grp('mender_arm' + i, [ax, 1.02, az]); arm.rotation.y = -a;
        arm.add(part('mender_arm_joint' + i, new THREE.SphereGeometry(.06, 10, 8), mats.joint));
        arm.add(part('mender_arm_a' + i, cyl(.03, .035, .34, 6), mats.carapace_dark, [.14, .12, 0], [0, 0, -1.0]));
        arm.add(part('mender_arm_elbow' + i, new THREE.SphereGeometry(.045, 10, 8), mats.joint, [.28, .20, 0]));
        arm.add(part('mender_arm_b' + i, cyl(.025, .03, .30, 6), mats.carapace_dark, [.42, .16, 0], [0, 0, 1.3]));
        arm.add(part('mender_dish' + i, new THREE.SphereGeometry(.11, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2.4), mats.carapace_pale, [.56, .10, 0], [0, 0, -Math.PI / 2]));
        arm.add(part('mender_dish_lamp' + i, new THREE.SphereGeometry(.03, 8, 6), mats.energy_scan, [.60, .10, 0]));
        g.add(arm);
        g.add(K.cableRun('mender_coolant' + i, [[0, 1.10, 0], [ax * .6, 1.16, az * .6], [ax, 1.04, az]], .016, mats.mender_glass));
      }
      g.scale.setScalar(.95);
      return g;
    },
  },
  ...[false, true].map((rage) => ({
    id: rage ? 'ram_enraged' : 'ram', file: rage ? 'enemy_ram_enraged.glb' : 'enemy_ram.glb', label: rage ? 'Ram — enraged' : 'Ram', size: '2.6 m', swatch: rage ? '#ff2e4a' : '#ff6f1a', ms: 'M4',
    stats: { Hp: '70 (+2 flat)', Front: '150° × ×0.35', Rear: '×2.2', Siege: '14 dps · r 6 m', Enrage: '<35% → ×1.6' },
    note: (rage ? 'Below 35% HP: head-plate seams and brow strip lit threat red, engine cylinders and stacks blown to red, flame cones off the exhausts, a hot ring underfoot — ×1.6 speed reads before it moves. ' : '') + 'The siege body: a low quadruped battering engine. Everything armoured is at the front — a wedge head of layered plates with a serrated ram bar and twin tusks — and everything soft is at the back: an exposed engine block with glowing cylinders, exhaust stacks and a fuel cell (`ram_engine`, the weak point; brighten on enrage). Piston legs, tow chains, a hazard skirt. Reads as Aegis\'s question asked while it is hitting your tower.',
    build(K) {
      const { part, box, cyl, grp, D, mats: M0, THREE } = K;
      const mats = rage ? { ...M0, eye: M0.rage_glow, engine_glow: M0.rage_glow, ram_plate: M0.carapace_dark } : M0;
      const g = grp(rage ? 'ram_enraged' : 'ram');
      for (const [sx, sz, n] of [[-1, -1, 'fl'], [1, -1, 'fr'], [-1, 1, 'bl'], [1, 1, 'br']]) g.add(limb(K, 'ram_leg_' + n, [sx * .48, .90, sz * .55], [sx * .70, .50, sz * .70], [sx * .62, .08, sz * .62], .09, 2));
      g.add(part('ram_chassis', box(1.0, .56, 1.7), mats.carapace_dark, [0, 1.02, .10]));
      g.add(K.hazardStripes('ram_skirt_l', 1.6, .10, [-.51, .80, .10], [0, Math.PI / 2, 0], 8));
      g.add(K.hazardStripes('ram_skirt_r', 1.6, .10, [.51, .80, .10], [0, -Math.PI / 2, 0], 8));
      // Head: wedge of layered plates + ram bar + tusks.
      const head = grp('ram_head', [0, 1.06, -.86]);
      head.add(part('ram_wedge', new THREE.CylinderGeometry(.62, .78, .70, 4), mats.ram_plate, [0, 0, -.10], [Math.PI / 2, Math.PI / 4, 0]));
      for (let i = 0; i < 3; i++) head.add(part('ram_head_plate' + i, box(1.10 - i * .18, .12, .36), mats.carapace_pale, [0, .30 - i * .17, -.22 - i * .12], [-.25, 0, 0]));
      head.add(part('ram_bar', box(1.20, .14, .14), mats.ram_plate, [0, -.06, -.52]));
      for (let i = 0; i < 7; i++) head.add(part('ram_tooth' + i, new THREE.ConeGeometry(.05, .14, 4), mats.chrome, [-.54 + i * .18, -.06, -.62], [-Math.PI / 2, 0, 0]));
      for (const s of [-1, 1]) head.add(part('ram_tusk' + (s < 0 ? '_l' : '_r'), new THREE.ConeGeometry(.07, .60, 6), mats.chrome, [s * .55, -.28, -.40], [-1.9, 0, s * .2]));
      head.add(part('ram_eye_l', new THREE.BoxGeometry(.14, .03, .02), mats.eye, [-.22, .04, -.46])); head.add(part('ram_eye_r', new THREE.BoxGeometry(.14, .03, .02), mats.eye, [.22, .04, -.46]));
      head.add(K.boltRing('ram_head_bolts', .50, 8, .36, mats.chrome));
      g.add(head);
      for (const s of [-1, 1]) g.add(K.hydraulic('ram_head_ram' + (s < 0 ? '_l' : '_r'), [s * .40, 1.30, -.10], [s * .40, 1.36, -.70], 1.1));
      // Rear: exposed engine block — the weak point.
      const eng = grp('ram_engine', [0, 1.14, .92]);
      eng.add(part('ram_engine_block', box(.70, .40, .36), mats.ram_rust, [0, 0, 0]));
      for (let i = 0; i < 4; i++) eng.add(part('ram_cylinder' + i, cyl(.07, .07, .22, 10), mats.engine_glow, [-.24 + i * .16, .30, 0]));
      for (let i = 0; i < 4; i++) eng.add(part('ram_cylinder_cap' + i, cyl(.09, .09, .04, 10), mats.chrome, [-.24 + i * .16, .43, 0]));
      eng.add(part('ram_fuel_cell', new THREE.SphereGeometry(.16, 12, 10), mats.weak, [0, -.02, .26]));
      eng.add(part('ram_fuel_cage', new THREE.TorusGeometry(.19, .015, 6, 16), mats.trim, [0, -.02, .26], [Math.PI / 2, 0, 0]));
      for (const s of [-1, 1]) { eng.add(part('ram_stack' + (s < 0 ? '_l' : '_r'), cyl(.06, .07, .50, 8), mats.ram_rust, [s * .30, .40, .14], [.5, 0, 0])); eng.add(part('ram_stack_glow' + (s < 0 ? '_l' : '_r'), cyl(.05, .05, .03, 8), mats.engine_glow, [s * .30, .63, .02], [.5, 0, 0])); }
      g.add(eng);
      g.add(K.finStack('ram_radiator', 6, .06, .50, .26).translateY(1.40).translateZ(.62).rotateX(90 * D));
      for (const s of [-1, 1]) g.add(K.cableRun('ram_hose' + (s < 0 ? '_l' : '_r'), [[s * .30, 1.30, .74], [s * .46, 1.20, .40], [s * .40, 1.30, -.10]], .03, mats.rubber));
      for (const s of [-1, 1]) for (let i = 0; i < 6; i++) g.add(part(`ram_chain${s}_${i}`, new THREE.TorusGeometry(.04, .012, 5, 8), mats.chrome, [s * .55, .78 - i * .06, .95 + (i % 2) * .03], [i % 2 ? 0 : Math.PI / 2, 0, 0]));
      if (rage) { // head plate seams glow, exhaust flares, a heat-shimmer cone off each stack
        const head = g.getObjectByName('ram_head'); for (let i = 0; i < 3; i++) head.add(part('ram_rage_seam' + i, box(1.06 - i * .18, .02, .02), mats.rage_glow, [0, .24 - i * .17, -.40 - i * .12], [-.25, 0, 0]));
        head.add(part('ram_rage_brow', box(.60, .03, .03), mats.rage_glow, [0, .10, -.47]));
        for (const s of [-1, 1]) g.add(part('ram_rage_flare' + (s < 0 ? '_l' : '_r'), new THREE.ConeGeometry(.08, .40, 8, 1, true), mats.rage_glow, [s * .30, 1.14 + .63 + .18, .92 - .06], [.5 + Math.PI, 0, 0]));
        g.add(part('ram_rage_ground', new THREE.RingGeometry(.9, 1.05, 24), mats.rage_glow, [0, .02, .1], [-Math.PI / 2, 0, 0]));
      }
      return g;
    },
  })),
  {
    id: 'broodmother', file: 'enemy_broodmother.glb', label: 'Broodmother', size: '2.6 m', swatch: '#e3bc66', ms: 'M3',
    stats: { Hp: '90', Speed: '1.4 m/s', Emits: 'Mote / 4 s', Death: '6 Motes', Bounty: '26' },
    note: 'A Cluster that keeps refilling: a broad brood-carrier on six legs with a translucent dorsal sac ring where Motes gestate at visible stages (`brood_sac0..7`, scale up as they fill), a rear birthing chute with a hinged hatch (`brood_hatch`, swing on emit), a vented incubator core underneath, and a mandibled head kept low. Reads as "kill it before the next one drops".',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K;
      const g = grp('broodmother');
      for (let i = 0; i < 3; i++) for (const s of [-1, 1]) { const z = -.55 + i * .55; g.add(limb(K, `brood_leg${s < 0 ? '_l' : '_r'}${i}`, [s * .40, .86, z], [s * .78, .56, z + (i - 1) * .12], [s * .74, .08, z + (i - 1) * .22], .06, 2)); }
      g.add(part('brood_belly', new THREE.SphereGeometry(.62, 16, 12), mats.carapace, [0, 1.0, 0])); g.getObjectByName('brood_belly').scale.set(1, .72, 1.35);
      g.add(part('brood_shell', new THREE.SphereGeometry(.64, 16, 12, 0, Math.PI * 2, 0, Math.PI * .55), mats.carapace_dark, [0, 1.02, 0])); g.getObjectByName('brood_shell').scale.set(1, .74, 1.35);
      g.add(part('brood_ridge', box(.10, .10, 1.6), mats.carapace_pale, [0, 1.46, 0]));
      for (let i = 0; i < 8; i++) { const a = (i / 8) * Math.PI * 2, x = Math.cos(a) * .48, z = Math.sin(a) * .70, fill = .55 + ((i * 3) % 8) / 8 * .45; const sac = grp('brood_sac' + i, [x, 1.30, z]); sac.scale.setScalar(fill); sac.add(part('brood_sac_skin' + i, new THREE.SphereGeometry(.17, 12, 10), mats.brood_sac)); sac.add(part('brood_sac_mote' + i, new THREE.IcosahedronGeometry(.08, 0), mats.mote_sac)); sac.add(part('brood_sac_ring' + i, new THREE.TorusGeometry(.16, .02, 6, 16), mats.joint, [0, -.08, 0], [Math.PI / 2, 0, 0])); g.add(sac); g.add(K.cableRun('brood_umb' + i, [[x * .4, 1.20, z * .4], [x * .8, 1.36, z * .8], [x, 1.30, z]], .012, mats.brood_membrane)); }
      // Incubator core underneath + vents.
      g.add(part('brood_core_housing', cyl(.22, .26, .20, 10), mats.joint, [0, .62, .10]));
      g.add(part('brood_core', new THREE.SphereGeometry(.14, 12, 10), mats.weak, [0, .58, .10]));
      for (const s of [-1, 1]) g.add(K.louvres('brood_vent' + (s < 0 ? '_l' : '_r'), .22, .14, 4).translateX(s * .58).translateY(.92).translateZ(-.20).rotateY(s * 90 * D));
      // Birthing chute + hatch at the rear.
      g.add(part('brood_chute', new THREE.CylinderGeometry(.22, .26, .40, 10), mats.carapace_dark, [0, .86, .92], [Math.PI / 2, 0, 0]));
      g.add(part('brood_chute_glow', new THREE.TorusGeometry(.20, .02, 6, 18), mats.mote_sac, [0, .86, 1.12]));
      const hatch = grp('brood_hatch', [0, 1.06, 1.10]); hatch.rotation.x = -.6; hatch.add(part('brood_hatch_plate', new THREE.CylinderGeometry(.24, .24, .04, 10), mats.carapace_pale, [0, -.20, 0], [Math.PI / 2, 0, 0])); g.add(hatch);
      // Head low at the front: segmented neck, plated skull with a crest, hooded compound eyes, four palps and toothed mandibles that close on a glowing gullet.
      for (let i = 0; i < 3; i++) g.add(part('brood_neck_seg' + i, new THREE.CylinderGeometry(.13 - i * .015, .15 - i * .015, .14, 8), mats.carapace_dark, [0, .96 - i * .07, -.84 - i * .13], [Math.PI / 2 - .5, 0, 0]));
      for (let i = 0; i < 3; i++) g.add(part('brood_neck_ring' + i, new THREE.TorusGeometry(.15 - i * .015, .02, 6, 12), mats.joint, [0, .96 - i * .07, -.84 - i * .13], [Math.PI / 2 - .5, 0, 0]));
      const head = grp('brood_head', [0, .76, -1.22]); head.rotation.x = .12;
      head.add(part('brood_skull', new THREE.SphereGeometry(.22, 14, 10), mats.carapace_dark)); head.getObjectByName('brood_skull').scale.set(1, .8, 1.25);
      head.add(part('brood_skull_plate', new THREE.SphereGeometry(.23, 14, 10, 0, Math.PI * 2, 0, Math.PI * .5), mats.carapace_pale, [0, .01, 0])); head.getObjectByName('brood_skull_plate').scale.set(1, .8, 1.25);
      head.add(part('brood_crest', box(.05, .10, .30), mats.carapace, [0, .20, -.02]));
      for (let i = 0; i < 3; i++) head.add(part('brood_crest_spine' + i, new THREE.ConeGeometry(.02, .10, 5), mats.chrome, [0, .28, -.10 + i * .10], [-.3, 0, 0]));
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        head.add(part('brood_brow' + sfx, box(.14, .05, .16), mats.carapace_pale, [s * .14, .12, -.16], [-.2, 0, s * .25]));
        head.add(part('brood_eye_socket' + sfx, new THREE.SphereGeometry(.07, 10, 8), mats.joint, [s * .15, .05, -.20]));
        head.add(part('brood_eye' + sfx, new THREE.SphereGeometry(.055, 12, 8), mats.eye, [s * .15, .05, -.23]));
        for (let k = 0; k < 3; k++) head.add(part(`brood_eyelet${sfx}${k}`, new THREE.SphereGeometry(.018, 6, 5), mats.eye, [s * (.08 + k * .03), .12 + k * .015, -.24 - k * .01]));
        head.add(part('brood_cheek' + sfx, box(.06, .10, .18), mats.carapace, [s * .20, -.06, -.14], [0, 0, s * .3]));
        head.add(seg(K, 'brood_palp_a' + sfx, [s * .10, -.10, -.26], [s * .18, -.20, -.38], .022, .014));
        head.add(seg(K, 'brood_palp_b' + sfx, [s * .05, -.12, -.27], [s * .06, -.26, -.36], .018, .012));
        head.add(part('brood_palp_tip' + sfx, new THREE.SphereGeometry(.02, 6, 5), mats.mote_sac, [s * .18, -.20, -.38]));
        const mand = grp('brood_mandible' + sfx, [s * .16, -.08, -.24]); mand.rotation.y = -s * .35;
        mand.add(part('brood_mandible_blade' + sfx, new THREE.ConeGeometry(.045, .34, 5), mats.chrome, [0, -.02, -.16], [-Math.PI / 2, 0, 0]));
        for (let k = 0; k < 4; k++) mand.add(part(`brood_mandible_tooth${sfx}${k}`, new THREE.ConeGeometry(.012, .05, 4), mats.chrome, [-s * .03, -.03, -.06 - k * .06], [0, 0, -s * Math.PI / 2]));
        head.add(mand);
      }
      head.add(part('brood_gullet', new THREE.SphereGeometry(.07, 10, 8), mats.mote_sac, [0, -.08, -.20]));
      head.add(part('brood_jaw', box(.22, .06, .18), mats.carapace_dark, [0, -.14, -.16], [.3, 0, 0]));
      for (let i = 0; i < 5; i++) head.add(part('brood_jaw_tooth' + i, new THREE.ConeGeometry(.012, .04, 4), mats.chrome, [-.08 + i * .04, -.10, -.25]));
      g.add(head);
      return g;
    },
  },
  {
    id: 'leaper', file: 'enemy_leaper.glb', label: 'Leaper', size: '1.7 m', swatch: '#6a7a5a', ms: 'M4',
    stats: { Hp: '30', Speed: '2.8 m/s', Jumps: 'barricades · tiers', Airborne: '×1.5 taken', Pose: 'stand' },
    note: 'Is your defence one kill-box? A hunched digitigrade jumper: oversized reverse-jointed hind legs with piston calves and splayed pad feet, a short body slung low between them, small grasping forearms, a long counterweight tail with fin plates, and a narrow head with a single forward lens. Standing: poised, knees bent, ready to go.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K, g = grp(this.id), P = 'stand';
      const crouch = P === 'windup' ? .55 : P === 'airborne' ? 1.25 : .85, bodyY = P === 'airborne' ? 1.6 : crouch + .35;
      const body = grp('leaper_body', [0, bodyY, 0]); if (P === 'airborne') body.rotation.x = .45;
      body.add(part('leaper_torso', new THREE.CapsuleGeometry(.18, .36, 6, 12), mats.leaper_skin, [0, 0, 0], [Math.PI / 2 + .3, 0, 0]));
      body.add(part('leaper_back_plate', new THREE.BoxGeometry(.26, .06, .40), mats.carapace, [0, .16, .0], [.3, 0, 0]));
      body.add(part('leaper_chest', new THREE.BoxGeometry(.22, .20, .08), mats.carapace_dark, [0, -.06, -.26], [.3, 0, 0]));
      body.add(part('leaper_core', new THREE.SphereGeometry(.05, 10, 8), mats.weak, [0, .02, .24]));
      // Head: narrow, forward lens
      body.add(part('leaper_neck', cyl(.05, .07, .16, 8), mats.joint, [0, .10, -.32], [1.0, 0, 0]));
      body.add(part('leaper_head', new THREE.BoxGeometry(.14, .12, .30), mats.carapace_dark, [0, .16, -.46], [-.1, 0, 0]));
      body.add(part('leaper_head_crest', new THREE.BoxGeometry(.03, .06, .22), mats.carapace, [0, .24, -.44]));
      body.add(part('leaper_lens_housing', new THREE.CylinderGeometry(.05, .06, .05, 10), mats.joint, [0, .15, -.61], [Math.PI / 2, 0, 0]));
      body.add(part('leaper_lens', new THREE.SphereGeometry(.04, 10, 8), P === 'windup' ? mats.weak : mats.eye, [0, .15, -.63]));
      // Forearms
      for (const s of [-1, 1]) { const tuck = P === 'airborne' ? .6 : 0; body.add(seg(K, 'leaper_arm_a' + (s < 0 ? '_l' : '_r'), [s * .18, -.04, -.20], [s * .26, -.22 + tuck * .1, -.30 + tuck * .12], .035, .03)); body.add(seg(K, 'leaper_arm_b' + (s < 0 ? '_l' : '_r'), [s * .26, -.22 + tuck * .1, -.30 + tuck * .12], [s * .20, -.36 + tuck * .2, -.42 + tuck * .18], .028, .02)); body.add(part('leaper_claw' + (s < 0 ? '_l' : '_r'), new THREE.ConeGeometry(.018, .10, 5), mats.chrome, [s * .20, -.40 + tuck * .2, -.46 + tuck * .18], [Math.PI, 0, 0])); }
      // Tail with fin plates
      const tail = grp('leaper_tail', [0, .0, .30]); tail.rotation.x = P === 'windup' ? -.8 : P === 'airborne' ? .2 : -.3;
      for (let i = 0; i < 6; i++) { tail.add(part('leaper_tail_seg' + i, cyl(.06 - i * .008, .07 - i * .008, .16, 8), mats.leaper_skin, [0, 0, .08 + i * .15], [Math.PI / 2, 0, 0])); if (i % 2) tail.add(part('leaper_tail_fin' + i, new THREE.BoxGeometry(.02, .14 - i * .01, .10), mats.carapace, [0, .08, .08 + i * .15])); }
      tail.add(part('leaper_tail_tip', new THREE.ConeGeometry(.03, .16, 6), mats.chrome, [0, 0, 1.0], [Math.PI / 2, 0, 0]));
      body.add(tail);
      g.add(body);
      // Hind legs: hip on body, knee forward-high, ankle back-low, piston calf, splayed pad foot.
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        const hip = [s * .20, bodyY - .10, .08];
        const knee = P === 'airborne' ? [s * .26, bodyY - .30, .40] : [s * .26, crouch + .40, -.30];
        const ankle = P === 'airborne' ? [s * .24, bodyY - .30, .95] : [s * .24, crouch * .35 + .05, .18];
        const foot = P === 'airborne' ? [s * .24, bodyY - .50, 1.15] : [s * .24, .06, -.10];
        g.add(part('leaper_hip' + sfx, new THREE.SphereGeometry(.11, 12, 8), mats.joint, hip));
        g.add(seg(K, 'leaper_thigh' + sfx, hip, knee, .10, .08, mats.leaper_skin));
        g.add(part('leaper_knee' + sfx, new THREE.SphereGeometry(.09, 12, 8), mats.joint, knee));
        g.add(part('leaper_knee_cap' + sfx, new THREE.BoxGeometry(.14, .16, .10), mats.carapace, [knee[0], knee[1] + .02, knee[2] - .08]));
        g.add(seg(K, 'leaper_shin' + sfx, knee, ankle, .07, .05, mats.leaper_skin));
        g.add(K.hydraulic('leaper_calf_ram' + sfx, [knee[0] + s * .04, knee[1] - .06, knee[2] + .10], [ankle[0] + s * .04, ankle[1] + .04, ankle[2] + .02], .8));
        g.add(part('leaper_ankle' + sfx, new THREE.SphereGeometry(.06, 10, 8), mats.joint, ankle));
        g.add(seg(K, 'leaper_meta' + sfx, ankle, foot, .05, .05, mats.leaper_skin));
        const pad = grp('leaper_foot' + sfx, foot); if (P === 'airborne') pad.rotation.x = 1.2;
        const splay = P === 'windup' ? 1.4 : 1;
        for (let i = 0; i < 3; i++) { const a = (i - 1) * .5 * splay; pad.add(part('leaper_toe' + sfx + i, new THREE.CapsuleGeometry(.03, .18, 4, 8), mats.leaper_pad, [Math.sin(a) * .12, -.02, -.10 - Math.cos(a) * .10], [Math.PI / 2, 0, -a])); pad.add(part('leaper_toe_claw' + sfx + i, new THREE.ConeGeometry(.02, .08, 5), mats.chrome, [Math.sin(a) * .22, -.03, -.22 - Math.cos(a) * .12], [-Math.PI / 2, 0, -a])); }
        pad.add(part('leaper_heel' + sfx, new THREE.SphereGeometry(.05, 8, 6), mats.leaper_pad, [0, -.02, .08]));
        g.add(pad);
      }
      if (P === 'windup') for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part('leaper_dust' + i, new THREE.BoxGeometry(.12, .02, .06), mats.dirt, [Math.cos(a) * .5, .02, Math.sin(a) * .5], [0, -a, 0])); }
      return g;
    },
  },
  {
    id: 'leaper_windup', file: 'enemy_leaper_windup.glb', label: 'Leaper — wind-up', size: '1.7 m', swatch: '#6a7a5a', ms: 'M4',
    stats: { Hp: '30', Speed: '2.8 m/s', Jumps: 'barricades · tiers', Airborne: '×1.5 taken', Pose: 'windup' },
    note: 'Is your defence one kill-box? A hunched digitigrade jumper: oversized reverse-jointed hind legs with piston calves and splayed pad feet, a short body slung low between them, small grasping forearms, a long counterweight tail with fin plates, and a narrow head with a single forward lens. Wind-up: legs fully compressed, tail cocked up, pads splayed, lens flaring — the tell before the jump.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K, g = grp(this.id), P = 'windup';
      const crouch = P === 'windup' ? .55 : P === 'airborne' ? 1.25 : .85, bodyY = P === 'airborne' ? 1.6 : crouch + .35;
      const body = grp('leaper_body', [0, bodyY, 0]); if (P === 'airborne') body.rotation.x = .45;
      body.add(part('leaper_torso', new THREE.CapsuleGeometry(.18, .36, 6, 12), mats.leaper_skin, [0, 0, 0], [Math.PI / 2 + .3, 0, 0]));
      body.add(part('leaper_back_plate', new THREE.BoxGeometry(.26, .06, .40), mats.carapace, [0, .16, .0], [.3, 0, 0]));
      body.add(part('leaper_chest', new THREE.BoxGeometry(.22, .20, .08), mats.carapace_dark, [0, -.06, -.26], [.3, 0, 0]));
      body.add(part('leaper_core', new THREE.SphereGeometry(.05, 10, 8), mats.weak, [0, .02, .24]));
      // Head: narrow, forward lens
      body.add(part('leaper_neck', cyl(.05, .07, .16, 8), mats.joint, [0, .10, -.32], [1.0, 0, 0]));
      body.add(part('leaper_head', new THREE.BoxGeometry(.14, .12, .30), mats.carapace_dark, [0, .16, -.46], [-.1, 0, 0]));
      body.add(part('leaper_head_crest', new THREE.BoxGeometry(.03, .06, .22), mats.carapace, [0, .24, -.44]));
      body.add(part('leaper_lens_housing', new THREE.CylinderGeometry(.05, .06, .05, 10), mats.joint, [0, .15, -.61], [Math.PI / 2, 0, 0]));
      body.add(part('leaper_lens', new THREE.SphereGeometry(.04, 10, 8), P === 'windup' ? mats.weak : mats.eye, [0, .15, -.63]));
      // Forearms
      for (const s of [-1, 1]) { const tuck = P === 'airborne' ? .6 : 0; body.add(seg(K, 'leaper_arm_a' + (s < 0 ? '_l' : '_r'), [s * .18, -.04, -.20], [s * .26, -.22 + tuck * .1, -.30 + tuck * .12], .035, .03)); body.add(seg(K, 'leaper_arm_b' + (s < 0 ? '_l' : '_r'), [s * .26, -.22 + tuck * .1, -.30 + tuck * .12], [s * .20, -.36 + tuck * .2, -.42 + tuck * .18], .028, .02)); body.add(part('leaper_claw' + (s < 0 ? '_l' : '_r'), new THREE.ConeGeometry(.018, .10, 5), mats.chrome, [s * .20, -.40 + tuck * .2, -.46 + tuck * .18], [Math.PI, 0, 0])); }
      // Tail with fin plates
      const tail = grp('leaper_tail', [0, .0, .30]); tail.rotation.x = P === 'windup' ? -.8 : P === 'airborne' ? .2 : -.3;
      for (let i = 0; i < 6; i++) { tail.add(part('leaper_tail_seg' + i, cyl(.06 - i * .008, .07 - i * .008, .16, 8), mats.leaper_skin, [0, 0, .08 + i * .15], [Math.PI / 2, 0, 0])); if (i % 2) tail.add(part('leaper_tail_fin' + i, new THREE.BoxGeometry(.02, .14 - i * .01, .10), mats.carapace, [0, .08, .08 + i * .15])); }
      tail.add(part('leaper_tail_tip', new THREE.ConeGeometry(.03, .16, 6), mats.chrome, [0, 0, 1.0], [Math.PI / 2, 0, 0]));
      body.add(tail);
      g.add(body);
      // Hind legs: hip on body, knee forward-high, ankle back-low, piston calf, splayed pad foot.
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        const hip = [s * .20, bodyY - .10, .08];
        const knee = P === 'airborne' ? [s * .26, bodyY - .30, .40] : [s * .26, crouch + .40, -.30];
        const ankle = P === 'airborne' ? [s * .24, bodyY - .30, .95] : [s * .24, crouch * .35 + .05, .18];
        const foot = P === 'airborne' ? [s * .24, bodyY - .50, 1.15] : [s * .24, .06, -.10];
        g.add(part('leaper_hip' + sfx, new THREE.SphereGeometry(.11, 12, 8), mats.joint, hip));
        g.add(seg(K, 'leaper_thigh' + sfx, hip, knee, .10, .08, mats.leaper_skin));
        g.add(part('leaper_knee' + sfx, new THREE.SphereGeometry(.09, 12, 8), mats.joint, knee));
        g.add(part('leaper_knee_cap' + sfx, new THREE.BoxGeometry(.14, .16, .10), mats.carapace, [knee[0], knee[1] + .02, knee[2] - .08]));
        g.add(seg(K, 'leaper_shin' + sfx, knee, ankle, .07, .05, mats.leaper_skin));
        g.add(K.hydraulic('leaper_calf_ram' + sfx, [knee[0] + s * .04, knee[1] - .06, knee[2] + .10], [ankle[0] + s * .04, ankle[1] + .04, ankle[2] + .02], .8));
        g.add(part('leaper_ankle' + sfx, new THREE.SphereGeometry(.06, 10, 8), mats.joint, ankle));
        g.add(seg(K, 'leaper_meta' + sfx, ankle, foot, .05, .05, mats.leaper_skin));
        const pad = grp('leaper_foot' + sfx, foot); if (P === 'airborne') pad.rotation.x = 1.2;
        const splay = P === 'windup' ? 1.4 : 1;
        for (let i = 0; i < 3; i++) { const a = (i - 1) * .5 * splay; pad.add(part('leaper_toe' + sfx + i, new THREE.CapsuleGeometry(.03, .18, 4, 8), mats.leaper_pad, [Math.sin(a) * .12, -.02, -.10 - Math.cos(a) * .10], [Math.PI / 2, 0, -a])); pad.add(part('leaper_toe_claw' + sfx + i, new THREE.ConeGeometry(.02, .08, 5), mats.chrome, [Math.sin(a) * .22, -.03, -.22 - Math.cos(a) * .12], [-Math.PI / 2, 0, -a])); }
        pad.add(part('leaper_heel' + sfx, new THREE.SphereGeometry(.05, 8, 6), mats.leaper_pad, [0, -.02, .08]));
        g.add(pad);
      }
      if (P === 'windup') for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part('leaper_dust' + i, new THREE.BoxGeometry(.12, .02, .06), mats.dirt, [Math.cos(a) * .5, .02, Math.sin(a) * .5], [0, -a, 0])); }
      return g;
    },
  },
  {
    id: 'leaper_airborne', file: 'enemy_leaper_airborne.glb', label: 'Leaper — airborne', size: '1.7 m', swatch: '#6a7a5a', ms: 'M4',
    stats: { Hp: '30', Speed: '2.8 m/s', Jumps: 'barricades · tiers', Airborne: '×1.5 taken', Pose: 'airborne' },
    note: 'Is your defence one kill-box? A hunched digitigrade jumper: oversized reverse-jointed hind legs with piston calves and splayed pad feet, a short body slung low between them, small grasping forearms, a long counterweight tail with fin plates, and a narrow head with a single forward lens. Airborne: legs trailing straight back, arms tucked, tail streaming, body pitched nose-down — mid-arc, the ×1.5 window. Origin stays at the ground under the body.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K, g = grp(this.id), P = 'airborne';
      const crouch = P === 'windup' ? .55 : P === 'airborne' ? 1.25 : .85, bodyY = P === 'airborne' ? 1.6 : crouch + .35;
      const body = grp('leaper_body', [0, bodyY, 0]); if (P === 'airborne') body.rotation.x = .45;
      body.add(part('leaper_torso', new THREE.CapsuleGeometry(.18, .36, 6, 12), mats.leaper_skin, [0, 0, 0], [Math.PI / 2 + .3, 0, 0]));
      body.add(part('leaper_back_plate', new THREE.BoxGeometry(.26, .06, .40), mats.carapace, [0, .16, .0], [.3, 0, 0]));
      body.add(part('leaper_chest', new THREE.BoxGeometry(.22, .20, .08), mats.carapace_dark, [0, -.06, -.26], [.3, 0, 0]));
      body.add(part('leaper_core', new THREE.SphereGeometry(.05, 10, 8), mats.weak, [0, .02, .24]));
      // Head: narrow, forward lens
      body.add(part('leaper_neck', cyl(.05, .07, .16, 8), mats.joint, [0, .10, -.32], [1.0, 0, 0]));
      body.add(part('leaper_head', new THREE.BoxGeometry(.14, .12, .30), mats.carapace_dark, [0, .16, -.46], [-.1, 0, 0]));
      body.add(part('leaper_head_crest', new THREE.BoxGeometry(.03, .06, .22), mats.carapace, [0, .24, -.44]));
      body.add(part('leaper_lens_housing', new THREE.CylinderGeometry(.05, .06, .05, 10), mats.joint, [0, .15, -.61], [Math.PI / 2, 0, 0]));
      body.add(part('leaper_lens', new THREE.SphereGeometry(.04, 10, 8), P === 'windup' ? mats.weak : mats.eye, [0, .15, -.63]));
      // Forearms
      for (const s of [-1, 1]) { const tuck = P === 'airborne' ? .6 : 0; body.add(seg(K, 'leaper_arm_a' + (s < 0 ? '_l' : '_r'), [s * .18, -.04, -.20], [s * .26, -.22 + tuck * .1, -.30 + tuck * .12], .035, .03)); body.add(seg(K, 'leaper_arm_b' + (s < 0 ? '_l' : '_r'), [s * .26, -.22 + tuck * .1, -.30 + tuck * .12], [s * .20, -.36 + tuck * .2, -.42 + tuck * .18], .028, .02)); body.add(part('leaper_claw' + (s < 0 ? '_l' : '_r'), new THREE.ConeGeometry(.018, .10, 5), mats.chrome, [s * .20, -.40 + tuck * .2, -.46 + tuck * .18], [Math.PI, 0, 0])); }
      // Tail with fin plates
      const tail = grp('leaper_tail', [0, .0, .30]); tail.rotation.x = P === 'windup' ? -.8 : P === 'airborne' ? .2 : -.3;
      for (let i = 0; i < 6; i++) { tail.add(part('leaper_tail_seg' + i, cyl(.06 - i * .008, .07 - i * .008, .16, 8), mats.leaper_skin, [0, 0, .08 + i * .15], [Math.PI / 2, 0, 0])); if (i % 2) tail.add(part('leaper_tail_fin' + i, new THREE.BoxGeometry(.02, .14 - i * .01, .10), mats.carapace, [0, .08, .08 + i * .15])); }
      tail.add(part('leaper_tail_tip', new THREE.ConeGeometry(.03, .16, 6), mats.chrome, [0, 0, 1.0], [Math.PI / 2, 0, 0]));
      body.add(tail);
      g.add(body);
      // Hind legs: hip on body, knee forward-high, ankle back-low, piston calf, splayed pad foot.
      for (const s of [-1, 1]) {
        const sfx = s < 0 ? '_l' : '_r';
        const hip = [s * .20, bodyY - .10, .08];
        const knee = P === 'airborne' ? [s * .26, bodyY - .30, .40] : [s * .26, crouch + .40, -.30];
        const ankle = P === 'airborne' ? [s * .24, bodyY - .30, .95] : [s * .24, crouch * .35 + .05, .18];
        const foot = P === 'airborne' ? [s * .24, bodyY - .50, 1.15] : [s * .24, .06, -.10];
        g.add(part('leaper_hip' + sfx, new THREE.SphereGeometry(.11, 12, 8), mats.joint, hip));
        g.add(seg(K, 'leaper_thigh' + sfx, hip, knee, .10, .08, mats.leaper_skin));
        g.add(part('leaper_knee' + sfx, new THREE.SphereGeometry(.09, 12, 8), mats.joint, knee));
        g.add(part('leaper_knee_cap' + sfx, new THREE.BoxGeometry(.14, .16, .10), mats.carapace, [knee[0], knee[1] + .02, knee[2] - .08]));
        g.add(seg(K, 'leaper_shin' + sfx, knee, ankle, .07, .05, mats.leaper_skin));
        g.add(K.hydraulic('leaper_calf_ram' + sfx, [knee[0] + s * .04, knee[1] - .06, knee[2] + .10], [ankle[0] + s * .04, ankle[1] + .04, ankle[2] + .02], .8));
        g.add(part('leaper_ankle' + sfx, new THREE.SphereGeometry(.06, 10, 8), mats.joint, ankle));
        g.add(seg(K, 'leaper_meta' + sfx, ankle, foot, .05, .05, mats.leaper_skin));
        const pad = grp('leaper_foot' + sfx, foot); if (P === 'airborne') pad.rotation.x = 1.2;
        const splay = P === 'windup' ? 1.4 : 1;
        for (let i = 0; i < 3; i++) { const a = (i - 1) * .5 * splay; pad.add(part('leaper_toe' + sfx + i, new THREE.CapsuleGeometry(.03, .18, 4, 8), mats.leaper_pad, [Math.sin(a) * .12, -.02, -.10 - Math.cos(a) * .10], [Math.PI / 2, 0, -a])); pad.add(part('leaper_toe_claw' + sfx + i, new THREE.ConeGeometry(.02, .08, 5), mats.chrome, [Math.sin(a) * .22, -.03, -.22 - Math.cos(a) * .12], [-Math.PI / 2, 0, -a])); }
        pad.add(part('leaper_heel' + sfx, new THREE.SphereGeometry(.05, 8, 6), mats.leaper_pad, [0, -.02, .08]));
        g.add(pad);
      }
      if (P === 'windup') for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part('leaper_dust' + i, new THREE.BoxGeometry(.12, .02, .06), mats.dirt, [Math.cos(a) * .5, .02, Math.sin(a) * .5], [0, -a, 0])); }
      return g;
    },
  },
  {
    id: 'carapace', file: 'enemy_carapace.glb', label: 'Carapace', size: '2.0 m', swatch: '#8d99ad', ms: 'M4',
    stats: { Hp: '48', Plates: '6 × 10 hp', Under: 'weak ×1.5', Bounty: '20', Answer: 'aim, not DPS' },
    note: 'The Bulwark redesign: armour you shoot OFF. A broad four-legged body wearing six detachable plates on numbered sockets (`carapace_plate_0…5` — head, two shoulders, two flanks, back), each over a glowing orange weak seam. The client instances `enemy_carapace_plate.glb` at each socket and drops it when its 10 hp is gone; the seam beneath then takes ×1.5. The body alone is soft and reads that way.',
    build(K) {
      const { part, box, cyl, grp, D, mats, THREE } = K, g = grp('carapace');
      for (const [sx, sz, n] of [[-1, -1, 'fl'], [1, -1, 'fr'], [-1, 1, 'bl'], [1, 1, 'br']]) g.add(limb(K, 'carapace_leg_' + n, [sx * .34, .82, sz * .30], [sx * .58, .50, sz * .48], [sx * .52, .08, sz * .50], .055, 3));
      g.add(part('carapace_body', new THREE.SphereGeometry(.50, 14, 10), mats.joint, [0, 1.0, 0])); g.getObjectByName('carapace_body').scale.set(1, .7, 1.2);
      g.add(part('carapace_belly', new THREE.SphereGeometry(.42, 12, 8), mats.carapace_dark, [0, .88, 0])); g.getObjectByName('carapace_belly').scale.set(1, .5, 1.1);
      g.add(part('carapace_neck', cyl(.10, .14, .22, 8), mats.joint, [0, 1.12, -.56], [.9, 0, 0]));
      g.add(part('carapace_head', new THREE.SphereGeometry(.18, 12, 8), mats.joint, [0, 1.10, -.74]));
      g.add(eyeSlit(K, 'carapace_eye', [0, 1.12, -.92], .16));
      g.add(part('carapace_core', new THREE.SphereGeometry(.10, 12, 8), mats.weak, [0, 1.22, .10]));
      // Six plate sockets: empty named nodes with the weak seam drawn beneath; the plate mesh is a separate file.
      const sockets = [['0', [0, 1.28, -.66], [-.7, 0, 0]], ['1', [-.44, 1.28, -.18], [0, 0, .8]], ['2', [.44, 1.28, -.18], [0, 0, -.8]], ['3', [-.50, 1.02, .22], [0, 0, 1.3]], ['4', [.50, 1.02, .22], [0, 0, -1.3]], ['5', [0, 1.40, .30], [.3, 0, 0]]];
      for (const [n, p, r] of sockets) { const s = grp('carapace_plate_' + n, p); s.rotation.set(...r); s.userData.role = 'plateSocket'; g.add(s); g.add(part('carapace_seam' + n, new THREE.TorusGeometry(.16, .015, 6, 20), mats.shell_seam, p, r)); g.add(part('carapace_seam_core' + n, new THREE.SphereGeometry(.06, 8, 6), mats.shell_seam, p)); }
      for (let i = 0; i < 4; i++) g.add(part('carapace_spine' + i, new THREE.ConeGeometry(.04, .14, 5), mats.chrome, [0, 1.36, .5 - i * .14], [-.3, 0, 0]));
      return g;
    },
  },
  {
    id: 'carapace_plate', file: 'enemy_carapace_plate.glb', label: 'Carapace plate', size: '0.5 m', swatch: '#a6b2c6', ms: 'M4',
    stats: { Hp: '10', Fits: 'carapace_plate_0…5', Drops: 'on break' },
    note: 'One armour plate, origin at its socket, facing +Y: a domed layered plate with a dark backing, six rivets, a chipped edge and a scorch mark so a bare one on the floor reads as shot off. Same mesh on all six sockets.',
    build(K) {
      const { part, grp, THREE, mats } = K, g = grp('carapace_plate');
      g.add(part('plate_backing', new THREE.CylinderGeometry(.22, .20, .04, 10), mats.carapace_dark, [0, .02, 0]));
      g.add(part('plate_dome', new THREE.SphereGeometry(.24, 12, 8, 0, Math.PI * 2, 0, Math.PI * .32), mats.shell_plate, [0, -.14, 0])); g.getObjectByName('plate_dome').scale.set(1, .9, 1.15);
      g.add(part('plate_ridge', new THREE.BoxGeometry(.04, .03, .34), mats.carapace_pale, [0, .10, 0]));
      for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; g.add(part('plate_rivet' + i, new THREE.CylinderGeometry(.018, .018, .015, 6), mats.chrome, [Math.cos(a) * .17, .05, Math.sin(a) * .17])); }
      g.add(part('plate_chip', new THREE.BoxGeometry(.08, .05, .06), mats.joint, [.17, .06, .12]));
      g.add(part('plate_scorch', new THREE.CylinderGeometry(.07, .07, .002, 10), mats.slag || mats.trim, [-.08, .085, -.04]));
      return g;
    },
  },
];
/**
 * Elite modifiers (M4 endless): overlays applied onto any built enemy. Each returns a group of extra parts
 * fitted to the host's bounding box so one function dresses every silhouette. The Roster shows them on a Drifter.
 */
export const ELITES = [
  { id: 'gilded', label: 'Elite — Gilded', swatch: '#e0b34a', stats: { Bounty: '×3', Hp: '×1.4', Tell: 'brass trim + gold eye' }, note: 'Brass filigree: a gold collar ring at the shoulder line, four hanging medallions, gilded toe caps and an emissive gold eye swap. Material treatment on the base mesh (metalness up, warm tint) is a client tint.',
    apply(K, host) {
      const { part, grp, THREE, mats } = K, b = new THREE.Box3().setFromObject(host), s = b.getSize(new THREE.Vector3()), c = b.getCenter(new THREE.Vector3()), g = grp('elite_gilded');
      const y = b.min.y + s.y * .78, r = Math.max(s.x, s.z) * .42;
      g.add(part('gild_collar', new THREE.TorusGeometry(r, .025, 8, 32), mats.gild, [c.x, y, c.z], [Math.PI / 2, 0, 0]));
      for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2; g.add(part('gild_medal' + i, new THREE.CylinderGeometry(.05, .05, .012, 8), mats.gild, [c.x + Math.cos(a) * r, y - .10, c.z + Math.sin(a) * r], [0, 0, Math.PI / 2])); }
      for (let i = 0; i < 6; i++) { const a = i / 6 * Math.PI * 2; g.add(part('gild_filigree' + i, new THREE.TorusGeometry(.06, .01, 6, 12, Math.PI), mats.gild, [c.x + Math.cos(a) * r, y + .06, c.z + Math.sin(a) * r], [0, -a, 0])); }
      g.add(part('gild_crest', new THREE.ConeGeometry(.05, .16, 4), mats.gild, [c.x, b.max.y + .06, c.z]));
      host.traverse((o) => { if (o.isMesh && o.material === mats.eye) o.material = mats.gild_eye; });
      return g;
    } },
  { id: 'juggernaut', label: 'Elite — Juggernaut', swatch: '#4a5160', stats: { Armor: '+3 flat', Knockback: 'immune', Speed: '×0.8' }, note: 'Plate overlay set: six riveted armour slabs bolted over the shoulders, flanks and shins, a brow plate over the eye, and a ground-drag skirt — the silhouette gets squarer and heavier without a new mesh.',
    apply(K, host) {
      const { part, grp, THREE, mats, box } = K, b = new THREE.Box3().setFromObject(host), s = b.getSize(new THREE.Vector3()), c = b.getCenter(new THREE.Vector3()), g = grp('elite_juggernaut');
      const slab = (n, w, h, pos, rot) => { const p = grp(n, pos); p.rotation.set(...rot); p.add(part(n + '_plate', box(w, h, .05), mats.jugg_plate)); p.add(part(n + '_rim', box(w + .04, h + .04, .02), mats.trim, [0, 0, .02])); for (const sx of [-1, 1]) for (const sy of [-1, 1]) p.add(part(`${n}_bolt${sx}${sy}`, new THREE.CylinderGeometry(.018, .018, .02, 6), mats.chrome, [sx * (w / 2 - .05), sy * (h / 2 - .05), -.03], [Math.PI / 2, 0, 0])); return p; };
      const hx = s.x / 2 + .04, hz = s.z / 2 + .04, ys = b.min.y + s.y * .72;
      for (const sx of [-1, 1]) { g.add(slab('jugg_shoulder' + sx, s.z * .5, s.y * .16, [c.x + sx * hx, ys, c.z], [0, sx * Math.PI / 2, 0])); g.add(slab('jugg_shin' + sx, .16, s.y * .22, [c.x + sx * s.x * .18, b.min.y + s.y * .2, b.min.z - .02], [0, 0, 0])); }
      g.add(slab('jugg_chest', s.x * .7, s.y * .2, [c.x, b.min.y + s.y * .58, b.min.z - .02], [0, 0, 0]));
      g.add(slab('jugg_back', s.x * .7, s.y * .26, [c.x, b.min.y + s.y * .6, b.max.z + .02], [0, Math.PI, 0]));
      g.add(slab('jugg_brow', s.x * .5, .08, [c.x, b.max.y - .08, b.min.z + s.z * .2], [-.3, 0, 0]));
      g.add(part('jugg_skirt', new THREE.CylinderGeometry(Math.max(hx, hz) * 1.05, Math.max(hx, hz) * 1.2, .10, 8, 1, true), mats.jugg_plate, [c.x, b.min.y + .12, c.z]));
      return g;
    } },
  { id: 'voltaic', label: 'Elite — Voltaic', swatch: '#f05ae6', stats: { Death: 'shock burst r 4 m', Tell: 'spine nodes' }, note: 'Arc-node attach: five magenta capacitor nodes climbing the spine on a conductor rail, arcs jumping between them (`volt_arcs`, flicker), a discharge ring at the feet that widens on death. Colour is Arc/shock magenta so the reaction fuel reads.',
    apply(K, host) {
      const { part, grp, THREE, mats } = K, b = new THREE.Box3().setFromObject(host), s = b.getSize(new THREE.Vector3()), c = b.getCenter(new THREE.Vector3()), g = grp('elite_voltaic');
      const z = b.max.z - .02, y0 = b.min.y + s.y * .35, y1 = b.max.y - .05;
      g.add(part('volt_rail', new THREE.BoxGeometry(.04, y1 - y0, .04), mats.trim, [c.x, (y0 + y1) / 2, z]));
      const arcs = grp('volt_arcs');
      for (let i = 0; i < 5; i++) { const y = y0 + (y1 - y0) * i / 4; g.add(part('volt_node' + i, new THREE.OctahedronGeometry(.055, 0), mats.volt_node, [c.x, y, z + .04])); g.add(part('volt_node_ring' + i, new THREE.TorusGeometry(.07, .008, 6, 12), mats.chrome, [c.x, y, z + .04], [Math.PI / 2, 0, 0])); if (i) { const pts = []; for (let k = 0; k <= 5; k++) pts.push(new THREE.Vector3(c.x + (k && k < 5 ? Math.sin(k * 7 + i) * .05 : 0), y0 + (y1 - y0) * (i - 1 + k / 5) / 4, z + .06 + (k && k < 5 ? Math.cos(k * 5 + i) * .04 : 0))); arcs.add(part('volt_arc' + i, new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 12, .008, 4, false), mats.volt_node)); } }
      g.add(arcs);
      g.add(part('volt_ground_ring', new THREE.TorusGeometry(Math.max(s.x, s.z) * .6, .02, 6, 32), mats.volt_node, [c.x, b.min.y + .02, c.z], [Math.PI / 2, 0, 0]));
      return g;
    } },
  { id: 'swift', label: 'Elite — Swift', swatch: '#7fe65a', stats: { Speed: '×1.4', Hp: '×0.7', Tell: 'leg fins + trail' }, note: 'Leg-fin attach + trail: venom-green translucent speed fins on both flanks and the calves, a swept crest, and a three-strand motion trail streaming behind on `swift_trail` (the client stretches it with velocity).',
    apply(K, host) {
      const { part, grp, THREE, mats } = K, b = new THREE.Box3().setFromObject(host), s = b.getSize(new THREE.Vector3()), c = b.getCenter(new THREE.Vector3()), g = grp('elite_swift');
      for (const sx of [-1, 1]) { for (let i = 0; i < 3; i++) g.add(part(`swift_fin${sx}_${i}`, new THREE.BoxGeometry(.015, s.y * .10, s.z * .28), mats.swift_fin, [c.x + sx * (s.x / 2 + .02), b.min.y + s.y * (.55 + i * .12), c.z + s.z * .15 + i * .04], [.35, 0, sx * .2])); g.add(part(`swift_calf_fin${sx}`, new THREE.BoxGeometry(.012, s.y * .16, .10), mats.swift_fin, [c.x + sx * s.x * .2, b.min.y + s.y * .22, b.max.z * .4 + .04], [.5, 0, 0])); }
      g.add(part('swift_crest', new THREE.BoxGeometry(.012, .12, .28), mats.swift_fin, [c.x, b.max.y + .02, c.z + .06], [.4, 0, 0]));
      const trail = grp('swift_trail', [c.x, b.min.y + s.y * .5, b.max.z]); for (let i = 0; i < 3; i++) trail.add(part('swift_trail' + i, new THREE.ConeGeometry(.03, .9, 6), mats.swift_fin, [(i - 1) * .12, (i - 1) * .10, .45], [-Math.PI / 2, 0, 0])); g.add(trail);
      return g;
    } },
];
