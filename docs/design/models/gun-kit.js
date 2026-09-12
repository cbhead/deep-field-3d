/**
 * Deep Field 3D — shared modelling vocabulary for hero assets.
 *
 * Extracted from the Sidearm rebuild so every weapon, tower and prop is built
 * the same way. The generation this replaces glued box primitives onto
 * constant-section extrusions; nothing was curved, cut or textured, which is
 * exactly what "blocked" looks like.
 *
 *   loft()      sweep a CHANGING cross-section along a line. Stations taper,
 *               crown and swell, so surfaces are compound instead of constant.
 *               A per-vertex displacement hook cuts detail INTO the surface —
 *               serrations, milled pockets, optic cuts, witness holes, M-LOK —
 *               rather than stacking proud boxes on top of it. Profile winding
 *               is auto-corrected, so callers need not care which way round
 *               they authored a section.
 *   sweep()     sweep a section along an arbitrary 2D path — guard bows, finger
 *               bones, forearms. rFn(t, ang) may return a scalar polar radius:
 *               use a superellipse for squared sections, because a round radius
 *               is what makes a part read as bent rod.
 *   revolve()   turned parts. UVs follow profile ARC LENGTH, never z — on a flat
 *               face every z is equal and a z-based v smears the map into
 *               radial streaks.
 *   engrave()   stroke-font marks cut into a flat.
 *
 * Creases are explicit: a profile vertex whose turn angle exceeds the threshold
 * gets split, so the edge stays razor-crisp while the faces either side of it
 * shade smooth. That combination — crisp machined edges, smooth curved faces —
 * is what reads as "machined product render" instead of "faceted low-poly".
 *
 * Surface is procedural PBR drawn on canvas at build time. Honest wear rides in
 * vertex colour: geometry knows where its own high points are, so serration
 * lands, muzzle crowns, control faces and palm-rubbed stipple burnish
 * themselves without a hand-painted mask.
 *
 * Metres, Y-up, muzzle toward −Z. See CLAUDE.md for the binding rules.
 */

/* ══ procedural surface library ═══════════════════════════════════════════ */

const H = (x, y) => { const n = Math.sin(x * 127.1 + y * 311.7) * 43758.5453; return n - Math.floor(n); };
function vnoise(x, y) {
  const xi = Math.floor(x), yi = Math.floor(y), xf = x - xi, yf = y - yi;
  const u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
  return (H(xi, yi) * (1 - u) + H(xi + 1, yi) * u) * (1 - v) + (H(xi, yi + 1) * (1 - u) + H(xi + 1, yi + 1) * u) * v;
}
function fbm(x, y, oct = 4, lac = 2.1, gain = .5) {
  let a = .5, f = 1, s = 0, n = 0;
  for (let i = 0; i < oct; i++) { s += a * vnoise(x * f, y * f); n += a; a *= gain; f *= lac; }
  return s / n;
}
const cv = (w, h = w) => { const c = document.createElement('canvas'); c.width = w; c.height = h; return c; };
const clamp = (v, a, b) => v < a ? a : v > b ? b : v;
const smooth = (e0, e1, x) => { const t = clamp((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t); };
const lerp = (a, b, t) => a + (b - a) * t;

/** Value-noise field rendered small and upscaled — full-res fbm is needlessly slow. */
function noiseCanvas(size, small, fn) {
  const c = cv(small), ctx = c.getContext('2d'), im = ctx.createImageData(small, small);
  for (let y = 0; y < small; y++) for (let x = 0; x < small; x++) {
    const v = clamp(fn(x / small, y / small), 0, 1) * 255, i = (y * small + x) * 4;
    im.data[i] = im.data[i + 1] = im.data[i + 2] = v; im.data[i + 3] = 255;
  }
  ctx.putImageData(im, 0, 0);
  const big = cv(size), bx = big.getContext('2d');
  bx.imageSmoothingEnabled = true; bx.drawImage(c, 0, 0, size, size);
  return big;
}
/** Height canvas → tangent-space normal map (Sobel). */
function toNormal(THREE, src, strength = 1) {
  const n = src.width, sx = src.getContext('2d').getImageData(0, 0, n, n).data;
  const out = cv(n), ox = out.getContext('2d'), im = ox.createImageData(n, n);
  const g = (x, y) => sx[(((y + n) % n) * n + ((x + n) % n)) * 4] / 255;
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    const dx = (g(x + 1, y - 1) + 2 * g(x + 1, y) + g(x + 1, y + 1)) - (g(x - 1, y - 1) + 2 * g(x - 1, y) + g(x - 1, y + 1));
    const dy = (g(x - 1, y + 1) + 2 * g(x, y + 1) + g(x + 1, y + 1)) - (g(x - 1, y - 1) + 2 * g(x, y - 1) + g(x + 1, y - 1));
    let vx = -dx * strength, vy = -dy * strength, vz = 1;
    const l = Math.hypot(vx, vy, vz); vx /= l; vy /= l; vz /= l;
    const i = (y * n + x) * 4;
    im.data[i] = (vx * .5 + .5) * 255; im.data[i + 1] = (vy * .5 + .5) * 255; im.data[i + 2] = (vz * .5 + .5) * 255; im.data[i + 3] = 255;
  }
  ox.putImageData(im, 0, 0);
  return out;
}
function texture(THREE, canvas, rep = 1, srgb = false) {
  const t = new THREE.CanvasTexture(canvas);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rep, rep); t.anisotropy = 16;
  if (srgb && THREE.SRGBColorSpace) t.colorSpace = THREE.SRGBColorSpace;
  else if (THREE.NoColorSpace) t.colorSpace = THREE.NoColorSpace;
  return t;
}

/* Roughness maps are drawn at 2K (cheap — canvas ops). Normal maps are derived
   through a per-pixel Sobel pass, so they are generated at 512 and tiled at a
   high repeat instead: identical texel density on parts this small, a quarter
   of the cost, and the viewer paints without a long blank frame. */
const TEX = 2048, NRM = 512;

/**
 * Registers the hero material family on K.mats. Guarded — the Gunsmith rebuilds
 * the model on every click and these maps are expensive to draw.
 */
export function heroSurfaces(THREE, mats) {
  if (mats.hero_dlc) return mats;
  const std = (name, o) => { const m = new THREE.MeshStandardMaterial({ vertexColors: true, ...o }); m.name = name; mats[name] = m; return m; };
  const cloth = (name, o) => { const m = new THREE.MeshStandardMaterial({ vertexColors: true, metalness: 0, ...o }); m.name = name; mats[name] = m; return m; };
  // Physical variant: anodising is a sealed oxide layer over metal, so it wants
  // a weak clearcoat lobe on top of the base specular — that second, tighter
  // highlight is a large part of what separates anodised alloy from paint.
  const phys = (name, o) => { const m = new THREE.MeshPhysicalMaterial({ vertexColors: true, ...o }); m.name = name; mats[name] = m; return m; };

  /* DLC black slide — cold hard black, fine draw-polish streaks running the
     length of the flats, micro-grain normal. Roughness variance is what stops
     a black metal reading as a black plastic blob. */
  const dlcR = noiseCanvas(TEX, 384, (u, v) => .44 + (fbm(u * 3, v * 34, 4) - .5) * .18 + (fbm(u * 90, v * 9, 2) - .5) * .07);
  {
    const ctx = dlcR.getContext('2d');
    for (let i = 0; i < 1400; i++) {
      const y = Math.random() * TEX, a = .015 + Math.random() * .035;
      ctx.strokeStyle = `rgba(${Math.random() < .5 ? 255 : 0},${Math.random() < .5 ? 255 : 0},255,${a})`;
      ctx.lineWidth = .5 + Math.random() * 1.5; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(TEX, y + (Math.random() - .5) * 8); ctx.stroke();
    }
  }
  const dlcH = noiseCanvas(NRM, 256, (u, v) => .5 + (fbm(u * 120, v * 18, 3) - .5) * .5);
  std('hero_dlc', {
    color: 0x14171d, metalness: .94, roughness: 1,
    roughnessMap: texture(THREE, dlcR, 1), normalMap: texture(THREE, toNormal(THREE, dlcH, .45), 3),
    normalScale: new THREE.Vector2(.18, .18), envMapIntensity: 1.15,
  });

  /* Graphite polymer frame — the second black. Same value, completely different
     surface: glass-filler mottle, matte, non-metallic, faint moulding texture. */
  const polyR = noiseCanvas(TEX, 512, (u, v) => .58 + (fbm(u * 130, v * 130, 3) - .5) * .20 + (fbm(u * 9, v * 9, 3) - .5) * .07);
  const polyH = noiseCanvas(NRM, 512, (u, v) => .5 + (fbm(u * 190, v * 190, 3) - .5) * .8);
  std('hero_polymer', {
    color: 0x1b1e24, metalness: .04, roughness: 1,
    roughnessMap: texture(THREE, polyR, 3), normalMap: texture(THREE, toNormal(THREE, polyH, .7), 4),
    normalScale: new THREE.Vector2(.5, .5), envMapIntensity: .34,
  });

  /* Grip stipple — packed pebble field, uniform across the panel. The
     palm-polished zone is a low-frequency amplitude falloff, not a bullseye. */
  const stipH = cv(NRM); {
    const c = stipH.getContext('2d');
    c.fillStyle = '#606060'; c.fillRect(0, 0, NRM, NRM);
    for (let i = 0; i < 30000; i++) {
      const x = Math.random() * NRM, y = Math.random() * NRM;
      const polish = .60 + .40 * fbm(x / NRM * 2.2, y / NRM * 2.2, 3);   // broad, soft rub
      const r = (2.0 + Math.random() * 2.4) * (.62 + .38 * polish);
      const g = c.createRadialGradient(x, y, 0, x, y, r);
      const peak = Math.round(96 + 128 * polish);
      g.addColorStop(0, `rgb(${peak},${peak},${peak})`); g.addColorStop(1, 'rgba(96,96,96,0)');
      c.fillStyle = g; c.beginPath(); c.arc(x, y, r, 0, 6.2832); c.fill();
    }
  }
  const stipR = noiseCanvas(TEX, 384, (u, v) => .72 + (fbm(u * 40, v * 40, 4) - .5) * .18 + (fbm(u * 2.2, v * 2.2, 3) - .5) * .12);
  std('hero_stipple', {
    color: 0x1c1f25, metalness: .05, roughness: 1,
    roughnessMap: texture(THREE, stipR, 2), normalMap: texture(THREE, toNormal(THREE, stipH, 1.3), 2),
    normalScale: new THREE.Vector2(1.0, 1.0), envMapIntensity: .30,
  });

  /* Bead-blasted stainless for controls and small parts; burnished variant for
     the faces a thumb actually touches. */
  const steelR = noiseCanvas(TEX, 384, (u, v) => .42 + (fbm(u * 14, v * 90, 4) - .5) * .22);
  const steelH = noiseCanvas(NRM, 384, (u, v) => .5 + (fbm(u * 30, v * 190, 3) - .5) * .45);
  const steelN = texture(THREE, toNormal(THREE, steelH, .45), 3);
  std('hero_steel', { color: 0x3f444d, metalness: .95, roughness: 1, roughnessMap: texture(THREE, steelR, 2), normalMap: steelN, normalScale: new THREE.Vector2(.25, .25), envMapIntensity: .8 });
  std('hero_steel_bright', { color: 0x5c636d, metalness: .96, roughness: .34, normalMap: steelN, normalScale: new THREE.Vector2(.14, .14), envMapIntensity: .95 });

  /* Bronze anodising — the Rifle family's finish. Same draw-polish story as the
     DLC, warmer and a shade less absorbent, so the two never read as the same
     metal wearing different paint. */
  phys('hero_bronze', {
    clearcoat: .30, clearcoatRoughness: .58,
    color: 0x3b2e1a, metalness: .90, roughness: 1,
    roughnessMap: texture(THREE, dlcR, 1), normalMap: texture(THREE, toNormal(THREE, dlcH, .45), 3),
    normalScale: new THREE.Vector2(.20, .20), envMapIntensity: 1.05,
  });
  std('hero_bronze_dark', { color: 0x241c10, metalness: .88, roughness: 1, roughnessMap: texture(THREE, dlcR, 2), envMapIntensity: .8 });

  /* Nitride barrel — near-black with turning rings from the lathe. */
  const nitR = noiseCanvas(TEX, 256, (u, v) => .34 + (fbm(u * 4, v * 120, 3) - .5) * .3);
  std('hero_nitride', { color: 0x1a1e24, metalness: .95, roughness: 1, roughnessMap: texture(THREE, nitR, 1), envMapIntensity: 1.1 });

  /* Parkerised phosphate — the Scattergun's finish. Manganese phosphate is a
     crystalline conversion coating, not "dark steel with the roughness turned
     up": it is matte, faintly green-grey, and visibly grainy at close range.
     The crystal structure is the thing the eye actually reads. */
  const parkR = noiseCanvas(TEX, 384, (u, v) => .70 + (fbm(u * 210, v * 210, 4) - .5) * .24 + (fbm(u * 11, v * 11, 3) - .5) * .08);
  const parkH = noiseCanvas(NRM, 384, (u, v) => .5 + (fbm(u * 270, v * 270, 3) - .5) * .95);
  const parkN = texture(THREE, toNormal(THREE, parkH, .95), 3);
  std('hero_park', {
    color: 0x2a2f36, metalness: .84, roughness: 1,
    roughnessMap: texture(THREE, parkR, 2), normalMap: parkN,
    normalScale: new THREE.Vector2(.6, .6), envMapIntensity: .55,
  });
  std('hero_park_dark', {
    color: 0x1b1f25, metalness: .8, roughness: 1,
    roughnessMap: texture(THREE, parkR, 3), normalMap: parkN,
    normalScale: new THREE.Vector2(.45, .45), envMapIntensity: .42,
  });

  /* Brushed stainless — the Ember canister. Brushing is directional, so this is
     the one place anisotropic UVs are correct rather than a defect. */
  const brushR = noiseCanvas(TEX, 384, (u, v) => .30 + (fbm(u * 3, v * 340, 3) - .5) * .30);
  const brushH = noiseCanvas(NRM, 384, (u, v) => .5 + (fbm(u * 4, v * 420, 2) - .5) * .7);
  std('hero_brushed', {
    color: 0x8d949e, metalness: .95, roughness: 1,
    roughnessMap: texture(THREE, brushR, 2), normalMap: texture(THREE, toNormal(THREE, brushH, .5), 2),
    normalScale: new THREE.Vector2(.3, .3), envMapIntensity: 1.0,
  });

  /* Ember glow. Emissive carries the light; the base colour stays dark so the
     part still reads as a body in shadow rather than a flat orange decal. */
  std('hero_ember', {
    color: 0x5e1c06, metalness: 0, roughness: .42,
    emissive: new THREE.Color(0xff5f18), emissiveIntensity: 1.6, envMapIntensity: .2,
  });
  std('hero_ember_glass', {
    color: 0x3a1204, metalness: 0, roughness: .10,
    emissive: new THREE.Color(0xff7a2a), emissiveIntensity: 1.15,
    transparent: true, opacity: .84, envMapIntensity: .5,
  });

  /* Hazard banding, drawn rather than tinted — diagonal stripes are a pattern,
     and a pattern has to come from a map. */
  const hazC = cv(256); {
    const c = hazC.getContext('2d');
    c.fillStyle = '#c07a1e'; c.fillRect(0, 0, 256, 256);
    c.fillStyle = '#14161a'; c.save(); c.translate(128, 128); c.rotate(-Math.PI / 4);
    for (let i = -7; i < 8; i++) c.fillRect(i * 46 - 12, -240, 24, 480);
    c.restore();
  }
  const hazR = noiseCanvas(TEX, 256, (u, v) => .56 + (fbm(u * 60, v * 60, 3) - .5) * .2);
  std('hero_hazard', {
    color: 0xffffff, map: texture(THREE, hazC, 2, true), roughnessMap: texture(THREE, hazR, 2),
    metalness: .45, roughness: 1, envMapIntensity: .5,
  });

  /* Fired 12-gauge hull: dyed plastic body, not a painted metal tube. */
  std('hero_hull', { color: 0x7b1622, metalness: .02, roughness: .54, envMapIntensity: .3 });

  /* Braided fuel hose — matte, fibrous, low environment response. */
  cloth('hero_hose', { color: 0x15181c, roughness: .88, envMapIntensity: .06 });

  std('hero_engrave', { color: 0x0a0b0e, metalness: .8, roughness: .62, envMapIntensity: .6 });
  std('hero_tritium', { color: 0x7de29a, metalness: .1, roughness: .25, emissive: new THREE.Color(0x4fbf72), emissiveIntensity: .45 });
  std('hero_tritium_ring', { color: 0xd9dee6, metalness: .9, roughness: .3 });
  std('hero_brass', { color: 0xb08a3e, metalness: .95, roughness: .26, envMapIntensity: 1.2 });

  /* Tactical glove — grey, twill weave plus stitched seam welts. */
  const gloH = cv(NRM); {
    const c = gloH.getContext('2d');
    c.fillStyle = '#808080'; c.fillRect(0, 0, NRM, NRM);
    const weave = noiseCanvas(NRM, 256, (u, v) => .5 + (fbm(u * 150, v * 150, 2) - .5) * .9);
    c.globalAlpha = .55; c.drawImage(weave, 0, 0); c.globalAlpha = 1;
    c.strokeStyle = '#3a3a3a'; c.lineWidth = 3;
    for (let k = 0; k < 7; k++) { const y = (k + .5) / 7 * NRM; c.beginPath(); c.moveTo(0, y); c.lineTo(NRM, y); c.stroke(); }
    c.strokeStyle = '#c8c8c8'; c.lineWidth = 2; c.setLineDash([6, 7]);
    for (let k = 0; k < 7; k++) { const y = (k + .5) / 7 * NRM; c.beginPath(); c.moveTo(0, y - 4); c.lineTo(NRM, y - 4); c.stroke(); }
    c.setLineDash([]);
  }
  const gloR = noiseCanvas(TEX, 256, (u, v) => .80 + (fbm(u * 40, v * 40, 4) - .5) * .22);
  cloth('hero_glove', {
    color: 0x16181c, roughness: 1,
    roughnessMap: texture(THREE, gloR, 2), normalMap: texture(THREE, toNormal(THREE, gloH, 1.1), 2.4),
    normalScale: new THREE.Vector2(.85, .85), envMapIntensity: .06,
  });
  cloth('hero_glove_pad', { color: 0x0f1114, roughness: .84, normalMap: texture(THREE, toNormal(THREE, gloH, .8), 4), normalScale: new THREE.Vector2(.6, .6), envMapIntensity: .05 });

  /* Charcoal sleeve — coarser weave, fully matte. */
  const slvH = noiseCanvas(NRM, 300, (u, v) => .5 + (fbm(u * 110, v * 110, 3) - .5) * .9);
  const slvR = noiseCanvas(TEX, 256, (u, v) => .74 + (fbm(u * 26, v * 26, 4) - .5) * .16);
  cloth('hero_sleeve', {
    color: 0x121417, roughness: 1,
    roughnessMap: texture(THREE, slvR, 2), normalMap: texture(THREE, toNormal(THREE, slvH, 1.2), 3),
    normalScale: new THREE.Vector2(.8, .8), envMapIntensity: .04,
  });
  return mats;
}

/* ══ geometry vocabulary ══════════════════════════════════════════════════ */

/** Turn-angle crease detection on a closed profile. */
function creaseFlags(pts, deg = 30) {
  const n = pts.length, out = new Array(n).fill(false), lim = Math.cos(deg * Math.PI / 180);
  for (let j = 0; j < n; j++) {
    const p = pts[(j - 1 + n) % n], c = pts[j], q = pts[(j + 1) % n];
    let ax = c[0] - p[0], ay = c[1] - p[1], bx = q[0] - c[0], by = q[1] - c[1];
    const la = Math.hypot(ax, ay) || 1, lb = Math.hypot(bx, by) || 1;
    out[j] = (ax / la * bx / lb + ay / la * by / lb) < lim;
  }
  return out;
}

function buildGeometry(THREE, pos, nrm, uv, col, idx) {
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  g.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  if (col) g.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
  g.setIndex(idx);
  if (nrm) g.setAttribute('normal', new THREE.Float32BufferAttribute(nrm, 3)); else g.computeVertexNormals();
  return g;
}

const BURNISH = [.62, .66, .72];
function pushWear(col, w) { const t = clamp(w || 0, 0, 1); col.push(lerp(1, BURNISH[0], t), lerp(1, BURNISH[1], t), lerp(1, BURNISH[2], t)); }

/** Signed area — used to force every lofted profile to a consistent winding. */
const area2 = (pts) => { let a = 0; for (let i = 0, n = pts.length; i < n; i++) { const p = pts[i], q = pts[(i + 1) % n]; a += p[0] * q[1] - q[0] * p[1]; } return a / 2; };

/** The hero materials all read vertex colour, so even a plain box needs the attribute. */
function solid(THREE, geo) {
  if (!geo.getAttribute('color')) {
    const n = geo.getAttribute('position').count, c = new Float32Array(n * 3).fill(1);
    geo.setAttribute('color', new THREE.BufferAttribute(c, 3));
  }
  return geo;
}
function boxMesh(THREE, name, w, h, d, material, pos) {
  const m = new THREE.Mesh(solid(THREE, new THREE.BoxGeometry(w, h, d)), material);
  m.name = name; m.position.set(...pos);
  return m;
}

/**
 * Sweep a changing cross-section along +Z.
 * stations: [{ z, pts: [[x,y],…] }] — every station the same length, CCW.
 * o.wear(x,y,z,j) → 0..1 burnish, baked to vertex colour.
 * o.creaseFrom: profile used for crease detection (defaults to the middle station).
 */
export function loft(THREE, name, stations, material, o = {}) {
  // Force CCW so the sweep always produces outward normals, whatever order the
  // caller happened to build the profile in.
  if (area2(stations[stations.length >> 1].pts) < 0) {
    stations = stations.map((s) => ({ ...s, pts: s.pts.slice().reverse() }));
    if (o.creaseFrom) o = { ...o, creaseFrom: o.creaseFrom.slice().reverse() };
  }
  const M = stations.length, N = stations[0].pts.length;
  const crease = o.crease || creaseFlags(o.creaseFrom || stations[M >> 1].pts, o.creaseAngle ?? 30);
  const ring = [];
  for (let j = 0; j < N; j++) { ring.push(j); if (crease[j]) ring.push(j); }
  const L = ring.length;
  // perimeter parameter per profile vertex (from the reference profile)
  const ref = o.creaseFrom || stations[M >> 1].pts, per = [0];
  for (let j = 1; j <= N; j++) per[j] = per[j - 1] + Math.hypot(ref[j % N][0] - ref[j - 1][0], ref[j % N][1] - ref[j - 1][1]);
  const total = per[N] || 1;
  // Isotropic by default: ~40 texture repeats per metre in both directions, so a
  // map never smears into stripes along whichever axis the sweep happens to run.
  const uScale = o.uScale ?? 40;
  const pos = [], uv = [], col = [], idx = [];
  let vAcc = 0;
  for (let i = 0; i < M; i++) {
    if (i) vAcc += Math.abs(stations[i].z - stations[i - 1].z);
    for (let k = 0; k < L; k++) {
      const j = ring[k], p = stations[i].pts[j];
      pos.push(p[0], p[1], stations[i].z);
      uv.push(per[j] * uScale, vAcc * (o.vScale ?? 40));
      pushWear(col, o.wear ? o.wear(p[0], p[1], stations[i].z, j) : 0);
    }
  }
  for (let i = 0; i < M - 1; i++) for (let k = 0; k < L; k++) {
    const k2 = (k + 1) % L; if (ring[k] === ring[k2]) continue;
    const a = i * L + k, b = i * L + k2, c = (i + 1) * L + k2, d = (i + 1) * L + k;
    idx.push(a, b, c, a, c, d);
  }
  // caps
  const V2 = THREE.Vector2;
  for (const [st, front] of [[stations[0], true], [stations[M - 1], false]]) {
    const contour = st.pts.map((p) => new V2(p[0], p[1]));
    let tris; try { tris = THREE.ShapeUtils.triangulateShape(contour, []); } catch { tris = null; }
    if (!tris) continue;
    const base = pos.length / 3;
    for (const p of st.pts) { pos.push(p[0], p[1], st.z); uv.push(p[0] * 12, p[1] * 12); pushWear(col, o.wear ? o.wear(p[0], p[1], st.z, -1) : 0); }
    for (const t of tris) front ? idx.push(base + t[2], base + t[1], base + t[0]) : idx.push(base + t[0], base + t[1], base + t[2]);
  }
  const mesh = new THREE.Mesh(buildGeometry(THREE, pos, null, uv, col, idx), material);
  mesh.name = name;
  if (o.pos) mesh.position.set(...o.pos);
  if (o.rot) mesh.rotation.set(...o.rot);
  return mesh;
}

/**
 * Sweep a radial cross-section along a 2D path in the (z,y) plane — guard bows,
 * finger bones, forearms. r(t, angle) lets a section be elliptical, swollen at a
 * knuckle, or carry a seam welt.
 */
export function sweep(THREE, name, path, rFn, material, o = {}) {
  const segs = o.radial ?? 20, samples = o.samples ?? path.length * 8;
  const pts = [];
  for (let i = 0; i <= samples; i++) {
    const t = i / samples, f = t * (path.length - 1), k = Math.min(Math.floor(f), path.length - 2), u = f - k;
    const p0 = path[Math.max(0, k - 1)], p1 = path[k], p2 = path[k + 1], p3 = path[Math.min(path.length - 1, k + 2)];
    const cr = (a, b, c, d) => .5 * ((2 * b) + (-a + c) * u + (2 * a - 5 * b + 4 * c - d) * u * u + (-a + 3 * b - 3 * c + d) * u * u * u);
    pts.push([cr(p0[0], p1[0], p2[0], p3[0]), cr(p0[1], p1[1], p2[1], p3[1]), cr(p0[2] ?? 0, p1[2] ?? 0, p2[2] ?? 0, p3[2] ?? 0)]);
  }
  const V3 = THREE.Vector3, pos = [], uv = [], col = [], idx = [];
  let up = new V3(...(o.up || [1, 0, 0]));
  for (let i = 0; i <= samples; i++) {
    const t = i / samples;
    const a = pts[Math.max(0, i - 1)], b = pts[Math.min(samples, i + 1)];
    const tan = new V3(b[0] - a[0], b[1] - a[1], b[2] - a[2]).normalize();
    let nx = up.clone().sub(tan.clone().multiplyScalar(up.dot(tan)));
    if (nx.lengthSq() < 1e-9) nx = new V3(0, 1, 0).sub(tan.clone().multiplyScalar(tan.y));
    nx.normalize(); up = nx.clone();
    const by = new V3().crossVectors(tan, nx);
    for (let s = 0; s < segs; s++) {
      const ang = s / segs * Math.PI * 2, r = rFn(t, ang);
      const rx = Math.cos(ang) * (Array.isArray(r) ? r[0] : r), ry = Math.sin(ang) * (Array.isArray(r) ? r[1] : r);
      pos.push(pts[i][0] + nx.x * rx + by.x * ry, pts[i][1] + nx.y * rx + by.y * ry, pts[i][2] + nx.z * rx + by.z * ry);
      uv.push(s / segs * (o.uRep ?? 1), t * (o.vRep ?? 4));
      pushWear(col, o.wear ? o.wear(t, ang) : 0);
    }
  }
  for (let i = 0; i < samples; i++) for (let s = 0; s < segs; s++) {
    const s2 = (s + 1) % segs, a = i * segs + s, b = i * segs + s2, c = (i + 1) * segs + s2, d = (i + 1) * segs + s;
    idx.push(a, b, c, a, c, d);
  }
  if (o.cap !== false) for (const [i, front] of [[0, true], [samples, false]]) {
    const base = pos.length / 3, o0 = i * segs;
    let cx = 0, cy = 0, cz = 0;
    for (let s = 0; s < segs; s++) { cx += pos[(o0 + s) * 3]; cy += pos[(o0 + s) * 3 + 1]; cz += pos[(o0 + s) * 3 + 2]; }
    pos.push(cx / segs, cy / segs, cz / segs); uv.push(.5, .5); pushWear(col, 0);
    for (let s = 0; s < segs; s++) {
      const a = o0 + s, b = o0 + (s + 1) % segs;
      front ? idx.push(base, b, a) : idx.push(base, a, b);
    }
  }
  const mesh = new THREE.Mesh(buildGeometry(THREE, pos, null, uv, col, idx), material);
  mesh.name = name;
  return mesh;
}

/** Turned part. pts = [[radius, z]] along the bore (−Z forward). */
export function revolve(THREE, name, pts, material, o = {}) {
  const segs = o.segs ?? 64, pos = [], uv = [], col = [], idx = [];
  const crease = [];
  for (let i = 0; i < pts.length; i++) {
    const p = pts[(i - 1 + pts.length) % pts.length], c = pts[i], q = pts[(i + 1) % pts.length];
    const ax = c[0] - p[0], ay = c[1] - p[1], bx = q[0] - c[0], by = q[1] - c[1];
    const la = Math.hypot(ax, ay) || 1, lb = Math.hypot(bx, by) || 1;
    crease.push(i > 0 && i < pts.length - 1 && (ax / la * bx / lb + ay / la * by / lb) < Math.cos((o.creaseAngle ?? 28) * Math.PI / 180));
  }
  // v follows arc length along the profile, not z: on a flat face every z is the
  // same, and a z-based v smears the map into radial streaks.
  const arc = [0];
  for (let i = 1; i < pts.length; i++) arc[i] = arc[i - 1] + Math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]);
  const rows = [];
  for (let i = 0; i < pts.length; i++) { rows.push(i); if (crease[i]) rows.push(i); }
  const K = o.scale ?? 40;
  for (let ri = 0; ri < rows.length; ri++) {
    const [r, z] = pts[rows[ri]];
    for (let s = 0; s <= segs; s++) {
      const a = s / segs * Math.PI * 2;
      pos.push(Math.cos(a) * r, Math.sin(a) * r, z);
      uv.push(s / segs * Math.max(r, 1e-4) * Math.PI * 2 * K, arc[rows[ri]] * K);
      pushWear(col, o.wear ? o.wear(r, z) : 0);
    }
  }
  const W = segs + 1;
  for (let ri = 0; ri < rows.length - 1; ri++) {
    if (rows[ri] === rows[ri + 1]) continue;
    for (let s = 0; s < segs; s++) {
      const a = ri * W + s, b = ri * W + s + 1, c = (ri + 1) * W + s + 1, d = (ri + 1) * W + s;
      idx.push(a, b, c, a, c, d);
    }
  }
  const mesh = new THREE.Mesh(buildGeometry(THREE, pos, null, uv, col, idx), material);
  mesh.name = name;
  if (o.pos) mesh.position.set(...o.pos);
  if (o.rot) mesh.rotation.set(...o.rot);
  return mesh;
}

/* ── stroke font for roll marks ───────────────────────────────────────────
   Industrial roll marks are stencil-like, so a geometric stroke face is the
   right shape. Each glyph is polylines in a 0..0.62 × 0..1 box; every segment
   becomes a real groove cut into the flat. */
const GLYPH = {
  B: [[[0, 0], [0, 1]], [[0, 1], [.40, 1], [.54, .87], [.54, .64], [.40, .52], [0, .52]], [[0, .52], [.44, .52], [.58, .38], [.58, .14], [.44, 0], [0, 0]]],
  O: [ellipse(.30, .5, .30, .5, 14)],
  D: [[[0, 0], [0, 1]], [[0, 1], [.32, 1], [.56, .78], [.58, .5], [.56, .22], [.32, 0], [0, 0]]],
  I: [[[.1, 0], [.1, 1]]],
  E: [[[.55, 1], [0, 1], [0, 0], [.55, 0]], [[0, .52], [.44, .52]]],
  G: [[[.58, .78], [.44, .95], [.24, 1], [.08, .86], [.02, .5], [.08, .14], [.24, 0], [.44, .05], [.58, .24], [.58, .44], [.30, .44]]],
  U: [[[0, 1], [0, .26], [.10, .06], [.30, 0], [.50, .06], [.60, .26], [.60, 1]]],
  A: [[[0, 0], [.30, 1], [.60, 0]], [[.11, .33], [.49, .33]]],
  R: [[[0, 0], [0, 1]], [[0, 1], [.40, 1], [.54, .87], [.54, .67], [.40, .55], [0, .55]], [[.26, .55], [.60, 0]]],
  M: [[[0, 0], [0, 1], [.30, .42], [.60, 1], [.60, 0]]],
  P: [[[0, 0], [0, 1]], [[0, 1], [.42, 1], [.56, .86], [.56, .64], [.42, .5], [0, .5]]],
  '0': [ellipse(.29, .5, .27, .5, 14)],
  '1': [[[.06, .80], [.28, 1], [.28, 0]], [[.05, 0], [.51, 0]]],
  '3': [[[.04, .90], [.24, 1], [.44, .97], [.55, .84], [.47, .65], [.26, .54], [.49, .47], [.58, .31], [.49, .11], [.26, .01], [.05, .09]]],
  '9': [ellipse(.28, .74, .26, .26, 12).concat([[.54, .74], [.52, .30], [.40, .04], [.18, 0]])],
  '2': [[[.04, .86], [.20, 1], [.42, 1], [.57, .86], [.55, .66], [.06, .06], [.06, 0], [.58, 0]]],
  '4': [[[.42, 0], [.42, 1]], [[.42, 1], [.02, .30], [.60, .30]]],
  '5': [[[.56, 1], [.10, 1], [.06, .56], [.24, .64], [.44, .60], [.57, .42], [.50, .14], [.28, .01], [.06, .08]]],
  '6': [[[.52, .92], [.32, 1], [.12, .86], [.05, .50], [.06, .20], [.24, .01], [.44, .04], [.56, .22], [.50, .44], [.28, .52], [.08, .44]]],
  '7': [[[.04, 1], [.58, 1], [.24, 0]]],
  '8': [ellipse(.30, .76, .24, .24, 12), ellipse(.30, .26, .28, .26, 12)],
  C: [[[.58, .80], [.42, .97], [.22, 1], [.07, .80], [.03, .5], [.07, .20], [.22, 0], [.42, .03], [.58, .20]]],
  T: [[[0, 1], [.60, 1]], [[.30, 1], [.30, 0]]],
  S: [[[.56, .86], [.38, 1], [.18, .97], [.06, .82], [.10, .62], [.34, .54], [.52, .44], [.56, .24], [.44, .05], [.22, .02], [.05, .14]]],
  N: [[[0, 0], [0, 1], [.58, 0], [.58, 1]]],
  L: [[[0, 1], [0, 0], [.54, 0]]],
  H: [[[0, 0], [0, 1]], [[.58, 0], [.58, 1]], [[0, .52], [.58, .52]]],
  F: [[[0, 0], [0, 1], [.55, 1]], [[0, .54], [.42, .54]]],
  V: [[[0, 1], [.30, 0], [.60, 1]]],
  W: [[[0, 1], [.14, 0], [.30, .62], [.46, 0], [.60, 1]]],
  X: [[[0, 0], [.60, 1]], [[0, 1], [.60, 0]]],
  Y: [[[0, 1], [.30, .52], [.60, 1]], [[.30, .52], [.30, 0]]],
  Z: [[[0, 1], [.60, 1], [0, 0], [.60, 0]]],
  K: [[[0, 0], [0, 1]], [[.56, 1], [.06, .46]], [[.20, .60], [.58, 0]]],
  J: [[[.50, 1], [.50, .20], [.38, .03], [.18, 0], [.04, .14]]],
  Q: [ellipse(.30, .5, .30, .5, 14), [[.36, .22], [.62, -.04]]],
  '-': [[[.08, .5], [.54, .5]]],
  '/': [[[.06, 0], [.52, 1]]],
  '×': [[[.06, .18], [.50, .62]], [[.50, .18], [.06, .62]]],
  '.': [[[.10, .03], [.16, .03]]],
  ' ': [],
};
function ellipse(cx, cy, rx, ry, n) {
  const out = [];
  for (let i = 0; i <= n; i++) { const a = i / n * Math.PI * 2; out.push([cx + Math.cos(a) * rx, cy + Math.sin(a) * ry]); }
  return out;
}
const ADV = { I: .26, '.': .28, ' ': .34, '1': .44 };

/**
 * Cut text into a flat face as real geometry.
 * plane 'left'  → the −X flat, text advancing toward −Z (reads muzzle-right).
 * plane 'right' → the +X flat, text advancing toward +Z.
 */
export function engrave(THREE, name, text, material, o) {
  const { x, z, y, cap = .0028, stroke = .00028, depth = .00024, plane = 'left' } = o;
  const g = new THREE.Group(); g.name = name;
  const dir = plane === 'left' ? 1 : -1, sx = plane === 'left' ? -1 : 1;
  const seg = solid(THREE, new THREE.BoxGeometry(1, 1, 1)), joint = solid(THREE, new THREE.SphereGeometry(stroke, 7, 5));
  let pen = 0;
  for (const ch of text.toUpperCase()) {
    const glyph = GLYPH[ch] ?? GLYPH[' '];
    for (const line of glyph) for (let i = 0; i < line.length - 1; i++) {
      const [ax, ay] = line[i], [bx, by] = line[i + 1];
      const z0 = z + dir * (pen + ax) * cap, y0 = y + ay * cap, z1 = z + dir * (pen + bx) * cap, y1 = y + by * cap;
      const len = Math.hypot(z1 - z0, y1 - y0); if (len < 1e-6) continue;
      const m = new THREE.Mesh(seg, material);
      m.name = `${name}_s`;
      m.scale.set(depth * 2, stroke * 2, len);
      m.position.set(x + sx * depth * .35, (y0 + y1) / 2, (z0 + z1) / 2);
      // rotate about X so the groove's local Z follows the stroke in the (z,y) plane
      m.rotation.x = -Math.atan2(y1 - y0, z1 - z0);
      g.add(m);
      const j = new THREE.Mesh(joint, material); j.name = `${name}_j`;
      j.position.set(x + sx * depth * .35, y0, z0); j.scale.set(depth / stroke * .6, .75, .75); g.add(j);
    }
    pen += (ADV[ch] ?? .62) + .16;
  }
  return g;
}

/* ══ the Sidearm ══════════════════════════════════════════════════════════ */


/**
 * Re-seat a subassembly's pivot without moving a vertex of it: the group lands
 * at `p` and every direct child is counter-translated, so the world transform
 * is unchanged but a rotation applied to the group now turns about `p`.
 *
 * Only needed for parts code ROTATES. A magazine tipping out of the well is the
 * case that matters — with its pivot at the weapon origin it swings through the
 * frame instead of dropping. Parts that only translate along an axis (slides,
 * bolts, carriers, charging handles, pressed controls) are pivot-independent
 * and must NOT be re-pivoted: the offset would be pure noise in the file.
 */
export function repivot(group, p) {
  for (const c of group.children) { c.position.x -= p[0]; c.position.y -= p[1]; c.position.z -= p[2]; }
  group.position.set(p[0], p[1], p[2]);
  group.userData.pivot = 'seated';
  return group;
}

/* ── shared exports ───────────────────────────────────────────────────────── */
export { clamp, smooth, lerp, fbm, solid, boxMesh, creaseFlags, pushWear, cv, noiseCanvas, toNormal, texture };
