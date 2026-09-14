/**
 * Deep Field 3D — melee platforms, hero standard (DESIGN-BRIEF §4; Melee.cs).
 *
 * The three platforms the sim already sells — Blade, Maul, Spear — and the eight
 * module models its attachment table names. Same discipline as the Sidearm:
 * measured against the real class, detail cut IN, parts continuous, wear from
 * the geometry's own high points.
 *
 *   blade   tactical short sword: 560 mm clip-point blade, 42 mm at the
 *           ricasso, 6.2 mm spine with distal taper to 2.2 mm, fuller cut into
 *           both flats, oval guard, G10 grip on two through-bolts, steel pommel.
 *   maul    10 lb double-faced sledge: 180 × 55 × 55 forged head with crowned
 *           faces, fibreglass-core haft 810 mm with steel overstrike collar and
 *           two overmoulded grip zones.
 *   spear   2.24 m boar spear: 320 mm leaf head on a median ridge, coned
 *           socket with wings, 30 mm composite shaft, cord grips, butt spike.
 *
 * Metres, Y-up, working end toward −Z, origin at the grip web of the primary
 * hand. Every platform exposes the five melee module slots as empty mount nodes
 * (`<id>_mount_<slot>`: edge · grip · infusion · counterweight · chargecell),
 * the same contract as the gunsmith. Edge modules REPLACE `<id>_edge`, grip
 * modules REPLACE `<id>_grip`; the rest attach.
 */
import { heroSurfaces, loft, sweep, revolve, engrave, clamp, smooth, lerp } from './gun-kit.js';

const span = (a, b, step) => { const o = []; for (let v = a; v <= b + 1e-9; v += step) o.push(v); return o; };
const sup = (ang, a, b, n) => 1 / Math.pow(Math.pow(Math.abs(Math.cos(ang) / a), n) + Math.pow(Math.abs(Math.sin(ang) / b), n), 1 / n);
const bump = (x) => { const a = Math.abs(x); return a >= 1 ? 0 : (1 - a * a) * (1 - a * a); };

/** Status glasses the melee cores need beyond the gun kit's ember. Guarded. */
function meleeSurfaces(THREE, mats) {
  heroSurfaces(THREE, mats);
  if (mats.hero_toxin_glass) return mats;
  const std = (name, o) => { const m = new THREE.MeshStandardMaterial({ vertexColors: true, ...o }); m.name = name; mats[name] = m; return m; };
  const glass = (name, base, em, i) => std(name, { color: base, metalness: 0, roughness: .10, emissive: new THREE.Color(em), emissiveIntensity: i, transparent: true, opacity: .84, envMapIntensity: .5 });
  glass('hero_cryo_glass', 0x0b2a3a, 0x4fc0e8, 1.1);
  glass('hero_volt_glass', 0x2a0a2a, 0xf05ae6, 1.2);
  glass('hero_toxin_glass', 0x0c2a0c, 0x7fe65a, 1.1);
  /* Tungsten counterweight: heavier grey than steel, near-mirror where the
     fingers spin it on. */
  std('hero_tungsten', { color: 0x34373c, metalness: .96, roughness: .30, envMapIntensity: .9 });
  /* Waxed cord wrap — cloth, dead matte. */
  std('hero_cord', { color: 0x1a1a1c, metalness: 0, roughness: .92, envMapIntensity: .05 });
  return mats;
}

/* ══ BLADE ═══════════════════════════════════════════════════════════════ */
const B = { len: .560, ys: .0120, yb: -.0300, t: .0062, guardZ: .000, gripZ0: .014, gripZ1: .140, pommelZ: .160 };

function bladeGeom(z) {
  const u = clamp(z / -B.len, 0, 1);                            // 0 ricasso → 1 point
  const ys = B.ys - .0108 * Math.pow(smooth(.68, 1, u), 1.3);   // clip point drops the spine over the last third
  const yb = B.yb + .0292 * Math.pow(smooth(.58, 1, u), 1.5);   // belly rises to meet it
  const t = B.t - .0040 * u;                                     // distal taper
  return { u, ys, yb, t };
}
function bladeStation(z) {
  const { ys, yb, t } = bladeGeom(z);
  const h = ys - yb, yBev = yb + h * .20;
  const fz = smooth(-.44, -.42, z) * (1 - smooth(-.08, -.06, z));   // fuller runs −0.42 … −0.08
  const fuller = (y) => fz * .0009 * bump((y - .0015) / .0052);
  const pts = [[0, yb]];
  for (let i = 0; i <= 9; i++) { const y = lerp(yBev, ys - .0012, i / 9); pts.push([t / 2 - (i ? fuller(y) : 0), y]); }
  pts.push([t * .30, ys], [-t * .30, ys]);
  for (let i = 9; i >= 0; i--) { const y = lerp(yBev, ys - .0012, i / 9); pts.push([-(t / 2 - (i ? fuller(y) : 0)), y]); }
  return { z, pts };
}
/** The whetted edge line in host frame — shared by the stock edge and the edge modules. */
function bladeEdgePath(step = .012) {
  return span(-B.len + .012, -.010, step).map((z) => { const { yb } = bladeGeom(z); return [0, yb + .0006, z]; });
}

function buildBlade(K) {
  const { THREE, mats: M, grp } = K, id = 'blade', g = grp(id);
  g.add(loft(THREE, id + '_blade', span(-B.len, 0, .004).map(bladeStation), M.hero_brushed,
    { creaseFrom: bladeStation(-.20).pts, creaseAngle: 24, uScale: 40,
      wear: (x, y, z) => { const { yb, ys } = bladeGeom(z); return smooth(yb + .012, yb + .003, y) * .45 + smooth(ys - .002, ys, y) * .18; } }));
  g.add(sweep(THREE, id + '_edge', bladeEdgePath(), (t) => { const k = 1 - .55 * smooth(.86, 1, t); return [.0011 * k, .0015 * k]; }, M.hero_steel_bright,
    { radial: 12, samples: 90, up: [1, 0, 0], vRep: 40, wear: () => .7 }));
  g.add(engrave(THREE, id + '_rollmark', 'DF-19', M.hero_engrave, { x: -B.t / 2 + .0002, z: -.020, y: -.010, cap: .0036, plane: 'left' }));
  /* Oval guard, crowned: the tang passes through it into the grip. */
  g.add(loft(THREE, id + '_guard', span(B.guardZ - .002, B.gripZ0, .0008).map((z) => {
    const u = (z - B.guardZ + .002) / (B.gripZ0 - B.guardZ + .002), crown = Math.sin(Math.PI * u);
    const a = .0150 + .0022 * crown, b = .0340 + .0030 * crown;
    return { z, pts: Array.from({ length: 36 }, (_, i) => { const ang = i / 36 * Math.PI * 2; const r = sup(ang, a, b, 2.6); return [Math.cos(ang) * r, Math.sin(ang) * r - .0060]; }) };
  }), M.hero_dlc, { creaseAngle: 40, wear: (x, y) => smooth(.034, .038, Math.abs(y + .006)) * .35 }));
  /* G10 grip: superelliptic section with a palm swell and three finger grooves cut in. */
  g.add(sweep(THREE, id + '_grip', [[0, 0, B.gripZ0], [0, -.001, .050], [0, -.003, .095], [0, -.004, B.gripZ1]], (t, ang) => {
    const groove = .0007 * Math.pow(Math.max(0, Math.sin(t * Math.PI * 3.5 - .6)), 6) * Math.max(0, -Math.sin(ang));
    const swell = .0012 * Math.sin(Math.PI * t);
    return sup(ang, .0128 + swell * .5, .0168 + swell, 3.4) - groove;
  }, M.hero_stipple, { radial: 40, samples: 64, up: [1, 0, 0], vRep: 5, wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 4) * .35 }));
  for (const z of [.045, .112]) for (const s of [-1, 1]) {
    g.add(revolve(THREE, `${id}_bolt_${z}_${s}`, [[0, 0], [.0034, 0], [.0038, .0008], [.0038, .0018], [.0026, .0024], [0, .0024]], M.hero_steel,
      { segs: 20, pos: [s * .0126, -.002, z], rot: [0, s * Math.PI / 2, 0], wear: (r) => smooth(.0030, .0038, r) * .5 }));
  }
  /* Pommel: turned steel, lanyard groove, threaded for a counterweight. */
  g.add(revolve(THREE, id + '_pommel', [[0, B.gripZ1 - .004], [.0130, B.gripZ1 - .004], [.0148, B.gripZ1 + .002], [.0148, B.pommelZ - .008], [.0132, B.pommelZ - .006], [.0132, B.pommelZ - .003], [.0140, B.pommelZ - .002], [.0110, B.pommelZ], [0, B.pommelZ]],
    M.hero_dlc, { segs: 48, creaseAngle: 30, pos: [0, -.004, 0], wear: (r, z) => smooth(.0140, .0148, r) * .45 + (z > B.pommelZ - .003 ? .3 : 0) }));
  mounts(K, g, id, { edge: [0, -.0300, -.280], grip: [0, -.002, .077], infusion: [-B.t / 2, -.004, -.046], counterweight: [0, -.004, B.pommelZ], chargecell: [0, B.ys, -.024] });
  return g;
}

/* ══ MAUL ════════════════════════════════════════════════════════════════ */
const H = { z: -.600, half: .090, hw: .0275, haft0: -.612, haft1: .200 };

function maulHeadStation(z) {                 // local z runs along the head (world X)
  const az = Math.abs(z), crown = smooth(.070, H.half, az);
  const hw = H.hw - .0058 * Math.pow(crown, 1.6) - .0014 * smooth(.060, .070, az);
  const notch = .0040 * (1 - smooth(.016, .020, az));           // charge-cell pocket in the top face
  return { z, pts: Array.from({ length: 48 }, (_, i) => {
    const ang = i / 48 * Math.PI * 2, r = sup(ang, hw, hw, 5.5);
    let x = Math.cos(ang) * r, y = Math.sin(ang) * r;
    if (y > 0 && Math.abs(x) < .0115) y -= notch * bump(x / .0125) / bump(0);
    return [x, y];
  }) };
}
function buildMaul(K) {
  const { THREE, mats: M, grp } = K, id = 'maul', g = grp(id);
  g.add(loft(THREE, id + '_head', span(-H.half, H.half, .002).map(maulHeadStation), M.hero_park_dark,
    { creaseFrom: maulHeadStation(0).pts, creaseAngle: 32, pos: [0, 0, H.z], rot: [0, Math.PI / 2, 0],
      wear: (x, y, z) => smooth(.076, H.half, Math.abs(z)) * .85 + smooth(H.hw - .003, H.hw, Math.abs(y)) * .12 }));
  /* Haft: fibreglass core in a polymer jacket. Oval, swelling toward the butt,
     rooted 12 mm inside the head so the eye reads as through. */
  g.add(loft(THREE, id + '_haft', span(H.haft0, H.haft1, .005).map((z) => {
    const u = (z - H.haft0) / (H.haft1 - H.haft0);
    const a = .0170 + .0030 * u + .0016 * smooth(.92, 1, u), b = .0135 + .0030 * u + .0016 * smooth(.92, 1, u);
    return { z, pts: Array.from({ length: 32 }, (_, i) => { const ang = i / 32 * Math.PI * 2, r = sup(ang, a, b, 2.4); return [Math.cos(ang) * r, Math.sin(ang) * r]; }) };
  }), M.hero_polymer, { creaseAngle: 60, wear: (x, y, z) => smooth(-.20, -.30, z) * (1 - smooth(-.45, -.55, z)) * .25 }));
  /* Overstrike collar: steel sleeve where a missed swing lands. */
  g.add(loft(THREE, id + '_collar', span(H.z + .030, H.z + .120, .003).map((z) => {
    const u = (z - H.z - .030) / .090, a = .0200 - .0018 * smooth(.85, 1, u), b = .0165 - .0018 * smooth(.85, 1, u);
    return { z, pts: Array.from({ length: 32 }, (_, i) => { const ang = i / 32 * Math.PI * 2, r = sup(ang, a, b, 2.4); return [Math.cos(ang) * r, Math.sin(ang) * r]; }) };
  }), M.hero_steel, { creaseAngle: 60, wear: (x, y, z) => (1 - smooth(H.z + .040, H.z + .080, z)) * .6 }));
  /* Two overmoulded grip zones, finger grooves cut in, the rear one flared at the butt. */
  const gripZone = (name, z0, z1, a, b, flare) => sweep(THREE, name, [[0, 0, z0], [0, 0, (z0 + z1) / 2], [0, 0, z1]], (t, ang) => {
    const groove = .0009 * Math.pow(Math.max(0, Math.sin(t * Math.PI * 8)), 8);
    const f = flare ? .0026 * smooth(.86, 1, t) : 0;
    return sup(ang, a + f, b + f, 2.6) - groove;
  }, M.hero_stipple, { radial: 36, samples: 72, up: [1, 0, 0], vRep: 6, wear: (t, ang) => Math.pow(Math.abs(Math.sin(ang)), 3) * .30 });
  g.add(gripZone(id + '_grip', -.100, .192, .0220, .0185, true));
  g.add(gripZone(id + '_foregrip', -.380, -.240, .0205, .0170, false));
  g.add(revolve(THREE, id + '_butt', [[0, H.haft1 - .004], [.0196, H.haft1 - .004], [.0216, H.haft1 + .002], [.0216, H.haft1 + .010], [.0180, H.haft1 + .013], [0, H.haft1 + .013]],
    M.hero_dlc, { segs: 48, creaseAngle: 30, wear: (r, z) => (z > H.haft1 + .011 ? .35 : 0) + smooth(.0206, .0216, r) * .3 }));
  g.add(engrave(THREE, id + '_rollmark', '10 LB FORGED', M.hero_engrave, { x: -.0188, z: .060, y: -.0022, cap: .0034, plane: 'left' }));
  mounts(K, g, id, { edge: [H.half, 0, H.z], grip: [0, 0, .046], infusion: [0, .006, H.z - H.hw], counterweight: [0, 0, H.haft1 + .013], chargecell: [0, H.hw - .004, H.z] });
  return g;
}

/* ══ SPEAR ═══════════════════════════════════════════════════════════════ */
const S = { tip: -1.720, neck: -1.400, sock0: -1.400, sock1: -1.300, shaft1: .450, butt: .520, r0: .0165, r1: .0150 };

function spearGeom(z) {
  const u = clamp((z - S.tip) / (S.neck - S.tip), 0, 1);      // 0 tip → 1 neck
  const hw = lerp(.0025, .0120, u) + .0240 * Math.pow(Math.sin(Math.PI * u), 1.15);
  const t2 = .0022 + .0034 * u;
  return { u, hw, t2 };
}
function spearStation(z) {
  const { hw, t2 } = spearGeom(z);
  return { z, pts: [[0, hw], [-t2 * .55, hw * .42], [-t2, 0], [-t2 * .55, -hw * .42], [0, -hw], [t2 * .55, -hw * .42], [t2, 0], [t2 * .55, hw * .42]] };
}
const spearEdgePath = (side, step = .008) => span(S.tip + .010, S.neck - .020, step).map((z) => [0, side * (spearGeom(z).hw - .0006), z]);
const shaftR = (z) => lerp(S.r0, S.r1, (z - S.sock1) / (S.shaft1 - S.sock1));

function buildSpear(K) {
  const { THREE, mats: M, grp } = K, id = 'spear', g = grp(id);
  g.add(loft(THREE, id + '_head', span(S.tip, S.neck, .004).map(spearStation), M.hero_brushed,
    { creaseAngle: 20, wear: (x, y, z) => { const { hw } = spearGeom(z); return smooth(hw * .55, hw * .95, Math.abs(y)) * .45 + (Math.abs(x) > spearGeom(z).t2 * .9 ? .2 : 0); } }));
  const edge = grp(id + '_edge');
  for (const s of [-1, 1]) edge.add(sweep(THREE, `${id}_edge_${s < 0 ? 'l' : 'r'}`, spearEdgePath(s), () => [.0010, .0013], M.hero_steel_bright, { radial: 12, samples: 80, up: [1, 0, 0], vRep: 30, wear: () => .7 }));
  g.add(edge);
  /* Coned socket with a rolled lip; the neck of the head is turned into it. */
  g.add(revolve(THREE, id + '_socket', [[0, S.sock0 - .006], [.0092, S.sock0 - .006], [.0104, S.sock0], [.0150, S.sock1 - .020], [.0176, S.sock1 - .006], [.0186, S.sock1 - .002], [.0186, S.sock1 + .004], [.0176, S.sock1 + .008], [.0166, S.sock1 + .012], [0, S.sock1 + .012]],
    M.hero_steel, { segs: 56, creaseAngle: 30, wear: (r, z) => smooth(.0176, .0186, r) * .4 + (z < S.sock0 ? .3 : 0) }));
  /* Wings: forged lugs that root inside the socket wall. */
  for (const s of [-1, 1]) {
    g.add(sweep(THREE, `${id}_wing_${s < 0 ? 'l' : 'r'}`, [[s * .004, 0, S.sock1 - .026], [s * .018, 0, S.sock1 - .034], [s * .034, 0, S.sock1 - .046], [s * .042, 0, S.sock1 - .058]], (t, ang) => {
      const taper = 1 - .55 * smooth(.3, 1, t);
      return [.0032 * taper, .0068 * taper];
    }, M.hero_steel, { radial: 18, samples: 30, up: [0, 0, 1], vRep: 3, wear: (t) => smooth(.6, 1, t) * .5 }));
  }
  g.add(revolve(THREE, id + '_socket_pin', [[0, -.020], [.0022, -.020], [.0022, .020], [0, .020]], M.hero_steel_bright, { segs: 14, pos: [0, 0, S.sock1 - .012], rot: [0, Math.PI / 2, 0] }));
  /* Composite shaft: a slow taper to the butt, turned, so the UVs follow arc length. */
  g.add(revolve(THREE, id + '_shaft', [[0, S.sock1 - .010], [S.r0, S.sock1 - .010], [S.r0 * .99, S.sock1 + .30], [shaftR(-.60), -.60], [shaftR(0), 0], [shaftR(.30), .30], [S.r1, S.shaft1 + .004], [0, S.shaft1 + .004]],
    M.hero_polymer, { segs: 48, creaseAngle: 60 }));
  /* Cord wraps: turns of waxed cord read as ridges in the section, over-wrapped at the ends. */
  const wrap = (name, z0, z1) => sweep(THREE, name, [[0, 0, z0], [0, 0, (z0 + z1) / 2], [0, 0, z1]], (t, ang) => {
    const turns = (z1 - z0) / .0038, ridge = .00055 * Math.abs(Math.sin(t * Math.PI * turns + ang * .5));
    const end = smooth(0, .06, t) * (1 - smooth(.94, 1, t));
    return shaftR(lerp(z0, z1, t)) + .0014 + ridge + .0012 * (1 - end);
  }, M.hero_cord, { radial: 32, samples: 110, up: [1, 0, 0], vRep: 14, wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 3) * .25 });
  g.add(wrap(id + '_grip', -.060, .160));
  g.add(wrap(id + '_foregrip', -.600, -.420));
  /* Butt: ferrule and spike. */
  g.add(revolve(THREE, id + '_butt', [[0, S.shaft1 - .010], [S.r1 + .0012, S.shaft1 - .010], [S.r1 + .0022, S.shaft1], [S.r1 + .0022, S.shaft1 + .020], [.0100, S.shaft1 + .030], [.0024, S.butt - .004], [0, S.butt]],
    M.hero_steel, { segs: 40, creaseAngle: 30, wear: (r, z) => smooth(S.shaft1 + .040, S.butt, z) * .6 }));
  mounts(K, g, id, { edge: [0, .0300, -1.560], grip: [0, 0, .050], infusion: [0, .0186, S.sock1 - .010], counterweight: [0, 0, S.shaft1 + .020], chargecell: [0, -shaftR(-.70), -.700] });
  return g;
}

function mounts(K, g, id, table) {
  for (const [slot, pos] of Object.entries(table)) { const m = K.grp(`${id}_mount_${slot}`, pos); m.userData.slot = slot; g.add(m); }
}
/** Shift a module built in host frame so its origin lands ON the mount. */
function atMount(g, host, slot) {
  const p = MOUNTS[host][slot];
  for (const c of g.children) { c.position.x -= p[0]; c.position.y -= p[1]; c.position.z -= p[2]; }
  return g;
}
const MOUNTS = {
  blade: { edge: [0, -.0300, -.280], grip: [0, -.002, .077], infusion: [-B.t / 2, -.004, -.046], counterweight: [0, -.004, B.pommelZ], chargecell: [0, B.ys, -.024] },
  maul: { edge: [H.half, 0, H.z], grip: [0, 0, .046], infusion: [0, .006, H.z - H.hw], counterweight: [0, 0, H.haft1 + .013], chargecell: [0, H.hw - .004, H.z] },
  spear: { edge: [0, .0300, -1.560], grip: [0, 0, .050], infusion: [0, .0186, S.sock1 - .010], counterweight: [0, 0, S.shaft1 + .020], chargecell: [0, -shaftR(-.70), -.700] },
};

/* ══ MODULES ═════════════════════════════════════════════════════════════
   Built per host, origin on the mount (`mount.add(mod)`), like the gunsmith's
   attachments. Edge and grip modules replace the stock part of that name. */
function edgeModule(K, host, kind) {
  const { THREE, mats: M, grp } = K, g = grp(`meleemod_edge_${kind}`), M2 = M.hero_steel_bright;
  const honed = kind === 'honed';
  const rFn = honed
    ? () => [.0014, .0022]
    : (t) => { const ph = (t * 46) % 1, tri = ph < .5 ? ph * 2 : 2 - ph * 2; return [.0009, .0005 + .0032 * Math.pow(tri, 1.6)]; };
  if (host === 'blade') g.add(sweep(THREE, `${g.name}_strip`, bladeEdgePath(.006), rFn, M2, { radial: 12, samples: 220, up: [1, 0, 0], vRep: 60, wear: () => honed ? 1 : .6 }));
  if (host === 'spear') for (const s of [-1, 1]) g.add(sweep(THREE, `${g.name}_strip_${s < 0 ? 'l' : 'r'}`, spearEdgePath(s, .004), rFn, M2, { radial: 12, samples: 180, up: [1, 0, 0], vRep: 40, wear: () => honed ? 1 : .6 }));
  if (host === 'maul') {
    /* Face caps on both striking faces: honed = hardened crowned insert; serrated = waffle face, grooves turned in. */
    for (const s of [-1, 1]) {
      const prof = honed
        ? [[0, 0], [.0215, 0], [.0225, .0012], [.0225, .0040], [.0180, .0060], [.0100, .0072], [0, .0076]]
        : [[0, 0], [.0225, 0], [.0225, .0034], ...Array.from({ length: 9 }, (_, i) => [.0225 - i * .0025, i % 2 ? .0034 : .0060]), [0, .0060]];
      g.add(revolve(THREE, `${g.name}_face_${s < 0 ? 'l' : 'r'}`, prof, M2, { segs: 40, creaseAngle: 30, pos: [s * (H.half - .0015), 0, H.z], rot: [0, s * Math.PI / 2, 0], wear: (r, z) => (z > .0058 ? .9 : .3) }));
    }
  }
  return atMount(g, host, 'edge');
}
function gripModule(K, host) {
  const { THREE, mats: M, grp } = K, g = grp('meleemod_grip_balanced');
  const T = { blade: { z0: B.gripZ0, z1: B.gripZ1, a: .0125, b: .0165, n: 3.4, path: [[0, 0], [0, -.001], [0, -.003], [0, -.004]] },
    maul: { z0: -.100, z1: .192, a: .0215, b: .0180, n: 2.6 }, spear: { z0: -.060, z1: .160, a: .0180, b: .0180, n: 2 } }[host];
  const path = (T.path || [[0, 0], [0, 0], [0, 0], [0, 0]]).map((p, i) => [p[0], p[1], lerp(T.z0, T.z1, i / 3)]);
  /* Machined alloy sleeve: deep finger grooves turned in, a balance ring at each end, knurl between. */
  g.add(sweep(THREE, `${g.name}_sleeve`, path, (t, ang) => {
    const groove = .0012 * Math.pow(Math.max(0, Math.sin(t * Math.PI * 5.5 - .4)), 4) * smooth(.08, .14, t) * (1 - smooth(.86, .92, t));
    const knurl = .00025 * Math.abs(Math.sin(ang * 14 + t * 160)) * smooth(.12, .18, t) * (1 - smooth(.82, .88, t));
    const ring = .0016 * ((1 - smooth(.04, .08, t)) + smooth(.92, .96, t));
    return sup(ang, T.a, T.b, T.n) - groove + knurl + ring;
  }, M.hero_brushed, { radial: 48, samples: 96, up: [1, 0, 0], vRep: 8, wear: (t, ang) => Math.pow(Math.abs(Math.cos(ang)), 3) * .3 }));
  return atMount(g, host, 'grip');
}
function counterweightModule(K, host) {
  const { THREE, mats: M, grp } = K, g = grp('meleemod_counterweight_heavy');
  const r = { blade: .0140, maul: .0216, spear: .0172 }[host], p = MOUNTS[host].counterweight;
  /* Screw-on tungsten slug, knurled band so it can be spun on by hand. */
  const prof = [[0, 0], [r * .62, 0], [r * .62, .0016], [r, .0016], [r, .0110], ...Array.from({ length: 10 }, (_, i) => [i % 2 ? r : r - .0007, .0110 + i * .0008]), [r, .0200], [r * .86, .0228], [r * .40, .0240], [0, .0240]];
  g.add(revolve(THREE, `${g.name}_slug`, prof, M.hero_tungsten, { segs: 56, creaseAngle: 30, pos: [p[0], p[1], p[2]], wear: (rr, z) => smooth(.011, .019, z) * .5 + (z > .0236 ? .4 : 0) }));
  return atMount(g, host, 'counterweight');
}
function infusionModule(K, host, kind) {
  const { THREE, mats: M, grp } = K, g = grp(`meleemod_infusion_${kind}`), p = MOUNTS[host].infusion;
  const glass = { ember: M.hero_ember_glass, cryo: M.hero_cryo_glass, volt: M.hero_volt_glass, toxin: M.hero_toxin_glass }[kind];
  /* The mount normal differs per host: the blade's is the left flat (−X), the
     maul's the head's front face (−Z), the spear's the socket top (+Y). */
  const rot = { blade: [0, -Math.PI / 2, 0], maul: [Math.PI, 0, 0], spear: [-Math.PI / 2, 0, 0] }[host];
  const cell = grp(`${g.name}_cell`, p); cell.rotation.set(...rot);
  /* Clamp saddle, then a turned cell standing off it along local +Z: brass end
     rings, a glass barrel showing the charge, a bleed valve on the cap. */
  cell.add(revolve(THREE, `${g.name}_saddle`, [[0, 0], [.0110, 0], [.0118, .0012], [.0118, .0034], [.0090, .0044], [0, .0044]], M.hero_dlc, { segs: 32, creaseAngle: 30 }));
  cell.add(revolve(THREE, `${g.name}_ring_a`, [[0, .0044], [.0084, .0044], [.0092, .0056], [.0092, .0090], [.0074, .0100], [0, .0100]], M.hero_brass, { segs: 32, creaseAngle: 30, wear: (r) => smooth(.0086, .0092, r) * .45 }));
  cell.add(revolve(THREE, `${g.name}_glass`, [[0, .0100], [.0072, .0100], [.0076, .0150], [.0076, .0260], [.0072, .0310], [0, .0310]], glass, { segs: 32, creaseAngle: 40 }));
  cell.add(revolve(THREE, `${g.name}_core`, [[0, .0110], [.0028, .0120], [.0030, .0290], [0, .0300]], glass, { segs: 16 }));
  cell.add(revolve(THREE, `${g.name}_ring_b`, [[0, .0310], [.0074, .0310], [.0092, .0320], [.0092, .0352], [.0084, .0364], [0, .0364]], M.hero_brass, { segs: 32, creaseAngle: 30, wear: (r) => smooth(.0086, .0092, r) * .45 }));
  cell.add(revolve(THREE, `${g.name}_valve`, [[0, .0364], [.0030, .0364], [.0030, .0400], [.0044, .0404], [.0044, .0424], [0, .0424]], M.hero_brass, { segs: 20, creaseAngle: 30 }));
  for (let i = 0; i < 3; i++) cell.add(revolve(THREE, `${g.name}_rib${i}`, [[.0076, .0150 + i * .0050], [.0084, .0155 + i * .0050], [.0076, .0160 + i * .0050]], M.hero_dlc, { segs: 32 }));
  g.add(cell);
  return atMount(g, host, 'infusion');
}

/* ══ registry ════════════════════════════════════════════════════════════ */
const P = (w) => ({ ...w, file: `melee_${w.id}_vm.glb`, worldFile: `melee_${w.id}_world.glb`, melee: true, ms: 'M4' });
export const MELEE = [
  P({ id: 'blade', label: 'Blade', swatch: '#8d949e', stats: { Recipe: 'Alloy 7', Damage: '19', Rate: '2.0/s', Reach: '2.4 m', Arc: '35°' },
    note: 'Fast and narrow — the single-target platform, and the one an infusion pays off on most. Measured against a tactical short sword: 560 mm clip-point blade, 42 mm at the ricasso, 6.2 mm spine with distal taper to 2.2 mm at the point. The fuller is cut INTO both flats in the loft profile, the whetted edge is its own bright part (`blade_edge`, which edge modules replace), the oval guard is crowned and the G10 grip carries a palm swell and three finger grooves as displacement on two through-bolts. Satin blade over a black guard and pommel: two finishes separated by surface. Wear rides the edge bevel and the spine crown.',
    build(K) { meleeSurfaces(K.THREE, K.mats); return buildBlade(K); } }),
  P({ id: 'maul', label: 'Maul', swatch: '#2a2f36', stats: { Recipe: 'Alloy 6 · Plating 4', Damage: '34', Rate: '0.8/s', Reach: '3.0 m', Arc: '80°', Knockback: '3.5 m' },
    note: 'Slow, wide, and it moves things. A 10 lb double-faced forged sledge head — 180 × 55 × 55, corners chamfered, both faces crowned so a strike lands on a dome, a charge-cell pocket milled into the top face — on an 810 mm fibreglass-core haft that roots 12 mm inside the eye. Steel overstrike collar where a missed swing lands, two overmoulded grip zones with finger grooves cut in, flared butt with a turned steel cap. Phosphate head, polymer haft, rubber grips: three matte blacks, three different surfaces. The faces burnish themselves.',
    build(K) { meleeSurfaces(K.THREE, K.mats); return buildMaul(K); } }),
  P({ id: 'spear', label: 'Spear', swatch: '#8d949e', stats: { Recipe: 'Alloy 5 · Flux 3', Damage: '22', Rate: '1.1/s', Reach: '4.2 m', Arc: '25°' },
    note: 'Reach — the platform that makes melee survivable against things that hurt to stand next to. Measured against a boar spear: 2.24 m overall, a 320 mm leaf head 62 mm at its widest on a median ridge (both edges are bright parts under `spear_edge`), turned into a coned socket with a rolled lip, forged wings that root inside the socket wall and a through-pin. 33→30 mm composite shaft turned as one profile, two waxed-cord grips whose turns read as ridges in the section, and a ferruled butt spike. Origin at the primary hand, so the off-hand grip sits 60 cm ahead of it.',
    build(K) { meleeSurfaces(K.THREE, K.mats); return buildSpear(K); } }),
];

const MOD = (id, slot, label, swatch, stats, note, build) => ({ id, slot, label, swatch, stats, note, file: `meleemod_${slot}_${id.replace(/^\w+?_/, '')}.glb`, melee: true, build: (K, host = 'blade') => build(K, host) });
export const MELEE_SLOTS = ['edge', 'grip', 'infusion', 'counterweight', 'chargecell'];
export const MELEE_MODS = [
  MOD('edge_honed', 'edge', 'Honed edge', '#c3ccd8', { Damage: '×1.18', Rate: '×0.92', Recipe: 'Alloy 5' },
    'Replaces `<host>_edge`: a wider mirror-polished bevel along the whole cutting line (both edges on the spear); on the maul it is a hardened crowned face insert on both striking faces.', (K, host) => { meleeSurfaces(K.THREE, K.mats); return edgeModule(K, host, 'honed'); }),
  MOD('edge_serrated', 'edge', 'Serrated edge', '#c3ccd8', { Damage: '×1.05', Rate: '×1.10', Recipe: 'Alloy 4 · Plating 1' },
    'Replaces `<host>_edge`: forty-six teeth as displacement along the edge sweep; the maul gets a waffle face with the grooves turned into the insert profile.', (K, host) => { meleeSurfaces(K.THREE, K.mats); return edgeModule(K, host, 'serrated'); }),
  MOD('grip_balanced', 'grip', 'Balanced grip', '#8d949e', { Rate: '×1.15', Recipe: 'Alloy 3' },
    'Replaces `<host>_grip`: a machined alloy sleeve with deep finger grooves turned in, a knurl band between them and a balance ring at each end, sized to each host\'s grip length and section.', (K, host) => { meleeSurfaces(K.THREE, K.mats); return gripModule(K, host); }),
  MOD('counterweight_heavy', 'counterweight', 'Heavy counterweight', '#34373c', { Damage: '×1.10', Rate: '×0.90', Reach: '×1.15', Recipe: 'Plating 3' },
    'Screw-on tungsten slug at the butt, knurled so it spins on by hand; diameter matched to each host\'s pommel.', (K, host) => { meleeSurfaces(K.THREE, K.mats); return counterweightModule(K, host); }),
  ...['ember', 'cryo', 'volt', 'toxin'].map((k) => MOD(`infusion_${k}`, 'infusion', { ember: 'Ember core', cryo: 'Cryo core', volt: 'Volt core', toxin: 'Toxin core' }[k],
    { ember: '#e8622b', cryo: '#4fc0e8', volt: '#f05ae6', toxin: '#7fe65a' }[k],
    { Applies: { ember: 'burn', cryo: 'chill', volt: 'shock', toxin: 'poison' }[k], Recipe: { ember: 'Flux 4', cryo: 'Flux 3 · Alloy 2', volt: 'Flux 5', toxin: 'Plating 3 · Flux 2' }[k] },
    'Core infusion: a turned cell on a clamp saddle — brass end rings, a glass barrel with the charge visible inside, three retaining ribs and a bleed valve on the cap. Same body for all four; only the glass changes. Stands off the ricasso (blade), the head\'s front face (maul) or the socket (spear).',
    (K, host) => { meleeSurfaces(K.THREE, K.mats); return infusionModule(K, host, k); })),
];
