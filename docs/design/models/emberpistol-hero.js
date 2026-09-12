/**
 * Deep Field 3D — Ember pistol, hero standard.
 *
 * The one weapon in the armoury with no real-world twin, so the discipline is
 * different: every part is grounded in a real MACHINE, even though the gun is
 * invented. It is a fuel-fed hand cannon, and the reference class is industrial
 * gas kit — a 26.5 mm signal pistol crossed with a cutting torch.
 *
 *   frame          break-action signal-pistol proportions: 150 mm barrel,
 *                  288 mm overall. Big bore, short reach.
 *   canister       a real pressure vessel — domed ends, a rolled seam, a
 *                  brazed boss for the valve. Not a capsule with a light in it.
 *   valve          turned brass handwheel with cast spokes, on a raised boss.
 *   hose           braided line from the canister's outlet to the frame's
 *                  inlet, with swaged ferrules at both ends. It has to LAND
 *                  somewhere at each end or it reads as a floating noodle.
 *   fins           turned cooling rings CUT into the barrel's profile, not
 *                  discs threaded on it.
 *
 * Finish is brushed steel, brass and ember glow — the pistol keeps its own
 * identity; only the quality level rises (CLAUDE.md).
 *
 * Metres, Y-up, muzzle toward −Z, origin at the grip web.
 */
import {
  heroSurfaces, loft, sweep, revolve, engrave, repivot,
  clamp, smooth, lerp,
} from './gun-kit.js';

const BORE = .0560;                 // bore axis above the grip web
const FZ0 = -.0400, FZ1 = .0480;    // frame front / rear
const FHW = .0148;                  // frame half-width (30 mm)
const FYT = BORE + .0150, FYB = BORE - .0250;
const BBL = .1500;
const MUZZLE = FZ0 - BBL;
const CAL = .0090;                  // 18 mm bore
const CAN_Y = BORE + .0128;         // canister axis
const CAN_R = .0208;
const CAN_Z0 = .0180, CAN_Z1 = .1060;

const span = (a, b, step) => { const o = []; for (let v = a; v <= b; v += step) o.push(v); return o; };
const sup = (ang, a, b, n) => 1 / Math.pow(Math.pow(Math.abs(Math.cos(ang) / a), n) + Math.pow(Math.abs(Math.sin(ang) / b), n), 1 / n);

/**
 * Frame. A milled steel block with the breech bored through it, lightening
 * pockets cut into both flanks and a raised rib on top carrying the sights.
 */
function frameStation(z) {
  const nose = smooth(FZ0, FZ0 + .0070, z);
  const tail = 1 - smooth(FZ1 - .0100, FZ1, z) * .16;
  const hw = (FHW - (1 - nose) * .0016) * tail;
  const yt = FYT - (1 - nose) * .0020, yb = FYB + (1 - nose) * .0028;
  // lightening pockets, two per flank
  const p1 = smooth(-.0300, -.0256, z) * (1 - smooth(-.0092, -.0048, z));
  const p2 = smooth(.0034, .0078, z) * (1 - smooth(.0242, .0286, z));
  const pk = Math.max(p1, p2);
  const flank = (y) => {
    const v = (y - yb) / (yt - yb);
    return hw - pk * .0026 * smooth(.16, .30, v) * (1 - smooth(.68, .82, v));
  };
  const pts = [];
  pts.push([hw - .0020, yb], [hw, yb + .0024]);
  for (let i = 0; i < 8; i++) { const y = lerp(yb + .0046, yt - .0042, i / 7); pts.push([flank(y), y]); }
  pts.push([hw - .0020, yt - .0014], [hw - .0046, yt]);
  pts.push([.0062, yt], [.0062, yt + .0038], [-.0062, yt + .0038], [-.0062, yt]);   // sight rib
  pts.push([-(hw - .0046), yt], [-(hw - .0020), yt - .0014]);
  for (let i = 0; i < 8; i++) { const y = lerp(yt - .0042, yb + .0046, i / 7); pts.push([-flank(y), y]); }
  pts.push([-hw, yb + .0024], [-(hw - .0020), yb]);
  return { z, pts };
}

/**
 * Barrel with cooling fins cut INTO the turned profile. A fin is a step in the
 * lathe cut; a disc threaded onto a tube is the thing we are replacing.
 */
function barrelProfile() {
  const p = [[0, FZ0 + .0100], [.0206, FZ0 + .0100], [.0206, FZ0 - .0060], [.0188, FZ0 - .0120]];
  const F0 = FZ0 - .0200, PITCH = .0112, N = 9;
  for (let i = 0; i < N; i++) {
    const z = F0 - i * PITCH;
    const r = .0132 - .0006 * (i / (N - 1));           // fins shrink toward the muzzle
    p.push([r, z + .0018], [r + .0046, z + .0012], [r + .0046, z - .0012], [r, z - .0018]);
  }
  const zEnd = F0 - (N - 1) * PITCH - .0120;
  p.push([.0126, zEnd], [.0126, MUZZLE + .0140], [.0150, MUZZLE + .0110], [.0150, MUZZLE + .0026],
    [.0146, MUZZLE], [CAL + .0016, MUZZLE], [CAL, MUZZLE + .0040],
    [CAL, FZ0 - .0120], [.0150, FZ0 - .0060], [.0150, FZ0 + .0100]);
  return p;
}

/** Canister: a real pressure vessel — torispherical ends and a rolled seam. */
function canisterProfile() {
  const p = [[0, CAN_Z0]];
  for (let i = 0; i <= 8; i++) {                       // front dome
    const a = i / 8 * Math.PI / 2;
    p.push([Math.sin(a) * CAN_R, CAN_Z0 + (1 - Math.cos(a)) * .0130]);
  }
  p.push([CAN_R, CAN_Z0 + .0180], [CAN_R + .0016, CAN_Z0 + .0196], [CAN_R + .0016, CAN_Z0 + .0228], [CAN_R, CAN_Z0 + .0244]);
  p.push([CAN_R, CAN_Z1 - .0150]);
  for (let i = 0; i <= 8; i++) {                       // rear dome
    const a = i / 8 * Math.PI / 2;
    p.push([Math.cos(a) * CAN_R, CAN_Z1 - .0150 + Math.sin(a) * .0150]);
  }
  return p;
}

export function buildEmberPistolHero(K) {
  const { THREE, mats, grp } = K;
  heroSurfaces(THREE, mats);
  const M = mats, id = 'emberpistol', g = grp(id);
  const ST = M.hero_steel;

  /* A fuel gun does not cycle a slide and ejects nothing — declaring a false
     ejection port would be a lie the engine then has to animate. What it does
     have is a breech block that kicks on discharge. */
  const br = grp(id + '_slide_assembly');
  br.userData.recoil = { axis: [0, 0, 1], travel: .0085 };
  g.add(br);

  /* ── frame + barrel ──────────────────────────────────────────────────── */
  g.add(loft(THREE, id + '_frame', span(FZ0, FZ1, .0004).map(frameStation), ST,
    { creaseFrom: frameStation(-.0180).pts, creaseAngle: 26,
      wear: (x, y) => smooth(FYT - .0030, FYT, y) * .38 + smooth(FHW - .0020, FHW - .0004, Math.abs(x)) * .26 }));
  g.add(revolve(THREE, id + '_barrel', barrelProfile(), ST,
    { segs: 76, creaseAngle: 32, pos: [0, BORE, 0],
      wear: (r, z) => smooth(.0168, .0178, r) * .5 + (z < MUZZLE + .0060 ? .5 : 0) }));
  g.add(engrave(THREE, id + '_rollmark', 'EMBER', M.hero_engrave,
    { x: -FHW, z: -.0240, y: BORE - .0090, cap: .0040, plane: 'left' }));

  /* Breech block, proud of the frame's rear — the part that actually moves. */
  br.add(revolve(THREE, id + '_breech', [
    [0, .0420], [.0172, .0420], [.0172, .0480], [.0164, .0492], [.0164, .0560], [.0140, .0576], [0, .0576],
  ], M.hero_steel_bright, { segs: 40, creaseAngle: 30, pos: [0, BORE, 0], wear: (r, z) => (z > .0570 ? .45 : .12) }));
  br.add(revolve(THREE, id + '_breech_wheel', Array.from({ length: 13 }, (_, i) => [i % 2 ? .0150 : .0164, .0486 + i * .0006])
    .concat([[.0150, .0564], [0, .0564]]), M.hero_steel,
    { segs: 36, pos: [0, BORE, 0], wear: (r) => smooth(.0156, .0164, r) * .6 }));

  /* ── canister ──────────────────────────────────────────────────────────
     The fuel vessel is what this gun reloads, so it carries the magazine name
     the rest of the armoury uses (`<id>_magazine`) — the viewer's magazine-slot
     hide and the reload sequence both look that name up, and calling it
     `_canister` only meant neither found it. The regulator valve below stays on
     the frame: the vessel screws into it and pulls off rearward. */
  const can = grp(id + '_magazine');
  can.add(revolve(THREE, id + '_canister', canisterProfile(), M.hero_brushed,
    { segs: 76, creaseAngle: 30, pos: [0, CAN_Y, 0],
      wear: (r) => smooth(CAN_R - .0006, CAN_R + .0016, r) * .35 }));
  // Sight glass: a real fill-level window with a machined bezel, not a glowing ring.
  can.add(revolve(THREE, id + '_sight_bezel', [
    [CAN_R - .0004, -.0072], [CAN_R + .0026, -.0062], [CAN_R + .0026, .0062], [CAN_R - .0004, .0072],
  ], M.hero_brass, { segs: 60, pos: [0, CAN_Y, .0580], wear: (r) => smooth(CAN_R + .0016, CAN_R + .0026, r) * .5 }));
  can.add(revolve(THREE, id + '_sight_glass', [
    [CAN_R + .0014, -.0054], [CAN_R + .0018, 0], [CAN_R + .0014, .0054],
  ], M.hero_ember_glass, { segs: 60, pos: [0, CAN_Y, .0580] }));
  for (const z of [CAN_Z0 + .0330, CAN_Z1 - .0270]) {
    can.add(revolve(THREE, `${id}_hazard_${Math.round(z * 1e4)}`, [
      [CAN_R + .0002, -.0062], [CAN_R + .0012, -.0054], [CAN_R + .0012, .0054], [CAN_R + .0002, .0062],
    ], M.hero_hazard, { segs: 64, pos: [0, CAN_Y, z] }));
  }
  // Pivot on the forward dome — the face that meets the regulator.
  repivot(can, [0, CAN_Y, CAN_Z0]);
  can.userData.drop = { axis: [0, 0, 1], tip: [1, 0, 0], clear: .0220 };
  g.add(can);
  // Brazed boss for the valve — the wheel grows out of the vessel.
  g.add(revolve(THREE, id + '_valve_boss', [
    [0, 0], [.0104, 0], [.0104, .0042], [.0086, .0056], [.0086, .0092], [.0074, .0100], [0, .0100],
  ], M.hero_brass, { segs: 32, creaseAngle: 30, pos: [0, CAN_Y + CAN_R - .0030, .0420], rot: [-Math.PI / 2, 0, 0] }));
  g.add(revolve(THREE, id + '_valve_rim', [
    [.0092, 0], [.0148, 0], [.0156, .0014], [.0156, .0044], [.0148, .0058], [.0092, .0058],
  ], M.hero_brass, { segs: 36, creaseAngle: 30, pos: [0, CAN_Y + CAN_R + .0062, .0420], rot: [-Math.PI / 2, 0, 0],
    wear: (r) => smooth(.0148, .0156, r) * .55 }));
  g.add(revolve(THREE, id + '_valve_hub', [
    [0, 0], [.0056, 0], [.0056, .0052], [.0038, .0064], [0, .0064],
  ], M.hero_brass, { segs: 24, pos: [0, CAN_Y + CAN_R + .0062, .0420], rot: [-Math.PI / 2, 0, 0] }));
  const VY = CAN_Y + CAN_R + .0090, VZ = .0420;
  for (let i = 0; i < 4; i++) {
    const a = i / 4 * Math.PI * 2 + Math.PI / 4, ca = Math.cos(a), sa = Math.sin(a);
    g.add(sweep(THREE, `${id}_valve_spoke${i}`, [
      [ca * .0030, VY, VZ + sa * .0030], [ca * .0072, VY, VZ + sa * .0072], [ca * .0116, VY, VZ + sa * .0116],
    ], () => [.0026, .0017], M.hero_brass, { radial: 14, samples: 26, up: [0, 1, 0] }));
  }

  /* ── braided hose: canister outlet → frame inlet ─────────────────────────
     Swaged ferrules at BOTH ends. A hose that simply stops in mid-air is the
     same defect as a sweep that does not root in the mass it joins. */
  g.add(sweep(THREE, id + '_hose', [
    [0, CAN_Y - CAN_R + .0030, CAN_Z0 + .0060], [0, CAN_Y - CAN_R - .0080, CAN_Z0 - .0060],
    [0, BORE - .0060, .0060], [0, BORE - .0140, -.0080], [0, BORE - .0168, -.0192],
  ], (t, ang) => {
    const braid = .00022 * Math.sin(ang * 8 + t * 46) * smooth(.10, .18, t) * (1 - smooth(.84, .94, t));
    return .0044 + braid;
  }, M.hero_hose, { radial: 24, samples: 108, up: [1, 0, 0], vRep: 26 }));
  for (const [y, z, rot] of [[CAN_Y - CAN_R + .0044, CAN_Z0 + .0086, .9], [BORE - .0170, -.0206, -1.26]]) {
    g.add(revolve(THREE, `${id}_ferrule_${Math.round(z * 1e4)}`,
      Array.from({ length: 7 }, (_, i) => [i % 2 ? .0052 : .0060, i * .0018]).concat([[.0052, .0116], [.0068, .0124], [.0068, .0150], [0, .0150]]),
      M.hero_brass, { segs: 26, pos: [0, y, z], rot: [rot, 0, 0], wear: (r) => smooth(.0054, .0060, r) * .5 }));
  }

  /* ── pilot nozzle + flame under the muzzle ───────────────────────────── */
  g.add(revolve(THREE, id + '_pilot_body', [
    [0, .0140], [.0050, .0140], [.0050, -.0060], [.0042, -.0090], [.0034, -.0104], [0, -.0104],
  ], M.hero_brass, { segs: 26, creaseAngle: 30, pos: [0, BORE - .0210, MUZZLE + .0120] }));
  g.add(revolve(THREE, id + '_pilot_flame', [
    [0, 0], [.0030, -.0020], [.0026, -.0070], [.0014, -.0128], [0, -.0168],
  ], M.hero_ember, { segs: 20, pos: [0, BORE - .0210, MUZZLE + .0020] }));
  g.add(sweep(THREE, id + '_pilot_line', [
    [0, BORE - .0182, -.0300], [0, BORE - .0230, -.0700], [0, BORE - .0236, -.1200], [0, BORE - .0216, MUZZLE + .0150],
  ], () => .0022, M.hero_hose, { radial: 14, samples: 60, up: [1, 0, 0], vRep: 30 }));

  /* ── sights ──────────────────────────────────────────────────────────── */
  g.add(loft(THREE, id + '_rear_sight', span(.0330, .0410, .0005).map((z) => {
    const t = (z - .0330) / .0080, w = .0072 - .0012 * smooth(.5, 1, t);
    const yb = FYT + .0038, yt = yb + .0062 - .0018 * smooth(.55, 1, t);
    const notch = .0022;
    return { z, pts: [
      [w, yb], [w, yt], [notch, yt], [notch, yt - .0026], [-notch, yt - .0026], [-notch, yt], [-w, yt], [-w, yb],
    ] };
  }), M.hero_steel, { creaseAngle: 24, wear: (x, y) => smooth(FYT + .0074, FYT + .0100, y) * .6 }));
  g.add(loft(THREE, id + '_front_sight', span(FZ0 - .0084, FZ0 - .0056, .0004).map((z) => {
    const t = Math.abs(z - (FZ0 - .0070)) / .0014;
    const w = .0018 - .0004 * t;
    return { z, pts: [[w, BORE + .0206], [w - .0004, BORE + .0286], [-(w - .0004), BORE + .0286], [-w, BORE + .0206]] };
  }), M.hero_steel, { creaseAngle: 24, wear: (x, y) => smooth(BORE + .0270, BORE + .0286, y) * .7 }));

  /* ── grip, guard, trigger ────────────────────────────────────────────── */
  g.add(sweep(THREE, id + '_grip', [
    [0, FYB + .0040, .0028], [0, .0110, .0182], [0, -.0140, .0322], [0, -.0400, .0452], [0, -.0546, .0530],
  ], (t, ang) => {
    const a = .0136 - .0016 * smooth(.1, .95, t) + .0012 * smooth(.86, 1, t);
    const b = .0172 - .0042 * smooth(.05, .7, t) + .0014 * smooth(.88, 1, t);
    const swell = .0006 * Math.pow(Math.abs(Math.cos(ang)), 3) * smooth(.20, .55, t) * (1 - smooth(.80, 1, t));
    return sup(ang, a + swell, b, 3.6);
  }, M.hero_stipple, { radial: 40, samples: 112, up: [1, 0, 0], vRep: 6,
    wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 4) * smooth(.15, .5, t) * .5 }));
  g.add(revolve(THREE, id + '_grip_cap', [[0, 0], [.0132, 0], [.0142, .0030], [.0142, .0062], [0, .0062]],
    M.hero_polymer, { segs: 28, pos: [0, -.0588, .0552], rot: [-1.14, 0, 0] }));
  /* Trigger guard: interior 34 × 33 mm, bar 17 mm wide × 6.5 mm thick, both
     legs buried in the frame and the grip's front strap. */
  g.add(sweep(THREE, id + '_trigger_guard', [
    [0, FYB + .0080, -.0380], [0, .0140, -.0416], [0, .0000, -.0402], [0, -.0104, -.0330],
    [0, -.0142, -.0230], [0, -.0134, -.0120], [0, -.0074, -.0030], [0, .0090, .0030],
    [0, FYB + .0080, .0080],
  ], (t, ang) => {
    const end = 1 - Math.sin(Math.PI * t) * .80;
    return sup(ang, .0076 + .0020 * end, .0032 + .0024 * end, 4);
  }, ST, { radial: 34, samples: 132, up: [1, 0, 0], vRep: 9, wear: (t) => Math.sin(Math.PI * t) * .2 }));
  g.add(sweep(THREE, id + '_trigger', [
    [0, .0400, -.0090], [0, .0240, -.0110], [0, .0090, -.0134], [0, -.0016, -.0158], [0, -.0088, -.0182], [0, -.0114, -.0212],
  ], (t, ang) => {
    const a = .0028 + .0005 * smooth(.20, .60, t);
    const groove = .00032 * Math.max(0, 1 - Math.abs(((t * 6) % 1) * 2 - 1) * 2.4)
                 * smooth(.34, .48, t) * Math.max(0, -Math.cos(ang));
    return [a, .0020 + .0003 * smooth(.2, .7, t) - groove];
  }, M.hero_steel_bright, { radial: 22, samples: 64, up: [1, 0, 0], vRep: 4,
    wear: (t, ang) => smooth(.45, .85, t) * Math.max(0, -Math.cos(ang)) * .55 }));
  g.add(revolve(THREE, id + '_trigger_pin', [[0, -.0092], [.0022, -.0092], [.0022, .0092], [0, .0092]],
    M.hero_steel, { segs: 16, pos: [0, .0372, -.0086], rot: [0, Math.PI / 2, 0] }));

  /* Canister latch on the left flank, at the regulator seat \u2014 the control the
     reload presses before the vessel comes off. Named to the armoury's contract
     so reload code finds it on every platform. */
  g.add(revolve(THREE, id + '_mag_release', [
    [0, 0], [.0052, 0], [.0052, .0030], [.0044, .0040], [0, .0040],
  ], M.hero_brass, { segs: 26, pos: [-.0132, CAN_Y - CAN_R + .0046, CAN_Z0 + .0090], rot: [0, -Math.PI / 2, 0],
    wear: (r) => smooth(.0044, .0052, r) * .5 }));
  g.add(revolve(THREE, id + '_mag_release_boss', [
    [0, 0], [.0080, 0], [.0080, .0018], [.0066, .0026], [0, .0026],
  ], M.hero_brushed, { segs: 26, pos: [-.0126, CAN_Y - CAN_R + .0046, CAN_Z0 + .0090], rot: [0, -Math.PI / 2, 0] }));

  /* ── pressure gauge on the left flank ────────────────────────────────── */
  g.add(revolve(THREE, id + '_gauge_body', [
    [0, 0], [.0092, 0], [.0100, .0016], [.0100, .0058], [.0086, .0070], [0, .0070],
  ], M.hero_brass, { segs: 32, creaseAngle: 30, pos: [-FHW + .0008, BORE + .0010, .0180], rot: [0, -Math.PI / 2, 0] }));
  g.add(revolve(THREE, id + '_gauge_face', [[0, 0], [.0078, 0]], M.hero_ember,
    { segs: 32, pos: [-FHW - .0062, BORE + .0010, .0180], rot: [0, -Math.PI / 2, 0] }));

  /* ── mounts ──────────────────────────────────────────────────────────── */
  const mount = (slot, pos, parent = g) => { const m = grp(`${id}_mount_${slot}`, pos); m.userData.slot = slot; parent.add(m); };
  mount('barrel', [0, BORE, FZ0]);
  mount('muzzle', [0, BORE, MUZZLE]);
  mount('optic', [0, FYT + .0038, .0200]);
  mount('magazine', [0, CAN_Y, CAN_Z1]);
  mount('magwell', [0, CAN_Y, CAN_Z0]);
  mount('stock', [0, BORE, FZ1]);
  mount('underbarrel', [0, BORE - .0150, FZ0 - .0600]);
  mount('infusion', [-FHW - .0010, BORE - .0060, -.0060]);
  return g;
}
