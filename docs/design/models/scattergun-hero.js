/**
 * Deep Field 3D — Scattergun, hero standard.
 *
 * Built to the vocabulary in gun-kit.js and measured against a real semi-auto
 * tactical 12-gauge rather than eyeballed:
 *
 *   12 gauge          18.5 mm bore, 20.4 mm rim — the hull drives the barrel,
 *                     the magazine tube, the loading port and the ejection
 *                     port. Get the shell right and the gun sizes itself.
 *   18" barrel        457 mm from the receiver face. 808 mm overall.
 *   receiver          41 mm wide, 45 mm tall — a tall boxed alloy receiver,
 *                     not a tube. Ejection port on the RIGHT only; loading
 *                     port cut up into the belly.
 *   magazine tube     under the barrel, 5 rounds, knurled end cap.
 *   handguard         one polymer shell over barrel AND tube — an oval in
 *                     section, 33 × 53 mm, vented on both flanks.
 *
 * Finish is parkerised phosphate, not bronze: the armoury keeps one finish per
 * weapon and only the quality level rises (CLAUDE.md).
 *
 * Metres, Y-up, muzzle toward −Z, origin at the grip web.
 */
import {
  heroSurfaces, loft, sweep, revolve, engrave,
  clamp, smooth, lerp,
} from './gun-kit.js';

const BORE = .0620;                    // bore axis above the grip web
const RAIL = .0880;                    // rail surface — 26 mm over bore
const RHW = .0205;                     // receiver half-width (41 mm)
const RZ0 = -.1360, RZ1 = .0560;       // receiver face / rear of receiver
const RYT = RAIL - .0086;              // receiver top
const RYB = BORE - .0280;              // receiver bottom
const BBL = .4570;                     // 18"
const MUZZLE = RZ0 - BBL;
const GA = .00925;                     // 12-gauge bore radius
const TUBE_Y = BORE - .0255;           // magazine tube axis
const HG_Z0 = RZ0 - .2450, HG_Z1 = RZ0 - .0060;
const HG_CY = (BORE + TUBE_Y) / 2;
const HG_HH = (BORE - TUBE_Y) / 2 + .0140;
const STK0 = .0520, STK1 = .2020;

const span = (a, b, step) => { const o = []; for (let v = a; v <= b; v += step) o.push(v); return o; };
const sup = (ang, a, b, n) => 1 / Math.pow(Math.pow(Math.abs(Math.cos(ang) / a), n) + Math.pow(Math.abs(Math.sin(ang) / b), n), 1 / n);

/**
 * Receiver. A tall alloy box with vertical walls, both service ports cut INTO
 * the walls rather than boxed on, and a plinth on top for the rail.
 */
function recvStation(z) {
  const nose = smooth(RZ0, RZ0 + .0075, z);
  const tail = 1 - smooth(RZ1 - .0090, RZ1, z) * .10;
  const hw = (RHW - (1 - nose) * .0020) * tail;
  const yt = RYT - (1 - nose) * .0016, yb = RYB + (1 - nose) * .0022;
  // ejection port — right flank only, 58 mm long
  const port = smooth(-.0790, -.0750, z) * (1 - smooth(-.0210, -.0170, z));
  // loading port — cut up into the belly between the side walls
  const load = smooth(-.1010, -.0970, z) * (1 - smooth(-.0350, -.0310, z));
  const rf = (y) => {
    const v = (y - yb) / (yt - yb);
    return hw - port * .0060 * smooth(.16, .26, v) * (1 - smooth(.70, .80, v));
  };
  const bf = (x) => yb + load * .0082 * (1 - smooth(.40, .72, Math.abs(x) / hw));
  const pts = [];
  pts.push([hw, yb + .0028]);
  for (let i = 0; i < 9; i++) { const y = lerp(yb + .0052, yt - .0048, i / 8); pts.push([rf(y), y]); }
  pts.push([hw - .0024, yt - .0018], [hw - .0054, yt]);
  pts.push([.0090, yt], [.0090, yt + .0036], [-.0090, yt + .0036], [-.0090, yt]);
  pts.push([-(hw - .0054), yt], [-(hw - .0024), yt - .0018]);
  for (let i = 0; i < 9; i++) { const y = lerp(yt - .0048, yb + .0052, i / 8); pts.push([-hw, y]); }
  pts.push([-hw, yb + .0028]);
  for (let i = 0; i <= 10; i++) { const x = lerp(-(hw - .0024), hw - .0024, i / 10); pts.push([x, bf(x)]); }
  return { z, pts };
}

/** Picatinny: 5.35 mm slots on 10.2 mm pitch, cut through the rib. */
function railSlot(z) {
  const p = (z + .0940) / .0102, f = p - Math.floor(p);
  const w = .00535 / .0102;
  return smooth(0, .10, f) * (1 - smooth(w - .10, w, f));
}
function railStation(z) {
  const cut = railSlot(z);
  const yb = RAIL - .0088, yt = RAIL - cut * .0034;
  const hw = .01055, nw = .00790;
  return { z, pts: [
    [hw, yb], [hw, yb + .0020], [nw, yb + .0040], [nw, yt - .0014], [nw - .0016, yt],
    [-(nw - .0016), yt], [-nw, yt - .0014], [-nw, yb + .0040], [-hw, yb + .0020], [-hw, yb],
  ] };
}

/**
 * Handguard: ONE shell over the barrel and the tube — on a real tactical
 * shotgun these are not two separate tubes with a bridge. Oval in section,
 * vented on both flanks, longitudinal grip ribs top and bottom.
 */
function hgVent(z) {
  const p = (z - (HG_Z0 + .0300)) / .0470;
  if (p < 0 || p >= 4) return 0;
  const f = p - Math.floor(p), w = .0310 / .0470;
  return smooth(0, .07, f) * (1 - smooth(w - .07, w, f));
}
function hgStation(z) {
  const t = clamp((z - HG_Z0) / (HG_Z1 - HG_Z0), 0, 1);
  const taper = 1 - .085 * (1 - smooth(0, .12, t)) - .03 * smooth(.90, 1, t);
  const a = .0166 * taper, b = HG_HH * taper, v = hgVent(z);
  const pts = [], N = 64;
  for (let i = 0; i < N; i++) {
    const ang = i / N * Math.PI * 2, ca = Math.cos(ang), sa = Math.sin(ang);
    let rad = sup(ang, a, b, 3.2);
    rad -= v * .0034 * Math.pow(Math.abs(ca), 6) * (1 - Math.pow(Math.abs(sa), 3));
    rad -= .0009 * Math.pow(Math.abs(sa), 12) * Math.pow(Math.abs(Math.sin(ang * 11)), 4);
    pts.push([ca * rad, HG_CY + sa * rad]);
  }
  return { z, pts };
}

/** Buttstock: comb, toe, cheek ridge, sling slot, all cut in. */
function stockStation(z) {
  const t = clamp((z - STK0) / (STK1 - STK0), 0, 1);
  const hw = .0198 - .0022 * smooth(.35, .95, t);
  const yt = lerp(RYT - .0050, RYT + .0020, smooth(.05, .80, t));
  const yb = lerp(RYB - .0020, -.0250, smooth(.04, .96, t));
  const comb = .0032 * smooth(.18, .40, t);
  const slot = .0044 * smooth(.20, .26, t) * (1 - smooth(.40, .46, t));
  const scoop = .0026 * smooth(.50, .62, t) * (1 - smooth(.82, .90, t));
  const face = (y) => {
    const v = (y - yb) / (yt - yb);
    return hw - slot * smooth(.50, .60, v) * (1 - smooth(.78, .86, v))
              - scoop * smooth(.16, .32, v) * (1 - smooth(.64, .80, v));
  };
  const pts = [];
  pts.push([hw - .0022, yb], [hw, yb + .0026]);
  for (let i = 0; i < 12; i++) { const y = lerp(yb + .0050, yt - .0046, i / 11); pts.push([face(y), y]); }
  pts.push([hw - .0020, yt - .0016], [hw - .0048, yt]);
  pts.push([.0070, yt + comb], [-.0070, yt + comb]);
  pts.push([-(hw - .0048), yt], [-(hw - .0020), yt - .0016]);
  for (let i = 0; i < 12; i++) { const y = lerp(yt - .0046, yb + .0050, i / 11); pts.push([-face(y), y]); }
  pts.push([-hw, yb + .0026], [-(hw - .0022), yb]);
  return { z, pts };
}

export function buildScattergunHero(K) {
  const { THREE, mats, grp } = K;
  heroSurfaces(THREE, mats);
  const M = mats, id = 'scattergun', g = grp(id);
  const PK = M.hero_park;

  /* Reciprocating group. A semi-auto shotgun's bolt runs the full length of a
     hull plus clearance — 62 mm here. The barrel stays on the receiver. */
  const bolt = grp(id + '_bolt_assembly');
  bolt.userData.recoil = { axis: [0, 0, 1], travel: .0620 };
  g.add(bolt);

  /* ── receiver + rail ─────────────────────────────────────────────────── */
  g.add(loft(THREE, id + '_receiver', span(RZ0, RZ1, .0005).map(recvStation), PK,
    { creaseFrom: recvStation(-.1100).pts, creaseAngle: 26,
      wear: (x, y) => smooth(RYT - .0040, RYT, y) * .40 + smooth(RHW - .0022, RHW - .0004, Math.abs(x)) * .24 }));
  g.add(loft(THREE, id + '_top_rail', span(-.1000, .0420, .0006).map(railStation), PK,
    { creaseFrom: railStation(-.0985).pts, creaseAngle: 24, wear: (x, y) => smooth(RAIL - .0016, RAIL, y) * .6 }));
  g.add(engrave(THREE, id + '_rollmark', '12 GAUGE', M.hero_engrave,
    { x: -RHW, z: -.0620, y: BORE - .0110, cap: .0042, plane: 'left' }));

  /* ── barrel + magazine tube ──────────────────────────────────────────── */
  g.add(revolve(THREE, id + '_barrel', [
    [0, RZ0 + .0120], [.0168, RZ0 + .0120], [.0168, RZ0 - .0060], [.0142, RZ0 - .0140],   // chamber swell
    [.0134, RZ0 - .0420], [.0126, RZ0 - .1300], [.0119, RZ0 - .2600], [.0116, RZ0 - .3900],
    [.0118, MUZZLE + .0090], [.0122, MUZZLE + .0026], [.0122, MUZZLE],                     // muzzle band
    [GA + .0006, MUZZLE], [GA, MUZZLE + .0032], [GA, RZ0 - .0140], [.0106, RZ0 - .0060], [.0106, RZ0 + .0120],
  ], M.hero_nitride, { segs: 72, creaseAngle: 30, pos: [0, BORE, 0],
    wear: (r, z) => (z < MUZZLE + .0100 ? .55 : 0) + smooth(.0120, .0124, r) * .2 }));
  g.add(revolve(THREE, id + '_mag_tube', [
    [0, RZ0 + .0040], [.0128, RZ0 + .0040], [.0128, RZ0 - .0090], [.0122, RZ0 - .0140],
    [.0122, RZ0 - .3760], [.0128, RZ0 - .3780], [.0128, RZ0 - .3900],
  ].concat(Array.from({ length: 13 }, (_, i) => [i % 2 ? .0126 : .0134, RZ0 - .3910 - i * .0011]))
    .concat([[.0126, RZ0 - .4070], [.0100, RZ0 - .4092], [0, RZ0 - .4092]]), PK,
    { segs: 56, creaseAngle: 32, pos: [0, TUBE_Y, 0], wear: (r) => smooth(.0130, .0134, r) * .5 }));
  // Barrel ring tying the tube to the barrel — one forging, not a floating clamp.
  g.add(revolve(THREE, id + '_barrel_ring', [
    [.0116, 0], [.0164, 0], [.0164, .0150], [.0116, .0150],
  ], PK, { segs: 40, pos: [0, BORE, RZ0 - .3840] }));
  g.add(loft(THREE, id + '_tube_web', span(RZ0 - .3846, RZ0 - .3700, .0007).map((z) => {
    const t = (z - (RZ0 - .3846)) / .0146, w = .0058 - .0020 * smooth(.3, 1, t);
    return { z, pts: [[w, TUBE_Y], [w - .0012, BORE - .0060], [-(w - .0012), BORE - .0060], [-w, TUBE_Y]] };
  }), PK, { creaseAngle: 34 }));

  /* ── handguard ───────────────────────────────────────────────────────── */
  g.add(loft(THREE, id + '_handguard', span(HG_Z0, HG_Z1, .0005).map(hgStation), M.hero_polymer,
    { creaseFrom: hgStation(HG_Z0 + .1200).pts, creaseAngle: 32,
      wear: (x, y) => smooth(.0152, .0166, Math.abs(x)) * .34 }));

  /* ── sights: ghost ring rear, hooded post front ──────────────────────── */
  g.add(revolve(THREE, id + '_rear_ghost', [
    [.0044, 0], [.0072, 0], [.0072, .0026], [.0044, .0026],
  ], M.hero_park_dark, { segs: 40, pos: [0, RAIL + .0092, .0300] }));
  g.add(loft(THREE, id + '_rear_base', span(.0268, .0384, .0006).map((z) => {
    const t = (z - .0268) / .0116, w = .0074 - .0018 * smooth(.4, 1, t);
    const yt = RAIL + .0066 - .0016 * smooth(.5, 1, t);
    return { z, pts: [[w, RAIL], [w - .0014, yt], [-(w - .0014), yt], [-w, RAIL]] };
  }), M.hero_park_dark, { creaseAngle: 28 }));
  const fz = MUZZLE + .0420;
  g.add(loft(THREE, id + '_front_base', span(fz - .0090, fz + .0090, .0006).map((z) => {
    const t = Math.abs(z - fz) / .0090, w = .0100 - .0016 * t * t;
    return { z, pts: [[w, BORE + .0100], [w - .0016, BORE + .0128], [-(w - .0016), BORE + .0128], [-w, BORE + .0100]] };
  }), M.hero_park_dark, { creaseAngle: 28 }));
  g.add(loft(THREE, id + '_front_post', span(fz - .0012, fz + .0012, .0004).map((z) => ({
    z, pts: [[.0013, BORE + .0126], [.0013, BORE + .0212], [-.0013, BORE + .0212], [-.0013, BORE + .0126]],
  })), M.hero_park_dark, { creaseAngle: 24, wear: (x, y) => smooth(BORE + .0195, BORE + .0212, y) * .7 }));
  for (const s of [-1, 1]) {
    g.add(sweep(THREE, `${id}_front_wing_${s > 0 ? 'r' : 'l'}`, [
      [s * .0088, BORE + .0104, fz + .0074], [s * .0092, BORE + .0192, fz + .0026],
      [s * .0092, BORE + .0206, fz - .0030], [s * .0088, BORE + .0108, fz - .0078],
    ], () => [.0015, .0021], M.hero_park_dark, { radial: 14, samples: 40, up: [0, 1, 0], wear: () => .3 }));
  }
  // Tritium dot in the front post, so the sight picture is legible in the dark.
  g.add(revolve(THREE, id + '_front_dot', [[0, 0], [.0011, 0], [.0011, .0010], [0, .0010]],
    M.hero_tritium, { segs: 16, pos: [0, BORE + .0180, fz - .0013], rot: [0, Math.PI, 0] }));

  /* ── bolt: carrier face in the port, handle proud on the right ───────── */
  bolt.add(revolve(THREE, id + '_bolt_face', [
    [0, 0], [.0104, 0], [.0104, .0044], [.0098, .0052], [.0098, .0300], [0, .0300],
  ], M.hero_steel_bright, { segs: 36, pos: [0, BORE, -.0330], wear: (r, z) => (z < .0010 ? .5 : .15) }));
  bolt.add(loft(THREE, id + '_bolt_carrier', span(-.0340, .0100, .0007).map((z) => {
    const t = (z + .0340) / .0440, hw = .0150 - .0010 * smooth(.7, 1, t);
    const yb = BORE - .0104, yt = BORE + .0118;
    return { z, pts: [[hw - .0018, yb], [hw, yb + .0022], [hw, yt - .0022], [hw - .0018, yt],
      [-(hw - .0018), yt], [-hw, yt - .0022], [-hw, yb + .0022], [-(hw - .0018), yb]] };
  }), M.hero_steel, { creaseAngle: 28, wear: (x) => smooth(.0138, .0150, Math.abs(x)) * .45 }));
  /* The handle is one part to the hand that pulls it — shaft and knob under one
     named group so reload code can travel `<id>_bolt_handle` and not two nodes
     that have to stay in step. */
  const handle = grp(id + '_bolt_handle');
  bolt.add(handle);
  handle.add(revolve(THREE, id + '_bolt_handle_shaft', [
    [0, 0], [.0032, 0], [.0032, .0126], [.0030, .0134], [0, .0134],
  ], M.hero_steel, { segs: 20, pos: [.0150, BORE + .0052, -.0300], rot: [0, Math.PI / 2, 0] }));
  handle.add(revolve(THREE, id + '_bolt_knob', Array.from({ length: 9 }, (_, i) => [i % 2 ? .0048 : .0056, .0004 + i * .0009])
    .concat([[.0048, .0086], [.0038, .0098], [0, .0098]]), M.hero_steel_bright,
    { segs: 28, pos: [.0278, BORE + .0052, -.0300], rot: [0, Math.PI / 2, 0], wear: (r) => smooth(.0050, .0056, r) * .6 }));

  /* ── controls ────────────────────────────────────────────────────────── */
  // Bolt release paddle on the right flank; safety cross-bolt through the guard.
  g.add(loft(THREE, id + '_bolt_release', span(-.0560, -.0400, .0006).map((z) => {
    const t = (z + .0560) / .0160, w = RHW + .0014 + .0020 * smooth(.15, .55, t) * (1 - smooth(.7, 1, t));
    const yc = BORE - .0056;
    return { z, pts: [[w, yc - .0046], [w, yc + .0046], [RHW - .0010, yc + .0052], [RHW - .0010, yc - .0052]] };
  }), M.hero_park_dark, { creaseAngle: 30, wear: (x) => smooth(RHW + .0020, RHW + .0034, x) * .55 }));
  g.add(revolve(THREE, id + '_safety', [
    [0, 0], [.0038, 0], [.0038, .0050], [.0032, .0058], [.0032, .0230], [.0040, .0238], [.0040, .0286], [0, .0286],
  ], M.hero_park_dark, { segs: 24, pos: [-.0128, RYB + .0055, -.0272], rot: [0, Math.PI / 2, 0], wear: (r, z) => (z > .0270 || z < .0006 ? .55 : .1) }));

  /* ── grip, guard, trigger ────────────────────────────────────────────── */
  // The grip's front strap and the guard's rear leg are one piece of polymer:
  // the guard roots INSIDE the grip mass rather than stopping short of it.
  g.add(sweep(THREE, id + '_grip', [
    [0, RYB + .0040, .0016], [0, .0130, .0180], [0, -.0140, .0330], [0, -.0420, .0470], [0, -.0580, .0556],
  ], (t, ang) => {
    const a = .0150 - .0018 * smooth(.1, .95, t) + .0014 * smooth(.86, 1, t);
    const b = .0182 - .0044 * smooth(.05, .7, t) + .0016 * smooth(.88, 1, t);
    const swell = .0007 * Math.pow(Math.abs(Math.cos(ang)), 3) * smooth(.20, .55, t) * (1 - smooth(.80, 1, t));
    return sup(ang, a + swell, b, 3.6);
  }, M.hero_stipple, { radial: 42, samples: 116, up: [1, 0, 0], vRep: 6,
    wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 4) * smooth(.15, .5, t) * .5 }));
  g.add(revolve(THREE, id + '_grip_cap', [[0, 0], [.0146, 0], [.0156, .0032], [.0156, .0066], [0, .0066]],
    M.hero_polymer, { segs: 30, pos: [0, -.0622, .0580], rot: [-1.12, 0, 0] }));
  /* Trigger guard. Interior 42 × 34 mm, bar 21 mm wide × 7 mm thick — a
     shotgun's guard is wide and flat-bottomed, and both legs bury themselves
     in the housing they grow out of. */
  g.add(sweep(THREE, id + '_trigger_guard', [
    [0, RYB + .0090, -.0690], [0, .0180, -.0730], [0, .0020, -.0712], [0, -.0092, -.0640],
    [0, -.0134, -.0540], [0, -.0132, -.0420], [0, -.0094, -.0330], [0, .0020, -.0264],
    [0, .0180, -.0220], [0, RYB + .0090, -.0150],
  ], (t, ang) => {
    const end = 1 - Math.sin(Math.PI * t) * .82;
    return sup(ang, .0092 + .0022 * end, .0034 + .0026 * end, 4);
  }, M.hero_polymer, { radial: 36, samples: 140, up: [1, 0, 0], vRep: 9,
    wear: (t) => (1 - smooth(.25, .45, t)) * 0 + Math.sin(Math.PI * t) * .18 }));
  g.add(sweep(THREE, id + '_trigger', [
    [0, .0470, -.0322], [0, .0300, -.0340], [0, .0140, -.0362], [0, .0010, -.0390], [0, -.0074, -.0414], [0, -.0104, -.0442],
  ], (t, ang) => {
    const a = .0030 + .0005 * smooth(.20, .60, t);
    const groove = .00035 * Math.max(0, 1 - Math.abs(((t * 6) % 1) * 2 - 1) * 2.4)
                 * smooth(.34, .48, t) * Math.max(0, -Math.cos(ang));
    return [a, .0021 + .0003 * smooth(.2, .7, t) - groove];
  }, M.hero_park_dark, { radial: 22, samples: 64, up: [1, 0, 0], vRep: 4,
    wear: (t, ang) => smooth(.45, .85, t) * Math.max(0, -Math.cos(ang)) * .5 }));
  g.add(revolve(THREE, id + '_trigger_pin', [[0, -.0104], [.0022, -.0104], [.0022, .0104], [0, .0104]],
    M.hero_steel, { segs: 16, pos: [0, .0432, -.0314], rot: [0, Math.PI / 2, 0] }));

  /* ── stock ───────────────────────────────────────────────────────────── */
  g.add(loft(THREE, id + '_stock', span(STK0, STK1, .0005).map(stockStation), M.hero_polymer,
    { creaseFrom: stockStation(.1300).pts, creaseAngle: 30,
      wear: (x, y) => smooth(.0176, .0198, Math.abs(x)) * .34 + smooth(RYT - .0020, RYT + .0020, y) * .3 }));
  g.add(loft(THREE, id + '_butt_pad', span(STK1, STK1 + .0140, .0005).map((z) => {
    const t = (z - STK1) / .0140;
    const hw = .0192 - .0030 * smooth(.5, 1, t);
    const yb = -.0250 + .0044 * smooth(.55, 1, t), yt = RYT + .0020 - .0040 * smooth(.55, 1, t);
    return { z, pts: [
      [hw - .0024, yb], [hw, yb + .0028], [hw, yt - .0030], [hw - .0026, yt],
      [-(hw - .0026), yt], [-hw, yt - .0030], [-hw, yb + .0028], [-(hw - .0024), yb],
    ] };
  }), M.hero_glove_pad, { creaseAngle: 30, wear: () => .14 }));
  for (const s of [-1, 1]) {
    g.add(revolve(THREE, `${id}_sling_loop_${s > 0 ? 'r' : 'l'}`, [[.0034, 0], [.0048, 0], [.0048, .0024], [.0034, .0024]],
      M.hero_park_dark, { segs: 22, pos: [s * .0160, -.0180, .1560], rot: [0, Math.PI / 2, 0] }));
  }

  /* ── spent hull + ejection ───────────────────────────────────────────────
     A FIRED 12-gauge hull: brass head with the rim and extractor groove, dyed
     plastic body, and an OPEN mouth where the crimp has blown out. Hidden as a
     template; the viewer clones it per shot. */
  const hull = grp(id + '_case');
  hull.add(revolve(THREE, id + '_case_head', [
    [0, 0], [.01020, 0], [.01020, .0030], [.00940, .0042], [.00940, .0148], [.00960, .0158], [0, .0158],
  ], M.hero_brass, { segs: 36, creaseAngle: 32, wear: (r, z) => (z < .0006 ? .45 : .12) }));
  hull.add(revolve(THREE, id + '_case_body', [
    [.00930, .0150], [.00930, .0620], [.00985, .0678], [.01000, .0700],
    [.00855, .0700], [.00850, .0640], [.00840, .0170],
  ], M.hero_hull, { segs: 36, creaseAngle: 34, wear: (r, z) => (z > .0620 ? .5 : .08) }));
  hull.visible = false;
  g.add(hull);

  /* ── loaded shell ─────────────────────────────────────────────────────
     This gun is tube-fed, so its reload is shells into the loading port, not a
     magazine swap — the prop a reload needs is a LOADED hull: same head, full
     length, and a folded six-point crimp closing the mouth instead of the blown
     one on `_case`. Pivot at the rim so code pushes it in along +Z. Hidden as a
     template; exported standalone for the off-hand to carry. */
  const shell = grp(id + '_shell');
  shell.add(revolve(THREE, id + '_shell_head', [
    [0, 0], [.01020, 0], [.01020, .0030], [.00940, .0042], [.00940, .0148], [.00960, .0158], [0, .0158],
  ], M.hero_brass, { segs: 36, creaseAngle: 32, wear: (r, z) => (z < .0006 ? .28 : .06) }));
  shell.add(revolve(THREE, id + '_shell_body', [
    [.00930, .0150], [.00930, .0560], [.00930, .0616],
    [.00880, .0640], [.00660, .0662], [.00230, .0676], [0, .0678],
  ], M.hero_hull, { segs: 36, creaseAngle: 34, wear: (r, z) => (z > .0616 ? .16 : .04) }));
  // The crimp: six folds cut into the dome as ridges, not a decal on it.
  for (let i = 0; i < 6; i++) {
    const a = i / 6 * Math.PI * 2;
    shell.add(loft(THREE, `${id}_shell_crimp_${i}`, span(.0618, .0676, .0006).map((z) => {
      const t = (z - .0618) / .0058, r = .0092 * (1 - .92 * t * t), w = .0011 * (1 - t);
      return { z, pts: [[-w, r - .0008], [w, r - .0008], [w, r + .0004], [-w, r + .0004]] };
    }), M.hero_hull, { creaseAngle: 30 }));
    shell.children[shell.children.length - 1].rotation.z = a;
  }
  shell.visible = false;
  g.add(shell);
  const ej = grp(id + '_eject', [RHW + .0020, BORE + .0020, -.0480]);
  ej.userData.eject = { dir: [.86, .44, .26], speed: 3.0, spin: 22 };
  g.add(ej);

  /* ── mounts ──────────────────────────────────────────────────────────── */
  const mount = (slot, pos, parent = g) => { const m = grp(`${id}_mount_${slot}`, pos); m.userData.slot = slot; parent.add(m); };
  mount('barrel', [0, BORE, RZ0]);
  mount('muzzle', [0, BORE, MUZZLE]);
  mount('optic', [0, RAIL, -.0300]);
  mount('magazine', [0, TUBE_Y, RZ0 - .3900]);
  // Where a shell is pushed in during a reload: the loading port under the
  // receiver, not the tube cap.
  mount('loadport', [0, BORE - .0150, -.0140]);
  mount('stock', [0, RYB + .0200, RZ1]);
  mount('underbarrel', [0, HG_CY - HG_HH, HG_Z0 + .0700]);
  mount('infusion', [-RHW - .0010, BORE - .0040, -.0300]);
  return g;
}
