/**
 * Deep Field 3D — hero Sidearm ("BODIED GUARD 3.0").
 *
 * Reference asset for the project's hero standard: a 154 × 21.8 mm striker-fired
 * micro-compact, DLC-black slide over a graphite polymer frame — two blacks
 * separated by surface, not by value. Serrations, the ejection pocket, the optic
 * cut and the magazine's witness holes are all cut INTO the geometry; the sight
 * picture actually lines up and the trigger actually has finger room inside its
 * guard. Modelling vocabulary lives in ./gun-kit.js; the rules are in CLAUDE.md.
 */
import {
  heroSurfaces, loft, sweep, revolve, engrave, repivot,
  clamp, smooth, lerp, boxMesh,
} from './gun-kit.js';

const BORE = .0545;         // bore axis height above the grip web
const HW = .0126;           // slide half-width (25.2 mm — correct for the class)
const SZ0 = -.1140, SZ1 = .0400;   // slide nose / breech end — 154 mm
const SY0 = .0442, SY1 = .0660;    // slide bottom / top — 21.8 mm
const RAKE = 17 * Math.PI / 180;

/* Cut-in detail, expressed as surface displacement — the whole point of the
   rebuild. Positive values move material inward. */
const SERR_F = [-.0870, -.0601], SERR_R = [-.0098, .0290], PITCH = .00235, SDEPTH = .00072;
const RAKE_K = .29;   // forward lean of the serration lines

function serration(z, y) {
  const inF = z > SERR_F[0] && z < SERR_F[1], inR = z > SERR_R[0] && z < SERR_R[1];
  if (!inF && !inR) return 0;
  const band = smooth(SY0 + .0012, SY0 + .0026, y) * (1 - smooth(SY1 - .0054, SY1 - .0040, y));
  if (band <= 0) return 0;
  const edge = inF ? Math.min(smooth(SERR_F[0], SERR_F[0] + .0016, z), 1 - smooth(SERR_F[1] - .0016, SERR_F[1], z))
    : Math.min(smooth(SERR_R[0], SERR_R[0] + .0016, z), 1 - smooth(SERR_R[1] - .0016, SERR_R[1], z));
  const p = (z + RAKE_K * (y - SY0)) / PITCH, f = p - Math.floor(p);
  const v = Math.max(0, 1 - Math.abs(2 * f - 1) * 1.55);      // V groove with a land between
  return SDEPTH * v * band * edge;
}
const serrPeak = (z, y) => {
  const inF = z > SERR_F[0] && z < SERR_F[1], inR = z > SERR_R[0] && z < SERR_R[1];
  if (!inF && !inR) return 0;
  const p = (z + RAKE_K * (y - SY0)) / PITCH, f = p - Math.floor(p);
  return Math.max(0, 1 - Math.abs(2 * f - 1) * 1.55) < .05 ? 1 : 0;   // the land, not the groove
};
/** Milled ejection pocket — right flat only. Deep enough to expose the barrel
 *  hood and the breech face standing inside it. */
function portPocket(z, y) {
  const a = smooth(-.0531, -.0521, z) * (1 - smooth(-.0237, -.0221, z));
  const b = smooth(SY0 + .0058, SY0 + .0070, y) * (1 - smooth(SY1 - .0048, SY1 - .0036, y));
  return .0044 * a * b;
}
/** Optic cut in the top of the slide. */
const opticCut = (z, x) => .0024 * smooth(.0020, .0036, z) * (1 - smooth(.0249, .0265, z)) * (1 - smooth(.0074, .0090, Math.abs(x)));

/** Slide cross-section at z. Fixed vertex count so it can be lofted. */
function slideStation(z) {
  const nose = smooth(SZ0, SZ0 + .0038, z);                     // short, crisp muzzle chamfer
  const tail = 1 - smooth(SZ1 - .0060, SZ1, z) * .35;           // slight rear radius
  const hw = HW - (1 - nose) * .0018, yt = SY1 - (1 - nose) * .0016 - (1 - tail) * .0006, yb = SY0 + (1 - nose) * .0010;
  const pts = [];
  const rightX = (y) => hw - serration(z, y) - portPocket(z, y);
  const leftX = (y) => -(hw - serration(z, y));
  const FLAT = 22;                                              // dense enough for a clean diagonal groove
  // bottom-right corner → up the right flat (CCW)
  pts.push([hw - .0009, yb], [hw, yb + .0014]);
  for (let i = 0; i < FLAT; i++) { const y = lerp(yb + .0026, yt - .0040, i / (FLAT - 1)); pts.push([rightX(y), y]); }
  // top-right chamfer
  pts.push([hw - .0008, yt - .0030], [hw - .0026, yt - .0012], [hw - .0044, yt - .0002]);
  // crowned top, with the optic pocket cut into it
  for (let i = 0; i < 11; i++) {
    const x = lerp(hw - .0050, -(hw - .0050), i / 10);
    const dome = -Math.pow(Math.abs(x) / (hw - .0050), 2) * .00055;
    pts.push([x, yt + dome - opticCut(z, x)]);
  }
  // top-left chamfer
  pts.push([-(hw - .0044), yt - .0002], [-(hw - .0026), yt - .0012], [-(hw - .0008), yt - .0030]);
  // down the left flat
  for (let i = 0; i < FLAT; i++) { const y = lerp(yt - .0040, yb + .0026, i / (FLAT - 1)); pts.push([leftX(y), y]); }
  pts.push([-hw, yb + .0014], [-(hw - .0009), yb]);
  // underside: outer lip, slide-rail channel, central flat (barrel tunnel relief)
  pts.push([-(hw - .0020), yb], [-(hw - .0026), yb + .0016], [-.0072, yb + .0016], [-.0072, yb + .0006],
    [.0072, yb + .0006], [.0072, yb + .0016], [hw - .0026, yb + .0016], [hw - .0020, yb]);
  return { z, pts };
}

function slideStations() {
  const zs = [];
  const dense = (a, b) => { for (let z = a; z <= b; z += .00018) zs.push(z); };
  let z = SZ0;
  while (z < SERR_F[0] - .001) { zs.push(z); z += .0007; }
  dense(SERR_F[0] - .001, SERR_F[1] + .001);
  z = SERR_F[1] + .001;
  while (z < SERR_R[0] - .001) { zs.push(z); z += .0006; }
  dense(SERR_R[0] - .001, SERR_R[1] + .001);
  z = SERR_R[1] + .001;
  while (z < SZ1) { zs.push(z); z += .0006; }
  zs.push(SZ1);
  return zs.map(slideStation);
}

/** Frame cross-section at z — dust cover and receiver, graphite polymer. */
function frameStation(z) {
  const nose = smooth(-.1046, -.1005, z);
  const hw = .0139 - (1 - nose) * .0016;
  const yb = .0218 + (1 - nose) * .0022, yt = SY0 - .0006;
  const rail = smooth(-.0817, -.0801, z) * (1 - smooth(-.0507, -.0491, z));   // accessory rail zone
  const pts = [];
  pts.push([hw - .0018, yb], [hw, yb + .0020]);
  for (let i = 0; i < 6; i++) { const y = lerp(yb + .0044, yt - .0026, i / 5); pts.push([hw - .0004 * Math.sin(i / 5 * Math.PI), y]); }
  pts.push([hw - .0012, yt - .0008], [hw - .0030, yt]);
  pts.push([.0104, yt], [.0104, yt - .0018], [-.0104, yt - .0018], [-.0104, yt]);   // slide-rail shelf
  pts.push([-(hw - .0030), yt], [-(hw - .0012), yt - .0008]);
  for (let i = 0; i < 6; i++) { const y = lerp(yt - .0026, yb + .0044, i / 5); pts.push([-(hw - .0004 * Math.sin(i / 5 * Math.PI)), y]); }
  pts.push([-hw, yb + .0020], [-(hw - .0018), yb]);
  // underside with the picatinny-ish accessory rail blended in
  const rw = .0090, rd = .0034 * rail;
  pts.push([-rw, yb], [-rw, yb - rd], [-.0060, yb - rd - .0012 * rail], [.0060, yb - rd - .0012 * rail], [rw, yb - rd], [rw, yb]);
  return { z, pts };
}

/** Grip cross-section, built in the sweep's local frame (local +Z runs upward). */
function gripStation(t) {
  // t: 0 at the top of the grip, 1 at the baseplate
  const y = lerp(.0218, -.0440, t);
  const swell = Math.sin(Math.PI * clamp(t * 1.05, 0, 1)) ;
  const hw = .0128 + swell * .0022 - smooth(.86, 1, t) * .0010;
  const zOff = -y * Math.tan(RAKE);
  const front = -.0120 + zOff, back = .0340 + zOff + .0062 * swell;
  // A rounded rect in (x, z): right flat, back strap, left flat, front strap.
  const r = .0042, pts = [];
  const corner = (cx, cz, a0) => { for (let i = 0; i <= 4; i++) { const a = a0 + i / 4 * Math.PI / 2; pts.push([cx + Math.cos(a) * r, cz + Math.sin(a) * r]); } };
  corner(hw - r, back - r, 0);
  corner(-(hw - r), back - r, Math.PI / 2);
  corner(-(hw - r), front + r, Math.PI);
  corner(hw - r, front + r, -Math.PI / 2);
  return { t, y, pts };
}

export function buildSidearmHero(K) {
  const { THREE, mats, grp } = K;
  heroSurfaces(THREE, mats);
  const id = 'sidearm', g = grp(id);
  const M = mats;

  /* Everything that reciprocates when the gun fires lives under one node with
     its travel axis declared, so the engine (and the viewer) can drive the
     cycle without knowing which parts belong to the slide. The barrel stays on
     the frame — it unlocks and sits still while the slide runs over it, which
     is why the muzzle does not move and the chamber is exposed at full travel. */
  const sl = grp(id + '_slide_assembly');
  sl.userData.recoil = { axis: [0, 0, 1], travel: .0240 };   // 24 mm, correct for the class
  g.add(sl);

  /* ── slide ─────────────────────────────────────────────────────────── */
  const slide = loft(THREE, id + '_slide', slideStations(), M.hero_dlc, {
    creaseFrom: slideStation(-.098).pts, creaseAngle: 26,
    // burnish the serration lands, the muzzle end and the top rib
    wear: (x, y, z) => Math.max(
      serrPeak(z, y) * .55 * (Math.abs(Math.abs(x) - HW) < .0012 ? 1 : 0),
      (1 - smooth(SZ0, SZ0 + .008, z)) * .8,
      y > SY1 - .0014 && Math.abs(x) < .009 ? .25 : 0,
    ),
  });
  sl.add(slide);

  // Ejection pocket interior: breech face, chamber mouth, extractor claw, firing-pin hole.
  const breech = grp(id + '_breech');
  breech.add(boxMesh(THREE, id + '_breech_face', .0104, .0138, .0022, M.hero_steel, [.0042, BORE + .0008, -.0214]));
  breech.add(revolve(THREE, id + '_firing_pin_hole', [[0, 0], [.00085, 0], [.00085, .0016], [.00055, .0020], [0, .0020]], M.hero_engrave, { segs: 20, pos: [0, BORE + .0008, -.0206] }));
  // Extractor: a real hooked claw standing off the breech wall.
  const claw = new THREE.Group(); claw.name = id + '_extractor';
  claw.add(sweep(THREE, id + '_extractor_body', [[0, 0, -.0196], [0, 0, -.0139], [0, 0, -.0066], [0, 0, -.0017]],
    (t) => [.0011 + .0006 * (1 - t), .0022], M.hero_steel_bright, { radial: 12, up: [1, 0, 0] }));
  claw.add(boxMesh(THREE, id + '_extractor_hook', .0016, .0034, .0026, M.hero_steel_bright, [-.0006, -.0014, -.0203]));
  claw.position.set(.0098, BORE + .0016, 0);
  breech.add(claw);
  sl.add(breech);
  // Chamber mouth / barrel hood showing at the front of the pocket.
  g.add(revolve(THREE, id + '_barrel_hood', [[.0062, -.0499], [.0092, -.0483], [.0092, -.0319], [.0080, -.0237], [.0080, -.0212], [0, -.0212]], M.hero_nitride, { segs: 40, pos: [0, BORE, 0] }));
  g.add(revolve(THREE, id + '_chamber_mouth', [[.0048, -.0515], [.0048, -.0483], [0, -.0483]], M.hero_engrave, { segs: 32, pos: [0, BORE, 0] }));

  /* ── roll marks (real grooves) ─────────────────────────────────────── */
  sl.add(engrave(THREE, id + '_rollmark_left', 'BODIED GUARD 3.0', M.hero_engrave, { plane: 'left', x: -HW, z: .0155, y: BORE - .0020, cap: .0027, stroke: .00024, depth: .00022 }));
  sl.add(engrave(THREE, id + '_rollmark_right', '9×19MM', M.hero_engrave, { plane: 'right', x: HW, z: -.0070, y: BORE - .0018, cap: .0023, stroke: .00022, depth: .00020 }));

  /* ── frame ─────────────────────────────────────────────────────────── */
  const fz = [];
  for (let z = -.1040; z <= .0420; z += .0006) fz.push(z);
  g.add(loft(THREE, id + '_frame', fz.map(frameStation), M.hero_polymer, { creaseFrom: frameStation(-.016).pts, creaseAngle: 26 }));

  // Grip: swept upright, palm swell in the profile, raked by the station offset.
  const gs = [];
  for (let i = 0; i <= 90; i++) {
    const st = gripStation(i / 90);
    gs.push({ z: -st.y, pts: st.pts });     // local +Z upward → rotate into place below
  }
  const gripMesh = loft(THREE, id + '_grip', gs, M.hero_polymer, { creaseFrom: gripStation(.5).pts, creaseAngle: 34 });
  gripMesh.geometry.rotateX(Math.PI / 2);
  g.add(gripMesh);

  // Stipple panels: thin shells standing 0.25 mm off the grip flats, so the
  // pebble normal map has its own surface and the polished patch reads.
  for (const s of [-1, 1]) {
    const ps = [];
    for (let i = 6; i <= 84; i++) {
      const st = gripStation(i / 90), hw = Math.max(...st.pts.map((p) => p[0]));
      const zc = (Math.max(...st.pts.map((p) => p[1])) + Math.min(...st.pts.map((p) => p[1]))) / 2;
      const pts = [];
      for (let k = 0; k <= 10; k++) {
        const u = k / 10, zz = lerp(zc - .0180, zc + .0190, u);
        pts.push([s * (hw + .00012), zz]);
      }
      for (let k = 10; k >= 0; k--) {
        const u = k / 10, zz = lerp(zc - .0180, zc + .0190, u);
        pts.push([s * (hw - .00040), zz]);
      }
      ps.push({ z: -st.y, pts });
    }
    const panel = loft(THREE, `${id}_grip_stipple${s > 0 ? 'r' : 'l'}`, ps, M.hero_stipple, { creaseAngle: 40 });
    panel.geometry.rotateX(Math.PI / 2);
    g.add(panel);
  }
  // Front strap and back strap texture panels.
  for (const [nm, dz] of [['front', -1], ['back', 1]]) {
    const ps = [];
    for (let i = 4; i <= 86; i++) {
      const st = gripStation(i / 90);
      const zEdge = dz < 0 ? Math.min(...st.pts.map((p) => p[1])) : Math.max(...st.pts.map((p) => p[1]));
      const pts = [];
      for (let k = 0; k <= 8; k++) { const x = lerp(-.0092, .0092, k / 8); pts.push([x, zEdge + dz * .00012]); }
      for (let k = 8; k >= 0; k--) { const x = lerp(-.0092, .0092, k / 8); pts.push([x, zEdge - dz * .00040]); }
      ps.push({ z: -st.y, pts });
    }
    const panel = loft(THREE, `${id}_grip_${nm}_strap`, ps, M.hero_stipple, { creaseAngle: 40 });
    panel.geometry.rotateX(Math.PI / 2);
    g.add(panel);
  }

  /* Trigger guard. Was a constant-section tube — 24 mm wide and 8 mm thick the
     whole way round, narrower than the 28 mm frame, so it hung underneath as a
     separate wire loop. A real polymer guard is one piece with the frame: a
     superelliptical section with flat sides FLUSH to the frame flats, thick
     where the legs root into the frame and slimming through the bow, squared
     off at the front, undercut at the rear so the hand can ride high. */
  const guardSec = (t, ang) => {
    const bow = Math.sin(Math.PI * t);
    const a = .0132 - .0004 * bow;               // half-width: 26 mm, matching the grip
    const b = .0040 - .0008 * bow;               // half-thickness: 8 mm at the roots, 6.4 mm at the bow
    const n = 5;                                 // >2 squares the section off
    return 1 / Math.pow(Math.pow(Math.abs(Math.cos(ang) / a), n) + Math.pow(Math.abs(Math.sin(ang) / b), n), 1 / n);
  };
  g.add(sweep(THREE, id + '_trigger_guard', [
    [0, .0340, -.0232], [0, .0160, -.0234], [0, .0000, -.0250], [0, -.0100, -.0300],
    [0, -.0124, -.0370], [0, -.0124, -.0460], [0, -.0090, -.0540], [0, .0020, -.0588],
    [0, .0170, -.0600], [0, .0340, -.0598],
  ], guardSec, M.hero_polymer, { radial: 40, samples: 112, up: [1, 0, 0], vRep: 8 }));

  // Beavertail at the rear of the frame.
  g.add(sweep(THREE, id + '_beavertail', [[0, .0210, .0282], [0, .0262, .0340], [0, .0312, .0388], [0, .0346, .0428]],
    (t) => [.0130 - t * .0034, .0052 - t * .0022], M.hero_polymer, { radial: 18, up: [1, 0, 0] }));

  /* ── barrel ────────────────────────────────────────────────────────── */
  g.add(revolve(THREE, id + '_barrel', [
    [0, -.1234], [.00435, -.1234], [.0050, -.1227], [.0074, -.1221], [.0076, -.1209],
    [.0076, -.1156], [.0080, -.1143], [.0080, -.0564], [.0092, -.0556], [.0092, -.0499], [0, -.0499],
  ], M.hero_nitride, {
    segs: 56, pos: [0, BORE, 0], vRep: 40,
    wear: (r, z) => z < -.1200 ? .85 : 0,            // polished crown
  }));
  g.add(revolve(THREE, id + '_bore', [[0, -.1237], [.00420, -.1237], [.00420, -.1135], [0, -.1135]], M.hero_engrave, { segs: 40, pos: [0, BORE, 0] }));

  /* ── sights ─────────────────────────────────────────────────────────
     Three faults here, all of them tells. The vials faced the MUZZLE: night
     sights are read by the shooter, so they belong in the rear faces. The sight
     picture did not line up — the front post stood taller than the rear block
     over a notch that was 4.4 mm wide, 3.4 mm deep and only cut through part of
     the block's length, which cannot be aimed. And the rear block was 19.6 mm
     across a 25 mm slide, nearly slide-width.
     Now: a 15 mm rear block with a square notch cut the FULL depth, glare
     grooves milled into its rear top face, a 3.6 mm front post whose top sits
     level with the rear block's top, and all three vials seated in the rear
     faces where the shooter can see them. */
  const SIGHT_TOP = SY1 + .0052, NOTCH_FLOOR = SY1 + .0018;
  const glare = (z) => {                          // anti-glare grooves, rear 4 mm of the top face
    if (z < .0296) return 0;
    const p = (z - .0296) / .0010, f = p - Math.floor(p);
    return .00034 * Math.max(0, 1 - Math.abs(2 * f - 1) * 1.5);
  };
  const rear = [];
  for (let z = .0255; z <= .0332; z += .0003) {
    const hw = .0075 - .0006 * (1 - smooth(.0255, .0275, z));   // ramped leading edge
    const yb = SY1 - .0004, yt = SIGHT_TOP - glare(z), nhw = .0016;
    rear.push({ z, pts: [
      [hw - .0008, yb], [hw, yb + .0010], [hw, yt - .0008], [hw - .0008, yt],
      [nhw, yt], [nhw, NOTCH_FLOOR], [-nhw, NOTCH_FLOOR], [-nhw, yt],
      [-(hw - .0008), yt], [-hw, yt - .0008], [-hw, yb + .0010], [-(hw - .0008), yb],
    ] });
  }
  sl.add(loft(THREE, id + '_rear_sight', rear, M.hero_dlc, { creaseFrom: rear[rear.length - 1].pts, creaseAngle: 28, wear: (x, y) => (y > SIGHT_TOP - .0004 ? .35 : 0) }));
  const front = [];
  for (let z = -.1031; z <= -.0997; z += .0003) {
    const hw = .0018, yb = SY1 - .0006, yt = SIGHT_TOP;
    front.push({ z, pts: [[hw, yb], [hw, yt - .0010], [hw - .0008, yt], [-(hw - .0008), yt], [-hw, yt - .0010], [-hw, yb]] });
  }
  sl.add(loft(THREE, id + '_front_sight', front, M.hero_dlc, { creaseAngle: 28, wear: (x, y) => (y > SIGHT_TOP - .0004 ? .4 : 0) }));
  // Vials seated in the rear faces, flush with them, facing the shooter.
  const vial = (n, x, y, z, r) => {
    sl.add(revolve(THREE, id + '_tritium_' + n, [[0, 0], [r, 0], [r, .0016], [0, .0016]], M.hero_tritium, { segs: 20, pos: [x, y, z] }));
    sl.add(revolve(THREE, id + '_tritium_ring_' + n, [[r, 0], [r + .0004, 0], [r + .0004, .0016], [r, .0016]], M.hero_tritium_ring, { segs: 20, pos: [x, y, z] }));
  };
  vial('rear_l', -.0042, SY1 + .0030, .0316, .00065);
  vial('rear_r', .0042, SY1 + .0030, .0316, .00065);
  vial('front', 0, SY1 + .0034, -.1013, .00070);

  // Optic cover plate sitting in the milled cut.
  sl.add(boxMesh(THREE, id + '_optic_plate', .0150, .0016, .0222, M.hero_dlc, [0, SY1 - .0016, .0143]));

  /* ── trigger + controls ─────────────────────────────────────────────
     The trigger was a 6.8 mm-wide, 5.6 mm-deep straight slab with a 1 mm box
     glued to its front for the safety — a card with a sticker on it. A real
     flat-faced blade is ~5 mm wide and ~3.4 mm thick, its shoe bows forward
     toward the bottom, and the safety tongue is an actual inset part with a
     split line down each side. All of that is geometry now: the blade is swept
     vertically with a bowing centreline, and the tongue is a proud centre band
     between two cut grooves rather than a separate box. */
  const tri = [];
  for (let i = 0; i <= 70; i++) {
    const t = i / 70, y = lerp(.0232, -.0058, t);
    const a = .0026 + .0005 * smooth(.18, .55, t);
    const zc = -.0414 - .0018 * smooth(.10, 1, t) - .0004 * t, d = .0018 + .0002 * smooth(.2, .7, t);
    const z0 = zc - d, z1 = zc + d;
    tri.push({ z: -y, pts: [
      [a - .0006, z0],
      [.0017, z0], [.0017, z0 + .0005], [.0011, z0 + .0005],
      [.0011, z0 - .0002], [-.0011, z0 - .0002],
      [-.0011, z0 + .0005], [-.0017, z0 + .0005], [-.0017, z0],
      [-(a - .0006), z0],
      [-a, z0 + .0006], [-a, z1 - .0006], [-(a - .0006), z1],
      [a - .0006, z1], [a, z1 - .0006], [a, z0 + .0006],
    ] });
  }
  const trigger = loft(THREE, id + '_trigger', tri, M.hero_dlc, { creaseAngle: 26, wear: (x, y, z) => (Math.abs(x) < .0011 ? .40 : .22) });
  trigger.geometry.rotateX(Math.PI / 2);
  g.add(trigger);
  g.add(revolve(THREE, id + '_trigger_pin', [[0, -.0138], [.0018, -.0138], [.0018, .0138], [0, .0138]], M.hero_steel, { segs: 20, pos: [0, .0272, -.0418], rot: [0, Math.PI / 2, 0], wear: () => .18 }));

  const ctl = (name, pts, pos, mat = M.hero_dlc, wear = .26) => g.add(revolve(THREE, id + '_' + name, pts, mat, { segs: 28, pos, rot: [0, Math.PI / 2, 0], wear: () => wear }));
  ctl('takedown', [[0, 0], [.0040, 0], [.0040, .0016], [.0032, .0022], [0, .0022]], [-.0142, .0312, -.0392]);
  ctl('mag_release', [[0, 0], [.0046, 0], [.0046, .0026], [.0038, .0034], [0, .0034]], [-.0148, -.0020, .0016]);
  // Slide stop with a scalloped thumb pad.
  g.add(sweep(THREE, id + '_slide_stop', [[-.0138, .0500, -.0115], [-.0141, .0500, -.0050], [-.0141, .0500, .0016], [-.0138, .0500, .0057]],
    (t) => [.0016 + .0008 * Math.sin(Math.PI * t), .0030], M.hero_dlc, { radial: 14, up: [0, 1, 0], wear: () => .34 }));
  sl.add(revolve(THREE, id + '_striker_indicator', [[0, 0], [.0016, 0], [.0016, .0010], [0, .0010]], M.hero_steel_bright, { segs: 18, pos: [0, BORE, .0402] }));
  // Slide-stop / locking-block pin heads.
  for (const s of [-1, 1]) for (const [y, z] of [[.0336, -.0206], [.0280, .0240], [.0240, .0059]])
    g.add(revolve(THREE, `${id}_pin${s > 0 ? 'r' : 'l'}_${Math.round(z * 1e4)}`, [[0, 0], [.0023, 0], [.0023, .0008], [.0019, .0011], [0, .0011]], M.hero_dlc, { segs: 20, pos: [s * .0139, y, z], rot: [0, s > 0 ? Math.PI / 2 : -Math.PI / 2, 0] }));

  /* ── magazine with real witness holes ─────────────────────────────── */
  const magTop = -.0060, magBot = -.0530, magW = .0104, magD = .0180;
  const holes = [-.0140, -.0220, -.0300, -.0380, -.0460];
  const witness = (y) => { let d = 0; for (const h of holes) d = Math.max(d, .0013 * (1 - smooth(.0013, .0021, Math.abs(y - h)))); return d; };
  const ms = [];
  for (let y = magTop; y >= magBot; y -= .00035) {
    const zOff = -y * Math.tan(RAKE), f = -.0072 + zOff, b = .0108 + zOff;
    const r = .0018, pts = [];
    const hw = magW - witness(y);
    const corner = (cx, cz, a0) => { for (let i = 0; i <= 3; i++) { const a = a0 + i / 3 * Math.PI / 2; pts.push([cx + Math.cos(a) * r, cz + Math.sin(a) * r]); } };
    corner(hw - r, b - r, 0); corner(-(magW - r), b - r, Math.PI / 2);
    corner(-(magW - r), f + r, Math.PI); corner(hw - r, f + r, -Math.PI / 2);
    ms.push({ z: -y, pts });
  }
  const mag = grp(id + '_magazine');
  const magBody = loft(THREE, id + '_mag_body', ms, M.hero_dlc, { creaseAngle: 34 });
  magBody.geometry.rotateX(Math.PI / 2);
  mag.add(magBody);
  for (const h of holes) mag.add(revolve(THREE, `${id}_mag_witness_${Math.round(-h * 1e4)}`, [[0, 0], [.0011, 0], [.0011, .0016], [0, .0016]], M.hero_engrave,
    { segs: 18, pos: [magW - .0016, h, -h * Math.tan(RAKE) + .0018], rot: [0, Math.PI / 2, 0] }));
  // Baseplate with the forward lip.
  const bz = -magBot * Math.tan(RAKE);
  mag.add(boxMesh(THREE, id + '_mag_floorplate', .0232, .0062, .0212, M.hero_polymer, [0, magBot - .0022, bz + .0020]));
  mag.add(sweep(THREE, id + '_mag_base_lip', [[-.0104, magBot - .0044, bz - .0086], [0, magBot - .0052, bz - .0092], [.0104, magBot - .0044, bz - .0086]],
    (t) => [.0026, .0044], M.hero_polymer, { radial: 14, up: [0, 1, 0] }));
  // A loaded round showing at the feed lips.
  mag.add(revolve(THREE, id + '_top_round', [[0, -.0092], [.0022, -.0088], [.0038, -.0070], [.0045, -.0040], [.0045, .0090], [.0048, .0096], [.0048, .0112], [0, .0112]], M.hero_brass,
    { segs: 32, pos: [0, magTop - .0030, .0030], rot: [Math.PI / 2 - .06, 0, 0] }));
  /* Pivot at the seated top face, not the weapon origin: a dropped magazine
     tips about the lip it just left, and the standalone prop exported from this
     group lands on the magwell by identity. (FORWARD-MANIFEST-reload Ask C.) */
  repivot(mag, [0, magTop, .0018 + .0060 * Math.tan(RAKE)]);
  mag.userData.drop = { axis: [0, -1, 0], tip: [1, 0, 0], clear: .0140 };
  g.add(mag);

  /* ── spent case + ejection ──────────────────────────────────────────
     A fired case, not a loaded round: no bullet, an open mouth you can see
     down, a rim and extractor groove for the claw to have gripped. Kept hidden
     as a template — the viewer clones it per shot. The eject node carries the
     port's throw direction so the engine does not have to guess it. */
  const spentCase = revolve(THREE, id + '_case', [
    [0, 0], [.00498, 0], [.00498, .0013], [.00432, .0020], [.00432, .0033],
    [.00483, .0043], [.00483, .0186], [.00470, .01915],
    [.00420, .01915], [.00420, .0060], [.00250, .0038], [0, .0034],
  ], M.hero_brass, { segs: 40, creaseAngle: 34, wear: (r, z) => (z > .0180 ? .5 : .12) });
  spentCase.visible = false;
  g.add(spentCase);
  const ejectNode = grp(id + '_eject', [.0104, BORE + .0010, -.0250]);
  ejectNode.userData.eject = { dir: [.78, .58, .24], speed: 2.2, spin: 26 };
  g.add(ejectNode);

  /* ── mounts (unchanged contract) ───────────────────────────────────── */
  const mount = (slot, pos, parent = g) => { const m = grp(`${id}_mount_${slot}`, pos); m.userData.slot = slot; parent.add(m); };
  mount('barrel', [0, BORE, -.1140]);
  mount('muzzle', [0, BORE, -.1270]);
  mount('optic', [0, SY1 - .0010, .0143], sl);
  mount('magazine', [0, -.0420, .0430]);
  // The face the magazine actually seats against — `_mount_magazine` is the
  // attachment origin for a drum and hangs 36 mm below it, so a reload prop
  // spawned there would float low. Reload code uses this node.
  mount('magwell', [0, -.0060, .0018 + .0060 * Math.tan(RAKE)]);
  mount('stock', [0, .0200, .0480]);
  mount('underbarrel', [0, .0180, -.0650]);
  mount('infusion', [-.0140, .0300, -.0500]);
  return g;
}

/* ══ hero gloved hands — two-handed, thumbs forward ═══════════════════════
   Anatomy first, glove second. Four digits of three phalanges each with real
   knuckle bulges and an oval (not round) section; a lofted palm carrying thenar
   and heel volume; tendon relief across the back of the hand; a forearm that
   swells at the belly instead of running as a straight tube. Glove hardware sits
   on top of that: a knuckle panel over the MCPs, a cinched wrist strap, seam
   welts down every finger.

   Every joint is authored directly in weapon space, so the digits actually
   contact the grip, the trigger and the frame flats we just built — the right
   index pad lands on the trigger face, both thumbs run forward along the frame,
   and the support hand's fingers overlay the firing hand's without intersecting
   them.                                                                      */

const ramp = (vals) => (t) => {
  const n = vals.length - 1, f = clamp(t, 0, 1) * n, k = Math.min(Math.floor(f), n - 1);
  return lerp(vals[k], vals[k + 1], f - k);
};

/** One gloved digit. joints: MCP→tip; radii: one per joint. */
function digit(THREE, name, joints, radii, mat) {
  const n = joints.length - 1, rAt = ramp(radii);
  const rFn = (t, ang) => {
    let r = rAt(t);
    for (let k = 1; k < n; k++) r += rAt(k / n) * .19 * Math.exp(-Math.pow((t - k / n) * 8.5, 2));
    const a = ((ang + Math.PI) % (Math.PI * 2)) - Math.PI;
    r += rAt(t) * .05 * Math.exp(-Math.pow(a * 3.4, 2));          // stitched seam welt along the top
    return [r * (1 - .18 * Math.pow(Math.cos(ang), 2)), r];        // oval section
  };
  return sweep(THREE, name, joints, rFn, mat, { radial: 20, samples: 72, up: [1, 0, 0], vRep: 3 });
}

/** Palm mass: swept wrist→MCP, breadth across the digits, thickness across the back. */
function palmMass(THREE, name, path, thick, breadth, mat) {
  const rt = ramp(thick), rb = ramp(breadth);
  return sweep(THREE, name, path, (t) => [rt(t), rb(t)], mat, { radial: 28, samples: 60, up: [1, 0, 0], vRep: 3 });
}

/* Pose data. Right hand is the firing hand (index on the trigger, thumb high on
   the right frame flat); left is the support hand, wrapping over the right and
   running its thumb forward along the left flat. */
const POSE = {
  r: {
    palm: { path: [[.0225, -.0540, .0560], [.0275, -.0450, .0420], [.0300, -.0360, .0250], [.0300, -.0280, .0060]],
      thick: [.0130, .0152, .0162, .0138], breadth: [.0240, .0300, .0345, .0345] },
    digits: [
      { n: 'index',  j: [[.0265, -.0140, -.0010], [.0235, -.0100, -.0180], [.0150, -.0040, -.0320], [.0060, -.0010, -.0446]], r: [.0086, .0082, .0073, .0059] },
      { n: 'middle', j: [[.0270, -.0320, -.0035], [.0210, -.0335, -.0150], [.0080, -.0350, -.0185], [-.0070, -.0350, -.0140]], r: [.0092, .0088, .0078, .0063] },
      { n: 'ring',   j: [[.0265, -.0480, .0015], [.0205, -.0495, -.0100], [.0080, -.0510, -.0135], [-.0065, -.0510, -.0090]], r: [.0088, .0084, .0074, .0059] },
      { n: 'little', j: [[.0250, -.0620, .0060], [.0200, -.0645, -.0040], [.0100, -.0665, -.0070], [-.0020, -.0665, -.0030]], r: [.0078, .0074, .0064, .0051] },
    ],
    thumb: { j: [[.0255, -.0100, .0430], [.0242, .0030, .0380], [.0198, .0190, .0180], [.0172, .0280, -.0130]], r: [.0112, .0102, .0090, .0072] },
    web: { j: [[.0200, -.0040, .0420], [.0168, .0070, .0405], [.0122, .0180, .0370]], t: [.0088, .0100, .0086], b: [.0105, .0125, .0104] },
    knuckles: [[.0400, -.0140, -.0020], [.0415, -.0320, -.0025], [.0410, -.0480, .0025], [.0395, -.0620, .0070]],
    tendonX: .0425,
    wrist: [.0240, -.0560, .0620], elbow: [.0640, -.1450, .2980],
  },
  l: {
    palm: { path: [[-.0250, -.0580, .0540], [-.0300, -.0480, .0390], [-.0320, -.0390, .0220], [-.0320, -.0310, .0030]],
      thick: [.0125, .0145, .0152, .0135], breadth: [.0235, .0290, .0335, .0335] },
    digits: [
      { n: 'index',  j: [[-.0290, -.0200, -.0060], [-.0230, -.0230, -.0230], [-.0110, -.0245, -.0300], [.0040, -.0245, -.0255]], r: [.0088, .0084, .0074, .0059] },
      { n: 'middle', j: [[-.0295, -.0350, -.0015], [-.0235, -.0370, -.0170], [-.0115, -.0385, -.0230], [.0035, -.0385, -.0190]], r: [.0092, .0088, .0078, .0063] },
      { n: 'ring',   j: [[-.0290, -.0510, .0035], [-.0230, -.0530, -.0115], [-.0110, -.0545, -.0175], [.0030, -.0545, -.0135]], r: [.0088, .0084, .0074, .0059] },
      { n: 'little', j: [[-.0275, -.0655, .0085], [-.0220, -.0680, -.0055], [-.0105, -.0695, -.0110], [.0020, -.0695, -.0075]], r: [.0078, .0074, .0064, .0051] },
    ],
    thumb: { j: [[-.0300, -.0160, .0380], [-.0282, -.0020, .0300], [-.0216, .0140, .0100], [-.0188, .0240, -.0220]], r: [.0112, .0102, .0090, .0072] },
    web: { j: [[-.0248, -.0080, .0400], [-.0220, .0030, .0375], [-.0184, .0130, .0330]], t: [.0086, .0096, .0082], b: [.0100, .0118, .0100] },
    knuckles: [[-.0420, -.0170, -.0060], [-.0435, -.0350, -.0015], [-.0430, -.0510, .0035], [-.0415, -.0655, .0085]],
    tendonX: -.0445,
    wrist: [-.0290, -.0620, .0580], elbow: [-.0820, -.1550, .2830],
  },
};

const FA_THICK = [.0175, .0290, .0330, .0300], FA_BREADTH = [.0215, .0360, .0405, .0365];

function heroHand(THREE, M, side, Pover) {
  const P = Pover || POSE[side], g = new THREE.Group(); g.name = 'hand_' + side;
  const nm = (s) => `hand_${side}_${s}`;

  g.add(palmMass(THREE, nm('palm'), P.palm.path, P.palm.thick, P.palm.breadth, M.hero_glove));
  for (const d of P.digits) g.add(digit(THREE, nm(d.n), d.j, d.r, M.hero_glove));
  g.add(digit(THREE, nm('thumb'), P.thumb.j, P.thumb.r, M.hero_glove));
  // Thenar web: the mass that fills the space between the palm and the beavertail.
  g.add(palmMass(THREE, nm('web'), P.web.j, P.web.t, P.web.b, M.hero_glove));

  // Glove knuckle panel riding the MCP line, and tendon relief behind it.
  g.add(sweep(THREE, nm('knuckle_panel'), P.knuckles,
    (t) => [.0040 + .0010 * Math.sin(Math.PI * t), .0086 + .0018 * Math.sin(Math.PI * t)],
    M.hero_glove_pad, { radial: 22, samples: 56, up: [1, 0, 0], vRep: 2, cap: false }));
  /* Forearm: one swept mass with an anatomical belly, a glove cuff over the
     wrist end and a cinch strap at the cuff seam. */
  const w = P.wrist, e = P.elbow;
  const at = (t) => [lerp(w[0], e[0], t), lerp(w[1], e[1], t) + Math.sin(Math.PI * t) * .0060, lerp(w[2], e[2], t)];
  const path = [at(0), at(.34), at(.70), at(1)];
  const rt = ramp(FA_THICK), rb = ramp(FA_BREADTH);
  g.add(sweep(THREE, nm('forearm'), path, (t) => [rt(t), rb(t)], M.hero_sleeve, { radial: 30, samples: 64, up: [0, 1, 0], vRep: 5, cap: false }));
  g.add(sweep(THREE, nm('cuff'), [at(-.02), at(.055), at(.13)],
    (t) => { const k = clamp(t * .13, 0, 1); return [rt(k) + .0022, rb(k) + .0024]; },
    M.hero_glove, { radial: 30, samples: 32, up: [0, 1, 0], vRep: 2 }));
  g.add(sweep(THREE, nm('cuff_strap'), [at(.115), at(.155)],
    (t) => { const k = .135; return [rt(k) + .0034, rb(k) + .0036]; },
    M.hero_glove_pad, { radial: 30, samples: 10, up: [0, 1, 0], cap: false }));
  return g;
}

export function buildHeroHands(K, pose = 'pistol') {
  const { THREE, mats, grp } = K;
  heroSurfaces(THREE, mats);
  const g = grp('hands');
  if (pose !== 'pistol') return g;     // rifle/tool platforms still use the older rig
  g.add(heroHand(THREE, mats, 'r'));
  g.add(heroHand(THREE, mats, 'l'));
  return g;
}

/* ══ off-hand reload poses ══════════════════════════════════════════════
   A reload's FIRING hand never leaves the grip — only the off-hand tells the
   story — so these files carry the off-hand alone: hide `hand_l` in the
   platform's own hands file and parent one of these under the same camera node.
   One set of three therefore serves every platform, instead of one set per
   platform per pose.

   Each pose is the authored support hand with its digits closed by a curl angle
   and the whole arm swung about its own wrist. The curl axis is derived from the
   hand's geometry — the MCP row crossed with the finger direction, signed by
   whichever way moves the fingertip toward the palm — so the fingers close into
   a grip instead of hinging about some global axis.

   `hands_mount_magazine` is where the magazine prop rides: parent
   `weapon_<id>_magazine` there and it travels with the hand.                 */

function curlChain(THREE, j, axis, ang) {
  const out = [j[0].slice()], q = new THREE.Quaternion();
  const step = new THREE.Quaternion().setFromAxisAngle(axis, ang);
  for (let k = 1; k < j.length; k++) {
    q.multiply(step);
    const seg = new THREE.Vector3(...j[k]).sub(new THREE.Vector3(...j[k - 1])).applyQuaternion(q);
    out.push(new THREE.Vector3(...out[k - 1]).add(seg).toArray());
  }
  return out;
}

function curledPose(THREE, side, ang) {
  const P = POSE[side];
  const mcp = P.digits.map((d) => new THREE.Vector3(...d.j[0]));
  const spread = mcp[3].clone().sub(mcp[0]).normalize();
  const fdir = new THREE.Vector3(...P.digits[0].j[3]).sub(mcp[0]).normalize();
  const axis = new THREE.Vector3().crossVectors(fdir, spread).normalize();
  const palmC = new THREE.Vector3(...P.palm.path[2]);
  const reach = (s) => new THREE.Vector3(
    ...curlChain(THREE, P.digits[0].j, axis.clone().multiplyScalar(s), ang)[3]).distanceTo(palmC);
  const ax = axis.multiplyScalar(reach(1) <= reach(-1) ? 1 : -1);
  return {
    ...P,
    digits: P.digits.map((d) => ({ ...d, j: curlChain(THREE, d.j, ax, ang) })),
    thumb: { ...P.thumb, j: curlChain(THREE, P.thumb.j, ax, ang * .45) },
  };
}

const OFFHAND = {
  magout: { pos: [-.0340, -.0880, .0960], rot: [.30, -.42, .55], curl: .60, mag: [.0040, .0500, -.0330] },
  magin:  { pos: [.0250, -.0020, -.0200], rot: [.06, -.10, .14], curl: .44, mag: [.0040, .0520, -.0340] },
  charge: { pos: [.0400, .1310, .0100], rot: [-.40, .30, -.20], curl: .95 },
};
export const OFFHAND_POSES = Object.keys(OFFHAND);

export function buildOffhandPose(K, pose) {
  const { THREE, mats, grp } = K;
  heroSurfaces(THREE, mats);
  const g = grp('hands');
  const P = OFFHAND[pose];
  if (!P) return g;
  const wrist = new THREE.Vector3(...POSE.l.wrist);
  const arm = grp('hand_l_root', [wrist.x + P.pos[0], wrist.y + P.pos[1], wrist.z + P.pos[2]]);
  arm.rotation.set(...P.rot);
  const h = heroHand(THREE, mats, 'l', curledPose(THREE, 'l', P.curl));
  h.position.copy(wrist).negate();
  arm.add(h);
  if (P.mag) {
    const m = grp('hands_mount_magazine', P.mag);
    m.userData.slot = 'magazine';
    arm.add(m);
  }
  g.add(arm);
  g.userData.offhand = { pose, side: 'l', replaces: 'hand_l' };
  return g;
}
