/**
 * Deep Field 3D — hero Rifle ("bronze-anodised AR-pattern carbine").
 *
 * Built to the project's hero standard on ./gun-kit.js — see CLAUDE.md. Measured
 * against a real 14.5" carbine rather than eyeballed: 25.4 mm receiver width,
 * 368 mm barrel, 10.2 mm Picatinny slot pitch, 36 mm M-LOK pitch, STANAG
 * magazine section, 89 mm carrier travel.
 *
 * Everything that used to be a box glued to a slab is now cut IN:
 *   · Picatinny cross-slots milled through the rail's top rib, full length
 *   · M-LOK slots cut through four faces of the handguard, not printed on it
 *   · the ejection port as a real pocket with the bolt carrier standing inside
 *   · the brass deflector as a swelled boss in the receiver wall
 *   · the forward-assist boss blended into the receiver, not stuck to it
 *   · A2 birdcage ports cut through the flash hider's wall
 *   · magwell flare and stock lightening pockets as displacement
 *
 * Bronze keeps its identity; only the quality level rises.
 */
import {
  heroSurfaces, loft, sweep, revolve, engrave, repivot,
  clamp, smooth, lerp, boxMesh,
} from './gun-kit.js';

const BORE = .068;              // bore axis above the grip web
const RAIL = .1030;             // top rail surface — 35 mm over bore
const UHW = .0180;              // upper receiver half-width (36 mm)
const UZ0 = -.170, UZ1 = .056;  // receiver face / rear of upper
const HG_LEN = .330, HG_HW = .0186, HG_YS = 1.18, HG_YC = .0046;
const BBL = .372;               // 14.5"
const MUZZLE = UZ0 - BBL;

/* ── cut-in detail ─────────────────────────────────────────────────────── */

/** Picatinny cross-slots: 5.35 mm wide, 10.2 mm pitch, cut through the top rib. */
function railSlot(z) {
  const p = (z - .004) / .0102, f = p - Math.floor(p);
  const w = .00535 / .0102;
  return smooth(.5 - w / 2 - .04, .5 - w / 2, f) * (1 - smooth(.5 + w / 2, .5 + w / 2 + .04, f));
}
/** M-LOK: 32 mm long, 7 mm wide, 36 mm pitch, cut clean through the wall. */
function mlokCut(z) {
  const p = (z + .186) / .036, f = p - Math.floor(p);
  const w = .032 / .036;
  return smooth(.5 - w / 2 - .03, .5 - w / 2 + .02, f) * (1 - smooth(.5 + w / 2 - .02, .5 + w / 2 + .03, f));
}
/** Ejection port — right wall only, deep enough to expose the carrier. */
const portPocket = (z, y) =>
  .0050 * smooth(-.0860, -.0844, z) * (1 - smooth(-.0356, -.0340, z))
        * smooth(BORE - .0098, BORE - .0082, y) * (1 - smooth(BORE + .0082, BORE + .0098, y));
/** Brass deflector — a swelled boss aft of the port, in the wall not on it. */
const deflector = (z, y) =>
  -.0042 * smooth(-.0340, -.0250, z) * (1 - smooth(-.0110, -.0040, z))
         * smooth(BORE - .0090, BORE - .0030, y) * (1 - smooth(BORE + .0040, BORE + .0100, y));
/** Forward-assist boss — blended into the rear-right shoulder. */
const assistBoss = (z, y) =>
  -.0050 * smooth(-.0060, .0030, z) * (1 - smooth(.0120, .0210, z))
         * smooth(BORE + .0010, BORE + .0060, y) * (1 - smooth(BORE + .0130, BORE + .0180, y));

/* ── sections ──────────────────────────────────────────────────────────── */

/** Upper receiver: flat-top, vertical walls, mating shelf, all cuts applied. */
function upperStation(z) {
  const nose = smooth(UZ0, UZ0 + .0060, z);
  const hw = UHW - (1 - nose) * .0014;
  const yb = .0520 + (1 - nose) * .0016, yt = .0930;   // raceway above the carrier
  const rightX = (y) => hw - portPocket(z, y) - deflector(z, y) - assistBoss(z, y);
  const pts = [];
  pts.push([hw - .0012, yb], [hw, yb + .0016]);
  for (let i = 0; i < 14; i++) { const y = lerp(yb + .0034, yt - .0038, i / 13); pts.push([rightX(y), y]); }
  pts.push([hw - .0010, yt - .0012], [hw - .0030, yt]);
  // flat top, dished slightly either side of the rail footprint
  for (let i = 0; i < 7; i++) { const x = lerp(hw - .0038, -(hw - .0038), i / 6); pts.push([x, yt - .0004 * (1 - Math.abs(x) / (hw - .0038))]); }
  pts.push([-(hw - .0030), yt], [-(hw - .0010), yt - .0012]);
  for (let i = 0; i < 14; i++) { const y = lerp(yt - .0038, yb + .0034, i / 13); pts.push([-hw, y]); }
  pts.push([-hw, yb + .0016], [-(hw - .0012), yb]);
  // underside: the takedown shelf the lower mates to
  pts.push([-.0112, yb], [-.0112, yb - .0022], [.0112, yb - .0022], [.0112, yb]);
  return { z, pts };
}

/** Picatinny rail with its cross-slots cut through the rib. */
function railStation(z) {
  const cut = railSlot(z);
  const yb = RAIL - .0086, yt = RAIL - cut * .0032;
  const bw = .0105, tw = .0080;
  return { z, pts: [
    [bw, yb], [bw, yb + .0030], [tw + .0012, yb + .0046], [tw, yb + .0058], [tw, yt - .0006], [tw - .0010, yt],
    [-(tw - .0010), yt], [-tw, yt - .0006], [-tw, yb + .0058], [-(tw + .0012), yb + .0046], [-bw, yb + .0030], [-bw, yb],
  ] };
}

/** Free-float handguard: octagonal tube with M-LOK cut through four faces. */
const OCT = (() => {
  const k = .4142, c = [[1, k], [k, 1], [-k, 1], [-1, k], [-1, -k], [-k, -1], [k, -1], [1, -k]];
  return c.map((p, i) => [p, c[(i + 1) % 8]]);
})();
const MLOK_FACES = new Set([3, 5, 7]);           // 9, 6 and 3 o'clock, as on a real rail
function hgStation(z) {
  const cut = mlokCut(z), taper = 1 - smooth(UZ0 - HG_LEN + .010, UZ0 - HG_LEN, z) * .10;
  const R = HG_HW * taper, pts = [];
  for (const [f, [a, b]] of OCT.entries()) {
    for (let i = 0; i < 4; i++) {
      const t = i / 4;
      const x = lerp(a[0], b[0], t), y = lerp(a[1], b[1], t);
      // pull the middle of the face in only where the slot is, so the cut has
      // defined edges instead of dragging the whole facet with it
      const across = Math.abs(t - .5);
      const d = MLOK_FACES.has(f) ? cut * .0030 * (1 - smooth(.26, .34, across)) : 0;
      const s = (R - d) / R;
      pts.push([x * R * s, BORE + HG_YC + y * R * s * HG_YS]);
    }
  }
  return { z, pts };
}

/** Lower receiver: flared magwell, buffer tower, grip tang. */
const MAG_Z = -.1130, MAG_D = .0380, MAG_W = .0098;   // STANAG section: 38 × 19 mm
const WELL_MOUTH = -.0030;                            // STANAG inserts ~51 mm
function lowerStation(z) {
  // magwell: a real box around the magazine, flared at the mouth
  const well = smooth(MAG_Z - MAG_D / 2 - .0060, MAG_Z - MAG_D / 2 - .0010, z)
             * (1 - smooth(MAG_Z + MAG_D / 2 + .0010, MAG_Z + MAG_D / 2 + .0060, z));
  const yt = .0518, body = .0180;
  const yb = lerp(body, WELL_MOUTH, well);
  const flare = well * smooth(WELL_MOUTH + .0140, WELL_MOUTH + .0020, yb) * .0026;
  const w = .0180 - .0012 * (1 - well) + flare;
  // the two downward lugs the trigger guard pins between
  const lug = Math.max(
    smooth(-.0936, -.0904, z) * (1 - smooth(-.0836, -.0804, z)),
    smooth(-.0104, -.0072, z) * (1 - smooth(.0062, .0094, z)),
  );
  const yLug = yb - lug * .0212;
  return { z, pts: [
    [w - .0016, yb], [w, yb + .0020], [w, yt - .0020], [w - .0018, yt],
    [.0100, yt], [.0100, yt - .0020], [-.0100, yt - .0020], [-.0100, yt],
    [-(w - .0018), yt], [-w, yt - .0020], [-w, yb + .0020], [-(w - .0016), yb],
    [-.0042, yb], [-.0042, yLug], [.0042, yLug], [.0042, yb],
  ] };
}

/** STANAG 30-round magazine: real curve, ribbed flanks, witness slots. */
function magStation(t) {
  // 190 mm magazine: feed lips up at the carrier, body hanging 126 mm clear of
  // the mouth, on a real 25 mm curve. The last version was a bare rounded box,
  // which is why it read as a flat paddle — the detail that makes a magazine
  // legible is the longitudinal ribbing down the flanks, the witness slots and
  // the grip band, all of which are cut ACROSS the section, not along it.
  const y = lerp(.0480, -.1400, t);
  const zc = MAG_Z - .0255 * t * t;
  const w = MAG_W, d = MAG_D / 2;
  const wit = .0013 * smooth(.44, .47, t) * (1 - smooth(.92, .95, t))
            * Math.max(0, 1 - Math.abs(((t * 6) % 1) * 2 - 1) * 1.9);
  const grip = .0009 * smooth(.60, .66, t) * (1 - smooth(.88, .93, t));
  const pts = [];
  const N = 56, n = 5.5;
  for (let i = 0; i < N; i++) {
    const a = i / N * Math.PI * 2, ca = Math.cos(a), sa = Math.sin(a);
    const rad = 1 / Math.pow(Math.pow(Math.abs(ca / w), n) + Math.pow(Math.abs(sa / d), n), 1 / n);
    let x = ca * rad; const z = sa * rad;
    const flank = Math.pow(Math.min(1, Math.abs(x) / w), 6);      // only the wide faces
    const u = (z / d + 1) / 2;
    const rib = .0013 * Math.pow(Math.abs(Math.sin(u * Math.PI * 3.5)), 4);
    x -= Math.sign(x) * flank * (rib + wit + grip);
    pts.push([x, zc + z]);
  }
  return { y, pts };
}

/** A2 birdcage: five ports cut clean through the wall, closed at the bottom. */
function hiderStation(z) {
  const L0 = MUZZLE - .0560;
  const t = (z - L0) / .0560;
  const open = smooth(.24, .30, t) * (1 - smooth(.86, .92, t));
  const R = .0110, r = .0072;
  const pts = [];
  const n = 28;
  for (let i = 0; i < n; i++) {
    const a = i / n * Math.PI * 2;
    // five slots around the top 300°; the bottom stays solid (that is the point)
    const slot = Math.max(0, 1 - Math.abs(((a / (Math.PI * 2) * 5 + .5) % 1) * 2 - 1) * 2.6);
    const bottom = smooth(-.35, .10, Math.sin(a));
    const d = open * slot * bottom * (R - r);
    pts.push([Math.cos(a) * (R - d), BORE + Math.sin(a) * (R - d)]);
  }
  for (let i = n - 1; i >= 0; i--) {
    const a = i / n * Math.PI * 2;
    pts.push([Math.cos(a) * r, BORE + Math.sin(a) * r]);
  }
  return { z, pts };
}

/** Collapsible stock: cheek rib, sling loop, lightening pockets cut in. */
/* Collapsible carbine stock. The previous one was 53 mm tall — a stubby bar,
   which is why it read as a bar of soap. A real one is a 108 mm buttplate
   hanging below and behind the buffer tube, tapering forward to a collar that
   wraps the tube, with a cheek ridge along the top, a sling slot cut through
   the side, and a release lever underneath. */
const STK0 = .0660, STK1 = .2010;
function stockStation(z) {
  const t = clamp((z - STK0) / (STK1 - STK0), 0, 1);
  const hw = .0188 + .0022 * smooth(.38, .92, t);
  const yt = lerp(.0840, .0884, smooth(.10, .80, t));               // shell over the tube
  const yb = lerp(.0374, -.0230, smooth(.34, .98, t));              // toe drops late, not gradually
  const cheek = .0030 * smooth(.20, .42, t);                        // comb along the top
  // sling slot cut through the flank, and the lightening scallop behind it
  const slot = .0042 * smooth(.16, .21, t) * (1 - smooth(.33, .38, t));
  const scallop = .0028 * smooth(.46, .56, t) * (1 - smooth(.80, .88, t));
  const face = (y) => {
    const v = (y - yb) / (yt - yb);
    return hw - slot * smooth(.52, .62, v) * (1 - smooth(.80, .88, v))
              - scallop * smooth(.18, .34, v) * (1 - smooth(.66, .82, v));
  };
  const pts = [];
  pts.push([hw - .0022, yb], [hw, yb + .0026]);
  for (let i = 0; i < 12; i++) { const y = lerp(yb + .0050, yt - .0046, i / 11); pts.push([face(y), y]); }
  pts.push([hw - .0020, yt - .0016], [hw - .0048, yt]);
  pts.push([.0072, yt + cheek], [-.0072, yt + cheek]);
  pts.push([-(hw - .0048), yt], [-(hw - .0020), yt - .0016]);
  for (let i = 0; i < 12; i++) { const y = lerp(yt - .0046, yb + .0050, i / 11); pts.push([-face(y), y]); }
  pts.push([-hw, yb + .0026], [-(hw - .0022), yb]);
  return { z, pts };
}

const span = (a, b, step) => { const o = []; for (let v = a; v <= b; v += step) o.push(v); return o; };

export function buildRifleHero(K) {
  const { THREE, mats, grp } = K;
  heroSurfaces(THREE, mats);
  const id = 'rifle', g = grp(id), M = mats;
  const BRZ = M.hero_bronze;

  /* ── reciprocating group: bolt carrier + charging handle ─────────────── */
  const bcg = grp(id + '_bolt_carrier');
  bcg.userData.recoil = { axis: [0, 0, 1], travel: .0889 };   // 3.5", real carrier travel
  g.add(bcg);
  bcg.add(revolve(THREE, id + '_carrier', [
    [0, -.0900], [.0088, -.0900], [.0088, -.0180], [.0076, -.0160], [.0076, .0180], [.0088, .0200], [.0088, .0420], [0, .0420],
  ], M.hero_steel, { segs: 40, pos: [0, BORE, 0], wear: (r, z) => (z < -.0700 ? .55 : .2) }));
  bcg.add(revolve(THREE, id + '_bolt_face', [[0, -.0906], [.0062, -.0906], [.0062, -.0898], [0, -.0898]], M.hero_steel_bright, { segs: 28, pos: [0, BORE, 0] }));
  /* Charging handle. The last one sat at y .0795 — inside the receiver, where
     it could never be seen. A real one rides a channel in the top-rear of the
     upper and its T emerges BEHIND the receiver face, above the buffer tube:
     a 40 mm bar with two wings, ridged on the pull faces, latch on the left. */
  bcg.add(sweep(THREE, id + '_charging_shaft', [[0, .0806, .0180], [0, .0806, .0400], [0, .0808, .0564]],
    () => [.0062, .0026], M.hero_bronze_dark, { radial: 16, up: [1, 0, 0], vRep: 3 }));
  const chT = [];
  for (let z = .0566; z <= .0662; z += .0004) {
    const t = (z - .0566) / .0096;
    const hw = .0202 - .0026 * smooth(.62, 1, t);
    const yb = .0752, yt = .0850 - .0014 * smooth(.66, 1, t);
    // ridged pull faces on the wings — displacement, not glued-on bars
    const ridge = .0007 * Math.max(0, 1 - Math.abs(((t * 4.5) % 1) * 2 - 1) * 2.2) * smooth(.18, .30, t);
    const wing = (y) => {
      const v = (y - yb) / (yt - yb);
      return yb + v * (yt - yb);
    };
    const pts = [];
    pts.push([hw - .0012, yb + ridge], [hw, yb + .0016]);
    for (let i = 0; i < 5; i++) { const y = wing(lerp(yb + .0028, yt - .0026, i / 4)); pts.push([hw - ridge, y]); }
    pts.push([hw - .0012, yt - .0010], [.0074, yt], [.0074, yt + .0032], [-.0074, yt + .0032], [-.0074, yt], [-(hw - .0012), yt - .0010]);
    for (let i = 0; i < 5; i++) { const y = wing(lerp(yt - .0026, yb + .0028, i / 4)); pts.push([-(hw - ridge), y]); }
    pts.push([-hw, yb + .0016], [-(hw - .0012), yb + ridge]);
    chT.push({ z, pts });
  }
  bcg.add(loft(THREE, id + '_charging_handle', chT, M.hero_bronze_dark,
    { creaseAngle: 26, wear: (x) => smooth(.0150, .0200, Math.abs(x)) * .5 }));
  // Left-side latch lever.
  bcg.add(sweep(THREE, id + '_charging_latch', [[-.0120, .0800, .0600], [-.0186, .0800, .0606], [-.0232, .0798, .0618]],
    (t) => [.0026 - .0006 * t, .0042 - .0010 * t], M.hero_bronze_dark, { radial: 14, up: [0, 1, 0], wear: () => .4 }));

  /* ── upper receiver, rail, controls ──────────────────────────────────── */
  g.add(loft(THREE, id + '_upper', span(UZ0, UZ1, .0006).map(upperStation), BRZ,
    { creaseFrom: upperStation(-.120).pts, creaseAngle: 26,
      wear: (x, y) => smooth(.0888, .0928, y) * .42 + smooth(UHW - .0020, UHW - .0002, Math.abs(x)) * .22 }));
  g.add(loft(THREE, id + '_top_rail', span(UZ0 - HG_LEN, UZ1 - .0020, .00035).map(railStation), BRZ,
    { creaseFrom: railStation(-.030).pts, creaseAngle: 24, wear: (x, y) => (y > RAIL - .0008 ? .45 : 0) }));
  g.add(revolve(THREE, id + '_forward_assist', [[0, 0], [.0056, 0], [.0056, .0026], [.0046, .0034], [0, .0034]], M.hero_bronze_dark,
    { segs: 24, pos: [UHW - .0046, BORE + .0092, .0068], rot: [0, Math.PI / 2, 0], wear: () => .3 }));
  g.add(revolve(THREE, id + '_port_cover_pin', [[0, -.0230], [.0016, -.0230], [.0016, .0230], [0, .0230]], M.hero_steel,
    { segs: 16, pos: [UHW - .0006, BORE - .0104, -.0600] }));

  /* ── lower receiver, grip, guard, controls ───────────────────────────── */
  g.add(loft(THREE, id + '_lower', span(-.160, .052, .0006).map(lowerStation), BRZ,
    { creaseFrom: lowerStation(-.020).pts, creaseAngle: 26,
      wear: (x, y) => (1 - smooth(WELL_MOUTH, WELL_MOUTH + .0055, y)) * .58 + smooth(.0150, .0186, y) * .2 }));
  // A-pattern grip: swept, raked 22°, with a palm shelf and texture panels.
  g.add(sweep(THREE, id + '_grip', [
    [0, .0190, .0034], [0, -.0060, .0186], [0, -.0320, .0328], [0, -.0560, .0458], [0, -.0700, .0538],
  ], (t, ang) => {
    const a = .0150 - .0020 * smooth(.55, 1, t), b = .0195 - .0040 * smooth(.5, 1, t) + .0022 * Math.sin(Math.PI * t);
    const n = 3.4;
    return 1 / Math.pow(Math.pow(Math.abs(Math.cos(ang) / a), n) + Math.pow(Math.abs(Math.sin(ang) / b), n), 1 / n);
  }, M.hero_polymer, { radial: 34, samples: 88, up: [1, 0, 0], vRep: 6 }));
  g.add(revolve(THREE, id + '_grip_cap', [[0, 0], [.0130, 0], [.0140, .0030], [.0140, .0062], [0, .0062]], M.hero_polymer,
    { segs: 28, pos: [0, -.0742, .0570], rot: [-1.16, 0, 0] }));
  /* Trigger guard. An AR's is nothing like a pistol bow: it is a slim bar,
     ~8.4 mm wide and 6 mm thick, pinned between two downward lugs on the
     receiver — front lug just behind the magwell, rear lug at the grip tang —
     with a near-flat bottom run of about 76 mm between them. Both ends root up
     INSIDE their lug so the joint is buried. */
  g.add(sweep(THREE, id + '_trigger_guard', [
    [0, .0110, -.0880], [0, -.0030, -.0862], [0, -.0108, -.0800], [0, -.0132, -.0700],
    [0, -.0136, -.0520], [0, -.0132, -.0340], [0, -.0108, -.0200], [0, -.0030, -.0080],
    [0, .0110, -.0020],
  ], (t, ang) => {
    const end = 1 - Math.sin(Math.PI * t) * .85;          // thickens into the lugs
    const a = .0042 + .0016 * end, b = .0030 + .0022 * end, n = 4;
    return 1 / Math.pow(Math.pow(Math.abs(Math.cos(ang) / a), n) + Math.pow(Math.abs(Math.sin(ang) / b), n), 1 / n);
  }, BRZ, { radial: 30, samples: 108, up: [1, 0, 0], vRep: 10, wear: (x, y) => (1 - smooth(-.0136, -.0100, y)) * .5 }));
  // Guard roll pin at the front lug and detent at the rear — the real fixings.
  g.add(revolve(THREE, id + '_guard_pin', [[0, -.0046], [.0018, -.0046], [.0018, .0046], [0, .0046]], M.hero_steel,
    { segs: 16, pos: [0, -.0010, -.0868], rot: [0, Math.PI / 2, 0] }));
  // Trigger: flat-faced blade with a bowing shoe, same treatment as the Sidearm.
  const tri = [];
  for (let i = 0; i <= 70; i++) {
    const t = i / 70, y = lerp(.0200, -.0110, t);
    const a = .0028 + .0005 * smooth(.18, .55, t);
    const zc = -.0424 - .0026 * smooth(.10, 1, t), d = .0019 + .0002 * smooth(.2, .7, t);
    const z0 = zc - d, z1 = zc + d;
    tri.push({ z: -y, pts: [
      [a - .0006, z0], [.0018, z0], [.0018, z0 + .0005], [.0012, z0 + .0005],
      [.0012, z0 - .0002], [-.0012, z0 - .0002], [-.0012, z0 + .0005], [-.0018, z0 + .0005], [-.0018, z0],
      [-(a - .0006), z0], [-a, z0 + .0006], [-a, z1 - .0006], [-(a - .0006), z1],
      [a - .0006, z1], [a, z1 - .0006], [a, z0 + .0006],
    ] });
  }
  const trigger = loft(THREE, id + '_trigger', tri, M.hero_bronze_dark, { creaseAngle: 26, wear: (x) => (Math.abs(x) < .0012 ? .42 : .24) });
  trigger.geometry.rotateX(Math.PI / 2);
  g.add(trigger);
  const ctl = (name, pts, pos, mat = M.hero_bronze_dark, wear = .26) =>
    g.add(revolve(THREE, id + '_' + name, pts, mat, { segs: 28, pos, rot: [0, Math.PI / 2, 0], wear: () => wear }));
  ctl('mag_release', [[0, 0], [.0050, 0], [.0050, .0028], [.0042, .0036], [0, .0036]], [UHW + .0026, .0020, -.0800]);
  ctl('bolt_catch', [[0, 0], [.0030, 0], [.0030, .0022], [0, .0022]], [-.0166, .0180, -.0790]);
  ctl('takedown_front', [[0, 0], [.0036, 0], [.0036, .0020], [0, .0020]], [-.0162, .0330, -.1500]);
  ctl('takedown_rear', [[0, 0], [.0036, 0], [.0036, .0020], [0, .0020]], [-.0162, .0330, .0340]);
  // Bilateral selector: a real lever each side on a common hub.
  for (const sd of [-1, 1]) {
    g.add(revolve(THREE, `${id}_selector_hub${sd > 0 ? 'r' : 'l'}`, [[0, 0], [.0058, 0], [.0058, .0026], [0, .0026]], M.hero_bronze_dark,
      { segs: 24, pos: [sd * (UHW + .0030), .0230, .0020], rot: [0, sd > 0 ? Math.PI / 2 : -Math.PI / 2, 0], wear: () => .3 }));
    g.add(sweep(THREE, `${id}_selector${sd > 0 ? 'r' : 'l'}`,
      [[sd * (UHW + .0044), .0230, .0010], [sd * (UHW + .0044), .0212, -.0070], [sd * (UHW + .0044), .0180, -.0132]],
      (t) => [.0022, .0044 - .0012 * t], M.hero_bronze_dark, { radial: 14, up: [0, 1, 0], wear: () => .34 }));
  }
  g.add(engrave(THREE, id + '_rollmark', 'DEEP FIELD', M.hero_engrave,
    { plane: 'left', x: -.0160, z: -.0930, y: .0140, cap: .0028, stroke: .00026, depth: .00024 }));

  /* ── handguard + barrel ──────────────────────────────────────────────── */
  g.add(loft(THREE, id + '_handguard', span(UZ0 - HG_LEN, UZ0 - .0020, .00060).map(hgStation), BRZ,
    { creaseFrom: hgStation(UZ0 - .060).pts, creaseAngle: 30,
      wear: (x, y) => smooth(HG_HW * .93, HG_HW * .995, Math.hypot(x, (y - BORE - HG_YC) / HG_YS)) * .46 }));
  g.add(revolve(THREE, id + '_barrel_nut', Array.from({ length: 15 }, (_, i) => [i % 2 ? .0182 : .0196, UZ0 - .0010 - i * .0009])
    .concat([[.0196, UZ0 - .0136], [0, UZ0 - .0136]]).reverse(), M.hero_bronze_dark, { segs: 40, pos: [0, BORE, 0] }));
  g.add(revolve(THREE, id + '_barrel', [
    [0, MUZZLE], [.0072, MUZZLE], [.0076, MUZZLE + .0008], [.0076, MUZZLE + .0140],
    [.0082, MUZZLE + .0150], [.0082, UZ0 - .2120], [.0098, UZ0 - .2100], [.0098, UZ0 - .1960],
    [.0086, UZ0 - .1940], [.0086, UZ0 - .0400], [.0112, UZ0 - .0380], [.0112, UZ0], [0, UZ0],
  ], M.hero_nitride, { segs: 48, pos: [0, BORE, 0], wear: (r, z) => (z < MUZZLE + .0030 ? .8 : 0) }));
  g.add(revolve(THREE, id + '_bore', [[0, MUZZLE - .0004], [.00281, MUZZLE - .0004], [.00281, MUZZLE + .0120], [0, MUZZLE + .0120]],
    M.hero_engrave, { segs: 32, pos: [0, BORE, 0] }));
  // Low-profile gas block and tube.
  g.add(loft(THREE, id + '_gas_block', span(UZ0 - .2180, UZ0 - .1900, .0005).map((z) => {
    const hw = .0112, yb = BORE - .0100, yt = BORE + .0138;
    return { z, pts: [[hw - .0014, yb], [hw, yb + .0016], [hw, yt - .0030], [hw - .0026, yt], [-(hw - .0026), yt], [-hw, yt - .0030], [-hw, yb + .0016], [-(hw - .0014), yb]] };
  }), M.hero_nitride, { creaseAngle: 28 }));
  g.add(revolve(THREE, id + '_gas_tube', [[0, UZ0 - .1900], [.0024, UZ0 - .1900], [.0024, UZ0 - .0130], [0, UZ0 - .0130]],
    M.hero_steel, { segs: 20, pos: [0, BORE + .0112, 0] }));
  // A2 birdcage with the ports cut through.
  g.add(loft(THREE, id + '_flash_hider', span(MUZZLE - .0560, MUZZLE - .0002, .00035).map(hiderStation), M.hero_nitride,
    { creaseAngle: 34, wear: (x, y, z) => (z < MUZZLE - .0540 ? .6 : .15) }));

  /* ── magazine ────────────────────────────────────────────────────────── */
  const mag = grp(id + '_magazine');
  const ms = [];
  for (let i = 0; i <= 110; i++) { const st = magStation(i / 110); ms.push({ z: -st.y, pts: st.pts }); }
  const magBody = loft(THREE, id + '_mag_body', ms, M.hero_polymer, { creaseAngle: 34, wear: (x) => smooth(MAG_W * .86, MAG_W * .99, Math.abs(x)) * .3 });
  magBody.geometry.rotateX(Math.PI / 2);
  mag.add(magBody);
  const fp = [];
  for (let i = 0; i <= 16; i++) {
    const s = i / 16, yy = lerp(-.1372, -.1536, s);
    const fw = MAG_W + .0024 + .0018 * smooth(.12, .5, s) - .0034 * smooth(.78, 1, s);
    const fd = MAG_D / 2 + .0026 + .0020 * smooth(.12, .5, s) - .0038 * smooth(.78, 1, s);
    const pts = [], N = 40, n = 5;
    for (let j = 0; j < N; j++) {
      const a = j / N * Math.PI * 2, ca = Math.cos(a), sa = Math.sin(a);
      const rad = 1 / Math.pow(Math.pow(Math.abs(ca / fw), n) + Math.pow(Math.abs(sa / fd), n), 1 / n);
      pts.push([ca * rad, MAG_Z - .0255 + sa * rad]);
    }
    fp.push({ z: -yy, pts });
  }
  const floor = loft(THREE, id + '_mag_floorplate', fp, M.hero_polymer, { creaseAngle: 32, wear: () => .3 });
  floor.geometry.rotateX(Math.PI / 2);
  mag.add(floor);
  mag.add(revolve(THREE, id + '_top_round', [
    [0, -.0230], [.0028, -.0220], [.0046, -.0170], [.0054, -.0090], [.0056, .0130], [.0058, .0140], [.0058, .0180], [0, .0180],
  ], M.hero_brass, { segs: 32, pos: [0, .0470, MAG_Z - .0060], rot: [Math.PI / 2 - .04, 0, 0] }));
  /* Pivot at the magwell mouth plane (t ≈ .271 on the curve, so 1.9 mm forward
     of MAG_Z) rather than the weapon origin — see gun-kit repivot(). */
  repivot(mag, [0, WELL_MOUTH, MAG_Z - .0019]);
  mag.userData.drop = { axis: [0, -1, 0], tip: [1, 0, 0], clear: .0180 };
  g.add(mag);

  /* ── buffer tube + stock ─────────────────────────────────────────────── */
  g.add(revolve(THREE, id + '_castle_nut', Array.from({ length: 11 }, (_, i) => [i % 2 ? .0168 : .0184, .0560 + i * .0008])
    .concat([[.0184, .0648], [0, .0648]]), M.hero_bronze_dark, { segs: 36, pos: [0, BORE - .0020, 0] }));
  g.add(revolve(THREE, id + '_buffer_tube', [
    [0, .0600], [.0146, .0600], [.0146, .2120], [.0132, .2140], [0, .2140],
  ], M.hero_bronze_dark, { segs: 36, pos: [0, BORE - .0020, 0] }));
  for (let i = 0; i < 6; i++) g.add(revolve(THREE, `${id}_stock_notch${i}`, [[.0146, 0], [.0152, .0010], [.0146, .0020]],
    M.hero_bronze_dark, { segs: 28, pos: [0, BORE - .0020, .0900 + i * .0190] }));
  g.add(loft(THREE, id + '_stock', span(STK0, STK1, .0006).map(stockStation), M.hero_polymer,
    { creaseFrom: stockStation(.130).pts, creaseAngle: 30,
      wear: (x, y) => smooth(.0186, .0208, Math.abs(x)) * .34 + smooth(.0840, .0880, y) * .3 }));
  // Release lever under the collar — the part you actually squeeze to adjust.
  g.add(sweep(THREE, id + '_stock_lever', [[0, .0424, .0780], [0, .0388, .0920], [0, .0394, .1060]],
    (t) => [.0056, .0044 - .0012 * Math.abs(t - .5) * 2], M.hero_polymer, { radial: 18, up: [1, 0, 0], wear: () => .3 }));
  g.add(loft(THREE, id + '_butt_pad', span(STK1, STK1 + .0130, .0005).map((z) => {
    const t = (z - STK1) / .0130;
    const hw = .0212 - .0032 * smooth(.5, 1, t);
    const yb = -.0230 + .0042 * smooth(.55, 1, t), yt = .0884 - .0038 * smooth(.55, 1, t);
    return { z, pts: [
      [hw - .0024, yb], [hw, yb + .0028], [hw, yt - .0030], [hw - .0026, yt],
      [-(hw - .0026), yt], [-hw, yt - .0030], [-hw, yb + .0028], [-(hw - .0024), yb],
    ] };
  }), M.hero_glove_pad, { creaseAngle: 30, wear: () => .12 }));
  g.add(revolve(THREE, id + '_sling_loop', [[.0030, 0], [.0042, 0], [.0042, .0022], [.0030, .0022]], M.hero_bronze_dark,
    { segs: 24, pos: [-.0150, -.0300, .1480], rot: [0, Math.PI / 2, 0] }));

  /* ── spent case + ejection ───────────────────────────────────────────── */
  const spent = revolve(THREE, id + '_case', [
    [0, 0], [.00480, 0], [.00480, .0012], [.00420, .0019], [.00420, .0032], [.00475, .0042],
    [.00470, .0290], [.00330, .0350], [.00295, .0400], [.00285, .0447],
    [.00235, .0447], [.00245, .0400], [.00280, .0350], [.00420, .0290], [.00420, .0060], [.00250, .0040], [0, .0036],
  ], M.hero_brass, { segs: 40, creaseAngle: 34, wear: (r, z) => (z > .0420 ? .5 : .12) });
  spent.visible = false;
  g.add(spent);
  const ej = grp(id + '_eject', [UHW + .0010, BORE + .0012, -.0560]);
  ej.userData.eject = { dir: [.82, .50, .28], speed: 3.4, spin: 34 };
  g.add(ej);

  /* ── mounts ──────────────────────────────────────────────────────────── */
  const mount = (slot, pos, parent = g) => { const m = grp(`${id}_mount_${slot}`, pos); m.userData.slot = slot; parent.add(m); };
  mount('barrel', [0, BORE, UZ0]);
  mount('muzzle', [0, BORE, MUZZLE - .0560]);
  mount('optic', [0, RAIL, -.0120]);
  mount('magazine', [0, WELL_MOUTH, MAG_Z]);
  mount('magwell', [0, WELL_MOUTH, MAG_Z - .0019]);
  mount('stock', [0, BORE - .0020, .0600]);
  mount('underbarrel', [0, BORE - HG_HW, UZ0 - .1600]);
  mount('infusion', [-UHW - .0010, BORE, -.0700]);
  return g;
}
