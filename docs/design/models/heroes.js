/**
 * Deep Field 3D — heroes (DESIGN-BRIEF §3.5).
 * Metres, Y-up, feet at y = 0, facing −Z. One shared 1.85 m armoured body rig
 * (`rig()`), posed three ways — standing / downed / revive-crouch — and dressed
 * per faction so the silhouette reads at 40 m:
 *   Forge   — builder. Heavy pauldrons, tool backpack with a crane arm, brass.
 *   Ember   — shooter. Fuel tanks on the back, hooded visor, ember glow lines.
 *   Tempest — striker. Slim frame, capacitor fins, antenna mast, violet arcs.
 * Faction hands (`hands_<faction>`) fork the base viewmodel arms from weapons.js
 * with the same plate/accent swap. `ui_nameplate` is the world-space tag plate.
 */
import { makeKit } from './tower-kit.js';
import { WEAPONS } from './weapons.js';

export function makeHeroKit(THREE) {
  const K = makeKit(THREE);
  const { mats } = K;
  const mat = (name, color, o = {}) => { const m = new THREE.MeshStandardMaterial({ color, roughness: .7, metalness: .2, flatShading: false, ...o }); m.name = name; mats[name] = m; return m; };
  mat('suit', 0x2b3a5c, { roughness: .85, metalness: .05 });
  mat('suit_dark', 0x1c2438, { roughness: .9, metalness: .05 });
  mat('armor', 0x8d99ad, { roughness: .5, metalness: .4 });
  mat('armor_dark', 0x3e4a63, { roughness: .6, metalness: .35 });
  mat('joint', 0x242e43, { roughness: .9, metalness: .1, flatShading: false });
  mat('visor', 0x0e1a26, { roughness: .1, metalness: .3, flatShading: false });
  mat('strap', 0x4a3b2c, { roughness: .95, metalness: 0 });
  const glow = (n, hex, i = 1) => mat(n, hex, { roughness: .3, metalness: .05, emissive: new THREE.Color(hex), emissiveIntensity: i, flatShading: false });
  glow('acc_forge', 0xf0c83a, .9); glow('acc_ember', 0xe8622b, 1.1); glow('acc_tempest', 0xf05ae6, 1.1); glow('acc_glacier', 0x4fc0e8, 1.1); glow('acc_specter', 0x9b5be8, 1.0);
  mat('plate_forge', 0xc89b3c, { roughness: .45, metalness: .5 });
  mat('plate_ember', 0x8a2f1c, { roughness: .6, metalness: .3 });
  mat('plate_tempest', 0x5a3f8a, { roughness: .55, metalness: .35 });
  mat('plate_glacier', 0xbfe3f2, { roughness: .35, metalness: .3 });
  mat('plate_specter', 0x2a2438, { roughness: .4, metalness: .45 });
  mat('frost_glass', 0xdff6ff, { roughness: .1, metalness: 0, transparent: true, opacity: .55, flatShading: false });
  mat('void_veil', 0x9b5be8, { roughness: .3, metalness: 0, transparent: true, opacity: .35, emissive: new THREE.Color(0x9b5be8), emissiveIntensity: .5, flatShading: false, side: THREE.DoubleSide, depthWrite: false });
  mat('glove', 0x3a3f4b, { roughness: .9, metalness: 0 });
  mat('sleeve', 0x2b3a5c, { roughness: .85, metalness: .05 });
  mat('skin_plate', 0xc89b3c, { roughness: .5, metalness: .3 });
  mat('gun_dark', 0x161b24, { roughness: .7, metalness: .3 });
  mat('plate_white', 0xe6ebf2, { roughness: .4, metalness: .2 });
  mat('glass_dark', 0x0e1a26, { roughness: .1, metalness: .2, transparent: true, opacity: .8 });
  mat('ammo_lamp', 0x7fe65a, { emissive: new THREE.Color(0x7fe65a), emissiveIntensity: .9 });
  return K;
}

const FACTIONS = {
  forge: { label: 'Forge', swatch: '#f0c83a', acc: 'acc_forge', plate: 'plate_forge', ability: 'Overdrive', passive: '−10% build cost', cd: '30 s', r: '10 m' },
  ember: { label: 'Ember', swatch: '#e8622b', acc: 'acc_ember', plate: 'plate_ember', ability: 'Ignition Wave', passive: '+30% burn duration', cd: '22 s', r: '6 m' },
  tempest: { label: 'Tempest', swatch: '#f05ae6', acc: 'acc_tempest', plate: 'plate_tempest', ability: 'Chain Surge', passive: '+12% fire rate', cd: '26 s', r: '7 m' },
  glacier: { label: 'Glacier', swatch: '#4fc0e8', acc: 'acc_glacier', plate: 'plate_glacier', ability: 'Cryo Field', passive: '+25% vs slowed', cd: '24 s', r: '8 m' },
  specter: { label: 'Specter', swatch: '#9b5be8', acc: 'acc_specter', plate: 'plate_specter', ability: 'Reveal Pulse', passive: 'weak points shown', cd: '34 s', r: 'map' },
};

/* ── rig ─────────────────────────────────────────────────────────────── */
const V = (THREE, p) => new THREE.Vector3(...p);
function seg(K, name, from, to, r0, r1, material) {
  const { THREE, mats } = K;
  const a = V(THREE, from), b = V(THREE, to), len = a.distanceTo(b);
  const m = new THREE.Mesh(new THREE.CylinderGeometry(r1, r0, len, 9), material || mats.suit);
  m.name = name; m.position.copy(a).lerp(b, .5);
  m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize());
  return m;
}
function armourSeg(K, name, from, to, r, material) {
  const g = K.grp(name + '_grp');
  const s = seg(K, name, from, to, r, r * .8, material); s.scale.set(1, .7, 1);
  const dir = V(K.THREE, to).sub(V(K.THREE, from)).normalize();
  s.position.addScaledVector(dir, 0); s.position.z -= r * .35;
  g.add(s);
  return g;
}
function plate(K, name, w, h, pos, rot = [0, 0, 0], face) {
  const { THREE, mats, grp, part } = K;
  const g = grp(name, pos); g.rotation.set(...rot);
  g.add(part(name + '_back', new THREE.BoxGeometry(w + .03, h + .03, .025), mats.armor_dark, [0, 0, .012]));
  g.add(part(name + '_face', new THREE.BoxGeometry(w, h, .03), face || mats.armor, [0, 0, -.008]));
  const rivet = new THREE.CylinderGeometry(.01, .01, .01, 6);
  for (const sx of [-1, 1]) for (const sy of [-1, 1]) { const m = new THREE.Mesh(rivet, mats.chrome); m.name = `${name}_rivet${sx}${sy}`; m.position.set(sx * (w / 2 - .025), sy * (h / 2 - .025), -.025); m.rotation.x = Math.PI / 2; g.add(m); }
  return g;
}
const sph = (K, name, r, pos, m) => K.part(name, new K.THREE.SphereGeometry(r, 12, 9), m || K.mats.joint, pos);

/**
 * Pose = joint positions in metres. Poses: stand / downed / revive.
 * Each is {hipC, chestC, headC, shoulderL/R, elbowL/R, handL/R, hipL/R, kneeL/R, footL/R, torsoRot, headRot}.
 */
const POSES = {
  stand: {
    hipC: [0, .98, 0], chestC: [0, 1.38, 0], headC: [0, 1.72, -.02], torsoRot: [0, 0, 0], headRot: [0, 0, 0],
    shoulderL: [-.24, 1.52, 0], elbowL: [-.30, 1.24, -.06], handL: [-.24, 1.02, -.20],
    shoulderR: [.24, 1.52, 0], elbowR: [.30, 1.24, .02], handR: [.20, 1.06, -.22],
    hipL: [-.12, .96, 0], kneeL: [-.14, .52, -.02], footL: [-.15, .06, .02],
    hipR: [.12, .96, 0], kneeR: [.15, .52, .06], footR: [.16, .06, .10],
  },
  downed: { // on the back, head toward +Z, one knee raised, arm reaching
    hipC: [0, .34, .10], chestC: [0, .38, .48], headC: [0, .44, .80], torsoRot: [Math.PI / 2 - .1, 0, 0], headRot: [Math.PI / 2 - .1, 0, 0],
    shoulderL: [-.24, .40, .60], elbowL: [-.36, .16, .74], handL: [-.30, .08, .96],
    shoulderR: [.24, .40, .60], elbowR: [.40, .44, .52], handR: [.42, .70, .40],
    hipL: [-.12, .34, .10], kneeL: [-.14, .18, -.36], footL: [-.15, .06, -.78],
    hipR: [.12, .34, .10], kneeR: [.16, .56, -.16], footR: [.18, .06, -.40],
  },
  revive: { // crouched over the downed ally: one knee down, hands forward and low
    hipC: [0, .64, .10], chestC: [0, 1.00, -.14], headC: [0, 1.30, -.28], torsoRot: [-.55, 0, 0], headRot: [-.45, 0, 0],
    shoulderL: [-.24, 1.12, -.16], elbowL: [-.30, .88, -.36], handL: [-.20, .60, -.56],
    shoulderR: [.24, 1.12, -.16], elbowR: [.30, .88, -.36], handR: [.20, .58, -.58],
    hipL: [-.12, .62, .10], kneeL: [-.16, .30, -.30], footL: [-.16, .06, -.10],
    hipR: [.12, .62, .10], kneeR: [.16, .10, .20], footR: [.18, .06, .52],
  },
};

function rig(K, id, F, pose) {
  const { part, grp, THREE, mats, D, cyl, box } = K, P = POSES[pose];
  const acc = mats[F.acc], fplate = mats[F.plate];
  const g = grp(id);
  // Torso block, oriented by torsoRot about the hip.
  const torso = grp(id + '_torso', P.hipC); torso.rotation.set(...P.torsoRot);
  torso.add(part(id + '_pelvis', box(.34, .18, .24), mats.suit_dark, [0, .04, 0]));
  torso.add(part(id + '_belt', box(.36, .05, .26), mats.strap, [0, .12, 0]));
  torso.add(part(id + '_buckle', box(.08, .05, .02), mats.brass, [0, .12, -.135]));
  for (const s of [-1, 1]) torso.add(part(id + `_pouch${s}`, box(.09, .10, .06), mats.strap, [s * .16, .04, -.10]));
  torso.add(part(id + '_abdomen', cyl(.15, .17, .16, 9), mats.suit, [0, .22, 0]));
  torso.add(part(id + '_chest', box(.42, .34, .28), mats.suit, [0, .44, 0]));
  torso.add(plate(K, id + '_chest_plate', .30, .22, [0, .46, -.145], [0, 0, 0], fplate));
  torso.add(plate(K, id + '_abs_plate', .22, .10, [0, .28, -.13]));
  torso.add(part(id + '_chest_lamp', box(.03, .03, .01), acc, [.10, .52, -.16]));
  torso.add(plate(K, id + '_back_plate', .30, .26, [0, .44, .145], [0, Math.PI, 0]));
  torso.add(part(id + '_collar', new THREE.TorusGeometry(.10, .025, 6, 10), mats.armor_dark, [0, .62, -.02], [Math.PI / 2, 0, 0]));
  for (const s of [-1, 1]) {
    torso.add(part(id + `_pauldron${s}`, new THREE.CylinderGeometry(.11, .14, .10, 7), fplate, [s * .27, .58, 0], [0, 0, s * 12 * D]));
    torso.add(part(id + `_pauldron_rim${s}`, new THREE.TorusGeometry(.13, .012, 6, 7), mats.armor_dark, [s * .28, .54, 0], [0, 0, s * 12 * D]));
    torso.add(part(id + `_strap${s}`, box(.05, .30, .30), mats.strap, [s * .12, .44, 0]).rotateY(0));
  }
  g.add(torso);
  // Head — always seated on the torso's collar (torso-local [0,.66,-.02]) so no pose can detach it.
  const headWorld = torso.localToWorld(new THREE.Vector3(0, .74, -.02));
  const head = grp(id + '_head', headWorld.toArray()); head.rotation.set(...P.headRot);
  head.add(part(id + '_neck', cyl(.05, .06, .12, 8), mats.joint, [0, -.10, 0]));
  head.add(part(id + '_neck_guard', new THREE.TorusGeometry(.07, .015, 6, 10), mats.armor_dark, [0, -.13, 0], [Math.PI / 2, 0, 0]));
  head.add(part(id + '_helmet', new THREE.SphereGeometry(.12, 10, 8), mats.armor, [0, 0, 0]));
  head.add(part(id + '_helmet_crest', box(.04, .05, .20), fplate, [0, .10, 0]));
  head.add(part(id + '_visor', box(.18, .06, .04), mats.visor, [0, .0, -.11]));
  head.add(part(id + '_visor_lamp', box(.16, .008, .005), acc, [0, .02, -.13]));
  head.add(part(id + '_jaw', box(.14, .06, .08), mats.armor_dark, [0, -.07, -.06]));
  head.add(part(id + '_ear_l', cyl(.04, .04, .02, 8), mats.armor_dark, [-.12, 0, 0], [0, 0, Math.PI / 2]));
  head.add(part(id + '_ear_r', cyl(.04, .04, .02, 8), mats.armor_dark, [.12, 0, 0], [0, 0, Math.PI / 2]));
  g.add(head);
  // Arms.
  for (const side of ['L', 'R']) {
    const s = side === 'L' ? -1 : 1, sfx = side.toLowerCase();
    g.add(sph(K, `${id}_shoulder_${sfx}`, .07, P['shoulder' + side]));
    g.add(seg(K, `${id}_upper_arm_${sfx}`, P['shoulder' + side], P['elbow' + side], .06, .05));
    g.add(armourSeg(K, `${id}_bicep_plate_${sfx}`, P['shoulder' + side], P['elbow' + side], .07, mats.armor_dark));
    g.add(sph(K, `${id}_elbow_${sfx}`, .055, P['elbow' + side]));
    g.add(seg(K, `${id}_forearm_${sfx}`, P['elbow' + side], P['hand' + side], .05, .045, mats.sleeve));
    g.add(armourSeg(K, `${id}_bracer_${sfx}`, P['elbow' + side], P['hand' + side], .065, fplate));
    const hand = grp(`${id}_hand_${sfx}`, P['hand' + side]);
    hand.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), V(THREE, P['elbow' + side]).sub(V(THREE, P['hand' + side])).normalize());
    hand.add(part(`${id}_palm_${sfx}`, box(.07, .09, .04), mats.glove, [0, -.03, 0]));
    for (let i = 0; i < 4; i++) hand.add(part(`${id}_finger_${sfx}${i}`, box(.014, .05, .016), mats.glove, [-.024 + i * .016, -.095, -.005], [.6, 0, 0]));
    hand.add(part(`${id}_thumb_${sfx}`, box(.016, .04, .016), mats.glove, [s * -.04, -.05, -.01], [0, 0, s * -.5]));
    hand.add(part(`${id}_knuckle_${sfx}`, box(.06, .02, .03), fplate, [0, -.06, -.02]));
    g.add(hand);
  }
  // Legs.
  for (const side of ['L', 'R']) {
    const sfx = side.toLowerCase();
    g.add(sph(K, `${id}_hip_${sfx}`, .075, P['hip' + side]));
    g.add(seg(K, `${id}_thigh_${sfx}`, P['hip' + side], P['knee' + side], .08, .065));
    g.add(armourSeg(K, `${id}_thigh_plate_${sfx}`, P['hip' + side], P['knee' + side], .09, mats.armor));
    g.add(sph(K, `${id}_knee_${sfx}`, .065, P['knee' + side]));
    g.add(part(`${id}_knee_cap_${sfx}`, new THREE.CylinderGeometry(.06, .07, .06, 7), fplate, V(THREE, P['knee' + side]).add(new THREE.Vector3(0, 0, -.045)).toArray(), [Math.PI / 2, 0, 0]));
    g.add(seg(K, `${id}_shin_${sfx}`, P['knee' + side], P['foot' + side], .06, .05, mats.suit_dark));
    g.add(armourSeg(K, `${id}_greave_${sfx}`, P['knee' + side], P['foot' + side], .075, mats.armor));
    const foot = grp(`${id}_foot_${sfx}`, P['foot' + side]);
    const up = V(THREE, P['knee' + side]).sub(V(THREE, P['foot' + side])).normalize();
    if (pose === 'downed') foot.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), up);
    foot.add(part(`${id}_boot_${sfx}`, box(.12, .10, .28), mats.suit_dark, [0, -.01, -.04]));
    foot.add(part(`${id}_boot_toe_${sfx}`, box(.12, .06, .08), mats.armor_dark, [0, -.03, -.20]));
    foot.add(part(`${id}_boot_sole_${sfx}`, box(.13, .02, .30), mats.rubber, [0, -.06, -.04]));
    foot.add(part(`${id}_boot_cuff_${sfx}`, new THREE.CylinderGeometry(.07, .065, .06, 8), mats.armor_dark, [0, .06, .02]));
    g.add(foot);
  }
  return { g, torso, head, P };
}

/* ── faction dressing ────────────────────────────────────────────────── */
function dress(K, id, fid, R) {
  const { part, grp, THREE, mats, D, cyl, box } = K, F = FACTIONS[fid], acc = mats[F.acc], fplate = mats[F.plate], T = R.torso;
  if (fid === 'forge') {
    // Tool backpack with folding crane arm; heavier pauldrons; tool loops.
    const pack = grp(id + '_pack', [0, .40, .24]);
    pack.add(part(id + '_pack_body', box(.36, .40, .18), mats.armor_dark));
    pack.add(K.louvres(id + '_pack_vents', .20, .10, 3).translateY(-.10).translateZ(.10));
    pack.add(part(id + '_pack_lamp', box(.12, .02, .01), acc, [0, .16, .095]));
    pack.add(part(id + '_crane_base', cyl(.05, .06, .08, 8), mats.brass, [.10, .24, 0]));
    pack.add(part(id + '_crane_arm', box(.04, .04, .40), mats.brass, [.10, .28, -.10], [-30 * D, 0, 0]));
    pack.add(part(id + '_crane_hook', new THREE.TorusGeometry(.03, .008, 6, 10, Math.PI * 1.5), mats.chrome, [.10, .46, -.30], [0, Math.PI / 2, 0]));
    pack.add(K.hydraulic(id + '_crane_ram', [.06, .10, .10], [.10, .34, -.06], .7));
    for (let i = 0; i < 3; i++) pack.add(part(id + '_tool' + i, cyl(.012, .012, .20, 6), i ? mats.chrome : mats.brass, [-.12 + i * .05, -.02, .11], [0, 0, 8 * D]));
    T.add(pack);
    for (const s of [-1, 1]) {
      T.add(part(id + `_pauldron_heavy${s}`, box(.16, .08, .22), fplate, [s * .30, .64, 0], [0, 0, s * 12 * D]));
      T.add(K.boltRing(id + `_pauldron_bolts${s}`, .10, 5, .69, mats.chrome).translateX(s * .30));
    }
    T.add(K.hazardStripes(id + '_hazard', .26, .03, [0, .28, -.15], [0, 0, 0], 5));
    T.add(part(id + '_spool', cyl(.06, .06, .08, 10), mats.brass, [-.20, .10, .06], [0, 0, Math.PI / 2]));
    T.add(K.cableRun(id + '_spool_cable', [[-.24, .10, .06], [-.28, .22, .16], [-.16, .34, .30]], .012));
  }
  if (fid === 'ember') {
    // Twin fuel tanks, braided hoses to the right bracer, hooded visor, glow lines.
    for (const s of [-1, 1]) {
      T.add(part(id + `_tank${s}`, cyl(.07, .07, .42, 12), mats.hazard, [s * .10, .38, .22]));
      T.add(part(id + `_tank_cap${s}`, cyl(.075, .05, .06, 12), mats.armor_dark, [s * .10, .62, .22]));
      T.add(part(id + `_tank_ring${s}`, new THREE.TorusGeometry(.072, .01, 6, 14), mats.armor_dark, [s * .10, .30, .22], [Math.PI / 2, 0, 0]));
      T.add(part(id + `_tank_glass${s}`, new THREE.TorusGeometry(.072, .012, 6, 14), acc, [s * .10, .44, .22], [Math.PI / 2, 0, 0]));
      T.add(part(id + `_tank_valve${s}`, cyl(.015, .015, .04, 6), mats.brass, [s * .10, .67, .22]));
    }
    T.add(part(id + '_tank_frame', box(.34, .04, .06), mats.armor_dark, [0, .20, .20]));
    T.add(part(id + '_tank_frame_b', box(.34, .04, .06), mats.armor_dark, [0, .58, .20]));
    T.add(K.cableRun(id + '_fuel_hose', [[.14, .30, .22], [.30, .22, .08], [.32, .14, -.06]], .014));
    T.add(part(id + '_glow_line_l', box(.01, .28, .005), acc, [-.14, .44, -.16]));
    T.add(part(id + '_glow_line_r', box(.01, .28, .005), acc, [.14, .44, -.16]));
    R.head.add(part(id + '_hood', new THREE.SphereGeometry(.14, 10, 8, 0, Math.PI * 2, 0, Math.PI * .42), mats.suit_dark, [0, .02, .04]));
    R.head.add(part(id + '_hood_brim', new THREE.TorusGeometry(.13, .015, 6, 14, Math.PI), mats.armor_dark, [0, .04, -.02], [0, 0, 0]));
    R.head.add(part(id + '_respirator', cyl(.03, .035, .04, 8), mats.armor_dark, [.05, -.06, -.11], [Math.PI / 2, 0, 0]));
    R.head.add(part(id + '_respirator_b', cyl(.03, .035, .04, 8), mats.armor_dark, [-.05, -.06, -.11], [Math.PI / 2, 0, 0]));
    R.g.add(part(id + '_igniter', cyl(.02, .025, .10, 8), mats.brass, R.P.handR.map((v, i) => v + [0.05, -.02, -.06][i]), [Math.PI / 2, 0, 0]));
    R.g.add(part(id + '_igniter_flame', new THREE.ConeGeometry(.012, .04, 6), acc, R.P.handR.map((v, i) => v + [0.05, -.02, -.13][i]), [-Math.PI / 2, 0, 0]));
  }
  if (fid === 'tempest') {
    // Capacitor fins down the spine, antenna mast, arc-gap gauntlet, slimmer plates.
    T.add(K.finStack(id + '_spine_fins', 6, .04, .16, .34).translateY(.44).translateZ(.16).rotateX(90 * D));
    T.add(part(id + '_cap_l', cyl(.04, .04, .22, 10), mats.armor_dark, [-.16, .40, .20]));
    T.add(part(id + '_cap_r', cyl(.04, .04, .22, 10), mats.armor_dark, [.16, .40, .20]));
    T.add(part(id + '_cap_glow_l', new THREE.TorusGeometry(.042, .008, 6, 12), acc, [-.16, .40, .20], [Math.PI / 2, 0, 0]));
    T.add(part(id + '_cap_glow_r', new THREE.TorusGeometry(.042, .008, 6, 12), acc, [.16, .40, .20], [Math.PI / 2, 0, 0]));
    T.add(part(id + '_mast', cyl(.008, .012, .50, 6), mats.chrome, [-.20, .82, .12]));
    T.add(part(id + '_mast_tip', new THREE.SphereGeometry(.02, 8, 6), acc, [-.20, 1.08, .12]));
    for (let i = 0; i < 3; i++) T.add(part(id + '_mast_ring' + i, new THREE.TorusGeometry(.02 + i * .008, .004, 6, 10), mats.chrome, [-.20, .70 + i * .10, .12], [Math.PI / 2, 0, 0]));
    T.add(part(id + '_chest_arc', box(.16, .008, .006), acc, [0, .40, -.165]));
    T.add(part(id + '_chest_arc_b', box(.008, .12, .006), acc, [0, .46, -.165]));
    for (const p of [[.10, .50, .18], [-.06, .30, .18], [.02, .62, .16]]) T.add(K.cableRun(id + '_wire' + p[0], [[p[0], p[1], p[2]], [p[0] * .3, p[1] + .08, .22], [-.20, .70, .12]], .006, acc));
    // Gauntlet electrodes on the right hand.
    const hand = R.g.getObjectByName(id + '_hand_r');
    for (const z of [-.03, .03]) hand.add(part(id + '_electrode' + z, cyl(.006, .004, .05, 6), mats.chrome, [z, -.08, -.03], [.4, 0, 0]));
    hand.add(part(id + '_arc_gap', box(.06, .006, .006), acc, [0, -.10, -.045]));
    R.head.add(part(id + '_visor_wide', box(.22, .05, .04), mats.visor, [0, .0, -.115]));
    R.head.add(part(id + '_horn_l', new THREE.ConeGeometry(.02, .10, 5), mats.armor_dark, [-.10, .10, .02], [0, 0, .5]));
    R.head.add(part(id + '_horn_r', new THREE.ConeGeometry(.02, .10, 5), mats.armor_dark, [.10, .10, .02], [0, 0, -.5]));
  }
  if (fid === 'glacier') {
    // Cryo backpack: a frosted dewar with a sight-glass, coolant coils to both bracers, pale plates rimed at the edges, frost-crystal pauldron spikes, a full-face frosted visor.
    const pack = grp(id + '_pack', [0, .42, .22]);
    pack.add(part(id + '_dewar', cyl(.11, .12, .40, 14), mats.plate_white, [0, 0, .06]));
    pack.add(part(id + '_dewar_glass', new THREE.TorusGeometry(.12, .02, 8, 24), mats.frost_glass, [0, .06, .06], [Math.PI / 2, 0, 0]));
    pack.add(part(id + '_dewar_core', cyl(.06, .06, .30, 10), acc, [0, 0, .06]));
    for (let i = 0; i < 4; i++) pack.add(part(id + '_dewar_band' + i, new THREE.TorusGeometry(.122, .006, 6, 20), mats.chrome, [0, -.16 + i * .1, .06], [Math.PI / 2, 0, 0]));
    pack.add(part(id + '_dewar_valve', cyl(.03, .035, .06, 8), mats.brass, [0, .24, .06]));
    pack.add(part(id + '_dewar_frost', new THREE.IcosahedronGeometry(.14, 1), mats.frost_glass, [0, -.18, .08]));
    T.add(pack);
    for (const s of [-1, 1]) { T.add(K.cableRun(id + '_coil' + s, [[s * .08, .30, .30], [s * .22, .36, .18], [s * .26, .16, .02], [s * .24, -.02, -.02]], .012, acc)); for (let i = 0; i < 3; i++) T.add(part(id + '_spike' + s + i, new THREE.ConeGeometry(.02, .10 + i * .03, 5), mats.frost_glass, [s * (.20 + i * .03), .58 + i * .02, -.02 + i * .04], [0, 0, s * (.5 + i * .25)])); }
    T.add(part(id + '_chest_rime', box(.26, .05, .01), mats.frost_glass, [0, .36, -.165]));
    R.head.add(part(id + '_visor_full', new THREE.SphereGeometry(.11, 14, 10, 0, Math.PI * 2, 0, Math.PI * .55), mats.frost_glass, [0, .0, -.02], [Math.PI / 2, 0, 0]));
    R.head.add(part(id + '_visor_glow', box(.18, .012, .01), acc, [0, .0, -.12]));
    R.head.add(part(id + '_hood_crest', new THREE.ConeGeometry(.03, .12, 5), mats.plate_white, [0, .16, .0]));
  }
  if (fid === 'specter') {
    // Sensor suite: a scanner mast with a rotating dish, a half-cloak of void veil off the left shoulder, a multi-lens targeting visor, dark plates with violet seam lines, a wrist scanner.
    T.add(part(id + '_mast_base', cyl(.03, .04, .10, 8), mats.armor_dark, [.18, .60, .14]));
    T.add(part(id + '_mast', cyl(.008, .012, .40, 6), mats.chrome, [.18, .84, .14]));
    const dish = grp(id + '_dish', [.18, 1.02, .14]); dish.add(part(id + '_dish_face', new THREE.SphereGeometry(.07, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2.5), mats.armor, [0, 0, 0], [-Math.PI / 2, 0, 0])); dish.add(part(id + '_dish_lamp', new THREE.SphereGeometry(.015, 8, 6), acc, [0, 0, -.05])); T.add(dish);
    for (let i = 0; i < 3; i++) T.add(part(id + '_seam' + i, box(.006, .14 - i * .03, .006), acc, [-.08 + i * .08, .32 + i * .06, -.165]));
    T.add(part(id + '_cloak', new THREE.CylinderGeometry(.16, .26, .70, 10, 1, true, Math.PI * .1, Math.PI * .9), mats.void_veil, [-.14, .18, .04], [0, Math.PI / 2, 0]));
    T.add(part(id + '_cloak_clasp', new THREE.OctahedronGeometry(.03, 0), acc, [-.20, .56, -.02]));
    const hand = R.g.getObjectByName(id + '_hand_l');
    if (hand) { hand.add(part(id + '_wrist_scanner', box(.05, .02, .07), mats.armor_dark, [0, -.02, .0])); hand.add(part(id + '_wrist_screen', box(.04, .004, .05), acc, [0, -.007, 0])); }
    R.head.add(part(id + '_visor_band', box(.22, .04, .03), mats.armor_dark, [0, .0, -.11]));
    for (const x of [-.06, -.02, .03, .07]) R.head.add(part(id + '_lens' + x, new THREE.CylinderGeometry(.012 + Math.abs(x) * .1, .012 + Math.abs(x) * .1, .02, 8), acc, [x, .0, -.125], [Math.PI / 2, 0, 0]));
    R.head.add(part(id + '_head_fin', box(.01, .08, .12), mats.armor_dark, [0, .14, .02]));
  }
}

const hero = (fid, pose) => {
  const F = FACTIONS[fid], sfx = pose === 'stand' ? '' : '_' + pose;
  return {
    id: `hero_${fid}${sfx}`, file: `hero_${fid}${sfx}.glb`, label: `${F.label}${pose === 'stand' ? '' : pose === 'downed' ? ' — downed' : ' — revive crouch'}`, swatch: F.swatch,
    size: pose === 'stand' ? '1.85 m' : pose === 'downed' ? 'prone' : 'crouch',
    stats: { Ability: F.ability, Cooldown: F.cd, Radius: F.r, Passive: F.passive },
    note: { forge: 'The builder. Heavy brass-plated pauldrons, a tool backpack with a folding crane arm and cable spool, hazard band across the abs plate. Reads "engineer" from the crane silhouette alone.',
      ember: 'The shooter. Twin hazard-striped fuel tanks with sight-glass rings, braided hose down to the right bracer, hooded helmet with respirators, ember glow lines on the chest, hand igniter with a pilot flame.',
      tempest: 'The striker. Slim violet plates, capacitor fin stack down the spine, antenna mast with rings, wired capacitors, arc-gap electrodes on the right gauntlet, wide visor and swept horns.',
      glacier: 'The controller. Frosted dewar backpack with a sight-glass ring and cryo core, coolant coils to both bracers, pale rimed plates, frost-crystal pauldron spikes, a full frosted visor with a cold glow line.',
      specter: 'The scout. Dark plates with violet seam lines, a scanner mast with a rotating dish, a half-cloak of void veil off the left shoulder, a four-lens targeting visor, a wrist scanner. Sees what towers cannot.' }[fid]
      + (pose === 'downed' ? ' Downed: on the back, head toward +Z, one knee up, right arm reaching for a revive — the pose that says "hold R".' : pose === 'revive' ? ' Revive crouch: one knee down, hands forward and low over the ally. Sits ~0.6 m in front of the downed origin.' : ''),
    build(K) { const R = rig(K, this.id, F, pose); dress(K, this.id, fid, R); return R.g; },
  };
};

export const HEROES = [
  ...['forge', 'ember', 'tempest', 'glacier', 'specter'].flatMap((f) => ['stand', 'downed', 'revive'].map((p) => hero(f, p))),
  ...['forge', 'ember', 'tempest', 'glacier', 'specter'].map((fid) => {
    const F = FACTIONS[fid];
    return {
      id: 'hands_' + fid, file: `hands_${fid}.glb`, label: `${F.label} hands`, swatch: F.swatch, size: 'viewmodel', stats: { Base: 'hands_firstperson', Swap: 'plates + accent' },
      note: 'Faction fork of the base first-person arms: knuckle, forearm and cuff plates in the faction plate colour, an accent lamp on each bracer' + (fid === 'ember' ? ', braided fuel hose along the right forearm' : fid === 'tempest' ? ', capacitor ring and arc electrodes on the right cuff' : fid === 'glacier' ? ', frosted coolant coil around the right cuff' : fid === 'specter' ? ', wrist scanner screen on the left cuff' : ', tool loop and brass bolts on the left cuff') + '.',
      build(K) {
        const g = WEAPONS.find((w) => w.id === 'hands').build(K);
        g.name = this.id;
        const { part, THREE, mats, cyl } = K, acc = mats[F.acc], fplate = mats[F.plate];
        g.traverse((o) => { if (o.isMesh && o.material === mats.skin_plate) o.material = fplate; });
        for (const side of ['l', 'r']) {
          const h = g.getObjectByName('hand_' + side);
          h.add(part(`hand_${side}_lamp`, new THREE.BoxGeometry(.02, .004, .04), acc, [0, .04, .18]));
          h.add(part(`hand_${side}_cuff_plate`, new THREE.BoxGeometry(.05, .01, .04), fplate, [0, .032, .07]));
        }
        const r = g.getObjectByName('hand_r'), l = g.getObjectByName('hand_l');
        if (fid === 'ember') r.add(K.cableRun('hand_r_hose', [[.03, 0, .28], [.04, -.01, .14], [.02, .0, .06]], .006));
        if (fid === 'tempest') { r.add(part('hand_r_cap_ring', new THREE.TorusGeometry(.032, .005, 6, 14), acc, [0, 0, .09])); for (const x of [-.012, .012]) r.add(part('hand_r_electrode' + x, cyl(.003, .002, .03, 6), mats.chrome, [x, .02, -.03], [-.4, 0, 0])); }
        if (fid === 'glacier') for (let i = 0; i < 3; i++) r.add(part('hand_r_coil' + i, new THREE.TorusGeometry(.034, .004, 6, 14), acc, [0, 0, .07 + i * .02]));
        if (fid === 'specter') { l.add(part('hand_l_scanner', new THREE.BoxGeometry(.05, .015, .06), mats.armor_dark, [-.03, .0, .10])); l.add(part('hand_l_screen', new THREE.BoxGeometry(.04, .004, .05), acc, [-.03, .008, .10])); }
        if (fid === 'forge') { l.add(part('hand_l_tool_loop', new THREE.TorusGeometry(.012, .003, 6, 10), mats.brass, [-.035, 0, .16], [0, Math.PI / 2, 0])); l.add(K.boltRing('hand_l_bolts', .03, 4, .036, mats.brass).translateZ(.07)); }
        return g;
      },
    };
  }),
  {
    id: 'ui_nameplate', file: 'ui_nameplate.glb', label: 'Nameplate', swatch: '#c3ccd8', size: '0.6 m', stats: { Anchor: 'head + 0.35 m', Faces: 'camera (billboard)' },
    note: 'World-space name-tag plate: chamfered dark plate with a brass rule, faction chip on the left (tint at runtime via `nameplate_chip`), HP bar track and fill (`nameplate_hp` scale X), and a hanging pin to the head. Text is a runtime label on `nameplate_text`.',
    build(K) {
      const { part, grp, THREE, mats, box, cyl } = K, g = grp('ui_nameplate');
      const shape = new THREE.Shape(); const w = .60, h = .14, c = .03;
      shape.moveTo(-w / 2 + c, -h / 2); shape.lineTo(w / 2 - c, -h / 2); shape.lineTo(w / 2, -h / 2 + c); shape.lineTo(w / 2, h / 2 - c); shape.lineTo(w / 2 - c, h / 2); shape.lineTo(-w / 2 + c, h / 2); shape.lineTo(-w / 2, h / 2 - c); shape.lineTo(-w / 2, -h / 2 + c); shape.closePath();
      g.add(part('nameplate_plate', new THREE.ExtrudeGeometry(shape, { depth: .01, bevelEnabled: false }), mats.chassis, [0, .07, .005], [0, Math.PI, 0]));
      g.add(part('nameplate_rule', box(.54, .004, .002), mats.brass, [0, .105, -.007]));
      g.add(part('nameplate_chip', box(.06, .06, .004), mats.energy_fuse, [-.25, .085, -.008]));
      g.add(part('nameplate_text', box(.34, .05, .001), mats.steel_plate, [.03, .09, -.008]));
      g.add(part('nameplate_hp_track', box(.50, .02, .003), mats.trim, [0, .03, -.008]));
      const hp = part('nameplate_hp', box(.50, .016, .003), mats.energy_scan, [0, .03, -.01]); hp.geometry.translate(.25, 0, 0); hp.position.x = -.25; g.add(hp);
      g.add(part('nameplate_pin', cyl(.004, .004, .08, 6), mats.chrome, [0, -.04, 0]));
      g.add(part('nameplate_pin_head', new THREE.SphereGeometry(.008, 8, 6), mats.brass, [0, -.08, 0]));
      return g;
    },
  },
];
export { FACTIONS };
