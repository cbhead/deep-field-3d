/**
 * Deep Field 3D — weapons & gunsmith (DESIGN-BRIEF §3.4).
 * Metres, Y-up, muzzle toward −Z. Weapon origin = the grip (where the firing
 * hand's web sits); the bore is ~6 cm above it. Every platform exposes the seven
 * gunsmith slots as empty mount nodes, so "the gun you built is the gun you see":
 *
 *   <w>_mount_barrel      receiver face; barrel attachments REPLACE <w>_barrel
 *   <w>_mount_muzzle      end of the stock barrel (barrel attachments carry
 *                         their own `attach_mount_muzzle` — re-parent there)
 *   <w>_mount_optic       top rail
 *   <w>_mount_magazine    mag well / tube cap; drum REPLACES <w>_magazine
 *   <w>_mount_stock       rear of receiver; brace REPLACES <w>_stock if present
 *   <w>_mount_underbarrel below the barrel root
 *   <w>_mount_infusion    left side of the receiver (the status-applier cell)
 *
 * Modelling vocabulary (all smooth-shaded, bevelled — no flat-shaded boxes):
 *   plate()  side profile (z,y) extruded across X with a rounded bevel — receivers,
 *            frames, grips, mags, stocks; shape.holes give REAL trigger guards,
 *            lightening cuts and handguard slots.
 *   bar()    cross-section (x,y) extruded along Z with bevel — rails, pads, blocks.
 *   lathe()  turned parts along the bore — barrels, muzzles, tubes, optics, rounds.
 *   chain()  capsule bones between joints — fingers and thumbs.
 * Attachments are built with their origin ON the mount, so `mount.add(attach)`.
 * Ammo files are a magazine + exposed round; tracers are unit-length along −Z.
 */
import { makeKit } from './tower-kit.js';
import { buildSidearmHero, buildHeroHands, buildOffhandPose, OFFHAND_POSES } from './sidearm-hero.js';
import { buildRifleHero } from './rifle-hero.js';
import { buildScattergunHero } from './scattergun-hero.js';
import { buildEmberPistolHero } from './emberpistol-hero.js';

export function makeWeaponKit(THREE) {
  const K = makeKit(THREE);
  const { mats } = K;
  const mat = (name, color, o = {}) => { const m = new THREE.MeshStandardMaterial({ color, roughness: .5, metalness: .6, ...o }); m.name = name; mats[name] = m; return m; };
  const phys = (name, color, o = {}) => { const m = new THREE.MeshPhysicalMaterial({ color, roughness: .7, metalness: 0, ...o }); m.name = name; mats[name] = m; return m; };
  mat('gun_metal', 0x3a4250, { roughness: .38, metalness: .85 });          // anodised receiver grey
  mat('gun_dark', 0x14171d, { roughness: .42, metalness: .8 });            // DLC black
  mat('gun_pale', 0x9aa4b4, { roughness: .28, metalness: .9 });            // bead-blasted aluminium
  mat('gun_bronze', 0x8a6b3a, { roughness: .32, metalness: .9 });          // nitride barrel / bolt
  phys('polymer', 0x1c2029, { roughness: .7, clearcoat: .3, clearcoatRoughness: .5 });
  phys('grip_rubber', 0x0f1114, { roughness: .96 });
  phys('shell_hull', 0x7a2a22, { roughness: .55, clearcoat: .2 });
  phys('glove', 0x2b2e35, { roughness: .82, clearcoat: .12, clearcoatRoughness: .7 });
  phys('sleeve', 0x2b3a5c, { roughness: .9 });
  mat('skin_plate', 0xc89b3c, { roughness: .45, metalness: .5 });
  mat('frost', 0xcfe7f2, { roughness: .8, metalness: .05 });
  const glow = (n, hex, i = 1) => mat(n, hex, { roughness: .3, metalness: .05, emissive: new THREE.Color(hex), emissiveIntensity: i });
  glow('infuse_ember', 0xe8622b); glow('infuse_cryo', 0x4fc0e8); glow('infuse_volt', 0xf05ae6); glow('infuse_gravium', 0x9b5be8, .8);
  glow('tracer_standard', 0xf4dca4, 1.2); glow('tracer_ap', 0xcfd7e4, 1.4); glow('tracer_hp', 0xe8862b, 1.2);
  /* Realistic tracer stack. Deliberately UNLIT (MeshBasicMaterial → KHR_materials_unlit)
     with the whole colour + falloff ramp baked into per-vertex RGBA, so the streak reads
     the same in any GLTF viewer and needs no additive-blend override on import. */
  const basic = (n, color, o = {}) => { const m = new THREE.MeshBasicMaterial({ color, ...o }); m.name = n; mats[n] = m; return m; };
  basic('tracer_streak', 0xffffff, { vertexColors: true, transparent: true, depthWrite: false, side: THREE.DoubleSide });
  basic('tracer_hot', 0xfff6e4);
  basic('tracer_hot_ap', 0xeaf2ff); basic('tracer_hot_hp', 0xffd49a);
  basic('tracer_hot_inc', 0xffc078); basic('tracer_hot_cryo', 0xdff4ff);
  basic('tracer_ice', 0x9fdcf5, { transparent: true, opacity: .5, depthWrite: false });
  basic('tracer_smoke_v', 0xffffff, { vertexColors: true, transparent: true, depthWrite: false, side: THREE.DoubleSide });
  glow('ammo_lamp', 0x7fe65a, .9);
  glow('flame_glow', 0xffa24a, .8); mats.flame_glow.transparent = true; mats.flame_glow.opacity = .35;
  mat('glass_dark', 0x0e1a26, { roughness: .1, metalness: .2, transparent: true, opacity: .8 });
  mats.optic_glass.metalness = .2; mats.optic_glass.roughness = .08;
  for (const n of ['chrome', 'brass', 'trim']) mats[n].flatShading = false;
  mats.chrome.metalness = .9; mats.chrome.roughness = .2; mats.brass.metalness = .9; mats.brass.roughness = .3;
  return K;
}

/* ── geometry vocabulary ─────────────────────────────────────────────── */
const shape = (THREE, pts) => { const s = new THREE.Shape(); pts.forEach(([x, y], i) => (i ? s.lineTo(x, y) : s.moveTo(x, y))); s.closePath(); return s; };
function rr(THREE, x, y, w, h, r) {
  r = Math.min(r, w / 2, h / 2);
  const s = new THREE.Shape();
  s.moveTo(x + r, y); s.lineTo(x + w - r, y); s.quadraticCurveTo(x + w, y, x + w, y + r);
  s.lineTo(x + w, y + h - r); s.quadraticCurveTo(x + w, y + h, x + w - r, y + h);
  s.lineTo(x + r, y + h); s.quadraticCurveTo(x, y + h, x, y + h - r);
  s.lineTo(x, y + r); s.quadraticCurveTo(x, y, x + r, y);
  return s;
}
const circ = (THREE, x, y, r) => new THREE.Path().absarc(x, y, r, 0, Math.PI * 2, false);
const poly = (THREE, cx, cy, r, n, a0 = 0) => shape(THREE, Array.from({ length: n }, (_, i) => [cx + Math.cos(a0 + i / n * Math.PI * 2) * r, cy + Math.sin(a0 + i / n * Math.PI * 2) * r]));
const extrude = (THREE, shp, depth, b, seg) => new THREE.ExtrudeGeometry(shp, { depth: Math.max(depth, .0004), bevelEnabled: b > 0, bevelThickness: b, bevelSize: b, bevelSegments: seg, curveSegments: 10 });
const finish = (K, name, geo, material, o) => { const m = new K.THREE.Mesh(geo, material); m.name = name; if (o.pos) m.position.set(...o.pos); if (o.rot) m.rotation.set(...o.rot); return m; };
/** Side profile in (z,y) → solid of thickness t across X, centred. */
function plate(K, name, shp, t, material, o = {}) {
  const b = o.b ?? .0015, geo = extrude(K.THREE, shp, t - 2 * b, b, o.seg ?? 2);
  geo.rotateY(-Math.PI / 2); geo.translate(t / 2 - b, 0, 0);
  return finish(K, name, geo, material, o);
}
/** Cross-section in (x,y) → solid of length len along Z, centred. */
function bar(K, name, shp, len, material, o = {}) {
  const b = o.b ?? .0015, geo = extrude(K.THREE, shp, len - 2 * b, b, o.seg ?? 2);
  geo.translate(0, 0, -(len / 2 - b));
  return finish(K, name, geo, material, o);
}
/** Turned part: pts = [[radius, distance]]; axis 'z' runs forward (−Z), 'y' up, 'x' right. */
function lathe(K, name, pts, material, pos = [0, 0, 0], o = {}) {
  const { THREE } = K, geo = new THREE.LatheGeometry(pts.map(([r, d]) => new THREE.Vector2(r, d)), o.seg ?? 32);
  if ((o.axis ?? 'z') === 'z') geo.rotateX(-Math.PI / 2); else if (o.axis === 'x') geo.rotateZ(-Math.PI / 2);
  return finish(K, name, geo, material, { pos, rot: o.rot });
}
/** Capsule bones through a joint chain. */
function chain(K, name, pts, r, material) {
  const { THREE, grp } = K, g = grp(name), Y = new THREE.Vector3(0, 1, 0);
  for (let i = 0; i < pts.length - 1; i++) {
    const a = new THREE.Vector3(...pts[i]), b = new THREE.Vector3(...pts[i + 1]), d = b.clone().sub(a), len = d.length();
    const m = new THREE.Mesh(new THREE.CapsuleGeometry(r, len, 4, 14), material); m.name = `${name}_${i}`;
    m.position.copy(a).add(b).multiplyScalar(.5); m.quaternion.setFromUnitVectors(Y, d.normalize()); g.add(m);
  }
  return g;
}
const tube = (K, name, r, len, pos, material) => lathe(K, name, [[0, -len / 2], [r, -len / 2], [r, len / 2], [0, len / 2]], material || K.mats.gun_metal, pos, { seg: 24 });
function mount(K, id, slot, pos, rot = [0, 0, 0]) { const g = K.grp(`${id}_mount_${slot}`, pos); g.rotation.set(...rot); g.userData.slot = slot; return g; }
/** Picatinny-style rail: T-section base with flush dark slots. flip = hangs below. */
function rail(K, name, len, pos, w = .021, flip = false) {
  const { THREE, mats, grp, part } = K, g = grp(name, pos); if (flip) g.rotation.z = Math.PI;
  g.add(bar(K, name + '_base', shape(THREE, [[-w / 2, 0], [w / 2, 0], [w / 2, .0045], [w / 2 - .0035, .008], [w / 2 - .0035, .010], [-w / 2 + .0035, .010], [-w / 2 + .0035, .008], [-w / 2, .0045]]), len, mats.gun_dark, { b: .0004 }));
  const n = Math.max(2, Math.floor(len / .010));
  for (let i = 0; i < n; i++) g.add(part(`${name}_slot${i}`, new THREE.BoxGeometry(w - .005, .0045, .0055), mats.trim, [0, .0078, -len / 2 + (i + .5) * (len / n)]));
  return g;
}
/** Torx screw heads. axis = the screw's own axis. */
function screws(K, name, pts, r = .0032, axis = 'x') {
  const { THREE, mats, grp } = K, g = grp(name), rot = axis === 'x' ? [0, 0, Math.PI / 2] : axis === 'z' ? [Math.PI / 2, 0, 0] : [0, 0, 0];
  const head = new THREE.CylinderGeometry(r, r * .9, .0018, 16), star = new THREE.CylinderGeometry(r * .45, r * .45, .001, 6);
  pts.forEach((p, i) => {
    const h = new THREE.Mesh(head, mats.chrome); h.name = `${name}_${i}`; h.position.set(...p); h.rotation.set(...rot); g.add(h);
    const s = new THREE.Mesh(star, mats.trim); s.name = `${name}_${i}_x`; s.position.set(...p); s.rotation.set(...rot);
    const off = axis === 'x' ? [Math.sign(p[0] || 1) * .0009, 0, 0] : axis === 'z' ? [0, 0, Math.sign(p[2] || 1) * .0009] : [0, .0009, 0];
    s.position.x += off[0]; s.position.y += off[1]; s.position.z += off[2]; g.add(s);
  });
  return g;
}
/** Rectangular plate (z,y) with n rounded through-slots; thickness t across X. */
function slotPlate(K, name, len, h, t, n, sw, sh, material, o = {}) {
  const { THREE } = K, s = rr(THREE, -len / 2, -h / 2, len, h, o.r ?? .003), step = len / n;
  for (let i = 0; i < n; i++) s.holes.push(rr(THREE, -len / 2 + (i + .5) * step - sw / 2, -sh / 2, sw, sh, sh / 2));
  return plate(K, name, s, t, material, { b: o.b ?? .0006, pos: o.pos, rot: o.rot });
}
const GUARD = (THREE, z0 = -.056) => rr(THREE, z0, -.012, .044, .024, .006);
const RAKE = 17;
/** Pistol grip at the origin: raked 17°, palm swell, rubber wrap panels, flared base. Rake is baked into the profile. */
function grip(K, id) {
  const { THREE, mats, grp, part, D } = K, tn = Math.tan(RAKE * D), H = .105;
  const fz = (y) => -.012 + -y * tn, bz = (y) => .034 + -y * tn + .006 * Math.sin(Math.PI * -y / H);
  const pts = [[-.012, .012]];
  for (let i = 0; i <= 6; i++) { const y = -H * i / 6; pts.push([fz(y) + (i === 1 ? .0025 : 0), y]); }
  pts.push([fz(-H) + .005, -H - .004], [bz(-H) - .004, -H - .004]);
  for (let i = 6; i >= 0; i--) { const y = -H * i / 6; pts.push([bz(y), y]); }
  pts.push([.056, .004], [.052, .012]);
  const g = grp(id + '_grip');
  g.add(plate(K, id + '_grip_body', shape(THREE, pts), .027, mats.polymer, { b: .0025, seg: 3 }));
  const pp = [];
  for (let i = 1; i <= 5; i++) { const y = -H * i / 6; pp.push([fz(y) + .006, y]); }
  for (let i = 5; i >= 1; i--) { const y = -H * i / 6; pp.push([bz(y) - .005, y]); }
  g.add(plate(K, id + '_grip_panels', shape(THREE, pp), .0295, mats.grip_rubber, { b: .0015 }));
  for (let i = 0; i < 6; i++) { const y = -.03 - i * .011; g.add(part(`${id}_grip_groove${i}`, new THREE.BoxGeometry(.0305, .0014, .026), mats.gun_dark, [0, y, (fz(y) + bz(y)) / 2])); }
  g.add(bar(K, id + '_grip_base', rr(THREE, -.015, -.005, .030, .009, .003), .050, mats.gun_dark, { b: .001, pos: [0, -H - .005, (fz(-H) + bz(-H)) / 2] }));
  return g;
}
/** Flat-faced trigger blade with safety tab; pos = top pivot inside the guard hole. */
function trigger(K, id, pos) {
  const { THREE, mats, grp, part } = K, g = grp(id + '_trigger_group', pos);
  g.add(plate(K, id + '_trigger', shape(THREE, [[-.003, .005], [.004, .005], [.0045, -.006], [.003, -.019], [-.002, -.021], [-.0045, -.010]]), .007, mats.gun_pale, { b: .0008 }));
  g.add(part(id + '_trigger_tab', new THREE.BoxGeometry(.002, .010, .0025), mats.chrome, [0, -.012, -.005]));
  return g;
}
/** Modern irons: fibre-optic front blade, serrated rear block with two dots. */
function sights(K, id, front, rear) {
  const { THREE, mats, grp, part } = K, g = grp(id + '_sights');
  g.add(bar(K, id + '_front_post', rr(THREE, -.0015, 0, .003, .009, .0008), .007, mats.gun_dark, { b: .0004, pos: front }));
  g.add(tube(K, id + '_front_fibre', .0012, .006, [front[0], front[1] + .0065, front[2]], mats.ammo_lamp));
  g.add(bar(K, id + '_rear_block', rr(THREE, -.011, 0, .022, .008, .0015), .010, mats.gun_dark, { b: .0006, pos: rear }));
  g.add(part(id + '_rear_notch', new THREE.BoxGeometry(.004, .005, .011), mats.trim, [rear[0], rear[1] + .006, rear[2]]));
  for (const s of [-1, 1]) g.add(part(`${id}_rear_dot${s}`, new THREE.SphereGeometry(.0011, 10, 8), mats.ammo_lamp, [rear[0] + s * .005, rear[1] + .004, rear[2] + .005]));
  return g;
}
/** Straight pistol magazine hanging from the grip: polymer body, extended base pad. */
function pistolMag(K, id, pos) {
  const { THREE, mats, grp } = K, g = grp(id + '_magazine', pos);
  g.add(bar(K, id + '_mag_body', rr(THREE, -.011, 0, .022, .024, .002), .036, mats.polymer, { b: .001, pos: [0, .0, 0] }));
  g.add(bar(K, id + '_mag_base', rr(THREE, -.0175, -.011, .035, .011, .003), .052, mats.gun_dark, { b: .0015 }));
  g.add(screws(K, id + '_mag_screws', [[0, -.011, -.02], [0, -.011, .02]], .0025, 'y'));
  return g;
}
/** Curved rifle magazine (real curve in the profile), witness strip with brass. */
function curvedMag(K, id, pos, h = .15, w = .024, depth = .052) {
  const { THREE, mats, grp, part } = K, g = grp(id + '_magazine', pos), n = 8, zc = (t) => -.022 * t * t;
  const f = [], b = [];
  for (let i = 0; i <= n; i++) { const t = i / n, y = -h * t; f.push([zc(t) - depth / 2, y]); b.push([zc(t) + depth / 2, y]); }
  g.add(plate(K, id + '_mag_body', shape(THREE, [...f, ...b.reverse()]), w, mats.polymer, { b: .003, seg: 3 }));
  g.add(bar(K, id + '_mag_base', rr(THREE, -w / 2 - .002, -.006, w + .004, .011, .003), depth + .008, mats.gun_dark, { b: .0015, pos: [0, -h, zc(1)], rot: [-.15, 0, 0] }));
  for (let i = 0; i < 5; i++) { const t = .2 + i * .15, y = -h * t; g.add(part(`${id}_mag_witness${i}`, new THREE.CylinderGeometry(.0028, .0028, .0016, 12), mats.trim, [w / 2, y, zc(t) - .006], [0, 0, Math.PI / 2])); g.add(part(`${id}_mag_round${i}`, new THREE.CylinderGeometry(.002, .002, .001, 12), mats.brass, [w / 2 + .0006, y, zc(t) - .006], [0, 0, Math.PI / 2])); }
  return g;
}
/** Adjustable stock on a buffer tube; +Z is rearward. */
function stockAdj(K, id, pos) {
  const { THREE, mats, grp, part } = K, g = grp(id + '_stock', pos);
  g.add(lathe(K, id + '_stock_tube', [[0, 0], [.013, 0], [.013, -.20], [.011, -.20], [.011, -.208], [0, -.208]], mats.gun_metal, [0, 0, 0]));
  for (let i = 0; i < 5; i++) g.add(part(`${id}_stock_notch${i}`, new THREE.TorusGeometry(.0132, .0008, 6, 28), mats.trim, [0, 0, .06 + i * .02]));
  const body = shape(THREE, [[.085, .018], [.20, .022], [.212, -.050], [.196, -.062], [.11, -.036], [.09, -.010]]);
  body.holes.push(rr(THREE, .12, -.036, .05, .016, .006));
  g.add(plate(K, id + '_stock_body', body, .034, mats.polymer, { b: .004, seg: 3 }));
  g.add(bar(K, id + '_cheek_riser', rr(THREE, -.014, 0, .028, .012, .004), .10, mats.gun_dark, { b: .0015, pos: [0, .013, .15] }));
  g.add(plate(K, id + '_stock_pad', shape(THREE, [[.208, .026], [.222, .024], [.226, -.060], [.212, -.064], [.204, -.046]]), .036, mats.grip_rubber, { b: .003 }));
  g.add(part(id + '_qd_cup', new THREE.CylinderGeometry(.005, .005, .002, 16), mats.trim, [-.0175, -.02, .13], [0, 0, Math.PI / 2]));
  g.add(bar(K, id + '_stock_lever', rr(THREE, -.007, -.006, .014, .012, .003), .03, mats.gun_dark, { b: .001, pos: [0, -.017, .105] }));
  return g;
}
/** Barrel group with a turned profile; flutes optional; muzzle threads + crown. */
function barrelGroup(K, id, r, len, from, o = {}) {
  const { THREE, mats, grp, part } = K, g = grp(id + '_barrel', from), rc = o.rc ?? r * 1.3, m = o.material || mats.gun_dark;
  g.add(lathe(K, id + '_barrel_tube', [[0, 0], [rc, 0], [rc, .018], [r, .022], [r, len - .03], [r * .93, len - .03], [r * .93, len - .004], [r * .8, len - .001], [r * .55, len], [0, len]], m, [0, 0, 0]));
  for (let i = 0; i < 3; i++) g.add(part(`${id}_thread${i}`, new THREE.TorusGeometry(r * .93, .0005, 6, 28), mats.gun_metal, [0, 0, -(len - .008 - i * .006)]));
  g.add(part(id + '_bore', new THREE.CylinderGeometry(r * .5, r * .5, .002, 16), mats.trim, [0, 0, -len], [Math.PI / 2, 0, 0]));
  if (o.flutes) { const [a, b] = o.flutes; for (let i = 0; i < 6; i++) { const t = i / 6 * Math.PI * 2; g.add(part(`${id}_flute${i}`, new THREE.CylinderGeometry(r * .18, r * .18, b - a, 8), mats.trim, [Math.cos(t) * r, Math.sin(t) * r, -(a + b) / 2], [Math.PI / 2, 0, 0])); } }
  if (o.gasBlock) { const d = o.gasBlock; g.add(bar(K, id + '_gas_block', rr(THREE, -r - .002, -r - .002, r * 2 + .004, r * 2 + .008, .002), .022, mats.gun_dark, { b: .001, pos: [0, .002, -d] })); g.add(tube(K, id + '_gas_tube', .0024, d - .03, [0, r + .006, -(d / 2 + .012)], mats.gun_pale)); g.add(screws(K, id + '_gas_screws', [[0, -r - .002, -d - .005], [0, -r - .002, -d + .005]], .002, 'y')); }
  return g;
}

/* ── first-person hands ──────────────────────────────────────────────── */
/**
 * One hand wrapping a bar. Everything is defined in the caller's frame, where the
 * bar runs along local Y, fingers cross its −Z face and the palm sits on +X
 * (mirrored for the left). Returns the arm group `hand_<side>` (origin at the
 * wrist, +Z toward the elbow — faction forks decorate this frame) with the
 * palm/finger rig re-parented under it.
 */
function wrapHand(K, side, P) {
  const { THREE, mats, grp, part } = K, s = side === 'l' ? -1 : 1, m = (p) => [p[0] * s, p[1], p[2]];
  const { hw, zf, zb, levels, indexPts, thumbPts, wrist, elbow } = P, rs = P.rs || [.0062, .0068, .0074, .0072];
  const rig = grp(`hand_${side}_rig`);
  const fingerPts = (r, y) => { const o = hw + r, f = zf - r, back = Math.min(zb, f + (P.wrapLen ?? .05)); return [[o + .006, y, f + .032], [o, y, f], [-o + .001, y - .001, f + .001], [-o, y - .003, back - .012], [-o + .005, y - .005, back]]; };
  levels.forEach((y, i) => {
    const r = rs[i], pts = (i === 3 && indexPts) ? indexPts : fingerPts(r, y);
    rig.add(chain(K, `hand_${side}_finger${i}`, pts.map(m), r, mats.glove));
    rig.add(part(`hand_${side}_knuckle${i}`, new THREE.SphereGeometry(r * 1.15, 12, 10), mats.glove, m(pts[1])));
  });
  rig.add(chain(K, `hand_${side}_thumb`, thumbPts.map(m), .0085, mats.glove));
  const yc = (levels[0] + levels[3]) / 2, h = levels[3] - levels[0] + .026, px = hw + .013, pl = P.palmLen ?? .082;
  rig.add(bar(K, `hand_${side}_palm`, rr(THREE, -.011, -h / 2, .022, h, .009), pl, mats.glove, { b: .005, seg: 3, pos: m([px, yc, zf + pl / 2 - .005]) }));
  if (P.heel !== false) rig.add(bar(K, `hand_${side}_heel`, rr(THREE, -.014, -.026, .028, .052, .010), .03, mats.glove, { b: .008, seg: 3, pos: m([.010, yc - .012, zb + .014]) }));
  rig.add(plate(K, `hand_${side}_knuckle_plate`, rr(THREE, zf + .006, yc - h / 2 + .012, .032, h - .024, .006), .004, mats.skin_plate, { b: .001, pos: m([px + .0125, 0, 0]) }));
  // Arm frame at the wrist, +Z toward the elbow.
  const arm = grp('hand_' + side), w = new THREE.Vector3(...m(wrist)), e = new THREE.Vector3(...m(elbow));
  arm.position.copy(w);
  arm.quaternion.setFromRotationMatrix(new THREE.Matrix4().lookAt(e, w, new THREE.Vector3(...(P.up || [0, 1, 0]))));
  arm.add(lathe(K, `hand_${side}_forearm`, [[0, .0], [.024, .0], [.028, -.006], [.031, -.03], [.037, -.12], [.041, -.22], [.040, -.245], [0, -.245]], mats.sleeve, [0, 0, 0]));
  arm.add(lathe(K, `hand_${side}_cuff`, [[.026, -.002], [.032, -.008], [.032, -.034], [.029, -.038], [.026, -.034], [.026, -.002]], mats.gun_dark, [0, 0, 0]));
  arm.add(part(`hand_${side}_cuff_strap`, new THREE.TorusGeometry(.0325, .0025, 8, 32), mats.skin_plate, [0, 0, .022]));
  arm.add(part(`hand_${side}_sleeve_roll`, new THREE.TorusGeometry(.042, .006, 10, 36), mats.sleeve, [0, 0, .238]));
  arm.add(bar(K, `hand_${side}_forearm_plate`, rr(THREE, -.015, -.003, .03, .006, .002), .09, mats.skin_plate, { b: .0015, pos: [0, .038, .15] }));
  arm.updateMatrix();
  rig.applyMatrix4(arm.matrix.clone().invert());
  arm.add(rig);
  return arm;
}
const PISTOL_R = { hw: .015, zf: -.012, zb: .038, levels: [-.068, -.050, -.032, -.010], indexPts: [[.028, -.006, .016], [.024, -.004, -.012], [.014, .002, -.028], [.005, .008, -.035]], thumbPts: [[.026, -.008, .034], [.006, -.010, .048], [-.016, -.014, .044], [-.022, -.020, .016], [-.020, -.028, -.004]], wrist: [.030, -.058, .076], elbow: [.05, -.15, .34] };

/* ── platforms ───────────────────────────────────────────────────────── */
const M = (w) => ({ ...w, file: `weapon_${w.id}_vm.glb`, worldFile: `weapon_${w.id}_world.glb` });

export const WEAPONS = [
  M({
    id: 'sidearm', handsPose: 'pistol', label: 'Sidearm', swatch: '#14171d', stats: { Cost: '0', Damage: '5', Rate: '3.0/s', Range: '60 m' },
    note: 'The starter, never taken away — and the hero asset the rest of the armoury is being rebuilt against. Striker-fired micro-compact, DLC-black slide over a graphite polymer frame: two blacks separated by surface, not colour. Every surface is a swept compound section rather than a constant extrusion, so the slide crowns, tapers at the nose and carries crisp machined chamfers with smooth faces between them. Detail is cut IN, not stacked on: forward-raked cocking serrations milled into both flats, a milled ejection pocket with the barrel hood and breech face standing inside it, a hooked extractor and firing-pin hole, an optic cut under its cover plate, five witness holes down the 15-round magazine. Roll-marked BODIED GUARD 3.0 on the left flat, 9×19MM on the right. Tritium night sights, three vials. Surface is procedural PBR at 2K — draw-polish on the DLC, glass-filler mottle in the polymer, a pebble stipple field with the palm patch rubbed smooth — and honest wear rides in vertex colour, so the serration lands, muzzle crown and control faces burnish themselves. Seven mounts, so even the free gun is a build.',
    build(K) {
      return buildSidearmHero(K);
    },
  }),
  M({
    id: 'rifle', handsPose: 'rifle', label: 'Rifle', swatch: '#7d6134', stats: { Cost: '120', Damage: '11', Rate: '2.2/s', Range: '80 m', Applies: 'mark' },
    note: 'Hitscan priority tool, rebuilt to the hero standard and measured against a real 14.5" carbine rather than eyeballed: 25.4 mm receiver, 368 mm barrel, 10.2 mm Picatinny slot pitch, 36 mm M-LOK pitch, STANAG magazine section, 89 mm carrier travel. Every detail that used to be a box glued to a slab is now cut in — Picatinny cross-slots milled through the rail rib for its full length, M-LOK cut clean through four faces of the free-float handguard, the ejection port a real pocket with the bolt carrier standing inside it, the brass deflector and forward-assist boss swelled out of the receiver wall instead of stuck onto it, A2 birdcage ports cut through the flash hider, magwell flare and stock lightening pockets as displacement. Bronze anodising keeps the weapon\'s identity; only the quality level rises. The carrier group reciprocates on its declared axis and throws a 5.56 case from the port.',
    build(K) {
      return buildRifleHero(K);
    },
  }),
  M({
    id: 'scattergun', handsPose: 'rifle', label: 'Scattergun', swatch: '#3a4250', stats: { Cost: '100', Damage: '24', Rate: '1.1/s', Range: '14 m' },
    note: 'Panic weapon for the perch, rebuilt to the hero standard and measured against a real semi-auto tactical 12-gauge: 18.5 mm bore, 18\" barrel, 41 \u00d7 45 mm boxed alloy receiver, 808 mm overall. The hull sizes the gun \u2014 barrel, magazine tube, loading port and ejection port all derive from it. Ejection port cut into the right wall only; loading port cut up into the belly between the side walls; one polymer handguard over barrel AND tube, vented on both flanks. Parkerised phosphate throughout: a crystalline conversion coating, not dark steel with the roughness turned up. Ghost-ring rear on a short rail, hooded tritium front post. The bolt assembly reciprocates 62 mm and throws fired hulls \u2014 brass head, dyed plastic body, blown-out crimp.',
    build(K) {
      return buildScattergunHero(K);
    },
  }),
  M({
    id: 'emberpistol', handsPose: 'pistol', label: 'Ember pistol', swatch: '#e8622b', stats: { Cost: '90', Damage: '4', Rate: '2.0/s', Range: '40 m', Applies: 'burn' },
    note: 'The shooter\'s half of Thermal Shock, rebuilt to the hero standard. No real-world twin, so every part is grounded in a real MACHINE instead: signal-pistol proportions (150 mm barrel, 288 mm overall, 18 mm bore), a canister built as an actual pressure vessel with torispherical ends and a rolled seam, a turned brass handwheel on a brazed boss, and a braided hose that lands in swaged ferrules at BOTH ends rather than floating. Cooling fins are steps cut into the barrel\u2019s turned profile, not discs threaded onto a tube. Fuel-fed, so it ejects nothing \u2014 what it has is a breech block that kicks 8.5 mm on discharge.',
    build(K) {
      return buildEmberPistolHero(K);
    },
  }),
  {
    id: 'wrench', handsPose: 'tool', file: 'weapon_wrench_vm.glb', label: 'Wrench', swatch: '#7fe65a', stats: { Role: 'melee · repair', Slot: 'starter tool' },
    note: 'Powered repair wrench: turned aluminium handle in a grooved rubber overmould, polymer motor housing with vent slots and a hazard band, vertical foregrip, forged head with a fixed upper jaw and worm-driven lower jaw, scanner glow in the gap, repair-status lamp that reads green when a structure is topped up. Origin at the grip.',
    build(K) {
      const { THREE, mats, grp, part } = K, id = 'wrench', g = grp(id);
      g.add(lathe(K, id + '_handle', [[0, .02], [.011, .02], [.011, 0], [.013, 0], [.013, -.120], [.016, -.122], [.016, -.130], [.012, -.135], [0, -.135]], mats.gun_pale, [0, .050, -.020]));
      g.add(lathe(K, id + '_handle_grip', [[.013, 0], [.0165, -.004], [.0165, -.100], [.013, -.104], [.013, 0]], mats.grip_rubber, [0, .050, .0]));
      for (let i = 0; i < 5; i++) g.add(part(`${id}_grip_ring${i}`, new THREE.TorusGeometry(.0165, .0012, 6, 28), mats.gun_dark, [0, .050, .014 + i * .018]));
      g.add(part(id + '_lanyard', new THREE.TorusGeometry(.011, .0018, 8, 20), mats.chrome, [0, .032, .112], [0, Math.PI / 2, 0]));
      g.add(plate(K, id + '_housing', shape(THREE, [[-.060, .014], [-.160, .014], [-.170, .030], [-.170, .090], [-.160, .100], [-.060, .100], [-.050, .080], [-.050, .030]]), .050, mats.polymer, { b: .006, seg: 3 }));
      for (const s of [-1, 1]) g.add(slotPlate(K, `${id}_vents${s}`, .060, .036, .002, 5, .006, .026, mats.gun_dark, { pos: [s * .0255, .060, -.115], r: .002 }));
      g.add(part(id + '_hazard_band', new THREE.BoxGeometry(.052, .0025, .046), mats.hazard, [0, .1005, -.115]));
      g.add(part(id + '_status_lamp', new THREE.BoxGeometry(.020, .003, .006), mats.ammo_lamp, [0, .1005, -.072]));
      g.add(plate(K, id + '_foregrip', rr(THREE, -.175, -.062, .026, .078, .007), .026, mats.polymer, { b: .004, seg: 3 }));
      for (let i = 0; i < 3; i++) g.add(part(`${id}_foregrip_groove${i}`, new THREE.BoxGeometry(.0272, .0014, .022), mats.gun_dark, [0, -.020 - i * .012, -.162]));
      g.add(bar(K, id + '_neck', rr(THREE, -.014, -.020, .028, .040, .005), .070, mats.gun_metal, { b: .002, pos: [0, .054, -.205] }));
      g.add(plate(K, id + '_jaw_fixed', rr(THREE, -.305, .070, .060, .046, .006), .030, mats.gun_pale, { b: .003, seg: 3 }));
      g.add(plate(K, id + '_jaw_moving', rr(THREE, -.305, .028, .050, .030, .005), .030, mats.gun_pale, { b: .003, seg: 3 }));
      g.add(part(id + '_jaw_face_a', new THREE.BoxGeometry(.031, .003, .050), mats.trim, [0, .0715, -.275]));
      g.add(part(id + '_jaw_face_b', new THREE.BoxGeometry(.031, .003, .050), mats.trim, [0, .0565, -.280]));
      g.add(lathe(K, id + '_worm', Array.from({ length: 13 }, (_, i) => [i % 2 ? .0095 : .011, -.015 + i * .0025]).concat([[0, .015], [0, -.015]]), mats.brass, [0, .045, -.235], { axis: 'x', seg: 24 }));
      for (const s of [-1, 1]) g.add(part(`${id}_jaw_glow${s}`, new THREE.BoxGeometry(.003, .010, .040), mats.energy_scan, [s * .0155, .064, -.278]));
      g.add(screws(K, id + '_screws', [[.0255, .070, -.080], [.0255, .040, -.150], [-.0255, .070, -.080], [-.0255, .040, -.150]], .003));
      return g;
    },
  },
  {
    id: 'hands', file: 'hands_firstperson.glb', label: 'First-person hands', swatch: '#3a3f4b', stats: { Base: 'faction-neutral', Variants: 'hands_<faction> (§3.5)' },
    poses: ['rifle', 'pistol', 'tool', ...OFFHAND_POSES],
    note: 'Base gloved forearms, posed per platform (weapon.handsPose). Fingers are capsule bone chains that actually wrap the grip — three fingers closed on the front strap, index on the trigger, thumb over the backstrap — with knuckle plates, wrist cuffs and rolled sleeves. Rifle: support hand C-clamps the handguard. Pistol: support hand cups the firing hand. Tool: right hand on the handle, left on the foregrip. Faction variants swap the plates.',
    build(K) {
      const { THREE, grp, D } = K, g = grp('hands'), pose = K.__handsPose || 'rifle';
      if (OFFHAND_POSES.includes(pose)) return buildOffhandPose(K, pose);
      if (pose === 'pistol') return buildHeroHands(K, 'pistol');
      const frame = (name, pos, rx) => { const f = grp(name, pos); f.rotation.x = rx; g.add(f); return f; };
      if (pose === 'tool') {
        frame('hands_frame_r', [0, .050, 0], -90 * D).add(wrapHand(K, 'r', { hw: .016, zf: -.017, zb: .017, levels: [-.092, -.072, -.052, -.032], thumbPts: [[.026, -.030, .012], [.008, -.030, .028], [-.012, -.034, .026], [-.020, -.052, .020], [-.018, -.070, .016]], wrist: [.030, -.115, .030], elbow: [.06, -.42, .05], up: [0, 0, 1] }));
        frame('hands_frame_l', [0, .012, -.162], 0).add(wrapHand(K, 'l', { hw: .013, zf: -.013, zb: .013, levels: [-.072, -.054, -.036, -.018], thumbPts: [[.024, -.014, .012], [.006, -.016, .024], [-.014, -.020, .022], [-.020, -.030, .004], [-.018, -.040, -.012]], wrist: [.028, -.062, .050], elbow: [.09, -.20, .30] }));
        return g;
      }
      frame('hands_frame_r', [0, 0, 0], -RAKE * D).add(wrapHand(K, 'r', { ...PISTOL_R, elbow: pose === 'rifle' ? [.04, -.18, .30] : PISTOL_R.elbow }));
      if (pose === 'pistol') frame('hands_frame_l', [0, 0, 0], -RAKE * D).add(wrapHand(K, 'l', { hw: .031, zf: -.028, zb: .040, levels: [-.078, -.060, -.042, -.024], thumbPts: [[.046, -.006, .030], [.036, -.008, .0], [.028, -.010, -.026], [.024, -.014, -.044]], wrist: [.046, -.070, .060], elbow: [.11, -.17, .32] }));
      // Support hand C-clamps the handguard: palm on the left, fingers under, thumb riding the rail.
      else frame('hands_frame_l', [0, .066, -.270], -90 * D).add(wrapHand(K, 'l', { hw: .019, zf: -.020, zb: .030, wrapLen: .034, levels: [0, .020, .040, .060], palmLen: .058, heel: false, thumbPts: [[.028, .028, .012], [.020, .050, .030], [.008, .072, .034], [-.004, .092, .034]], wrist: [.034, -.030, .0], elbow: [.08, -.30, -.16], up: [0, 0, 1] }));
      return g;
    },
  },
];

/* ── attachments (origin on the mount) ───────────────────────────────── */
const A = (id, slot, label, swatch, stats, note, build) => ({ id, slot, label, swatch, stats, note, file: `attach_${id}.glb`, build });
export const ATTACHMENTS = [
  A('longbarrel', 'barrel', 'Long barrel', '#c3ccd8', { Damage: '×1.10', Rate: '×0.92', Range: '×1.25', Recipe: 'Alloy 6' },
    'Replaces the stock barrel with a bore-matched extended tube, fitted per host: rifle — 42 cm fluted nitride, flutes and rifle-length gas block clear of the handguard (the rifle keeps its own barrel nut); scattergun — 46 cm smoothbore with a knurled receiver collar, mid-barrel band under the tube-mag clamp and a fibre bead at the crown; pistols — 11 cm threaded extension on a knurled collar. Threaded crown; carries `attach_mount_muzzle` at its end.', (K, host = 'rifle') => {
      const { grp, mats, THREE } = K, g = grp('attach_longbarrel');
      const P = {
        rifle:      { r: .0085, rc: .011, len: .42, flutes: [.19, .36], gasBlock: .32, clamp: false },
        scattergun: { r: .0105, rc: .013, len: .48, clamp: true, bead: true, band: .305 },
        sidearm:    { r: .0075, rc: .0075, len: .11, clamp: true, material: mats.gun_bronze },
        emberpistol:{ r: .007, rc: .007, len: .11, clamp: true, material: mats.gun_bronze },
      }[host] || { r: .0085, rc: .011, len: .42, flutes: [.19, .36], gasBlock: .32 };
      g.add(barrelGroup(K, 'attach_longbarrel', P.r, P.len, [0, 0, 0], { rc: P.rc, flutes: P.flutes, gasBlock: P.gasBlock, material: P.material }));
      // Knurled collar sits on the receiver face, one step larger than the barrel's own root collar so it reads as a clamp and never z-fights a host nut.
      if (P.clamp) { const R = P.rc + .004, Ri = R - .0015; g.add(lathe(K, 'attach_longbarrel_clamp', Array.from({ length: 13 }, (_, i) => [i % 2 ? Ri : R, .003 + i * .0012]).concat([[R, .020], [0, .020], [0, .003]]).reverse(), mats.gun_pale, [0, 0, 0])); }
      if (P.band) { g.add(lathe(K, 'attach_longbarrel_band', [[0, 0], [P.r + .0025, 0], [P.r + .0025, .012], [0, .012]], mats.gun_dark, [0, 0, -P.band + .006])); g.add(screws(K, 'attach_longbarrel_band_screws', [[P.r + .0025, 0, -P.band], [-(P.r + .0025), 0, -P.band]], .0024)); }
      if (P.bead) { g.add(bar(K, 'attach_longbarrel_bead_ramp', rr(THREE, -.003, 0, .006, .006, .001), .012, mats.gun_dark, { b: .0005, pos: [0, P.r, -(P.len - .012)] })); g.add(tube(K, 'attach_longbarrel_bead_fibre', .0013, .008, [0, P.r + .007, -(P.len - .012)], mats.ammo_lamp)); }
      g.add(mount(K, 'attach', 'muzzle', [0, 0, -P.len]));
      return g;
    }),
  A('shortbarrel', 'barrel', 'Short barrel', '#8d99ad', { Damage: '×0.95', Rate: '×1.18', Range: '×0.8', Recipe: 'Alloy 4' },
    'Chopped 16 cm barrel inside a ported turned shroud: reads as a snub the moment it is mounted. `attach_mount_muzzle` at the end.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_shortbarrel');
      g.add(barrelGroup(K, 'attach_shortbarrel', .0085, .16, [0, 0, 0], { rc: .011 }));
      g.add(lathe(K, 'attach_shortbarrel_shroud', [[.0125, .020], [.017, .024], [.017, .120], [.0125, .124], [.0125, .020]], mats.gun_dark, [0, 0, 0]));
      for (let i = 0; i < 4; i++) for (const a of [Math.PI / 4, Math.PI * .75, Math.PI * 1.25, Math.PI * 1.75]) g.add(part(`attach_shortbarrel_port${i}_${a.toFixed(2)}`, new THREE.BoxGeometry(.006, .0022, .012), mats.trim, [Math.cos(a) * .017, Math.sin(a) * .017, -.040 - i * .020], [0, 0, a]));
      g.add(mount(K, 'attach', 'muzzle', [0, 0, -.16]));
      return g;
    }),
  A('compensator', 'muzzle', 'Compensator', '#9aa6b7', { Damage: '×1.12', Recipe: 'Alloy 4 · Plating 2' },
    'Turned three-chamber muzzle device: upward gas ports, side slots, chamfered pale crown and a timing index.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_compensator');
      g.add(lathe(K, 'attach_compensator_body', [[0, 0], [.009, 0], [.013, .003], [.014, .012], [.012, .014], [.014, .016], [.014, .028], [.012, .030], [.014, .032], [.014, .046], [.013, .050], [0, .050]], mats.gun_dark, [0, 0, 0]));
      g.add(lathe(K, 'attach_compensator_crown', [[0, .050], [.013, .050], [.011, .054], [.006, .055], [0, .055]], mats.gun_pale, [0, 0, 0]));
      for (const d of [.008, .022, .038]) g.add(bar(K, `attach_compensator_port${d}`, rr(THREE, -.004, -.002, .008, .004, .001), .007, mats.trim, { b: .0004, pos: [0, .0132, -d] }));
      for (const s of [-1, 1]) for (const d of [.021, .039]) g.add(part(`attach_compensator_slot${s}_${d}`, new THREE.BoxGeometry(.002, .012, .003), mats.trim, [s * .0135, 0, -d]));
      g.add(part('attach_compensator_index', new THREE.BoxGeometry(.0016, .0016, .040), mats.hazard, [0, -.0138, -.025]));
      g.add(part('attach_compensator_bore', new THREE.CylinderGeometry(.005, .005, .002, 16), mats.trim, [0, 0, -.055], [Math.PI / 2, 0, 0]));
      return g;
    }),
  A('rangefinder', 'optic', 'Rangefinder', '#7fe65a', { Range: '×1.2', Recipe: 'Flux 3' },
    'QD-clamped optic: turned tube with an objective bell and rubber eyecup, knurled elevation and windage turrets, laser emitter under the tube, green readout window on the left.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_rangefinder');
      g.add(bar(K, 'attach_rangefinder_clamp', rr(THREE, -.016, 0, .032, .012, .002), .060, mats.gun_dark, { b: .001 }));
      g.add(bar(K, 'attach_rangefinder_qd', rr(THREE, -.002, -.003, .004, .006, .001), .030, mats.gun_pale, { b: .0005, pos: [.017, .006, .0] }));
      g.add(bar(K, 'attach_rangefinder_riser', rr(THREE, -.006, 0, .012, .012, .002), .050, mats.gun_metal, { b: .001, pos: [0, .012, -.005] }));
      g.add(lathe(K, 'attach_rangefinder_tube', [[0, -.050], [.011, -.050], [.013, -.046], [.013, -.030], [.011, -.030], [.011, .020], [.013, .022], [.015, .050], [.015, .064], [.012, .066], [0, .066]], mats.gun_metal, [0, .034, -.005]));
      g.add(part('attach_rangefinder_oc_glass', new THREE.CylinderGeometry(.009, .009, .001, 24), mats.optic_glass, [0, .034, .0445], [Math.PI / 2, 0, 0]));
      g.add(part('attach_rangefinder_obj_glass', new THREE.CylinderGeometry(.012, .012, .001, 24), mats.optic_glass, [0, .034, -.070], [Math.PI / 2, 0, 0]));
      g.add(part('attach_rangefinder_eyecup', new THREE.TorusGeometry(.012, .0025, 10, 32), mats.grip_rubber, [0, .034, .045]));
      const knurl = Array.from({ length: 13 }, (_, i) => [i % 2 ? .0072 : .008, .002 + i * .0007]).concat([[.007, .012], [0, .012], [0, 0], [.007, 0]]);
      g.add(lathe(K, 'attach_rangefinder_elev', knurl, mats.chrome, [0, .047, -.005], { axis: 'y', seg: 24 }));
      g.add(lathe(K, 'attach_rangefinder_windage', knurl, mats.chrome, [.011, .034, -.005], { axis: 'x', seg: 24 }));
      g.add(part('attach_rangefinder_readout', new THREE.BoxGeometry(.0015, .008, .020), mats.energy_scan, [-.0115, .034, -.012]));
      g.add(lathe(K, 'attach_rangefinder_laser', [[0, 0], [.004, 0], [.004, .038], [.003, .040], [0, .040]], mats.gun_dark, [0, .018, -.020], { seg: 20 }));
      g.add(part('attach_rangefinder_laser_lens', new THREE.CylinderGeometry(.0025, .0025, .001, 16), mats.energy_scan, [0, .018, -.0605], [Math.PI / 2, 0, 0]));
      g.add(screws(K, 'attach_rangefinder_screws', [[-.0165, .006, -.020], [-.0165, .006, .020]], .0025));
      return g;
    }),
  A('drumfeed', 'magazine', 'Drum feed', '#b08a3e', { Rate: '×1.15', Recipe: 'Alloy 5 · Flux 2' },
    'Replaces the magazine: turned polymer drum with spoked faces, spring-tension dial, feed tower and a viewing window showing the coiled ammunition. Fitted per host: the tower matches the well it fills (rifle mag well, scattergun loading port, pistol grip base), the drum is 45 mm on long guns and 36 mm under a pistol grip, and the window shows shell hulls on the scattergun.', (K, host = 'rifle') => {
      const { grp, part, THREE, mats } = K, g = grp('attach_drumfeed');
      const P = {
        rifle:      { w: .024, d: .052, R: .045, z: .010 },
        scattergun: { w: .020, d: .050, R: .042, z: .010, shells: true },
        sidearm:    { w: .022, d: .036, R: .036, z: .015 },
        emberpistol:{ w: .022, d: .036, R: .036, z: .015 },
      }[host] || { w: .024, d: .052, R: .045, z: .010 };
      const tw = P.w / 2, R = P.R, cy = -(.040 + R - .010), cz = P.z, hw = .021;
      g.add(bar(K, 'attach_drumfeed_tower', rr(THREE, -tw, -.040, P.w, .040, .003), P.d, mats.polymer, { b: .002 }));
      g.add(lathe(K, 'attach_drumfeed_drum', [[0, -hw], [R - .005, -hw], [R, -hw + .004], [R, hw - .004], [R - .005, hw], [0, hw]], mats.polymer, [0, cy, cz], { axis: 'x', seg: 48 }));
      for (const s of [-1, 1]) { g.add(part(`attach_drumfeed_face${s}`, new THREE.CylinderGeometry(R - .009, R - .009, .002, 48), mats.gun_dark, [s * (hw + .0005), cy, cz], [0, 0, Math.PI / 2])); for (let i = 0; i < 6; i++) g.add(part(`attach_drumfeed_spoke${s}_${i}`, new THREE.BoxGeometry(.0025, .004, (R - .009) * 1.45), mats.gun_metal, [s * (hw + .0015), cy, cz], [i * Math.PI / 6, 0, 0])); }
      // Window on the right face, above centre, looking at the coil end-on.
      const wy = cy + R * .33, wz = cz - .018, wh = P.shells ? .020 : .014;
      g.add(part('attach_drumfeed_window', new THREE.BoxGeometry(.0025, wh, .022), mats.glass_dark, [hw + .0025, wy, wz]));
      if (P.shells) for (let i = 0; i < 2; i++) { const p = [hw + .0035, wy + .005 - i * .010, wz + i * .004]; g.add(part(`attach_drumfeed_shell_base${i}`, new THREE.CylinderGeometry(.0058, .0058, .001, 16), mats.brass, p, [0, 0, Math.PI / 2])); g.add(part(`attach_drumfeed_shell_hull${i}`, new THREE.CylinderGeometry(.0045, .0045, .0012, 16), mats.shell_hull, [p[0] + .0004, p[1], p[2]], [0, 0, Math.PI / 2])); }
      else for (let i = 0; i < 3; i++) g.add(part(`attach_drumfeed_round${i}`, new THREE.CylinderGeometry(.0028, .0028, .001, 12), mats.brass, [hw + .0035, wy + .004 - i * .006, wz + i * .003], [0, 0, Math.PI / 2]));
      g.add(lathe(K, 'attach_drumfeed_dial', Array.from({ length: 9 }, (_, i) => [i % 2 ? .009 : .010, .001 + i * .0006]).concat([[.008, .007], [0, .007], [0, 0], [.008, 0]]), mats.chrome, [-(hw + .0075), cy, cz], { axis: 'x', seg: 24 }));
      g.add(part('attach_drumfeed_dial_arm', new THREE.BoxGeometry(.002, .0015, .014), mats.gun_dark, [-(hw + .012), cy, cz]));
      g.add(screws(K, 'attach_drumfeed_screws', [[tw + .0005, -.020, -P.d * .36], [tw + .0005, -.020, P.d * .36]], .0025));
      return g;
    }),
  A('bracestock', 'stock', 'Brace stock', '#8d99ad', { Rate: '×1.08', Range: '×1.05', Recipe: 'Alloy 3' },
    'Skeleton brace: notched tube with a locking collar, twin struts with lightening cuts, rubber-faced pad, sling loop. Replaces any factory stock; fits pistols too.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_bracestock');
      g.add(lathe(K, 'attach_bracestock_tube', [[0, 0], [.011, 0], [.011, -.140], [.009, -.142], [0, -.142]], mats.gun_metal, [0, 0, 0]));
      for (let i = 0; i < 5; i++) g.add(part(`attach_bracestock_notch${i}`, new THREE.TorusGeometry(.0112, .0008, 6, 28), mats.trim, [0, 0, .030 + i * .020]));
      g.add(bar(K, 'attach_bracestock_lock', rr(THREE, -.015, -.012, .030, .012, .003), .020, mats.gun_dark, { b: .001, pos: [0, -.008, .040] }));
      g.add(bar(K, 'attach_bracestock_lever', rr(THREE, -.003, -.002, .006, .004, .001), .030, mats.chrome, { b: .0004, pos: [0, -.022, .045] }));
      const strut = shape(THREE, [[.100, -.002], [.185, -.002], [.185, -.060], [.170, -.064], [.100, -.020]]); strut.holes.push(rr(THREE, .12, -.034, .045, .014, .006));
      for (const s of [-1, 1]) g.add(plate(K, `attach_bracestock_strut${s}`, strut, .005, mats.gun_metal, { b: .001, pos: [s * .012, 0, 0] }));
      g.add(plate(K, 'attach_bracestock_pad', rr(THREE, .176, -.064, .012, .080, .004), .030, mats.gun_metal, { b: .002 }));
      g.add(plate(K, 'attach_bracestock_rubber', rr(THREE, .186, -.065, .007, .082, .003), .032, mats.grip_rubber, { b: .0015 }));
      g.add(part('attach_bracestock_loop', new THREE.TorusGeometry(.006, .0015, 8, 20), mats.chrome, [0, -.068, .170], [0, Math.PI / 2, 0]));
      return g;
    }),
  A('stabilizer', 'underbarrel', 'Stabilizer', '#9b5be8', { Damage: '×1.18', Rate: '×0.95', Recipe: 'Gravium 2' },
    'Gravium counter-mass under the barrel: a dense bevelled block in a rail cradle with two gyro rings, a violet field slit, and a grooved foregrip. Heavy, and it should look heavy.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_stabilizer');
      g.add(bar(K, 'attach_stabilizer_cradle', rr(THREE, -.015, -.008, .030, .008, .002), .060, mats.gun_dark, { b: .001 }));
      g.add(screws(K, 'attach_stabilizer_screws', [[.0155, -.004, -.020], [.0155, -.004, .020]], .0025));
      g.add(bar(K, 'attach_stabilizer_mass', rr(THREE, -.013, -.038, .026, .030, .006), .070, mats.gun_dark, { b: .003, seg: 3 }));
      g.add(part('attach_stabilizer_slit', new THREE.BoxGeometry(.028, .003, .050), mats.infuse_gravium, [0, -.023, 0]));
      for (const z of [-.028, .028]) { g.add(part(`attach_stabilizer_gyro${z}`, new THREE.TorusGeometry(.015, .003, 12, 40), mats.chrome, [0, -.023, z])); g.add(part(`attach_stabilizer_core${z}`, new THREE.SphereGeometry(.005, 16, 12), mats.infuse_gravium, [0, -.023, z])); }
      g.add(plate(K, 'attach_stabilizer_foregrip', rr(THREE, .006, -.100, .026, .064, .007), .022, mats.polymer, { b: .004, seg: 3 }));
      for (let i = 0; i < 3; i++) g.add(part(`attach_stabilizer_groove${i}`, new THREE.BoxGeometry(.0232, .0014, .020), mats.gun_dark, [0, -.058 - i * .012, .019]));
      return g;
    }),
  A('embercoil', 'infusion', 'Ember coil', '#e8622b', { Applies: 'burn', Recipe: 'Flux 4' },
    'Infusion cell: a brass-wound coil around a glowing ember core, hex heat fins on the outboard face and a hose stub into the receiver.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_embercoil');
      g.add(bar(K, 'attach_embercoil_plate', rr(THREE, -.006, -.015, .006, .030, .002), .050, mats.gun_dark, { b: .001 }));
      g.add(tube(K, 'attach_embercoil_core', .008, .040, [-.014, 0, 0], mats.infuse_ember));
      for (let i = 0; i < 7; i++) g.add(part(`attach_embercoil_wind${i}`, new THREE.TorusGeometry(.0095, .0014, 8, 28), mats.brass, [-.014, 0, -.018 + i * .006]));
      const fin = poly(THREE, 0, 0, .017, 6, Math.PI / 6); fin.holes.push(circ(THREE, 0, 0, .0085));
      for (let i = 0; i < 4; i++) g.add(bar(K, `attach_embercoil_fin${i}`, fin, .0014, mats.gun_pale, { b: .0003, pos: [-.014, 0, -.020 + i * .013], rot: [0, 0, 0] }));
      g.add(part('attach_embercoil_hose', new THREE.CylinderGeometry(.003, .003, .012, 12), mats.grip_rubber, [-.008, -.010, .020], [0, 0, Math.PI / 2]));
      g.add(screws(K, 'attach_embercoil_screws', [[-.0065, .012, -.020], [-.0065, -.012, .020]], .0025));
      return g;
    }),
  A('cryocell', 'infusion', 'Cryo cell', '#4fc0e8', { Applies: 'chill', Recipe: 'Flux 3 · Alloy 2' },
    'Infusion cell: a turned pressure canister with a frost-blue sight window, bleed valve wheel and rime rings, clamped to the receiver.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_cryocell');
      g.add(bar(K, 'attach_cryocell_plate', rr(THREE, -.006, -.015, .006, .030, .002), .050, mats.gun_dark, { b: .001 }));
      g.add(lathe(K, 'attach_cryocell_can', [[0, -.024], [.009, -.024], [.011, -.021], [.011, .021], [.009, .024], [0, .024]], mats.gun_pale, [-.016, 0, 0]));
      g.add(part('attach_cryocell_window', new THREE.BoxGeometry(.003, .008, .028), mats.infuse_cryo, [-.027, 0, 0]));
      for (const z of [-.016, .016]) g.add(part(`attach_cryocell_rime${z}`, new THREE.TorusGeometry(.0115, .002, 8, 28), mats.frost, [-.016, 0, z]));
      g.add(lathe(K, 'attach_cryocell_valve', [[0, 0], [.004, 0], [.004, .008], [.002, .010], [0, .010]], mats.brass, [-.016, .010, -.020], { axis: 'y', seg: 16 }));
      g.add(part('attach_cryocell_valve_wheel', new THREE.TorusGeometry(.005, .0012, 8, 20), mats.chrome, [-.016, .021, -.020], [Math.PI / 2, 0, 0]));
      g.add(screws(K, 'attach_cryocell_screws', [[-.0065, .012, -.020], [-.0065, -.012, .020]], .0025));
      return g;
    }),
  A('voltcap', 'infusion', 'Volt cap', '#f05ae6', { Applies: 'shock', Recipe: 'Flux 5' },
    'Infusion cell: a bevelled capacitor block with two turned electrodes on ceramic insulators and a visible jagged arc glowing magenta, charge stripe along the side.', (K) => {
      const { grp, part, THREE, mats } = K, g = grp('attach_voltcap');
      g.add(bar(K, 'attach_voltcap_plate', rr(THREE, -.006, -.015, .006, .030, .002), .050, mats.gun_dark, { b: .001 }));
      g.add(bar(K, 'attach_voltcap_body', rr(THREE, -.009, -.013, .018, .026, .003), .044, mats.gun_metal, { b: .002, pos: [-.015, 0, 0] }));
      g.add(part('attach_voltcap_stripe', new THREE.BoxGeometry(.0015, .004, .036), mats.infuse_volt, [-.0245, -.006, 0]));
      for (const z of [-.012, .012]) { g.add(lathe(K, `attach_voltcap_insulator${z}`, [[0, 0], [.007, 0], [.006, .004], [.0055, .006], [0, .006]], mats.ceramic, [-.015, .013, z], { axis: 'y', seg: 16 })); g.add(lathe(K, `attach_voltcap_electrode${z}`, [[0, 0], [.003, 0], [.0025, .008], [.0015, .012], [0, .012]], mats.chrome, [-.015, .019, z], { axis: 'y', seg: 16 })); }
      g.add(chain(K, 'attach_voltcap_arc', [[-.015, .030, -.012], [-.013, .034, -.006], [-.017, .033, .0], [-.014, .036, .006], [-.015, .030, .012]], .0007, mats.infuse_volt));
      g.add(screws(K, 'attach_voltcap_screws', [[-.0065, .012, -.020], [-.0065, -.012, .020]], .0025));
      return g;
    }),
];

/** Soft-edged streak ribbon along −Z with colour + alpha baked into vertex RGBA.
 *  Sampled with t = 0 at the head → 1 at the tail; edges of the ribbon are alpha 0,
 *  the centre line carries a(t), so it reads as a soft streak with no texture and
 *  no additive blending. `zHead` places the head (defaults to −len). */
function ribbon(K, name, material, { len = 1, segs = 48, w, a, c, roll = 0, zHead } = {}) {
  const { THREE } = K, pos = [], col = [], idx = [];
  for (let i = 0; i <= segs; i++) {
    const t = i / segs, z = -len * (1 - t), hw = w(t), al = a(t), [r, g, b] = c(t);
    pos.push(-hw, 0, z, 0, 0, z, hw, 0, z);
    col.push(r, g, b, 0, r, g, b, al, r, g, b, 0);
  }
  for (let i = 0; i < segs; i++) {
    const o = i * 3;
    idx.push(o, o + 1, o + 4, o, o + 4, o + 3, o + 1, o + 2, o + 5, o + 1, o + 5, o + 4);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 4));
  geo.setIndex(idx); geo.computeVertexNormals();
  const m = new THREE.Mesh(geo, material);
  m.name = name; m.rotation.z = roll; m.position.z = (zHead ?? -len) + len;
  return m;
}

/* ── ammo + tracers ──────────────────────────────────────────────────── */
const AMMO_DEF = {
  standard: { label: 'Standard', swatch: '#f4dca4', tip: 'chrome', band: null, tracer: 'tracer_standard', stats: { Damage: '×1.0', Recipe: '—' }, note: 'Brass case, plain jacketed tip; pale-gold tracer.' },
  ap: { label: 'Armour-piercing', swatch: '#cfd7e4', tip: 'trim', band: 'chrome', tracer: 'tracer_ap', stats: { Damage: '×0.85', Armor: 'ignores flat', Recipe: 'Plating 4' }, note: 'Black penetrator tip with a steel band; thin white-hot tracer.' },
  hollowpoint: { label: 'Hollow point', swatch: '#e8862b', tip: 'gun_pale', band: 'hazard', tracer: 'tracer_hp', stats: { Damage: '×1.0', Unarmored: '×1.3', Recipe: 'Alloy 5' }, note: 'Dimpled cavity tip and amber band; wide orange tracer that reads as a mushrooming hit.' },
  incendiary: { label: 'Incendiary', swatch: '#e8622b', tip: 'infuse_ember', band: 'infuse_ember', tracer: 'infuse_ember', stats: { Damage: '×0.9', Applies: 'burn', Recipe: 'Flux 3 · Alloy 2' }, note: 'Ember-glass tip; tracer trails flame petals.' },
  cryo: { label: 'Cryo rounds', swatch: '#4fc0e8', tip: 'infuse_cryo', band: 'infuse_cryo', tracer: 'infuse_cryo', stats: { Damage: '×0.9', Applies: 'chill', Recipe: 'Flux 4' }, note: 'Frost-blue tip; tracer sheds ice crystals.' },
};
export const AMMO = Object.entries(AMMO_DEF).map(([id, d]) => ({
  id: 'ammo_' + id, file: `ammo_${id}.glb`, label: d.label, swatch: d.swatch, stats: d.stats, note: d.note + ' Turned bottleneck rounds (rim, extractor groove, shoulder, neck) in a bevelled magazine with the top round exposed, plus a loose round for the armory grid.',
  build(K) {
    const { grp, part, THREE, mats } = K, g = grp('ammo_' + id);
    const round = (name, pos, rot) => {
      const r = grp(name, pos); r.rotation.set(...rot);
      r.add(lathe(K, name + '_case', [[0, 0], [.0058, 0], [.0058, .002], [.0046, .0026], [.0046, .0042], [.0056, .0052], [.0056, .028], [.0048, .0325], [.0042, .034], [.0042, .0385], [0, .0385]], mats.brass, [0, 0, 0], { axis: 'y', seg: 24 }));
      if (d.band) r.add(part(name + '_band', new THREE.TorusGeometry(.0043, .0008, 8, 20), mats[d.band], [0, .037, 0], [Math.PI / 2, 0, 0]));
      const tip = id === 'hollowpoint' ? [[0, .009], [.0022, .009], [.0022, .012], [.0030, .011], [.0040, .006], [.0042, 0], [0, 0]] : [[0, 0], [.0042, 0], [.0038, .004], [.0025, .010], [.0008, .014], [0, .0145]];
      r.add(lathe(K, name + '_tip', tip, mats[d.tip], [0, .038, 0], { axis: 'y', seg: 24 }));
      if (id === 'ap') r.add(lathe(K, name + '_penetrator', [[0, 0], [.0012, 0], [.0005, .004], [0, .0045]], mats.chrome, [0, .052, 0], { axis: 'y', seg: 12 }));
      return r;
    };
    const W = .03, H = .09, Dp = .05, n = 'ammo_' + id;
    g.add(bar(K, n + '_mag', rr(THREE, -W / 2, .006, W, H, .003), Dp, mats.polymer, { b: .002 }));
    for (let i = 0; i < 4; i++) g.add(bar(K, `${n}_rib${i}`, rr(THREE, -W / 2 - .002, .018 + i * .02, W + .004, .005, .002), Dp + .004, mats.gun_dark, { b: .0008 }));
    g.add(bar(K, n + '_base', rr(THREE, -W / 2 - .004, 0, W + .008, .006, .002), Dp + .008, mats.gun_metal, { b: .001 }));
    g.add(part(n + '_base_tab', new THREE.BoxGeometry(.012, .004, .008), mats.gun_pale, [0, .003, -Dp / 2 - .006]));
    g.add(bar(K, n + '_lips', rr(THREE, -W / 2, H + .006, W, .008, .002), Dp, mats.gun_metal, { b: .001 }));
    g.add(part(n + '_lip_cut', new THREE.BoxGeometry(W - .012, .009, Dp - .012), mats.trim, [0, H + .010, 0]));
    g.add(part(n + '_follower', new THREE.BoxGeometry(W - .014, .003, Dp - .014), mats[d.tracer] || mats.brass, [0, H + .004, 0]));
    for (let i = 0; i < 5; i++) {
      g.add(part(`${n}_witness${i}`, new THREE.CylinderGeometry(.0035, .0035, .004, 16), mats.trim, [W / 2, .02 + i * .015, .004], [0, 0, Math.PI / 2]));
      g.add(part(`${n}_witness_brass${i}`, new THREE.CylinderGeometry(.0028, .0028, .002, 16), mats.brass, [W / 2 + .0005, .02 + i * .015, .004], [0, 0, Math.PI / 2]));
    }
    g.add(part(n + '_label', new THREE.BoxGeometry(.001, .026, .02), mats[d.tracer] || mats.brass, [-W / 2 - .0005, .05, .006]));
    g.add(part(n + '_label_stripe', new THREE.BoxGeometry(.0012, .004, .02), mats.gun_dark, [-W / 2 - .0006, .058, .006]));
    g.add(round(n + '_top', [0, H + .012, Dp / 2 - .012], [-Math.PI / 2, 0, 0]));
    g.add(round(n + '_second', [0, H + .003, Dp / 2 - .014], [-Math.PI / 2, 0, 0]));
    // Loose round for the armory grid. The Z term was +1.35, which aimed it −X:
    // its 52 mm length ran back through the magazine and surfaced at the base as
    // a round that looked like a modelling mistake. Now it points away, resting
    // on the ground plane clear of the ribs.
    g.add(round(n + '_loose', [W / 2 + .014, .0058, .014], [-Math.PI / 2, 0, -1.35]));
    return g;
  },
}));
/* Per-round tracer character. Colour grade + falloff are baked into vertex RGBA by
   `ribbon`, so every one of these is unlit, alpha-blended geometry that reads the same
   in-engine as in any GLTF viewer. `fall` is the exponential decay of the streak
   (higher = shorter, hotter burn); `w0`/`wk` are head and tail half-widths. */
const TRACER_VFX = {
  standard: {
    hot: [1, .97, .88], mid: [1, .70, .28], tail: [.70, .24, .06],
    fall: 3.4, w0: .0045, wk: .011, head: .0075, headMat: 'tracer_hot',
    smoke: { len: .62, w0: .006, wk: .026, a: .10, c: [.48, .45, .42] },
    note: 'Crossed soft-edged ribbons graded white-hot \u2192 amber \u2192 ember, a hot filament behind the head, and a thin smoke wisp at the tail.',
  },
  ap: {
    hot: [1, 1, 1], mid: [.72, .82, 1], tail: [.34, .42, .78],
    fall: 5.4, w0: .0026, wk: .0034, head: .0050, headMat: 'tracer_hot_ap',
    smoke: null, profile: 'hard', filament: false, lance: true, rails: true,
    note: 'Not a plume \u2014 a bolt. Near-constant width, a hard-cut tail that ends at ~85% instead of fading out, a forward lance through the head, and two hairline rails flanking the core. Cold steel-white into violet; no smoke, since the penetrator does not burn.',
  },
  hollowpoint: {
    hot: [1, .93, .72], mid: [1, .54, .15], tail: [.52, .15, .04],
    fall: 2.4, w0: .0095, wk: .030, head: .0105, headMat: 'tracer_hot_hp',
    smoke: { len: .74, w0: .010, wk: .042, a: .14, c: [.44, .38, .34] },
    note: 'Widest and slowest-dying of the set \u2014 a fat orange streak that keeps blooming behind the round, so the trail itself telegraphs the mushrooming hit.',
  },
  incendiary: {
    hot: [1, .92, .70], mid: [.98, .40, .09], tail: [.40, .09, .02],
    fall: 2.1, w0: .0065, wk: .020, head: .0090, headMat: 'tracer_hot_inc',
    smoke: { len: .80, w0: .012, wk: .048, a: .17, c: [.34, .28, .26] },
    licks: 5,
    note: 'Ember-graded streak shedding flame licks that peel off-axis and fall behind, over the heaviest smoke plume of the set.',
  },
  cryo: {
    hot: [.94, .99, 1], mid: [.42, .76, .95], tail: [.14, .36, .60],
    fall: 3.0, w0: .0042, wk: .013, head: .0080, headMat: 'tracer_hot_cryo',
    smoke: { len: .70, w0: .008, wk: .036, a: .13, c: [.62, .76, .84] },
    shards: 6,
    note: 'Cold blue streak trailing vapour rather than smoke, shedding ice shards that tumble off the flight line.',
  },
};
export const TRACERS = Object.entries(AMMO_DEF).map(([id, d]) => {
  const V = TRACER_VFX[id];
  return {
    id: 'vfx_tracer_' + id, file: `vfx_tracer_${id}.glb`, label: d.label + ' tracer', swatch: d.swatch,
    stats: { Length: '1 m unit', Forward: '\u2212Z' },
    note: 'Unit-length streak the client stretches to the hit (head at z = \u22121). ' + V.note
      + ' Unlit, alpha-blended, colour and falloff in vertex RGBA \u2014 no material overrides on import.',
    build(K) {
      const { grp, part, THREE, mats } = K, g = grp('vfx_tracer_' + id), n = g.name;
      const mix = (p, q, t) => p.map((v, i) => v + (q[i] - v) * t);
      const grade = (t) => (t < .35 ? mix(V.hot, V.mid, t / .35) : mix(V.mid, V.tail, (t - .35) / .65));
      /* Soft rounds fade exponentially into nothing. AP holds near-full brightness then
         cuts \u2014 a bolt of finite length, which is what separates it from the plumes. */
      const alpha = V.profile === 'hard'
        ? (t) => (t < .60 ? .92 - .18 * (t / .60) : .74 * Math.pow(Math.max(0, 1 - (t - .60) / .26), 1.6))
        : (t) => Math.exp(-V.fall * t) * .95;
      const width = (t) => V.w0 + (V.wk - V.w0) * Math.pow(t, .7);
      // main streak: two ribbons crossed so the light reads from any viewing angle
      for (const [k, roll] of [['a', 0], ['b', Math.PI / 2]])
        g.add(ribbon(K, `${n}_streak_${k}`, mats.tracer_streak, { w: width, a: alpha, c: grade, roll }));
      // Incandescent head — the one solid element, what the eye tracks. AP skips it:
      // its lance already terminates in a bright point, and a sphere would round off
      // the pointed silhouette that separates it from Standard.
      if (!V.lance) g.add(part(n + '_head', new THREE.SphereGeometry(V.head, 14, 10), mats[V.headMat], [0, 0, -1]));
      // hot filament right behind the head, crossed off the main streak's axis
      if (V.filament !== false) for (const [k, roll] of [['a', Math.PI / 4], ['b', -Math.PI / 4]])
        g.add(ribbon(K, `${n}_core_${k}`, mats.tracer_streak,
          { len: V.head * 45, segs: 20, zHead: -1, w: () => V.w0 * .55, a: (t) => Math.exp(-4.5 * t), c: () => V.hot, roll }));
      // AP lance: an elongated dart tapering to a point exactly at the head, so the
      // silhouette reads pointed rather than round. Spans z −.875 → −1; never overshoots.
      if (V.lance) {
        g.add(lathe(K, n + '_lance', [[0, 0], [.0030, .025], [.0052, .070], [.0016, .113], [0, .125]], mats.tracer_hot_ap, [0, 0, -.875], { seg: 12 }));
      }
      // AP rails: hairline guides flanking the bolt — engineered, not organic
      if (V.rails) for (const [k, x] of [['l', -.0085], ['r', .0085]]) {
        const rail = ribbon(K, `${n}_rail_${k}`, mats.tracer_streak, {
          len: .52, segs: 20, zHead: -1, w: () => .0011, roll: Math.PI / 2,
          a: (t) => .5 * Math.pow(Math.max(0, 1 - t), .8), c: (t) => mix(V.mid, V.tail, t),
        });
        rail.position.x = x;
        g.add(rail);
      }
      if (V.smoke) {
        const S = V.smoke;
        g.add(ribbon(K, n + '_smoke', mats.tracer_smoke_v,
          { len: S.len, segs: 28, w: (t) => S.w0 + (S.wk - S.w0) * t, a: (t) => S.a * Math.sin(t * Math.PI), c: () => S.c, roll: Math.PI / 3 }));
      }
      // flame licks: short ribbons peeling off the flight line and trailing back
      for (let i = 0; i < (V.licks || 0); i++) {
        const t = .12 + i * .15, a = i * 2.4;
        const f = ribbon(K, `${n}_lick${i}`, mats.tracer_streak,
          { len: .10 + i * .035, segs: 12, w: (u) => .004 + .014 * u, a: (u) => .55 * Math.exp(-2.2 * u) * (1 - t * .7), c: (u) => mix(V.mid, V.tail, u) });
        f.position.set(Math.cos(a) * .012, Math.sin(a) * .012, -1 + t);
        f.rotation.set(Math.sin(a) * .5, Math.cos(a) * .5, a);
        g.add(f);
      }
      // ice shards: small tumbling solids, shrinking and dimming down the trail
      for (let i = 0; i < (V.shards || 0); i++) {
        const t = .10 + i * .14, a = i * 1.9;
        const c = part(`${n}_shard${i}`, new THREE.OctahedronGeometry(.009 - i * .0009, 0), mats.tracer_ice,
          [Math.cos(a) * (.006 + t * .028), Math.sin(a) * (.006 + t * .028), -1 + t], [a, a * .7, a * 1.3]);
        c.scale.set(1, 1, 2.4);
        g.add(c);
      }
      return g;
    },
  };
});
