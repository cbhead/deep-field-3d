/**
 * Deep Field 3D — the two stream weapons, hero standard.
 *
 *   poisonstream   The toxin applier (Weapons.cs: 2.8 dmg · 6/s · 18 m · 40 doses).
 *                  Reference machine: a pressure-fed chemical applicator built on
 *                  paintball-marker architecture — a milled body with the working
 *                  parts inside, a top-fed reservoir on a feed neck, a rear gas
 *                  bottle that doubles as the stock, a regulator and braided line
 *                  from the bottle to the ASA under the grip, a fluted and ported
 *                  barrel with a fan tip. 300 mm barrel, 620 mm overall.
 *
 *   cryosprayer    The chill applier (2.2 · 7/s · 14 m · 45 doses). Reference
 *                  machine: a CO₂ extinguisher crossed with a cryogen transfer
 *                  line — a horizontal insulated dewar under the lance tube, a
 *                  vacuum-jacketed braided feed into a solenoid valve block, a
 *                  slotted discharge horn where the frost lives. 230 mm lance,
 *                  horn 72 mm at the mouth, 700 mm overall with the tube stock.
 *
 * Neither ejects anything; each has a valve armature that kicks on discharge
 * (`<id>_slide_assembly` with userData.recoil). Each carries its vessel as
 * `<id>_magazine` with a `_mount_magwell` at the seat, so reload code finds the
 * same contract as the rest of the armoury (ASSET-DELIVERY Ask D).
 *
 * Metres, Y-up, muzzle toward −Z, origin at the grip web.
 */
import { heroSurfaces, loft, sweep, revolve, engrave, repivot, clamp, smooth, lerp } from './gun-kit.js';

const span = (a, b, step) => { const o = []; for (let v = a; v <= b + 1e-9; v += step) o.push(v); return o; };
const sup = (ang, a, b, n) => 1 / Math.pow(Math.pow(Math.abs(Math.cos(ang) / a), n) + Math.pow(Math.abs(Math.sin(ang) / b), n), 1 / n);
/** Rounded-rectangle station, CCW, corners as quarter arcs. */
function rrect(hw, yb, yt, rad, n = 5) {
  const pts = [], r = Math.min(rad, hw, (yt - yb) / 2);
  const corner = (cx, cy, a0) => { for (let i = 0; i <= n; i++) { const a = a0 + i / n * Math.PI / 2; pts.push([cx + Math.cos(a) * r, cy + Math.sin(a) * r]); } };
  corner(hw - r, yt - r, 0); corner(-hw + r, yt - r, Math.PI / 2); corner(-hw + r, yb + r, Math.PI); corner(hw - r, yb + r, Math.PI * 1.5);
  return pts;
}

function sprayerSurfaces(THREE, mats) {
  heroSurfaces(THREE, mats);
  const std = (name, o) => { if (mats[name]) return mats[name]; const m = new THREE.MeshStandardMaterial({ vertexColors: true, ...o }); m.name = name; mats[name] = m; return m; };
  const glass = (name, base, em, i) => std(name, { color: base, metalness: 0, roughness: .08, emissive: new THREE.Color(em), emissiveIntensity: i, transparent: true, opacity: .55, envMapIntensity: .6 });
  glass('hero_toxin_glass', 0x0c2a0c, 0x7fe65a, .35);
  glass('hero_cryo_glass', 0x0b2a3a, 0x4fc0e8, .35);
  std('hero_toxin', { color: 0x1f4a12, metalness: 0, roughness: .35, emissive: new THREE.Color(0x7fe65a), emissiveIntensity: 1.4, envMapIntensity: .2 });
  std('hero_cryo', { color: 0x12384a, metalness: 0, roughness: .35, emissive: new THREE.Color(0x4fc0e8), emissiveIntensity: 1.3, envMapIntensity: .2 });
  /* Rime: frost is a dielectric crust, dead matte, very low env response (CLAUDE.md §5). */
  std('hero_rime', { color: 0xd8e6ee, metalness: 0, roughness: .95, envMapIntensity: .05 });
  return mats;
}

/* ══ POISON STREAM ═══════════════════════════════════════════════════════ */
const PS = { bore: .0560, hw: .0150, z0: -.1000, z1: .1200, yt: .0760, yb: .0330, bbl: .3000, neckZ: -.0200, canR: .0300, canY0: .0960, canY1: .2000, tankR: .0380, tankZ0: .1240, tankZ1: .3600 };

function psBodyStation(z) {
  const nose = smooth(PS.z0, PS.z0 + .010, z), tail = 1 - smooth(PS.z1 - .014, PS.z1, z) * .22;
  const hw = (PS.hw - (1 - nose) * .0026) * tail, yt = PS.yt - (1 - nose) * .004, yb = PS.yb + (1 - nose) * .003;
  /* Two lightening pockets per flank and a milled feed-neck boss on top — all displacement. */
  const pk = Math.max(smooth(-.084, -.076, z) * (1 - smooth(-.046, -.038, z)), smooth(.010, .018, z) * (1 - smooth(.070, .078, z)));
  const boss = smooth(PS.neckZ - .024, PS.neckZ - .016, z) * (1 - smooth(PS.neckZ + .016, PS.neckZ + .024, z));
  const pts = rrect(hw, yb, yt, .0045, 4).map(([x, y]) => {
    const v = (y - yb) / (yt - yb);
    const cut = pk * .0024 * smooth(.20, .34, v) * (1 - smooth(.62, .76, v)) * smooth(hw - .002, hw, Math.abs(x));
    const top = boss * .0030 * smooth(.85, 1, v) * (1 - smooth(.55, .85, Math.abs(x) / hw));
    return [x - Math.sign(x) * cut, y + top];
  });
  return { z, pts };
}
/** Barrel: 12 flutes cut into the tube, deepening to through-ports over the front third. */
function psBarrelStation(z) {
  const u = (PS.z0 - z) / PS.bbl, R = .0118 - .0014 * smooth(.05, .30, u);
  const root = 1 - smooth(0, .06, u);                          // plain at the breech
  const port = smooth(.55, .62, u) * (1 - smooth(.90, .95, u));
  const N = 60;
  return { z, pts: Array.from({ length: N }, (_, i) => {
    const ang = i / N * Math.PI * 2, f = Math.pow(Math.max(0, Math.cos(ang * 6)), 3);
    const depth = (1 - root) * (.0012 + .0022 * port * (Math.sin(u * Math.PI * 22) > .3 ? 1 : 0)) * f;
    const r = R - depth;
    return [Math.cos(ang) * r, Math.sin(ang) * r];
  }) };
}
export function buildPoisonStreamHero(K) {
  const { THREE, mats, grp } = K;
  sprayerSurfaces(THREE, mats);
  const M = mats, id = 'poisonstream', g = grp(id), B = PS.bore;
  const arm = grp(id + '_slide_assembly'); arm.userData.recoil = { axis: [0, 0, 1], travel: .0120 }; g.add(arm);

  g.add(loft(THREE, id + '_body', span(PS.z0, PS.z1, .0005).map(psBodyStation), M.hero_dlc,
    { creaseFrom: psBodyStation(-.020).pts, creaseAngle: 28, wear: (x, y) => smooth(PS.yt - .004, PS.yt, y) * .30 + smooth(PS.hw - .002, PS.hw, Math.abs(x)) * .18 }));
  g.add(engrave(THREE, id + '_rollmark', 'TOXIN 40', M.hero_engrave, { x: -PS.hw + .0004, z: .034, y: B - .012, cap: .0038, plane: 'left' }));
  g.add(engrave(THREE, id + '_rollmark_r', 'POISONSTREAM', M.hero_engrave, { x: PS.hw - .0004, z: -.028, y: B - .012, cap: .0032, plane: 'right' }));
  /* Barrel: threads into the body face; fluted, ported. */
  g.add(loft(THREE, id + '_barrel', span(PS.z0 - PS.bbl, PS.z0 + .012, .002).map(psBarrelStation), M.hero_nitride,
    { creaseAngle: 50, pos: [0, B, 0], wear: (x, y, z) => smooth(.0105, .0118, Math.hypot(x, y)) * .35 + (z < PS.z0 - PS.bbl + .02 ? .3 : 0) }));
  /* Fan tip: round at the thread, closing to a 14 × 2 mm slot at the face. */
  const TZ = PS.z0 - PS.bbl;
  g.add(loft(THREE, id + '_tip', span(TZ - .026, TZ + .002, .001).map((z) => {
    const u = clamp((TZ + .002 - z) / .028, 0, 1), a = lerp(.0098, .0082, u), b = lerp(.0098, .0030, smooth(.35, 1, u)), n = lerp(2, 3.2, u);
    return { z, pts: Array.from({ length: 40 }, (_, i) => { const ang = i / 40 * Math.PI * 2, r = sup(ang, a, b, n); return [Math.cos(ang) * r, Math.sin(ang) * r]; }) };
  }), M.hero_brass, { creaseAngle: 40, pos: [0, B, 0], wear: (x, y, z) => (z < TZ - .022 ? .5 : .1) }));
  g.add(revolve(THREE, id + '_tip_slot', [[0, 0], [.0060, 0]], M.hero_toxin, { segs: 20, pos: [0, B, TZ - .0262] }));
  /* Feed neck + reservoir. The reservoir is the `_magazine`: a glass cylinder on a
     brass neck, graduated, with the dose level visible inside. It pulls UP. */
  g.add(revolve(THREE, id + '_neck', [[0, 0], [.0150, 0], [.0150, .0100], [.0126, .0130], [.0126, .0190], [.0146, .0206], [.0146, .0232], [.0120, .0240], [0, .0240]],
    M.hero_brass, { segs: 40, creaseAngle: 30, pos: [0, PS.yt + .0020, PS.neckZ], rot: [-Math.PI / 2, 0, 0], wear: (r) => smooth(.0140, .0150, r) * .4 }));
  const can = grp(id + '_magazine');
  const cy0 = PS.canY0, cy1 = PS.canY1, R = PS.canR;
  can.add(revolve(THREE, id + '_reservoir', [[0, cy0], [.0120, cy0], [.0120, cy0 + .004], [R - .003, cy0 + .008], [R, cy0 + .014], [R, cy1 - .020], [R - .002, cy1 - .012], [R - .008, cy1 - .004], [.014, cy1], [0, cy1]],
    M.hero_toxin_glass, { segs: 64, creaseAngle: 30, rot: [-Math.PI / 2, 0, 0] }));
  can.add(revolve(THREE, id + '_dose', [[0, cy0 + .010], [R - .0028, cy0 + .010], [R - .0028, cy0 + .072], [0, cy0 + .072]], M.hero_toxin, { segs: 48, rot: [-Math.PI / 2, 0, 0] }));
  for (let i = 0; i < 6; i++) can.add(revolve(THREE, `${id}_grad${i}`, [[R, cy0 + .020 + i * .012], [R + .0007, cy0 + .0208 + i * .012], [R, cy0 + .0216 + i * .012]], M.hero_dlc, { segs: 64, rot: [-Math.PI / 2, 0, 0] }));
  can.add(revolve(THREE, id + '_cap', [[0, cy1 - .006], [.0150, cy1 - .006], [.0158, cy1 - .002], [.0158, cy1 + .006], [.0120, cy1 + .009], [0, cy1 + .009]], M.hero_brass, { segs: 40, creaseAngle: 30, rot: [-Math.PI / 2, 0, 0], wear: (r) => smooth(.0150, .0158, r) * .5 }));
  for (const c of can.children) c.position.z = PS.neckZ;
  repivot(can, [0, cy0, PS.neckZ]);
  can.userData.drop = { axis: [0, 1, 0], tip: [0, 0, 1], clear: .0300 };
  g.add(can);
  /* Rear bottle — the stock. A real pressure vessel with torispherical ends and a
     regulator body turned onto its front boss. */
  const stock = grp(id + '_stock');
  const tp = [[0, PS.tankZ0]];
  for (let i = 0; i <= 8; i++) { const a = i / 8 * Math.PI / 2; tp.push([Math.sin(a) * PS.tankR, PS.tankZ0 + (1 - Math.cos(a)) * .020]); }
  tp.push([PS.tankR, PS.tankZ1 - .030]);
  for (let i = 0; i <= 8; i++) { const a = i / 8 * Math.PI / 2; tp.push([Math.cos(a) * PS.tankR, PS.tankZ1 - .030 + Math.sin(a) * .030]); }
  stock.add(revolve(THREE, id + '_bottle', tp, M.hero_brushed, { segs: 72, creaseAngle: 30, pos: [0, B - .004, 0], wear: (r) => smooth(PS.tankR - .001, PS.tankR, r) * .25 }));
  stock.add(revolve(THREE, id + '_bottle_band', [[PS.tankR, .2200], [PS.tankR + .0012, .2210], [PS.tankR + .0012, .2340], [PS.tankR, .2350]], M.hero_hazard, { segs: 72, pos: [0, B - .004, 0] }));
  stock.add(revolve(THREE, id + '_regulator', [[0, PS.z1 - .006], [.0160, PS.z1 - .006], [.0180, PS.z1], [.0180, PS.tankZ0 + .010], [.0140, PS.tankZ0 + .016], [0, PS.tankZ0 + .016]], M.hero_brass, { segs: 40, creaseAngle: 30, pos: [0, B - .004, 0], wear: (r) => smooth(.0170, .0180, r) * .4 }));
  stock.add(revolve(THREE, id + '_gauge', [[0, 0], [.0070, 0], [.0078, .0012], [.0078, .0050], [.0064, .0060], [0, .0060]], M.hero_brass, { segs: 28, creaseAngle: 30, pos: [-.0176, B - .004, PS.tankZ0 + .004], rot: [0, -Math.PI / 2, 0] }));
  stock.add(revolve(THREE, id + '_gauge_face', [[0, 0], [.0058, 0]], M.hero_toxin_glass, { segs: 28, pos: [-.0236, B - .004, PS.tankZ0 + .004], rot: [0, -Math.PI / 2, 0] }));
  stock.add(revolve(THREE, id + '_butt_pad', [[.0100, PS.tankZ1 - .004], [PS.tankR - .006, PS.tankZ1 - .004], [PS.tankR - .002, PS.tankZ1 + .004], [PS.tankR - .006, PS.tankZ1 + .012], [.0100, PS.tankZ1 + .012]], M.hero_stipple, { segs: 56, creaseAngle: 30, pos: [0, B - .004, 0] }));
  g.add(stock);
  /* Valve armature — the moving part, proud of the body's rear face under the bottle. */
  arm.add(revolve(THREE, id + '_armature', [[0, PS.z1 - .030], [.0080, PS.z1 - .030], [.0080, PS.z1 + .006], [.0092, PS.z1 + .008], [.0092, PS.z1 + .020], [.0070, PS.z1 + .022], [0, PS.z1 + .022]],
    M.hero_steel_bright, { segs: 32, creaseAngle: 30, pos: [0, PS.yb + .0100, 0], wear: (r, z) => (z > PS.z1 + .018 ? .5 : .15) }));
  /* Grip, guard, trigger — same anatomy as the pistols, sized for a two-hand carbine hold. */
  g.add(sweep(THREE, id + '_grip', [[0, PS.yb + .004, .0100], [0, .0100, .0230], [0, -.0160, .0370], [0, -.0420, .0500], [0, -.0580, .0580]], (t, ang) => {
    const a = .0140 - .0014 * smooth(.1, .95, t) + .0012 * smooth(.86, 1, t), b = .0180 - .0040 * smooth(.05, .7, t) + .0016 * smooth(.88, 1, t);
    const swell = .0007 * Math.pow(Math.abs(Math.cos(ang)), 3) * smooth(.2, .55, t) * (1 - smooth(.8, 1, t));
    return sup(ang, a + swell, b, 3.6);
  }, M.hero_stipple, { radial: 40, samples: 112, up: [1, 0, 0], vRep: 6, wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 4) * smooth(.15, .5, t) * .45 }));
  /* ASA block under the grip: where the bottle line lands. */
  g.add(revolve(THREE, id + '_asa', [[0, 0], [.0090, 0], [.0090, .0140], [.0074, .0160], [0, .0160]], M.hero_brass, { segs: 28, creaseAngle: 30, pos: [0, -.0600, .0600], rot: [-1.14, 0, 0] }));
  g.add(sweep(THREE, id + '_trigger_guard', [[0, PS.yb + .006, -.0360], [0, .0140, -.0400], [0, .0000, -.0390], [0, -.0100, -.0320], [0, -.0140, -.0220], [0, -.0130, -.0110], [0, -.0070, -.0020], [0, .0090, .0060], [0, PS.yb + .006, .0100]],
    (t, ang) => { const end = 1 - Math.sin(Math.PI * t) * .80; return sup(ang, .0076 + .0020 * end, .0032 + .0024 * end, 4); }, M.hero_dlc, { radial: 34, samples: 132, up: [1, 0, 0], vRep: 9 }));
  g.add(sweep(THREE, id + '_trigger', [[0, .0380, -.0090], [0, .0230, -.0110], [0, .0090, -.0134], [0, -.0016, -.0158], [0, -.0088, -.0182], [0, -.0114, -.0212]],
    (t) => [.0028 + .0005 * smooth(.2, .6, t), .0020 + .0003 * smooth(.2, .7, t)], M.hero_steel_bright, { radial: 22, samples: 64, up: [1, 0, 0], vRep: 4, wear: (t, ang) => smooth(.45, .85, t) * Math.max(0, -Math.cos(ang)) * .55 }));
  /* Vertical foregrip under the barrel, rooted in a clamp ring. */
  g.add(revolve(THREE, id + '_clamp', [[.0120, -.010], [.0160, -.010], [.0170, -.008], [.0170, .008], [.0160, .010], [.0120, .010]], M.hero_dlc, { segs: 40, creaseAngle: 30, pos: [0, B, -.2200] }));
  g.add(sweep(THREE, id + '_foregrip', [[0, B - .012, -.2200], [0, B - .040, -.2170], [0, B - .070, -.2120], [0, B - .092, -.2060]], (t, ang) => {
    const groove = .0007 * Math.pow(Math.max(0, Math.sin(t * Math.PI * 4 - .4)), 6) * Math.max(0, -Math.sin(ang));
    return sup(ang, .0120 + .0016 * smooth(.85, 1, t), .0130 + .0010 * Math.sin(Math.PI * t), 3.2) - groove;
  }, M.hero_stipple, { radial: 32, samples: 60, up: [1, 0, 0], vRep: 5, wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 3) * .3 }));
  /* Braided gas line: ASA → regulator. Ferrules at both ends. */
  g.add(sweep(THREE, id + '_hose', [[0, -.0500, .0700], [0, -.0560, .0860], [0, -.0420, .1080], [0, -.0100, .1220], [0, B - .022, .1300]], (t, ang) => .0040 + .00022 * Math.sin(ang * 8 + t * 44) * smooth(.10, .18, t) * (1 - smooth(.84, .94, t)),
    M.hero_hose, { radial: 24, samples: 96, up: [1, 0, 0], vRep: 22 }));
  for (const [y, z, rot] of [[-.0520, .0720, -1.14], [B - .0240, .1280, -.35]]) {
    g.add(revolve(THREE, `${id}_ferrule_${Math.round(z * 1e4)}`, Array.from({ length: 7 }, (_, i) => [i % 2 ? .0050 : .0058, i * .0018]).concat([[.0050, .0116], [.0066, .0124], [.0066, .0150], [0, .0150]]),
      M.hero_brass, { segs: 26, pos: [0, y, z], rot: [rot, 0, 0], wear: (r) => smooth(.0052, .0058, r) * .5 }));
  }
  /* Sights: rear notch on the body's rear top, front post on the barrel clamp. */
  g.add(loft(THREE, id + '_rear_sight', span(.0700, .0800, .0005).map((z) => { const t = (z - .07) / .01, w = .0070 - .0010 * smooth(.5, 1, t), yb = PS.yt + .002, yt = yb + .0060; return { z, pts: [[w, yb], [w, yt], [.0020, yt], [.0020, yt - .0026], [-.0020, yt - .0026], [-.0020, yt], [-w, yt], [-w, yb]] }; }), M.hero_steel, { creaseAngle: 24 }));
  g.add(loft(THREE, id + '_front_sight', span(-.2260, -.2220, .0004).map((z) => ({ z, pts: [[.0018, B + .017], [.0014, B + .026], [-.0014, B + .026], [-.0018, B + .017]] })), M.hero_steel, { creaseAngle: 24, wear: (x, y) => smooth(B + .024, B + .026, y) * .6 }));

  const mount = (slot, pos, parent = g) => { const m = grp(`${id}_mount_${slot}`, pos); m.userData.slot = slot; parent.add(m); };
  mount('barrel', [0, B, PS.z0]); mount('muzzle', [0, B, TZ - .0262]); mount('optic', [0, PS.yt, .0400]);
  mount('magazine', [0, PS.canY1, PS.neckZ]); mount('magwell', [0, PS.yt + .0260, PS.neckZ]); mount('stock', [0, B, PS.z1]);
  mount('underbarrel', [0, B - .012, -.1700]); mount('infusion', [-PS.hw, B - .008, .0000]);
  return g;
}

/* ══ CRYO SPRAYER ════════════════════════════════════════════════════════ */
const CS = { bore: .0580, hw: .0160, z0: -.0900, z1: .1000, yt: .0780, yb: .0340, lance: .2300, hornZ0: -.3200, hornZ1: -.4200, dewR: .0340, dewZ0: -.3000, dewZ1: -.1200, dewY: .0130 };

function csBodyStation(z) {
  const nose = smooth(CS.z0, CS.z0 + .010, z), tail = 1 - smooth(CS.z1 - .014, CS.z1, z) * .20;
  const hw = (CS.hw - (1 - nose) * .0026) * tail, yt = CS.yt - (1 - nose) * .004, yb = CS.yb + (1 - nose) * .003;
  /* Heat-exchange ribs: nine grooves cut across both flanks over the valve block. */
  const rib = smooth(-.060, -.056, z) * (1 - smooth(.020, .024, z)) * (Math.sin((z + .06) * Math.PI / .0092) > .2 ? 1 : 0);
  const pts = rrect(hw, yb, yt, .0040, 4).map(([x, y]) => {
    const v = (y - yb) / (yt - yb);
    const cut = rib * .0016 * smooth(.12, .22, v) * (1 - smooth(.78, .88, v)) * smooth(hw - .002, hw, Math.abs(x));
    return [x - Math.sign(x) * cut, y];
  });
  return { z, pts };
}
/** Discharge horn: bell with eight vent slots cut through the wall over the flare. */
function csHornStation(z) {
  const u = clamp((CS.hornZ0 - z) / (CS.hornZ0 - CS.hornZ1), 0, 1);
  const R = .0140 + .0220 * Math.pow(u, 1.8) + .0020 * smooth(.94, 1, u);
  const slot = smooth(.30, .36, u) * (1 - smooth(.78, .84, u));
  const N = 64;
  return { z, pts: Array.from({ length: N }, (_, i) => {
    const ang = i / N * Math.PI * 2, s = Math.pow(Math.max(0, Math.cos(ang * 8)), 6);
    const r = R - slot * .0030 * s;
    return [Math.cos(ang) * r, Math.sin(ang) * r];
  }) };
}
export function buildCryoSprayerHero(K) {
  const { THREE, mats, grp } = K;
  sprayerSurfaces(THREE, mats);
  const M = mats, id = 'cryosprayer', g = grp(id), B = CS.bore;
  const arm = grp(id + '_slide_assembly'); arm.userData.recoil = { axis: [0, 0, 1], travel: .0060 }; g.add(arm);

  g.add(loft(THREE, id + '_body', span(CS.z0, CS.z1, .0005).map(csBodyStation), M.hero_steel,
    { creaseFrom: csBodyStation(.050).pts, creaseAngle: 28, wear: (x, y) => smooth(CS.yt - .004, CS.yt, y) * .30 + smooth(CS.hw - .002, CS.hw, Math.abs(x)) * .18 }));
  g.add(engrave(THREE, id + '_rollmark', 'CRYO 45', M.hero_engrave, { x: -CS.hw + .0004, z: .046, y: B - .012, cap: .0038, plane: 'left' }));
  g.add(engrave(THREE, id + '_rollmark_r', 'LN2 - KEEP UPRIGHT', M.hero_engrave, { x: CS.hw - .0004, z: .030, y: B - .012, cap: .0026, plane: 'right' }));
  /* Lance: a turned stainless tube with a frost sleeve where the cold reaches the metal. */
  const LZ = CS.z0 - CS.lance;
  /* Revolve profiles run front → rear (increasing z) so the surface faces out. */
  g.add(revolve(THREE, id + '_barrel', [[.0110, CS.z0 - .012], [.0070, CS.z0 - .012], [.0070, LZ - .004], [.0100, LZ - .004], [.0100, LZ + .010], [.0092, LZ + .030], [.0096, CS.z0 - .030], [.0110, CS.z0 - .012], [.0140, CS.z0 - .006], [.0140, CS.z0 + .010], [0, CS.z0 + .010]],
    M.hero_brushed, { segs: 56, creaseAngle: 30, pos: [0, B, 0], wear: (r, z) => (z < LZ + .04 ? .35 : 0) }));
  g.add(revolve(THREE, id + '_rime_lance', Array.from({ length: 13 }, (_, i) => [i % 2 ? .0118 : .0104, LZ + .040 + i * .004]).concat([[.0100, LZ + .092], [.0100, LZ + .040]]), M.hero_rime, { segs: 40 }));
  g.add(loft(THREE, id + '_horn', span(CS.hornZ1, CS.hornZ0 + .012, .0015).map(csHornStation), M.hero_brushed,
    { creaseAngle: 45, pos: [0, B, 0], wear: (x, y, z) => smooth(CS.hornZ1 + .010, CS.hornZ1, z) * .5 }));
  g.add(revolve(THREE, id + '_horn_bore', [[0, 0], [.0300, 0]], M.hero_cryo, { segs: 48, pos: [0, B, CS.hornZ1 + .0005] }));
  g.add(revolve(THREE, id + '_rime_horn', [[.0150, CS.hornZ0 - .026], [.0170, CS.hornZ0 - .024], [.0186, CS.hornZ0 - .012], [.0176, CS.hornZ0 - .002], [.0140, CS.hornZ0 + .004]], M.hero_rime, { segs: 48, pos: [0, B, 0] }));
  /* Dewar — the `_magazine`: vacuum flask on a manifold at the body's front foot.
     Fill/vent neck on top, relief valve, frost line where the liquid stands. */
  g.add(revolve(THREE, id + '_manifold', [[0, CS.dewZ1], [.0180, CS.dewZ1], [.0220, CS.dewZ1 + .006], [.0220, CS.z0], [.0200, CS.z0 + .004], [0, CS.z0 + .004]], M.hero_steel,
    { segs: 48, creaseAngle: 30, pos: [0, CS.dewY, 0], wear: (r) => smooth(.0210, .0220, r) * .3 }));
  const dew = grp(id + '_magazine'), R = CS.dewR;
  const dp = [[0, CS.dewZ1]];
  for (let i = 0; i <= 8; i++) { const a = i / 8 * Math.PI / 2; dp.push([Math.sin(a) * R, CS.dewZ1 - (1 - Math.cos(a)) * .014]); }
  dp.push([R, CS.dewZ1 - .022], [R + .0014, CS.dewZ1 - .024], [R + .0014, CS.dewZ1 - .030], [R, CS.dewZ1 - .032], [R, CS.dewZ0 + .028]);
  for (let i = 0; i <= 8; i++) { const a = i / 8 * Math.PI / 2; dp.push([Math.cos(a) * R, CS.dewZ0 + .028 - Math.sin(a) * .028]); }
  dew.add(revolve(THREE, id + '_dewar', dp.reverse(), M.hero_brushed, { segs: 72, creaseAngle: 30, pos: [0, CS.dewY, 0], wear: (r) => smooth(R - .001, R + .0014, r) * .3 }));
  dew.add(revolve(THREE, id + '_dewar_rime', Array.from({ length: 9 }, (_, i) => [i % 2 ? R + .0022 : R + .0010, CS.dewZ0 + .050 + i * .003]).concat([[R + .0005, CS.dewZ0 + .078], [R + .0005, CS.dewZ0 + .050]]), M.hero_rime, { segs: 72, pos: [0, CS.dewY, 0] }));
  dew.add(revolve(THREE, id + '_sight_bezel', [[R - .0004, -.0080], [R + .0028, -.0068], [R + .0028, .0068], [R - .0004, .0080]], M.hero_steel_bright, { segs: 72, pos: [0, CS.dewY, CS.dewZ0 + .095] }));
  dew.add(revolve(THREE, id + '_sight_glass', [[R + .0014, -.0058], [R + .0020, 0], [R + .0014, .0058]], M.hero_cryo_glass, { segs: 72, pos: [0, CS.dewY, CS.dewZ0 + .095] }));
  dew.add(revolve(THREE, id + '_fill_neck', [[0, 0], [.0070, 0], [.0070, .0060], [.0090, .0070], [.0090, .0110], [.0060, .0126], [0, .0126]], M.hero_steel, { segs: 28, creaseAngle: 30, pos: [0, CS.dewY + R - .002, CS.dewZ0 + .050], rot: [-Math.PI / 2, 0, 0] }));
  dew.add(revolve(THREE, id + '_relief_valve', [[0, 0], [.0044, 0], [.0044, .0090], [.0060, .0100], [.0060, .0150], [0, .0150]], M.hero_brass, { segs: 24, creaseAngle: 30, pos: [.0120, CS.dewY + R - .003, CS.dewZ0 + .140], rot: [-Math.PI / 2, 0, 0] }));
  dew.add(revolve(THREE, id + '_band', [[R, CS.dewZ0 + .110], [R + .0012, CS.dewZ0 + .111], [R + .0012, CS.dewZ0 + .122], [R, CS.dewZ0 + .123]], M.hero_hazard, { segs: 72, pos: [0, CS.dewY, 0] }));
  repivot(dew, [0, CS.dewY, CS.dewZ1]);
  dew.userData.drop = { axis: [0, 0, -1], tip: [1, 0, 0], clear: .0400 };
  g.add(dew);
  /* Vacuum-jacketed feed: dewar top boss → valve block inlet on the body's front foot. */
  g.add(sweep(THREE, id + '_feed', [[0, CS.dewY + R - .004, CS.dewZ1 - .030], [0, CS.dewY + R + .012, CS.dewZ1 - .010], [0, B - .026, CS.z0 - .020], [0, B - .022, CS.z0 + .010]],
    (t, ang) => .0056 + .0005 * Math.sin(ang * 6 + t * 40) * smooth(.15, .25, t) * (1 - smooth(.75, .85, t)), M.hero_hose, { radial: 24, samples: 80, up: [1, 0, 0], vRep: 16 }));
  for (const [y, z, rot] of [[CS.dewY + R - .002, CS.dewZ1 - .032, -1.0], [B - .0230, CS.z0 + .004, .10]]) {
    g.add(revolve(THREE, `${id}_ferrule_${Math.round(Math.abs(z) * 1e4)}`, Array.from({ length: 7 }, (_, i) => [i % 2 ? .0064 : .0072, i * .0018]).concat([[.0064, .0116], [.0080, .0124], [.0080, .0150], [0, .0150]]),
      M.hero_steel_bright, { segs: 26, pos: [0, y, z], rot: [rot, 0, 0], wear: (r) => smooth(.0066, .0072, r) * .5 }));
  }
  /* Solenoid armature: proud of the body's rear face, the part that kicks. */
  arm.add(revolve(THREE, id + '_armature', [[0, CS.z1 - .030], [.0090, CS.z1 - .030], [.0090, CS.z1 + .004], [.0104, CS.z1 + .006], [.0104, CS.z1 + .016], [.0080, CS.z1 + .018], [0, CS.z1 + .018]],
    M.hero_steel_bright, { segs: 32, creaseAngle: 30, pos: [0, B - .006, 0], wear: (r, z) => (z > CS.z1 + .014 ? .5 : .15) }));
  /* Tube stock with a rubber pad; the `_stock` a brace would replace. */
  const stock = grp(id + '_stock');
  stock.add(revolve(THREE, id + '_stock_tube', [[0, CS.z1 - .004], [.0130, CS.z1 - .004], [.0130, CS.z1 + .190], [.0116, CS.z1 + .192], [.0116, CS.z1 + .196], [0, CS.z1 + .196]], M.hero_steel, { segs: 40, creaseAngle: 30, pos: [0, B - .004, 0] }));
  for (let i = 0; i < 5; i++) stock.add(revolve(THREE, `${id}_stock_notch${i}`, [[.0130, CS.z1 + .060 + i * .020], [.0122, CS.z1 + .061 + i * .020], [.0130, CS.z1 + .062 + i * .020]], M.hero_dlc, { segs: 40, pos: [0, B - .004, 0] }));
  stock.add(loft(THREE, id + '_stock_body', span(CS.z1 + .130, CS.z1 + .200, .002).map((z) => { const u = (z - CS.z1 - .130) / .070; return { z, pts: rrect(.0160 + .0010 * u, B - .058 - .012 * u, B + .012 + .002 * u, .006, 4) }; }), M.hero_polymer, { creaseAngle: 40 }));
  stock.add(loft(THREE, id + '_stock_pad', span(CS.z1 + .200, CS.z1 + .214, .002).map((z) => ({ z, pts: rrect(.0172, B - .071, B + .015, .007, 4) })), M.hero_stipple, { creaseAngle: 40 }));
  g.add(stock);
  /* Grip, guard, trigger. */
  g.add(sweep(THREE, id + '_grip', [[0, CS.yb + .004, .0100], [0, .0100, .0230], [0, -.0160, .0370], [0, -.0420, .0500], [0, -.0580, .0580]], (t, ang) => {
    const a = .0140 - .0014 * smooth(.1, .95, t) + .0012 * smooth(.86, 1, t), b = .0180 - .0040 * smooth(.05, .7, t) + .0016 * smooth(.88, 1, t);
    return sup(ang, a, b, 3.6);
  }, M.hero_stipple, { radial: 40, samples: 112, up: [1, 0, 0], vRep: 6, wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 4) * smooth(.15, .5, t) * .45 }));
  g.add(sweep(THREE, id + '_trigger_guard', [[0, CS.yb + .006, -.0360], [0, .0140, -.0400], [0, .0000, -.0390], [0, -.0100, -.0320], [0, -.0140, -.0220], [0, -.0130, -.0110], [0, -.0070, -.0020], [0, .0090, .0060], [0, CS.yb + .006, .0100]],
    (t, ang) => { const end = 1 - Math.sin(Math.PI * t) * .80; return sup(ang, .0076 + .0020 * end, .0032 + .0024 * end, 4); }, M.hero_steel, { radial: 34, samples: 132, up: [1, 0, 0], vRep: 9 }));
  g.add(sweep(THREE, id + '_trigger', [[0, .0380, -.0090], [0, .0230, -.0110], [0, .0090, -.0134], [0, -.0016, -.0158], [0, -.0088, -.0182], [0, -.0114, -.0212]],
    (t) => [.0028 + .0005 * smooth(.2, .6, t), .0020 + .0003 * smooth(.2, .7, t)], M.hero_dlc, { radial: 22, samples: 64, up: [1, 0, 0], vRep: 4, wear: (t, ang) => smooth(.45, .85, t) * Math.max(0, -Math.cos(ang)) * .4 }));
  /* Insulated carry handle over the dewar — the off-hand hold, since the vessel itself is at −190 °C. */
  g.add(sweep(THREE, id + '_handle', [[0, CS.dewY - R + .010, CS.dewZ0 + .040], [0, CS.dewY - R - .012, CS.dewZ0 + .046], [0, CS.dewY - R - .020, CS.dewZ0 + .080], [0, CS.dewY - R - .020, CS.dewZ0 + .130], [0, CS.dewY - R - .012, CS.dewZ0 + .164], [0, CS.dewY - R + .010, CS.dewZ0 + .170]],
    (t, ang) => { const end = 1 - Math.sin(Math.PI * t) * .35; const groove = .0006 * Math.pow(Math.max(0, Math.sin(t * Math.PI * 9)), 6) * Math.max(0, -Math.sin(ang)); return sup(ang, .0090 + .002 * end, .0070 + .002 * end, 3) - groove; },
    M.hero_stipple, { radial: 28, samples: 96, up: [1, 0, 0], vRep: 8, wear: (t, ang) => Math.max(0, -Math.sin(ang)) * smooth(.3, .5, t) * (1 - smooth(.5, .7, t)) * .4 }));
  /* Sights. */
  g.add(loft(THREE, id + '_rear_sight', span(.0500, .0600, .0005).map((z) => { const t = (z - .05) / .01, w = .0070 - .0010 * smooth(.5, 1, t), yb = CS.yt + .002, yt = yb + .0060; return { z, pts: [[w, yb], [w, yt], [.0020, yt], [.0020, yt - .0026], [-.0020, yt - .0026], [-.0020, yt], [-w, yt], [-w, yb]] }; }), M.hero_steel, { creaseAngle: 24 }));
  g.add(loft(THREE, id + '_front_sight', span(CS.z0 - .012, CS.z0 - .008, .0004).map((z) => ({ z, pts: [[.0018, B + .014], [.0014, B + .024], [-.0014, B + .024], [-.0018, B + .014]] })), M.hero_steel, { creaseAngle: 24, wear: (x, y) => smooth(B + .022, B + .024, y) * .6 }));

  const mount = (slot, pos, parent = g) => { const m = grp(`${id}_mount_${slot}`, pos); m.userData.slot = slot; parent.add(m); };
  mount('barrel', [0, B, CS.z0]); mount('muzzle', [0, B, CS.hornZ1]); mount('optic', [0, CS.yt, .0200]);
  mount('magazine', [0, CS.dewY, CS.dewZ0]); mount('magwell', [0, CS.dewY, CS.dewZ1]); mount('stock', [0, B, CS.z1]);
  mount('underbarrel', [0, CS.dewY - R, CS.dewZ0 + .100]); mount('infusion', [-CS.hw, B - .008, .0300]);
  return g;
}
