/**
 * Deep Field 3D — shared modelling vocabulary for environment kits.
 *
 * Companion to `gun-kit.js`, same rules (see CLAUDE.md), different domain. The
 * gun kit's primitives — loft / sweep / revolve / engrave — are geometry-generic
 * and are imported here rather than re-implemented; what environment work needs
 * on top of them is a vocabulary of REAL STRUCTURAL SECTIONS and a way to build
 * hundreds of repeated small parts without hundreds of draw calls.
 *
 * Why that matters: the tell of a fake industrial set is a beam that is a solid
 * box. A real rolled beam is an I — you see the web, the flange tips catch
 * light, and the fillet between them is what makes it read as rolled steel
 * rather than extruded cheese. Same for a channel stringer, an angle brace, a
 * tube handrail, a corrugated duct wall. So:
 *
 *   SECT.*        real section profiles to specification (UB, PFC, angle, CHS,
 *                 SHS, chequer plate, corrugated sheet), in metres.
 *   member()      run a section along an axis via loft(), with optional sag and
 *                 end taper so a 10 m beam is not laser-straight.
 *   grating()     open-steel floor grating as REAL bearing bars — 41/100 to
 *                 spec. You can see the lane through the deck because the holes
 *                 are holes, not an alpha map pretending.
 *   meshInfill()  weldmesh balustrade infill, which IS an alpha map — 3 mm wire
 *                 at arm's length is the one case where geometry is the wrong
 *                 answer. The split is deliberate; see the note on each.
 *   rivets()/bolts()  merged instance fields. Dome rivets stand proud in
 *                 reality, so they are real geometry — just not 400 draw calls.
 *   stain()       rewrite vertex colour from a vertex's own position and normal:
 *                 soot collects on upward faces and in corners, rust bleeds
 *                 below fixings, walking surfaces burnish where feet land.
 *                 Environment wear is world-driven, unlike a gun's, which comes
 *                 from its own profile high points.
 *
 * Materials separate by SURFACE, not value (rule 4): mill scale, cast iron,
 * galvanising spangle, red-oxide primer, scaled rust, refractory, vitreous slag
 * and dry clinker are all "grey-brown industrial" and none of them shade alike.
 * envMapIntensity stays low on every matte one (rule 5).
 *
 * Metres, Y-up. Piece origin at footprint centre on the ground.
 */
import { loft, sweep, revolve as gkRevolve, solid, fbm, cv, noiseCanvas, toNormal, texture, clamp, smooth, lerp } from './gun-kit.js';

const TEX = 1024, NRM = 512, TAU = Math.PI * 2;

/* ══ surfaces ═════════════════════════════════════════════════════════════ */

/** Guarded: the map viewer rebuilds a piece on every click. */
export function envSurfaces(THREE, mats) {
  if (mats.env_mill) return mats;
  const std = (name, o) => { const m = new THREE.MeshStandardMaterial({ vertexColors: true, ...o }); m.name = name; mats[name] = m; return m; };

  /* Hot-rolled structural steel, mill scale intact: blue-black, patchy, and
     semi-glossy where the scale is unbroken — it flakes off in plates, so the
     roughness variance is large-scale blotches, not fine grain. */
  const millR = noiseCanvas(TEX, 256, (u, v) => .52 + (fbm(u * 5, v * 5, 4) - .5) * .38 + (fbm(u * 40, v * 40, 2) - .5) * .1);
  const millH = noiseCanvas(NRM, 256, (u, v) => .5 + (fbm(u * 7, v * 7, 4) - .5) * .55 + (fbm(u * 60, v * 60, 2) - .5) * .25);
  const millN = texture(THREE, toNormal(THREE, millH, .7), 1);
  std('env_mill', {
    color: 0x3c4149, metalness: .86, roughness: 1,
    roughnessMap: texture(THREE, millR, 1), normalMap: millN,
    normalScale: new THREE.Vector2(.55, .55), envMapIntensity: .7,
  });
  std('env_mill_dark', {
    color: 0x272c33, metalness: .8, roughness: 1,
    roughnessMap: texture(THREE, millR, 2), normalMap: millN,
    normalScale: new THREE.Vector2(.45, .45), envMapIntensity: .5,
  });

  /* Cast iron: sand-cast, so the surface is granular all over and the grain is
     isotropic — no rolling direction. Matte, and less reflective than rolled
     steel because the skin is oxide and embedded sand. */
  const castR = noiseCanvas(TEX, 320, (u, v) => .70 + (fbm(u * 150, v * 150, 3) - .5) * .22 + (fbm(u * 8, v * 8, 3) - .5) * .12);
  const castH = noiseCanvas(NRM, 320, (u, v) => .5 + (fbm(u * 170, v * 170, 3) - .5) * .85 + (fbm(u * 20, v * 20, 2) - .5) * .3);
  const castN = texture(THREE, toNormal(THREE, castH, 1.0), 2);
  std('env_cast', {
    color: 0x40444b, metalness: .72, roughness: 1,
    roughnessMap: texture(THREE, castR, 2), normalMap: castN,
    normalScale: new THREE.Vector2(.7, .7), envMapIntensity: .42,
  });

  /* Cast-iron tread plate. The teardrop pattern is a real raised pattern, so it
     belongs in a height map at its real 35 mm pitch — proud teardrops modelled
     as geometry would be 40 000 of them per bay. */
  const trH = cv(NRM); {
    const c = trH.getContext('2d');
    c.fillStyle = '#5a5a5a'; c.fillRect(0, 0, NRM, NRM);
    const cell = NRM / 8;                                  // 8 teardrops ≈ 0.28 m
    for (let gy = 0; gy < 8; gy++) for (let gx = 0; gx < 8; gx++) {
      const cx = (gx + .5) * cell, cy = (gy + .5) * cell, a = (gx + gy) % 2 ? .5 : -.5;
      c.save(); c.translate(cx, cy); c.rotate(a);
      const g = c.createLinearGradient(-cell * .3, 0, cell * .3, 0);
      g.addColorStop(0, '#8a8a8a'); g.addColorStop(.5, '#dedede'); g.addColorStop(1, '#8a8a8a');
      c.fillStyle = g; c.beginPath(); c.ellipse(0, 0, cell * .34, cell * .1, 0, 0, TAU); c.fill();
      c.restore();
    }
    // Wear: the pattern is worn flat down the middle of the run.
    const w = c.createLinearGradient(0, 0, 0, NRM);
    w.addColorStop(0, 'rgba(90,90,90,0)'); w.addColorStop(.5, 'rgba(90,90,90,.75)'); w.addColorStop(1, 'rgba(90,90,90,0)');
    c.fillStyle = w; c.fillRect(0, 0, NRM, NRM);
  }
  std('env_tread', {
    color: 0x44484f, metalness: .7, roughness: 1,
    roughnessMap: texture(THREE, castR, 3), normalMap: texture(THREE, toNormal(THREE, trH, 1.5), 1),
    normalScale: new THREE.Vector2(1, 1), envMapIntensity: .38,
  });

  /* Hot-dip galvanising. The spangle — big flat zinc crystals with visible grain
     boundaries — is the whole identity of the finish, and it is the reason
     galvanised steel never reads as painted. Brighter and smoother than mill
     scale, and the crystals shade independently. */
  /* Drawn at 256 and upscaled: the spangle is a large-scale feature, and a
     per-pixel Voronoi at 512 costs four times as much for no visible gain. */
  const galvH = cv(NRM); {
    const S = 256, small = cv(S), sc = small.getContext('2d'), seeds = [];
    for (let i = 0; i < 70; i++) seeds.push([Math.random() * S, Math.random() * S, 118 + Math.random() * 74]);
    const im = sc.createImageData(S, S);
    for (let y = 0; y < S; y++) for (let x = 0; x < S; x++) {
      let best = 1e9, v = 128, second = 1e9;
      for (const [sx, sy, sv] of seeds) { const d = (x - sx) ** 2 + (y - sy) ** 2; if (d < best) { second = best; best = d; v = sv; } else if (d < second) second = d; }
      const i4 = (y * S + x) * 4, val = lerp(74, v, smooth(0, 240, second - best));
      im.data[i4] = im.data[i4 + 1] = im.data[i4 + 2] = val; im.data[i4 + 3] = 255;
    }
    sc.putImageData(im, 0, 0);
    const c = galvH.getContext('2d'); c.imageSmoothingEnabled = true; c.drawImage(small, 0, 0, NRM, NRM);
  }
  const galvR = noiseCanvas(TEX, 256, (u, v) => .40 + (fbm(u * 16, v * 16, 4) - .5) * .3);
  std('env_galv', {
    color: 0x7f868f, metalness: .88, roughness: 1,
    roughnessMap: texture(THREE, galvR, 2), normalMap: texture(THREE, toNormal(THREE, galvH, .8), 2),
    normalScale: new THREE.Vector2(.45, .45), envMapIntensity: .85,
  });

  /* Red-oxide primer on steel. It is PAINT: the metal underneath is hidden, so
     metalness drops right down and the environment barely registers — which is
     what stops primed steelwork looking like painted chrome. */
  const primeR = noiseCanvas(TEX, 256, (u, v) => .78 + (fbm(u * 30, v * 30, 3) - .5) * .2);
  std('env_prime', {
    color: 0x6d3a28, metalness: .22, roughness: 1,
    roughnessMap: texture(THREE, primeR, 2), normalMap: millN,
    normalScale: new THREE.Vector2(.3, .3), envMapIntensity: .2,
  });

  /* Scaled rust — laminar, flaking, and completely non-metallic once it has
     grown this far. Rule 5: a high envMapIntensity here makes rust look wet. */
  const rustR = noiseCanvas(TEX, 320, (u, v) => .86 + (fbm(u * 60, v * 60, 4) - .5) * .18);
  const rustH = noiseCanvas(NRM, 320, (u, v) => .5 + (fbm(u * 26, v * 26, 5, 2.4, .6) - .5) * 1.1);
  std('env_rust', {
    color: 0x7a4526, metalness: .12, roughness: 1,
    roughnessMap: texture(THREE, rustR, 2), normalMap: texture(THREE, toNormal(THREE, rustH, 1.6), 2),
    normalScale: new THREE.Vector2(1, 1), envMapIntensity: .08,
  });

  /* Refractory firebrick / ladle lining: buff, chalky, porous, zero metal. */
  const refR = noiseCanvas(TEX, 256, (u, v) => .84 + (fbm(u * 80, v * 80, 3) - .5) * .18);
  const refH = noiseCanvas(NRM, 256, (u, v) => .5 + (fbm(u * 100, v * 100, 4) - .5) * .8);
  std('env_refract', {
    color: 0x9a8468, metalness: 0, roughness: 1,
    roughnessMap: texture(THREE, refR, 2), normalMap: texture(THREE, toNormal(THREE, refH, 1.1), 2),
    normalScale: new THREE.Vector2(.8, .8), envMapIntensity: .06,
  });
  /* Same brick after a campaign in the heat: slagged, darker, part-glazed. */
  std('env_refract_burnt', {
    color: 0x4e4038, metalness: .05, roughness: 1,
    roughnessMap: texture(THREE, noiseCanvas(TEX, 256, (u, v) => .48 + (fbm(u * 22, v * 22, 4) - .5) * .5), 2),
    normalMap: texture(THREE, toNormal(THREE, refH, 1.3), 3), normalScale: new THREE.Vector2(.9, .9), envMapIntensity: .3,
  });

  /* Two slags, because there are two. Vitreous slag cooled fast and is a black
     glass — smooth, dark, sharply specular in patches. Clinker cooled slow and
     is a dry crumbly aggregate. Same colour, opposite surfaces. */
  const slagR = noiseCanvas(TEX, 256, (u, v) => .30 + (fbm(u * 12, v * 12, 4) - .5) * .55);
  std('env_slag', {
    color: 0x1d1e22, metalness: .18, roughness: 1,
    roughnessMap: texture(THREE, slagR, 2),
    normalMap: texture(THREE, toNormal(THREE, noiseCanvas(NRM, 256, (u, v) => .5 + (fbm(u * 18, v * 18, 4) - .5) * .9), 1.2), 2),
    normalScale: new THREE.Vector2(.7, .7), envMapIntensity: .55,
  });
  const clinkH = noiseCanvas(NRM, 320, (u, v) => .5 + (fbm(u * 44, v * 44, 5, 2.3, .62) - .5) * 1.2);
  std('env_clinker', {
    color: 0x33313a, metalness: .04, roughness: 1,
    roughnessMap: texture(THREE, noiseCanvas(TEX, 256, (u, v) => .88 + (fbm(u * 70, v * 70, 3) - .5) * .16), 2),
    normalMap: texture(THREE, toNormal(THREE, clinkH, 1.7), 2), normalScale: new THREE.Vector2(1.1, 1.1), envMapIntensity: .05,
  });

  /* Molten iron, graded three ways so a pour has depth instead of one flat
     orange. Base colours stay dark — emissive carries the light, so the metal
     still reads as a body in shadow rather than a decal. */
  std('env_molten', { color: 0x5a1c02, metalness: 0, roughness: .34, emissive: new THREE.Color(0xff7a1c), emissiveIntensity: 2.4, envMapIntensity: .1 });
  std('env_molten_hot', { color: 0x7a3a06, metalness: 0, roughness: .28, emissive: new THREE.Color(0xffd08a), emissiveIntensity: 3.4, envMapIntensity: .1 });
  /* Skin: molten metal oxidises the instant it meets air, so a pour is never
     bright all over — it is bright at the break and dull where the skin held. */
  std('env_molten_skin', { color: 0x3a1404, metalness: .1, roughness: .62, emissive: new THREE.Color(0xc24a0e), emissiveIntensity: 1.1, envMapIntensity: .2 });
  std('env_ember', { color: 0x2a1008, metalness: .05, roughness: .7, emissive: new THREE.Color(0xe8622b), emissiveIntensity: .85, envMapIntensity: .12 });

  /* Tarnished brass and copper for pipework and valve furniture. */
  std('env_brass', { color: 0x8d6b2c, metalness: .9, roughness: .42, roughnessMap: texture(THREE, castR, 3), envMapIntensity: .8 });
  std('env_copper', { color: 0x6f4a33, metalness: .88, roughness: .5, envMapIntensity: .6 });

  /* Hazard banding. Stripes are a pattern and a pattern comes from a map — and
     this one is paint on steel that has been kicked, so it chips. */
  const hazC = cv(512); {
    const c = hazC.getContext('2d');
    c.fillStyle = '#c8801e'; c.fillRect(0, 0, 512, 512);
    c.fillStyle = '#16181c'; c.save(); c.translate(256, 256); c.rotate(-Math.PI / 4);
    for (let i = -8; i < 9; i++) c.fillRect(i * 92 - 24, -480, 46, 960);
    c.restore();
    for (let i = 0; i < 260; i++) {                          // chips down to primer
      const x = Math.random() * 512, y = Math.random() * 512, r = 1.5 + Math.random() * 6;
      c.fillStyle = Math.random() < .6 ? 'rgba(109,58,40,.85)' : 'rgba(60,65,73,.8)';
      c.beginPath(); c.ellipse(x, y, r, r * (.5 + Math.random()), Math.random() * 3, 0, TAU); c.fill();
    }
  }
  std('env_haz', {
    color: 0xffffff, map: texture(THREE, hazC, 1, true),
    roughnessMap: texture(THREE, primeR, 2), metalness: .2, roughness: 1, envMapIntensity: .18,
  });

  /* Weldmesh infill — the one alpha map in the kit. 3 mm wire at 50 mm pitch
     would be ~160 welded bars per balustrade panel for a part the player never
     stands on; alphaTest (not blending) keeps it depth-correct and sorts fine. */
  const meshA = cv(256); {
    const c = meshA.getContext('2d');
    c.fillStyle = '#000'; c.fillRect(0, 0, 256, 256);
    c.strokeStyle = '#fff'; c.lineWidth = 14;               // 3 mm wire at 50 mm pitch
    for (let i = 0; i < 4; i++) { const p = (i + .5) * 64; c.beginPath(); c.moveTo(p, 0); c.lineTo(p, 256); c.moveTo(0, p); c.lineTo(256, p); c.stroke(); }
  }
  const meshM = new THREE.MeshStandardMaterial({
    color: 0x6e757e, metalness: .85, roughness: .5, envMapIntensity: .6,
    alphaMap: texture(THREE, meshA), transparent: false, alphaTest: .5, side: THREE.DoubleSide,
  });
  meshM.name = 'env_mesh'; mats.env_mesh = meshM;

  /* Lamp glass and steam. Both lit by something else, both barely metal. */
  std('env_lamp', { color: 0xfff0cc, metalness: 0, roughness: .22, emissive: new THREE.Color(0xffdf9e), emissiveIntensity: 1.8, envMapIntensity: .2 });
  std('env_lamp_off', { color: 0xb9bcc2, metalness: .1, roughness: .18, envMapIntensity: .9 });
  const steamM = new THREE.MeshStandardMaterial({ color: 0xd2d7e0, metalness: 0, roughness: 1, transparent: true, opacity: .3, depthWrite: false, envMapIntensity: .04 });
  steamM.name = 'env_steam'; mats.env_steam = steamM;
  return mats;
}

/* ══ revolve, with the winding sorted out ═════════════════════════════════
 *
 * `revolve()` emits outward normals only when the profile is traversed with
 * INCREASING z. Work the maths at the +X point and the face normal comes out
 * proportional to (dz, 0, −dr): a wall (dz > 0) faces out, but a FLAT disc
 * (dz = 0, dr > 0) faces −Z — which `rot: [−π/2, 0, 0]` then turns into −Y.
 * So every horizontal surface authored outward-from-the-axis — a melt pool, a
 * dial face, a lid top — renders backfacing and is invisible from the only
 * angle anyone sees it from. Twelve profiles in this kit had it wrong, and
 * hand-reversing them just moves the trap rather than removing it.
 *
 * In the (r, z) half-plane that gives three cases, and they need telling apart
 * — a single enclosed-area test gets two of them wrong, because closing an
 * OPEN profile back on itself invents an edge that can outvote the real ones
 * (a 1 m tube wall nets −0.025 against a −0.36 closing edge; a 3-point sliver
 * is numerical noise):
 *
 *   closed section   — both ends meet, or both sit on the axis: trace
 *                      counter-clockwise.
 *   open wall        — z must increase end to end.
 *   open flat        — no z to speak of, so the sign comes from dr: radius
 *                      must DECREASE, which faces the disc +Z and therefore
 *                      +Y once it is laid down.
 *
 * `o.inward: true` flips whichever test applies, for the surfaces whose
 * visible face IS the inside — a vessel lining, a reflector dish.
 */
function profileWinding(pts) {
  const n = pts.length, a = pts[0], b = pts[n - 1];
  const rs = pts.map((p) => p[0]), zs = pts.map((p) => p[1]);
  const dr = Math.max(...rs) - Math.min(...rs), dz = Math.max(...zs) - Math.min(...zs);
  if (Math.hypot(a[0] - b[0], a[1] - b[1]) < 1e-6 || (a[0] < 1e-6 && b[0] < 1e-6)) {
    let s = 0;
    for (let i = 0; i < n; i++) { const p = pts[i], q = pts[(i + 1) % n]; s += p[0] * q[1] - q[0] * p[1]; }
    return Math.sign(s) || 1;
  }
  if (dz < .15 * dr) return -Math.sign(b[0] - a[0]) || 1;
  return Math.sign(b[1] - a[1]) || 1;
}
export function revolve(THREE, name, pts, material, o = {}) {
  const want = o.inward ? -1 : 1;
  return gkRevolve(THREE, name, profileWinding(pts) === want ? pts : pts.slice().reverse(), material, o);
}

/* ══ merged instancing ════════════════════════════════════════════════════ */

/**
 * Concatenate [{geo, m}] into one BufferGeometry. Rule 9 wants subassemblies
 * organised for decimation, and a rivet row is one subassembly, not 40 of them.
 */
export function merge(THREE, list, name, material) {
  let vc = 0, ic = 0;
  for (const { geo } of list) {
    vc += geo.attributes.position.count;
    ic += geo.index ? geo.index.count : geo.attributes.position.count;
  }
  const pos = new Float32Array(vc * 3), nrm = new Float32Array(vc * 3), uv = new Float32Array(vc * 2), col = new Float32Array(vc * 3), idx = new Uint32Array(ic);
  const V = new THREE.Vector3(), N3 = new THREE.Matrix3();
  let vo = 0, io = 0;
  for (const { geo, m, c } of list) {
    const p = geo.attributes.position, n = geo.attributes.normal, t = geo.attributes.uv;
    N3.getNormalMatrix(m);
    for (let i = 0; i < p.count; i++) {
      V.fromBufferAttribute(p, i).applyMatrix4(m);
      pos[(vo + i) * 3] = V.x; pos[(vo + i) * 3 + 1] = V.y; pos[(vo + i) * 3 + 2] = V.z;
      if (n) { V.fromBufferAttribute(n, i).applyMatrix3(N3).normalize(); nrm[(vo + i) * 3] = V.x; nrm[(vo + i) * 3 + 1] = V.y; nrm[(vo + i) * 3 + 2] = V.z; }
      if (t) { uv[(vo + i) * 2] = t.getX(i); uv[(vo + i) * 2 + 1] = t.getY(i); }
      col[(vo + i) * 3] = c ? c[0] : 1; col[(vo + i) * 3 + 1] = c ? c[1] : 1; col[(vo + i) * 3 + 2] = c ? c[2] : 1;
    }
    if (geo.index) for (let i = 0; i < geo.index.count; i++) idx[io + i] = geo.index.getX(i) + vo;
    else for (let i = 0; i < p.count; i++) idx[io + i] = i + vo;
    io += geo.index ? geo.index.count : p.count;
    vo += p.count;
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  g.setAttribute('normal', new THREE.BufferAttribute(nrm, 3));
  g.setAttribute('uv', new THREE.BufferAttribute(uv, 2));
  g.setAttribute('color', new THREE.BufferAttribute(col, 3));
  g.setIndex(new THREE.BufferAttribute(idx, 1));
  const mesh = new THREE.Mesh(g, material); mesh.name = name;
  return mesh;
}

/**
 * Geometry of a template mesh WITH its own transform applied.
 *
 * member() returns an oriented mesh — the section runs along local Z and the
 * mesh carries the rotation that points it along the world axis you asked for.
 * merge() only ever sees geometry, so handing it `tmpl.geometry` silently
 * throws that rotation away and every instance comes back running along Z.
 * Always bake a template before merging it.
 */
export function bake(mesh) {
  mesh.updateMatrix();
  return mesh.geometry.clone().applyMatrix4(mesh.matrix);
}

const cache = new WeakMap();
const tmpl = (THREE, key, make) => {
  let c = cache.get(THREE); if (!c) cache.set(THREE, c = {});
  return c[key] || (c[key] = make());
};

/** Dome-head rivet: a real spherical cap, because a driven rivet stands proud. */
const rivetGeo = (THREE, r) => tmpl(THREE, 'riv' + r, () => {
  const g = new THREE.SphereGeometry(r, 10, 4, 0, TAU, 0, Math.PI * .44);
  g.scale(1, .62, 1); g.rotateX(Math.PI / 2);              // dome along +Z
  return g;
});
const boltGeo = (THREE, r, h) => tmpl(THREE, 'blt' + r + '_' + h, () => {
  const g = new THREE.CylinderGeometry(r, r * .96, h, 6); g.rotateX(Math.PI / 2); g.translate(0, 0, h / 2);
  return g;
});

/**
 * A row of rivets from `a` to `b` (world points), each dome facing `dir`.
 * One mesh. Spacing follows the real rule of thumb — about 5 diameters.
 */
export function rivets(THREE, name, a, b, n, r, material, dir = [0, 0, 1]) {
  const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), D = new THREE.Vector3(...dir).normalize();
  const geo = rivetGeo(THREE, r), list = [];
  const q = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 0, 1), D);
  for (let i = 0; i < n; i++) {
    const p = A.clone().lerp(B, n === 1 ? .5 : i / (n - 1));
    list.push({ geo, m: new THREE.Matrix4().compose(p, q, new THREE.Vector3(1, 1, 1)) });
  }
  return merge(THREE, list, name, material);
}
export function bolts(THREE, name, pts, r, h, material, dir = [0, 1, 0]) {
  const geo = boltGeo(THREE, r, h), D = new THREE.Vector3(...dir).normalize();
  const q = new THREE.Quaternion().setFromUnitVectors(new THREE.Vector3(0, 0, 1), D);
  return merge(THREE, pts.map((p) => ({ geo, m: new THREE.Matrix4().compose(new THREE.Vector3(...p), q, new THREE.Vector3(1, 1, 1)) })), name, material);
}

/**
 * Open-steel floor grating, to the 41/100 product standard: 30 × 5 mm bearing
 * bars at 41 mm pitch running along Z, twisted cross rods at 100 mm along X,
 * bound by a flat edge bar. The voids are real, so you see the lane through the
 * deck and light falls through in stripes — which is most of what sells a
 * walkway. This is the piece to decimate first: the binding bar carries the
 * silhouette on its own.
 */
export function grating(THREE, name, w, d, material, o = {}) {
  const pitch = o.pitch ?? .041, bar = o.bar ?? .005, dep = o.depth ?? .030, rod = o.rod ?? .0065;
  const n = Math.max(2, Math.round((w - bar) / pitch)), list = [];
  const bg = tmpl(THREE, `gb${bar}_${dep}_${d}`, () => new THREE.BoxGeometry(bar, dep, d));
  for (let i = 0; i <= n; i++) {
    const x = -w / 2 + bar / 2 + i * ((w - bar) / n);
    list.push({ geo: bg, m: new THREE.Matrix4().makeTranslation(x, -dep / 2, 0) });
  }
  const nr = Math.max(1, Math.round(d / .1));
  const rg = tmpl(THREE, `gr${rod}_${w}`, () => new THREE.BoxGeometry(w - .004, rod, rod));
  for (let i = 0; i <= nr; i++) {
    // Twist about the rod's OWN length (X here) — a cross rod is twisted
    // square bar. Rotating about Z instead swings a 4 m rod through 0.5 rad
    // and drops the whole panel 1.9 m below its own deck.
    const m = new THREE.Matrix4().makeRotationX(.5);
    m.setPosition(0, -rod * .5, -d / 2 + (d / nr) * i);
    list.push({ geo: rg, m });
  }
  // Binding bar: flat edge bar all round, standing to the bar depth.
  const bx = tmpl(THREE, `gxx${d}_${dep}`, () => new THREE.BoxGeometry(.005, dep, d));
  const bz = tmpl(THREE, `gxz${w}_${dep}`, () => new THREE.BoxGeometry(w, dep, .005));
  for (const s of [-1, 1]) list.push({ geo: bx, m: new THREE.Matrix4().makeTranslation(s * (w / 2 - .0025), -dep / 2, 0) });
  for (const s of [-1, 1]) list.push({ geo: bz, m: new THREE.Matrix4().makeTranslation(0, -dep / 2, s * (d / 2 - .0025)) });
  const mesh = merge(THREE, list, name, material);
  if (o.pos) mesh.position.set(...o.pos);
  if (o.rot) mesh.rotation.set(...o.rot);
  mesh.userData.decimate = 'grating';
  return mesh;
}

/** Weldmesh balustrade infill — see env_mesh above for why this one is a map. */
export function meshInfill(THREE, name, w, h, material, o = {}) {
  const g = new THREE.PlaneGeometry(w, h);
  const uv = g.attributes.uv;
  for (let i = 0; i < uv.count; i++) uv.setXY(i, uv.getX(i) * w / .2, uv.getY(i) * h / .2);
  uv.needsUpdate = true;
  const m = new THREE.Mesh(solid(THREE, g), material); m.name = name;
  if (o.pos) m.position.set(...o.pos);
  if (o.rot) m.rotation.set(...o.rot);
  return m;
}

/* ══ real structural sections ═════════════════════════════════════════════
   Profiles in the loft cross-section plane (X = width, Y = depth), centred,
   metres, CCW. Dimensions are to rolled-section tables — a UB 305×165 really is
   305 deep, 165 wide, 10.2 web, 15.7 flange with an 8.9 root fillet, and using
   the real numbers is why it reads as steel rather than as a shape. */

const arc = (cx, cy, r, a0, a1, n = 4) => {
  const out = [];
  for (let i = 0; i <= n; i++) { const a = a0 + (a1 - a0) * i / n; out.push([cx + Math.cos(a) * r, cy + Math.sin(a) * r]); }
  return out;
};

export const SECT = {
  /** Universal beam / column. */
  i(h, b, tw, tf, r = tf * .6) {
    const hh = h / 2, hb = b / 2, hw = tw / 2;
    return [
      [-hb, -hh], [hb, -hh], [hb, -hh + tf], [hw + r, -hh + tf],
      ...arc(hw + r, -hh + tf + r, r, -Math.PI / 2, Math.PI, 3).slice(1),
      [hw, hh - tf - r],
      ...arc(hw + r, hh - tf - r, r, Math.PI, Math.PI / 2, 3).slice(1),
      [hb, hh - tf], [hb, hh], [-hb, hh], [-hb, hh - tf], [-hw - r, hh - tf],
      ...arc(-hw - r, hh - tf - r, r, Math.PI / 2, 0, 3).slice(1),
      [-hw, -hh + tf + r],
      ...arc(-hw - r, -hh + tf + r, r, 0, -Math.PI / 2, 3).slice(1),
      [-hb, -hh + tf],
    ];
  },
  /** Parallel-flange channel, toes toward +X. */
  channel(h, b, tw, tf) {
    const hh = h / 2, hw = tw;
    return [[-b / 2, -hh], [b / 2, -hh], [b / 2, -hh + tf], [-b / 2 + hw, -hh + tf],
      [-b / 2 + hw, hh - tf], [b / 2, hh - tf], [b / 2, hh], [-b / 2, hh]];
  },
  /** Equal angle, heel at −X/−Y, with the real filleted root and tapered toes. */
  angle(a, t, r = t * .8) {
    const h = -a / 2;
    return [[h, h], [h + a, h], [h + a, h + t * .84], [h + a - t * .25, h + t],
      ...arc(h + t + r, h + t + r, r, -Math.PI / 2, -Math.PI, 3),
      [h + t, h + a - t * .25], [h + t * .84, h + a], [h, h + a]];
  },
  /** Circular hollow section / solid round. */
  tube(r, n = 20) { return arc(0, 0, r, 0, TAU - TAU / n, n - 1); },
  /** Square hollow section outline with real corner radius. */
  sq(a, r = a * .12, n = 3) {
    const h = a / 2 - r;
    return [...arc(h, -h, r, -Math.PI / 2, 0, n), ...arc(h, h, r, 0, Math.PI / 2, n),
      ...arc(-h, h, r, Math.PI / 2, Math.PI, n), ...arc(-h, -h, r, Math.PI, Math.PI * 1.5, n)];
  },
  /** Flat plate with chamfered arrises — a cut plate is never a sharp box. */
  plate(w, t, c = Math.min(t * .35, .004)) {
    const hw = w / 2, ht = t / 2;
    return [[-hw + c, -ht], [hw - c, -ht], [hw, -ht + c], [hw, ht - c], [hw - c, ht], [-hw + c, ht], [-hw, ht - c], [-hw, -ht + c]];
  },
  /**
   * Corrugated sheet as a closed section: the corrugation is the profile, so a
   * duct wall is genuinely fluted instead of being a flat box with ribs glued
   * on. 76 mm pitch × 19 mm depth is the real industrial sheet.
   */
  corrug(len, t, pitch = .076, amp = .019, n = 0) {
    const N = n || Math.max(8, Math.round(len / pitch * 4)), top = [], bot = [];
    for (let i = 0; i <= N; i++) {
      const x = -len / 2 + len * i / N, y = Math.sin(x / pitch * TAU) * amp / 2;
      top.push([x, y + t / 2]); bot.push([x, y - t / 2]);
    }
    return [...bot, ...top.reverse()];
  },
  /** Arched duct bore with a fluted crown — tunnel section, springing at Y 0. */
  arch(w, h, t, pitch = .12, amp = .022) {
    const hw = w / 2, out = [], inn = [], N = 30;
    const cycles = Math.max(4, Math.round(Math.PI * hw / pitch));
    for (let i = 0; i <= N; i++) {
      const u = i / N, a = Math.PI * u;
      const flute = Math.sin(u * cycles * Math.PI) * amp * .5;
      out.push([-Math.cos(a) * (hw + t + flute), Math.sin(a) * (h + t + flute)]);
      inn.push([-Math.cos(a) * hw, Math.sin(a) * h]);
    }
    return [...out, ...inn.reverse()];
  },
};

/* ══ members ══════════════════════════════════════════════════════════════ */

const AXIS = { x: [0, Math.PI / 2, 0], y: [-Math.PI / 2, 0, 0], z: [0, 0, 0] };

/**
 * Run a section along an axis. `sag` bows the member down at midspan (a real
 * 10 m beam is not laser-straight, and the tiny bow is what makes a long span
 * believable); `stations` adds intermediate cuts so a lap, a splice or a taper
 * can change the section mid-run.
 *
 * Environment UVs default to ~1.6 repeats/metre in BOTH directions (rule 7:
 * isotropic). The gun kit's 40/m is right for a part 120 mm long and would tile
 * a brick map 400 times across a beam.
 */
export function member(THREE, name, pts, len, material, o = {}) {
  const n = o.stations ?? (o.sag ? 6 : 1), st = [];
  for (let i = 0; i <= n; i++) {
    const t = i / n, z = -len / 2 + len * t;
    const sag = o.sag ? -o.sag * Math.sin(Math.PI * t) : 0;
    const sc = o.taper ? lerp(1, o.taper, t) : 1;
    st.push({ z, pts: (o.at ? o.at(t, pts) : pts).map(([x, y]) => [x * sc, y * sc + sag]) });
  }
  const m = loft(THREE, name, st, material, {
    creaseAngle: o.creaseAngle ?? 24, uScale: o.uScale ?? 1.6, vScale: o.vScale ?? 1.6, wear: o.wear,
  });
  const r = o.rot || AXIS[o.axis || 'z'];
  m.rotation.set(...r);
  if (o.pos) m.position.set(...o.pos);
  if (o.spin) m.rotateZ(o.spin);
  return m;
}

/** Convenience: a straight round bar/tube run between two points (handrails). */
export function bar(THREE, name, a, b, r, material, o = {}) {
  const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b);
  const m = member(THREE, name, SECT.tube(r, o.segs ?? 14), A.distanceTo(B), material, { uScale: o.uScale ?? 2.4, vScale: o.vScale ?? 2.4, creaseAngle: 80 });
  m.position.copy(A.clone().add(B).multiplyScalar(.5));
  m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 0, 1), B.clone().sub(A).normalize());
  return m;
}

/* ══ wear ═════════════════════════════════════════════════════════════════ */

/**
 * Rewrite vertex colour across a subtree from each vertex's own position and
 * normal (rule 6 — wear is never hand-painted). Environment wear is world-
 * driven, not profile-driven: soot settles on upward faces and stays in
 * corners, rust bleeds down from fixings, a walking surface burnishes where
 * feet actually land, and everything near a pour gets baked warm.
 */
export function stain(THREE, root, fn) {
  root.updateMatrixWorld(true);
  const V = new THREE.Vector3(), N = new THREE.Vector3(), N3 = new THREE.Matrix3();
  root.traverse((o) => {
    if (!o.isMesh) return;
    const g = o.geometry, p = g.attributes.position, nr = g.attributes.normal;
    if (!g.attributes.color) g.setAttribute('color', new THREE.BufferAttribute(new Float32Array(p.count * 3).fill(1), 3));
    const c = g.attributes.color;
    N3.getNormalMatrix(o.matrixWorld);
    for (let i = 0; i < p.count; i++) {
      V.fromBufferAttribute(p, i).applyMatrix4(o.matrixWorld);
      if (nr) N.fromBufferAttribute(nr, i).applyMatrix3(N3).normalize(); else N.set(0, 1, 0);
      const t = fn(V.x, V.y, V.z, N.x, N.y, N.z, o.name) || [1, 1, 1];
      c.setXYZ(i, t[0] * c.getX(i), t[1] * c.getY(i), t[2] * c.getZ(i));
    }
    c.needsUpdate = true;
  });
  return root;
}

/**
 * The Foundry's standard staining: soot on up-facing surfaces graded by height
 * (it falls, so low horizontal faces catch most), a warm bake toward a heat
 * source, and an optional polished track where boots run.
 */
export function foundrySoot(o = {}) {
  const { heat = null, reach = 9, track = null, lift = 0 } = o;
  return (x, y, z, nx, ny, nz) => {
    const up = clamp(ny, 0, 1);
    const soot = up * (1 - smooth(0, 7, y - lift)) * .34 + (1 - up) * .06 + smooth(.2, -.6, ny) * .1;
    let r = 1 - soot * .78, g = 1 - soot * .86, b = 1 - soot * .92;
    if (heat) {
      const d = Math.hypot(x - heat[0], (y - heat[1]) * .7, z - heat[2]);
      const h = smooth(reach, reach * .18, d) * (.35 + .65 * clamp(nx * 0 + 1 - up * .4, 0, 1));
      r += h * .30; g += h * .11; b -= h * .06;
    }
    if (track) {
      const inT = smooth(track.w, track.w * .35, Math.abs(z - track.z)) * up * smooth(track.y + .12, track.y, Math.abs(y - track.y) + track.y);
      const p = inT * .5; r = lerp(r, 1.16, p); g = lerp(g, 1.18, p); b = lerp(b, 1.22, p);
    }
    return [clamp(r, 0, 2), clamp(g, 0, 2), clamp(b, 0, 2)];
  };
}

/** tower-kit's part(), with the vertex-colour attribute the env materials read. */
export function ep(K, name, geo, material, pos = [0, 0, 0], rot = [0, 0, 0]) {
  const m = K.part(name, geo, material, pos, rot);
  solid(K.THREE, m.geometry);
  return m;
}

export { loft, sweep, solid, clamp, smooth, lerp, fbm };
