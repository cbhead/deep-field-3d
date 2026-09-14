/**
 * Deep Field 3D — The Toaster environment kit.
 *
 * Commissioned in docs/FORWARD-MANIFEST-toaster.md. A rural property on a
 * clear summer afternoon at 320 × 160 m — six times the area of any map before it — built and
 * playable upstream today as grayboxes, at zero §4 violations. This file is
 * the art for it.
 *
 * The rule that shapes almost every decision here is the manifest's own:
 *
 *   > A file that will be instanced is drawn ONCE PER PART, and its triangles
 *   > are multiplied by every placement. Part count is draw calls. Put the
 *   > detail in the texture.
 *
 * So this kit is built to the opposite brief from the Foundry. There, a piece
 * is placed a handful of times and detail is cut into the geometry; here the
 * terrain tile lands 128 times, the road module ninety, and the trees about
 * 1,500. A grass tuft modelled as geometry is fifty thousand parts. Every
 * budget below is arithmetic rather than taste, and the detail lives in the
 * canvases in `floor-textures.js` and in the leaf/bark/board atlases below.
 *
 * The second rule, carried over from the Switchyard: a tiling module may only
 * contain features true at its own repeat distance. A 4 m road module has no
 * mailbox; a 20 m ground tile has no road, no fence and no tree.
 *
 * Where geometry IS the answer the hero standard still applies — the vehicles
 * (models/vehicles.js), the four building shells and the warp gate are placed
 * once or a handful of times and are modelled properly.
 *
 * Metres, Y-up, +X east, +Z north, y 0 at grade. Origin at footprint centre on
 * grade unless an entry says otherwise.
 */
import { floorMaterials } from './floor-textures.js';
import { buildSpaceSky } from './space-sky.js';
import {
  envSurfaces, SECT, member, bar, merge, bake, ep, revolve, sweep, solid, clamp, smooth, lerp,
} from './env-kit.js';

export const TOASTER = [];
const P = (o) => { TOASTER.push({ ...o, file: o.file || o.id + '.glb', swatch: o.swatch || '#9c8d57', dir: 'maps/toaster/' }); };

/* Deterministic hash noise — no RNG anywhere in this kit. Upstream places the
   treeline and the scatter from a hashed lattice so every client draws the
   identical wood without a byte crossing the wire; the art has to be able to
   do the same or a "variant 2" would differ between machines. */
const hash = (x, y = 0, z = 0) => {
  let h = Math.sin(x * 127.1 + y * 311.7 + z * 74.7) * 43758.5453;
  return h - Math.floor(h);
};
const vnoise = (x, z) => {
  const xi = Math.floor(x), zi = Math.floor(z), xf = x - xi, zf = z - zi;
  const u = xf * xf * (3 - 2 * xf), v = zf * zf * (3 - 2 * zf);
  return lerp(lerp(hash(xi, zi), hash(xi + 1, zi), u), lerp(hash(xi, zi + 1), hash(xi + 1, zi + 1), u), v);
};

/* ── materials ─────────────────────────────────────────────────────────── */

const cv = (w, h = w) => { const c = document.createElement('canvas'); c.width = w; c.height = h; return c; };
const rgba = (r, g, b, a = 1) => `rgba(${r | 0},${g | 0},${b | 0},${a})`;
const rng = (seed) => { let s = seed >>> 0; return () => { s += 0x6D2B79F5; let t = Math.imul(s ^ (s >>> 15), 1 | s); t ^= t + Math.imul(t ^ (t >>> 7), 61 | t); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; };
const tx = (THREE, c, rep = 1, srgb = false) => {
  const t = new THREE.CanvasTexture(c);
  t.wrapS = t.wrapT = THREE.RepeatWrapping; t.repeat.set(rep, rep); t.anisotropy = 8;
  if (srgb) t.colorSpace = THREE.SRGBColorSpace;
  return t;
};

/**
 * The leaf-card atlas: one 1024² sheet, four cells, shared by all three
 * canopies and by the three LOD1 billboards. Drawn near-white so a single
 * sheet can be tinted to three summer greens — oak, maple and pine — rather than
 * baking three — the manifest asks for one atlas, and one atlas is also one
 * texture bind across ~1,500 instanced trees.
 *
 * glTF alphaMode MASK, which is `alphaTest` here: blended leaves sort badly
 * per instance and cost a transparent pass on the most-instanced thing in the
 * game.
 */
function leafAtlas() {
  const N = 1024, C = N / 2, R = rng(4177), c = cv(N), x = c.getContext('2d');
  x.clearRect(0, 0, N, N);
  // Cell 0,0 — broadleaf cluster (oak/maple share it; species reads in tint
  // and in canopy silhouette, not in leaf shape at 90 m).
  const leaf = (cx, cy, r, ang, v) => {
    x.save(); x.translate(cx, cy); x.rotate(ang);
    x.fillStyle = rgba(v, v, v, .92);
    x.beginPath(); x.moveTo(0, -r);
    x.bezierCurveTo(r * .78, -r * .5, r * .62, r * .55, 0, r);
    x.bezierCurveTo(-r * .62, r * .55, -r * .78, -r * .5, 0, -r);
    x.fill();
    x.strokeStyle = rgba(v * .72, v * .72, v * .72, .8); x.lineWidth = Math.max(1, r * .07);
    x.beginPath(); x.moveTo(0, -r); x.lineTo(0, r); x.stroke();
    x.restore();
  };
  for (let i = 0; i < 150; i++) {
    const a = R() * 6.283, rad = Math.pow(R(), .6) * C * .46;
    leaf(C * .5 + Math.cos(a) * rad, C * .5 + Math.sin(a) * rad, 20 + R() * 34, R() * 6.283, 170 + R() * 85);
  }
  // Cell 1,0 — a sparser spray for canopy edges, so the outline is not a disc.
  for (let i = 0; i < 64; i++) {
    const a = R() * 6.283, rad = Math.pow(R(), .45) * C * .44;
    leaf(C * 1.5 + Math.cos(a) * rad, C * .5 + Math.sin(a) * rad, 18 + R() * 26, R() * 6.283, 165 + R() * 85);
  }
  // Cell 0,1 — conifer sprig: needle fans off a central shoot.
  for (let s = 0; s < 10; s++) {
    const sx = C * .16 + R() * C * .68, sy = C + C * .12 + R() * C * .7, len = C * .16 + R() * C * .2, ang = -1.2 + R() * 2.4;
    x.strokeStyle = rgba(150 + R() * 60, 150 + R() * 60, 150 + R() * 60, .85);
    for (let n = 0; n < 26; n++) {
      const t = n / 26, px = sx + Math.cos(ang) * len * t, py = sy + Math.sin(ang) * len * t, side = n % 2 ? 1 : -1;
      x.lineWidth = 2.4; x.beginPath(); x.moveTo(px, py);
      x.lineTo(px + Math.cos(ang + side * 1.05) * 16, py + Math.sin(ang + side * 1.05) * 16); x.stroke();
    }
  }
  // Cell 1,1 — dead/kept leaves, the ones still on an oak in November.
  for (let i = 0; i < 110; i++) {
    const a = R() * 6.283, rad = Math.pow(R(), .6) * C * .44;
    leaf(C * 1.5 + Math.cos(a) * rad, C * 1.5 + Math.sin(a) * rad, 16 + R() * 30, R() * 6.283, 120 + R() * 70);
  }
  return c;
}

/** Sawn board / bark family: one sheet, tinted per surface. */
function boardAtlas() {
  const N = 1024, R = rng(9091), c = cv(N), x = c.getContext('2d');
  x.fillStyle = rgba(190, 190, 190); x.fillRect(0, 0, N, N);
  // 200 mm boards → 8 across at 1.6 m of coverage.
  const BW = N / 8;
  for (let i = 0; i < 8; i++) {
    const v = 150 + R() * 70;
    x.fillStyle = rgba(v, v * .98, v * .94); x.fillRect(i * BW, 0, BW - 2, N);
    x.fillStyle = rgba(40, 36, 32, .55); x.fillRect(i * BW + BW - 3, 0, 3, N);
    // Grain, knots, split ends — the things that say "sawn" at 2 m.
    for (let g = 0; g < 90; g++) {
      const gx = i * BW + 3 + R() * (BW - 8);
      x.strokeStyle = rgba(v * .72, v * .68, v * .62, .3 + R() * .3); x.lineWidth = .8 + R() * 1.6;
      x.beginPath(); x.moveTo(gx, 0);
      for (let s = 1; s <= 8; s++) x.lineTo(gx + Math.sin(s * 1.7 + i) * 3.5, N * s / 8);
      x.stroke();
    }
    for (let k = 0; k < 3; k++) {
      const kx = i * BW + 6 + R() * (BW - 14), ky = R() * N, kr = 4 + R() * 9;
      x.fillStyle = rgba(v * .5, v * .44, v * .38, .85); x.beginPath(); x.ellipse(kx, ky, kr, kr * 1.4, 0, 0, 6.283); x.fill();
      x.strokeStyle = rgba(v * .62, v * .56, v * .5, .5); x.lineWidth = 1.4;
      x.beginPath(); x.ellipse(kx, ky, kr * 1.9, kr * 2.6, 0, 0, 6.283); x.stroke();
    }
  }
  return c;
}

/** Vertical-furrowed bark, used by every trunk; tinted per species. */
function barkAtlas() {
  const N = 512, R = rng(3313), c = cv(N), x = c.getContext('2d');
  x.fillStyle = rgba(160, 160, 160); x.fillRect(0, 0, N, N);
  for (let i = 0; i < 900; i++) {
    const bx = R() * N, w = 3 + R() * 14, v = R() < .5 ? 90 + R() * 50 : 180 + R() * 60;
    x.fillStyle = rgba(v, v, v, .5);
    x.beginPath(); x.moveTo(bx, 0);
    for (let s = 1; s <= 6; s++) x.lineTo(bx + Math.sin(s * 2.1 + i) * 5, N * s / 6);
    for (let s = 6; s >= 0; s--) x.lineTo(bx + w + Math.sin(s * 2.1 + i) * 5, N * s / 6);
    x.fill();
  }
  return c;
}

/** Running-bond brick. Every building on this property is brick — the two
 *  houses, the shed, the wing walls — which is most of why the first pass's
 *  painted clapboard read as generic suburban rather than as this place. */
function brickAtlas() {
  const N = 1024, R = rng(5501), c = cv(N), x = c.getContext('2d');
  // 8 courses per tile → with 1.6 m coverage a course is 200 mm, about right
  // for brick-plus-bed at the distance this is read from.
  const CH = N / 14, BW = N / 6;
  x.fillStyle = rgba(126, 118, 110); x.fillRect(0, 0, N, N);            // mortar
  for (let r = 0; r < 14; r++) {
    const off = (r % 2) * BW / 2;
    for (let i = -1; i < 7; i++) {
      const v = 150 + R() * 78, bx = i * BW + off, by = r * CH;
      x.fillStyle = rgba(v, v * .93, v * .88);
      x.fillRect(bx + 2, by + 2, BW - 4, CH - 4);
      // Face mottle — fired brick is never one value across a face.
      for (let m = 0; m < 5; m++) {
        const w = 4 + R() * 16;
        x.fillStyle = rgba(v * (.82 + R() * .3), v * .88, v * .84, .35);
        x.fillRect(bx + 3 + R() * (BW - w - 6), by + 3 + R() * (CH - 8), w, 3 + R() * 5);
      }
    }
  }
  return c;
}

/** Dashed stucco / EIFS — the gable fields and the entry surround. */
function stuccoAtlas() {
  const N = 512, R = rng(7717), c = cv(N), x = c.getContext('2d');
  x.fillStyle = rgba(184, 178, 166); x.fillRect(0, 0, N, N);
  for (let i = 0; i < 24000; i++) {
    const v = 150 + R() * 80;
    x.fillStyle = rgba(v, v * .98, v * .94, .5);
    x.fillRect(R() * N, R() * N, 1 + R() * 2, 1 + R() * 2);
  }
  return c;
}

/** Warp membrane: radial alpha with filaments. The material already asked for
 *  alphaTest, but alphaTest with no map tests a constant 1 and masks nothing —
 *  the gate was a hard flat silhouette instead of a surface with an edge. */
function warpAtlas() {
  const N = 512, R = rng(4409), c = cv(N), x = c.getContext('2d'), h = N / 2;
  x.clearRect(0, 0, N, N);
  // Body: opaque core falling off to nothing just inside the aperture edge,
  // so the MASK cut lands in the membrane rather than on the frame.
  const gr = x.createRadialGradient(h, h, 0, h, h, h);
  gr.addColorStop(0, 'rgba(255,255,255,1)');
  gr.addColorStop(.62, 'rgba(235,235,255,0.98)');
  gr.addColorStop(.88, 'rgba(200,210,255,0.6)');
  gr.addColorStop(1, 'rgba(180,200,255,0)');
  x.fillStyle = gr; x.beginPath(); x.arc(h, h, h, 0, 6.283); x.fill();
  // Filaments drawn from the rim inward: structure for the pulse to travel
  // along, so a brightening reads as something moving rather than a dimmer.
  x.globalCompositeOperation = 'lighter';
  for (let i = 0; i < 200; i++) {
    const a = R() * 6.283, r0 = h * (.18 + R() * .34), r1 = h * (.9 + R() * .1);
    x.strokeStyle = `rgba(255,255,255,${.1 + R() * .26})`;
    x.lineWidth = .7 + R() * 2.2;
    x.beginPath(); x.moveTo(h + Math.cos(a) * r0, h + Math.sin(a) * r0);
    for (let s = 1; s <= 5; s++) {
      const t = s / 5, rr = r0 + (r1 - r0) * t, aa = a + Math.sin(t * 3 + i) * .16;
      x.lineTo(h + Math.cos(aa) * rr, h + Math.sin(aa) * rr);
    }
    x.stroke();
  }
  /* Voids, so the mask has holes and the thing does not read as a sheet.
     Many and small: a handful of big ones cut to background and the membrane
     came back looking mouldy rather than woven. */
  x.globalCompositeOperation = 'destination-out';
  for (let i = 0; i < 90; i++) {
    const a = R() * 6.283, r = h * (.3 + R() * .58), rr = 1.5 + R() * 5.5;
    x.fillStyle = `rgba(0,0,0,${.5 + R() * .45})`;
    x.beginPath(); x.ellipse(h + Math.cos(a) * r, h + Math.sin(a) * r, rr, rr * (.5 + R()), a, 0, 6.283); x.fill();
  }
  x.globalCompositeOperation = 'source-over';
  return c;
}

let CACHE = null;
export function makeToasterMats(THREE, mats) {
  if (mats.toa_grass) return mats;
  envSurfaces(THREE, mats);
  const F = floorMaterials(THREE);
  const std = (name, o) => { const m = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: .95, metalness: 0, envMapIntensity: .12, ...o }); m.name = name; mats[name] = m; return m; };

  // Ground family comes from the shared floor bakes so a road module and the
  // tile it lies on cannot drift apart.
  mats.toa_grass = F.grass; mats.toa_asphalt = F.asphalt; mats.toa_gravel = F.gravel;

  CACHE = CACHE && CACHE.THREE === THREE ? CACHE
    : { THREE, leaf: leafAtlas(), board: boardAtlas(), bark: barkAtlas(), brick: brickAtlas(), stucco: stuccoAtlas(), warp: warpAtlas() };
  const leafTex = (cell) => {
    const t = tx(THREE, CACHE.leaf, 1, true);
    t.repeat.set(.5, .5); t.offset.set(cell[0] * .5, cell[1] * .5); t.wrapS = t.wrapT = THREE.ClampToEdgeWrapping;
    return t;
  };
  /* MASK, not blend: alphaTest with transparent:false keeps leaves in the
     opaque pass, which is the only way ~1,500 instanced canopies are
     affordable. DoubleSide because a card is a card from both faces. The
     threshold is LOW (.34): mipmapping averages a leaf's alpha toward zero
     with distance, and at .5 a crown forty metres off eroded to a cloud of
     dots around bare limbs. */
  const leafMat = (name, color, cell) => std(name, {
    color, map: leafTex(cell), alphaTest: .34, transparent: false, side: THREE.DoubleSide,
    roughness: .88, envMapIntensity: .05,
  });
  /* Three greens that differ in VALUE and warmth, not in hue alone: a mid
     yellow-green oak, a lighter fresher maple, and a dark blue-green conifer.
     Read at 90 m the belt has to have depth in it. */
  leafMat('toa_leaf_oak', 0x5f8a34, [0, 0]);
  leafMat('toa_leaf_maple', 0x74a03e, [1, 1]);
  leafMat('toa_leaf_pine', 0x33543a, [0, 1]);
  leafMat('toa_leaf_edge', 0x6e9a40, [1, 0]);

  const boardMat = (name, color, rep, o = {}) => std(name, { color, map: tx(THREE, CACHE.board, rep, true), roughness: .96, envMapIntensity: .05, ...o });
  boardMat('toa_timber_red', 0x8a4030, 1.4);      // oxide-red board, dressing only
  boardMat('toa_timber_bare', 0x9a9184, 1.4);     // grey unpainted board
  boardMat('toa_fence', 0x8e8474, 2.2);
  boardMat('toa_deck', 0x8a7f6d, 2.0);

  /* Masonry. The property is brick end to end, in three firings: the main
     house and the shed are the same warm red-brown, the ranch a browner
     blend, the south house a pale buff. Gable fields, the entry surround and
     the dormer cheeks are dashed stucco — the light panels between the brick
     that give these rooflines their two-tone read. */
  const brickMat = (name, color, rep) => std(name, { color, map: tx(THREE, CACHE.brick, rep, true), roughness: .93, envMapIntensity: .07 });
  brickMat('toa_brick_red', 0x8d5a46, 1.15);      // vehickle house + shed
  brickMat('toa_brick_brown', 0x7d5742, 1.15);    // buggy house
  brickMat('toa_brick_buff', 0xa8917a, 1.15);     // grnmchn house
  std('toa_stucco', { color: 0xb9b09c, map: tx(THREE, CACHE.stucco, 2.2, true), roughness: .96, envMapIntensity: .05 });
  std('toa_stucco_warm', { color: 0xc0ae90, map: tx(THREE, CACHE.stucco, 2.2, true), roughness: .96, envMapIntensity: .05 });
  std('toa_doorpanel', { color: 0xd8d8d2, roughness: .66, metalness: .12, envMapIntensity: .3 });

  std('toa_bark', { color: 0x5a4636, map: tx(THREE, CACHE.bark, 2.4, true), roughness: .98, envMapIntensity: .04 });
  std('toa_bark_pine', { color: 0x6a4530, map: tx(THREE, CACHE.bark, 2.8, true), roughness: .98, envMapIntensity: .04 });

  std('toa_shingle', { color: 0x4a413a, roughness: .93, envMapIntensity: .08 });
  std('toa_shingle_brown', { color: 0x54473a, roughness: .93, envMapIntensity: .08 });
  std('toa_corrugate', { color: 0x6d716e, roughness: .62, metalness: .55, envMapIntensity: .5 });
  std('toa_concrete', { color: 0x9a968c, roughness: .94, envMapIntensity: .08 });
  std('toa_interior', { color: 0xb4ab99, roughness: .92, envMapIntensity: .06 });
  std('toa_floorboard', { color: 0x7c6448, map: tx(THREE, CACHE.board, 3.0, true), roughness: .82, envMapIntensity: .12 });
  /* Windows are opaque and slightly emissive rather than glass: the manifest
     forbids transparency anywhere in this kit, and a dark pane with a warm
     inner glow reads better at 90 m than a mirror would. envMapIntensity is
     kept LOW: at .55 the smooth dark pane mirrored the studio box at every
     glancing angle and every window on the property read as a cream square. */
  std('toa_window', { color: 0x161c24, roughness: .42, metalness: .05, envMapIntensity: .18, emissive: new THREE.Color(0xffc98a), emissiveIntensity: .05 });
  std('toa_water', { color: 0x2f5563, roughness: .12, metalness: .08, envMapIntensity: .9 });
  std('toa_riprap', { color: 0x6f6a5e, roughness: .96, envMapIntensity: .07 });
  // Water-tumbled stone: the same rock as riprap but rounded and burnished, so
  // it differs from it in roughness rather than in lightness. Wet variant is
  // the same colour again, darkened in vertex colour, with a damp sheen.
  std('toa_riverrock', { color: 0x8b877c, roughness: .58, metalness: .04, envMapIntensity: .16 });
  std('toa_riverrock_wet', { color: 0x7b776d, roughness: .24, metalness: .06, envMapIntensity: .34 });
  std('toa_hay', { color: 0xb49a55, roughness: .98, envMapIntensity: .04 });
  std('toa_tarp', { color: 0x4d5a4a, roughness: .78, envMapIntensity: .1 });
  std('toa_rust', { color: 0x7a4228, roughness: .92, metalness: .15, envMapIntensity: .18 });
  std('toa_paint_faded', { color: 0x8c9aa0, roughness: .72, metalness: .05, envMapIntensity: .35 });
  std('toa_tank', { color: 0xcfcdc4, roughness: .55, metalness: .2, envMapIntensity: .4 });
  // Galvanised steel — bins, gates, wire, the mill. One finish, differing
  // from the propane tank's enamel in roughness and response, not in value.
  std('toa_galv', { color: 0x9da19c, roughness: .46, metalness: .7, envMapIntensity: .5 });
  std('toa_galv_ds', { color: 0x9da19c, roughness: .46, metalness: .7, envMapIntensity: .5, side: THREE.DoubleSide });
  std('toa_tpost', { color: 0x3c4a3c, roughness: .72, metalness: .35, envMapIntensity: .2 });
  std('toa_red_paint', { color: 0x8a2c22, roughness: .58, metalness: .1, envMapIntensity: .3 });
  std('toa_linen', { color: 0xe6e2d8, roughness: .96, envMapIntensity: .04, side: THREE.DoubleSide });
  std('toa_dirt', { color: 0x6b5a44, roughness: .98, envMapIntensity: .05 });
  std('toa_glow', { color: 0xffd9a0, roughness: .4, emissive: new THREE.Color(0xffc07a), emissiveIntensity: 1.3, envMapIntensity: 0 });
  std('toa_warp_frame', { color: 0x2b2e36, roughness: .48, metalness: .6, envMapIntensity: .45 });
  {
    const wt = tx(THREE, CACHE.warp, 1, true);
    wt.wrapS = wt.wrapT = THREE.ClampToEdgeWrapping;
    std('toa_warp_membrane', {
      color: 0xff3a52, map: wt, alphaMap: wt, alphaTest: .38, transparent: false,
      emissive: new THREE.Color(0xff2a44), emissiveMap: wt, emissiveIntensity: 1.6,
      roughness: .3, envMapIntensity: 0, side: THREE.DoubleSide,
    });
  }
  // The soffit bead: the aperture edge on an active gate, and the only part
  // of the FRAME that changes between states.
  std('toa_warp_rim', { color: 0xff6a70, roughness: .35, emissive: new THREE.Color(0xff3a44), emissiveIntensity: 2.1, envMapIntensity: 0 });
  std('toa_warp_dark', { color: 0x1a1c22, roughness: .7, metalness: .3, envMapIntensity: .2 });
  std('toa_sky', { color: 0x8ab4dc, roughness: 1, metalness: 0, side: THREE.BackSide, envMapIntensity: 0 });
  return mats;
}

/* ── local helpers ─────────────────────────────────────────────────────────
   Boxes and cards, merged. Nothing in this kit may ship four hundred draw
   calls, so anything repeated inside a piece is merged into one named part
   before it leaves the builder. */

/** Merged boxes. Entries: [w, h, d, x, y, z, rotY?, colour?]. */
function MB(K, name, mat, list) {
  const { THREE } = K, items = [];
  for (const [w, h, d, x, y, z, ry = 0, c] of list) {
    const m = new THREE.Matrix4().makeRotationY(ry); m.setPosition(x, y, z);
    items.push({ geo: new THREE.BoxGeometry(w, h, d), m, c });
  }
  return merge(THREE, items, name, mat);
}

/** Merged cards. Entries: [w, h, x, y, z, rotY?, rotX?, colour?]. */
function MC(K, name, mat, list) {
  const { THREE } = K, items = [], e = new THREE.Euler();
  for (const [w, h, x, y, z, ry = 0, rx = 0, c] of list) {
    e.set(rx, ry, 0, 'YXZ');
    const m = new THREE.Matrix4().makeRotationFromEuler(e); m.setPosition(x, y, z);
    items.push({ geo: new THREE.PlaneGeometry(w, h), m, c });
  }
  return merge(THREE, items, name, mat);
}

/** Merged tapered cylinders — trunks, limbs, posts, pipes. */
function MCyl(K, name, mat, list) {
  const { THREE } = K, items = [], e = new THREE.Euler();
  for (const [rt, rb, h, x, y, z, rx = 0, rz = 0, segs = 6, c] of list) {
    e.set(rx, 0, rz, 'ZYX');
    const m = new THREE.Matrix4().makeRotationFromEuler(e); m.setPosition(x, y, z);
    items.push({ geo: new THREE.CylinderGeometry(rt, rb, h, segs, 1, false), m, c });
  }
  return merge(THREE, items, name, mat);
}

const tri = (o) => { let n = 0; o.traverse((m) => { if (m.isMesh) n += (m.geometry.index ? m.geometry.index.count : m.geometry.attributes.position.count) / 3; }); return Math.round(n); };
const parts = (o) => { let n = 0; o.traverse((m) => { if (m.isMesh) n++; }); return n; };

/* ═══ P0 — the instanced set ════════════════════════════════════════════════
   Terrain, roads and trees. These three decide whether the map runs at all. */

/**
 * Terrain tile. 20 × 20 m, relief ≤ 0.15 m and EXACTLY zero within 2 m of
 * every edge, so tiles seam and the roads lie flat on them.
 *
 * A 14 × 14 grid is 392 triangles, which leaves room inside the 900 budget for
 * a variant's one piece of furniture. Variation that can be done in vertex
 * colour is done in vertex colour — it costs nothing and it is what stops 128
 * copies reading as wallpaper.
 */
function terrainTile(K, v) {
  const { THREE, grp, mats } = K, g = grp('toaster_terrain');
  const S = 20, N = 14, geo = new THREE.PlaneGeometry(S, S, N, N);
  geo.rotateX(-Math.PI / 2);
  const p = geo.attributes.position, col = new Float32Array(p.count * 3);
  for (let i = 0; i < p.count; i++) {
    const x = p.getX(i), z = p.getZ(i);
    // Edge damping: full relief in the middle, dead flat from 8 m out.
    const damp = 1 - smooth(7, 8, Math.max(Math.abs(x), Math.abs(z)));
    let y = (vnoise(x * .18 + v * 11, z * .18 - v * 7) - .5) * .22 + (vnoise(x * .52, z * .52) - .5) * .07;
    let r = 1, gr = 1, b = 1;
    if (v === 1) {
      // Mown strip along +Z and a bare-earth scrape beside it.
      const mown = 1 - smooth(1.6, 2.6, Math.abs(x - 3.5));
      y = lerp(y, y * .25, mown); gr = lerp(gr, 1.06, mown); b = lerp(b, .86, mown); r = lerp(r, .88, mown);
      const bare = 1 - smooth(1.8, 3.4, Math.hypot(x + 4.5, z + 2.5));
      r = lerp(r, 1.05, bare); gr = lerp(gr, .82, bare); b = lerp(b, .62, bare); y = lerp(y, y * .4 - .03, bare);
    } else if (v === 2) {
      // A patch burnt off by the sun, as a field does where the soil is thin.
      const drift = (1 - smooth(2.2, 5.4, Math.hypot(x - 2, z - 3.2))) * (.6 + .4 * vnoise(x, z));
      r = lerp(r, 1.16, drift); gr = lerp(gr, 1.02, drift); b = lerp(b, .68, drift); y += drift * .05;
    } else if (v === 3) {
      // Tyre ruts running along +X, damped out at the seams like everything.
      for (const rz of [-.9, .9]) {
        const rut = 1 - smooth(.35, .95, Math.abs(z - rz));
        y -= rut * .06 * damp; r = lerp(r, .86, rut * .8); gr = lerp(gr, .84, rut * .8); b = lerp(b, .8, rut * .8);
      }
    } else if (v === 4) {
      // Rough pasture: grazed unevenly, so the sward is clumpy and yellower,
      // with the paler lines the stock wear walking to water.
      const clump = vnoise(x * .7 + 3, z * .7 - 5), track = 1 - smooth(.5, 1.4, Math.abs(vnoise(x * .11, z * .11 + 2) - .5) * 14);
      y += (clump - .5) * .07;
      r = lerp(1.0, 1.12, clump); gr = lerp(.9, 1.0, clump); b = lerp(.6, .74, clump);
      r = lerp(r, .92, track * .7); gr = lerp(gr, .86, track * .7); b = lerp(b, .66, track * .7);
    } else if (v === 5) {
      // Cut hayfield: pale gold stubble in 3 m mower widths along +X, with a
      // raked windrow every third pass where the baler has not been yet.
      const pass = ((z + 10) % 3 + 3) % 3, band = 1 - smooth(.3, .7, Math.abs(pass - 1.5));
      const wr = Math.floor((z + 10) / 3) % 3 === 1 ? band : 0;
      y = y * .3 + wr * .09;
      r = 1.24 - band * .06 + wr * .1; gr = 1.1 - band * .05 + wr * .04; b = .68 - band * .03 - wr * .04;
    }
    p.setY(i, y * damp);
    col[i * 3] = r; col[i * 3 + 1] = gr; col[i * 3 + 2] = b;
  }
  geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
  geo.computeVertexNormals();
  const uv = geo.attributes.uv;
  for (let i = 0; i < uv.count; i++) { uv.setXY(i, uv.getX(i) * 1.6, uv.getY(i) * 1.6); }
  const ground = new THREE.Mesh(geo, mats.toa_grass); ground.name = 'toaster_terrain_ground';
  ground.receiveShadow = true; g.add(ground);

  if (v === 2) {
    // The one bit of furniture the tile is allowed: a fallen branch, which is
    // true at a 20 m repeat in a way a mailbox or a fence post is not.
    g.add(MCyl(K, 'toaster_terrain_branch', mats.toa_bark, [
      [.05, .10, 3.4, -3.2, .09, 4.6, 0, Math.PI / 2 - .06, 5],
      [.03, .055, 1.1, -4.4, .12, 5.3, .5, Math.PI / 2 + .7, 5],
      [.02, .04, .8, -2.1, .11, 4.1, -.4, Math.PI / 2 - .8, 5],
    ]));
  }
  return g;
}

P({
  id: 'toaster_terrain', label: 'Terrain tile (20 m)', size: '20×20 m', swatch: '#6d8f45',
  instanced: true, budgetTris: 900, budgetParts: 4,
  stats: { Tile: '20×20', Relief: '≤0.15 m', Seam: 'flat 2 m in', Variants: '6' },
  note: 'Green summer pasture, 20 m, in four variants off one shared albedo/roughness/normal set. Relief is ≤0.15 m and damped to EXACTLY zero from 8 m out so tiles seam and roads lie flat. 128 instanced placements, so the tile carries no road, no fence and no grass tuft — a tuft a metre is fifty thousand parts, and tufts are `toaster_terrain_scatter`. v0 plain; v1 a mown strip and a bare scrape; v2 a sun-burnt patch and a fallen branch (the only furniture true at a 20 m repeat); v3 tyre ruts along +X. Variation that can ride in vertex colour does, because it is free and because a hundred and twenty-eight copies of one tile is wallpaper at any rotation. v4 and v5 are FIELD tiles rather than lawn — rough grazed pasture, and cut hay stubble in 3 m mower widths with a windrow every third pass — assigned by region in the level file (`fields`), so the property reads as fields divided by fences rather than one mown park.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) { return terrainTile(K, v % 6); },
});

/**
 * Asphalt road module. run +X, repeat 4.0, width 6.0, top surface at y 0.05.
 *
 * The section is a real crowned carriageway rather than a slab: 1.5 % fall
 * from the crown to each edge, and the edge breaks away instead of ending in
 * a kerb, because a country road has no kerb. Everything else — the wheel
 * tracks, the faded centre line, the ragged margin — is in the texture, where
 * a module placed ninety times needs it to be.
 */
P({
  id: 'toaster_road_asphalt', label: 'Road module — asphalt', size: '4×6 m', swatch: '#3a3a3e',
  instanced: true, budgetTris: 200, budgetParts: 2, run: '+X', repeat: 4.0, width: 6.0,
  stats: { Run: '+X · 4 m', Width: '6 m', Top: 'y 0.05', Crown: '1.5%' },
  note: 'Two-lane country asphalt, 4 m of it. Crowned 1.5% to each edge and broken away at the margin — no kerb. Top surface at y 0.05, a centimetre proud of the gravel so their junction is a crossing rather than a z-fight. No posts, drains, mailboxes or potholes: all of them would repeat every four metres. Vehicle handling is read off the road CENTRE LINES rather than off this mesh, so the art has to sit on the line, not beside it.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_road_asphalt');
    const W = 6, L = 4, NX = 4, NZ = 8;
    const geo = new THREE.PlaneGeometry(L, W, NX, NZ); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position, col = new Float32Array(p.count * 3);
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), z = p.getZ(i), t = Math.abs(z) / (W / 2);
      // Crown, then the edge crumbles the last 300 mm.
      const edge = smooth(.86, 1, t);
      const y = .05 - t * t * .045 - edge * (.02 + vnoise(x * 3 + 7, z * 3) * .03);
      p.setY(i, y);
      p.setZ(i, z - Math.sign(z) * edge * vnoise(x * 2.3, z * 2.3 + 4) * .18);
      const w = 1 - edge * .25;
      col[i * 3] = w; col[i * 3 + 1] = w; col[i * 3 + 2] = w;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    geo.computeVertexNormals();
    // V across the road so the baked centre line lands on the centre line.
    const uv = geo.attributes.uv;
    for (let i = 0; i < uv.count; i++) uv.setXY(i, uv.getX(i), uv.getY(i));
    const m = new THREE.Mesh(geo, mats.toa_asphalt); m.name = 'toaster_road_asphalt_surface';
    m.receiveShadow = true; g.add(m);
    return g;
  },
});

P({
  id: 'toaster_road_gravel', label: 'Road module — gravel', size: '4×4 m', swatch: '#847c6a',
  instanced: true, budgetTris: 150, budgetParts: 1, run: '+X', repeat: 4.0, width: 4.0,
  stats: { Run: '+X · 4 m', Width: '4 m', Top: 'y 0.04', Ruts: 'baked' },
  note: 'Gravel drive, 4 m, up to the barn. Same contract as the asphalt at 4 m wide and y 0.04 — a centimetre lower, so the asphalt crosses it cleanly. Flatter section than the road (a drive is graded, not crowned) with the ruts and the weedy centre crown in the texture. Seven placements.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_road_gravel');
    const W = 4, L = 4, geo = new THREE.PlaneGeometry(L, W, 3, 6); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position, col = new Float32Array(p.count * 3);
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), z = p.getZ(i), t = Math.abs(z) / (W / 2), edge = smooth(.8, 1, t);
      p.setY(i, .04 - t * t * .012 - edge * .015 + (vnoise(x * 4, z * 4) - .5) * .012);
      const w = 1 - edge * .2; col[i * 3] = w; col[i * 3 + 1] = w; col[i * 3 + 2] = w;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    geo.computeVertexNormals();
    const m = new THREE.Mesh(geo, mats.toa_gravel); m.name = 'toaster_road_gravel_surface';
    m.receiveShadow = true; g.add(m);
    return g;
  },
});

/**
 * Asphalt arc: 30° of the same 6 m carriageway on a 12 m centreline radius, so
 * twelve of them make the circular drive north of the Vehickle house.
 *
 * Origin at the CENTRE OF CURVATURE, not on the road — the arc starts on the
 * +X axis and sweeps toward +Z. Placed from its own centre, twelve copies land
 * on one circle from one transform each; placed from the road surface they
 * would each need their own chord maths, which is the class of mistake that
 * cost the Switchyard a pass.
 */
P({
  id: 'toaster_road_arc', label: 'Road module — asphalt arc', size: '30° · r 12 m', swatch: '#3a3a3e',
  instanced: true, budgetTris: 200, budgetParts: 1, arcDeg: 30, radius: 12, width: 6,
  stats: { Arc: '30°', Radius: '12 m (centreline)', Width: '6 m', Origin: 'centre of curvature' },
  note: 'A 30° arc of 6 m asphalt on a 12 m centreline radius; twelve make the circular drive. ORIGIN AT THE CENTRE OF CURVATURE, arc starting on +X and sweeping toward +Z — none of which is recoverable from the file, which is why `arcDeg`, `radius` and `width` ship in the manifest. Same crown and broken edge as the straight module. Without it the drive is laid from twelve straight modules and reads as a dodecagon.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_road_arc');
    const R0 = 12, W = 6, A = Math.PI / 6, NA = 8, NR = 4;
    const pos = [], uv = [], col = [], idx = [];
    for (let i = 0; i <= NA; i++) {
      const a = A * i / NA;
      for (let j = 0; j <= NR; j++) {
        const t = j / NR, r = R0 - W / 2 + W * t, k = Math.abs(t - .5) * 2, edge = smooth(.86, 1, k);
        const rr = r + Math.sign(t - .5) * edge * vnoise(i * 2.1, j * 2.1) * .16;
        pos.push(Math.cos(a) * rr, .05 - k * k * .045 - edge * (.02 + vnoise(i * 3, j * 3 + 7) * .03), Math.sin(a) * rr);
        uv.push(a * R0 / 4, t * 1);
        const w = 1 - edge * .25; col.push(w, w, w);
      }
    }
    for (let i = 0; i < NA; i++) for (let j = 0; j < NR; j++) {
      const a0 = i * (NR + 1) + j, b0 = a0 + NR + 1;
      idx.push(a0, b0, a0 + 1, a0 + 1, b0, b0 + 1);
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    geo.setIndex(idx); geo.computeVertexNormals();
    const m = new THREE.Mesh(geo, mats.toa_asphalt); m.name = 'toaster_road_arc_surface';
    m.receiveShadow = true; g.add(m);
    return g;
  },
});

/* ── trees ────────────────────────────────────────────────────────────────
   About 780 placements today and nearer 1,500 once the belt is dressed, so
   this is the most instanced thing in the game by an order of magnitude.
   EXACTLY two parts per LOD0 file: culling, visibility ranges and mesh LOD all
   act on a multimesh's whole bounding box, so the belt is cut into 40 m cells
   and every extra part multiplies the number of them. */

/**
 * The enemy lane: a worn two-rut track, 3.4 m wide, in trodden grass. Asked
 * for in FORWARD-MANIFEST-hero §E — every other map dresses its lane with
 * `<map>_path_ground` and this one was laying the gravel road module. The
 * module is the SAME grass material as the tile with the wear in vertex
 * colour, so its edges are invisible against the field and only the ruts
 * show: the line twelve waves walk reads as exactly that, not as tarmac
 * across a hayfield. Where the lane runs down the county road it lies over the
 * asphalt a metre narrower than the carriageway.
 */
P({
  id: 'toaster_path_ground', label: 'Lane module — worn track', size: '4×3.4 m', swatch: '#7d6f4c',
  instanced: true, budgetTris: 200, budgetParts: 1, run: '+X', repeat: 4.0, width: 3.4,
  stats: { Run: '+X · 4 m', Width: '3.4 m', Ruts: '2 · 1.5 m track', Top: 'y 0.06' },
  note: 'The enemy lane, 4 m of it: two tyre-and-boot ruts 1.5 m apart in trodden grass, no kerb, 3.4 m wide. Same grass material as the terrain tile, with the wear — bare earth in the ruts, bruised grass between — carried in vertex colour, so the module\u2019s edges vanish into the field and only the track shows. Laid along every walked route at y 0.06; over the county road it sits on the asphalt a metre narrower than the carriageway. Instanced, about 230 placements.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_path_ground');
    const W = 3.4, L = 4, geo = new THREE.PlaneGeometry(L, W, 4, 14); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position, col = new Float32Array(p.count * 3);
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), z = p.getZ(i);
      const rut = 1 - smooth(.16, .44, Math.abs(Math.abs(z) - .75));
      const wear = 1 - smooth(1.15, 1.7, Math.abs(z));
      p.setY(i, -rut * .05 + (vnoise(x * 2.1, z * 2.1) - .5) * .012 * wear);
      // Bare earth in the rut, bruised grass between, untouched at the edge.
      const r = lerp(1, lerp(.94, .8, rut), wear), gg = lerp(1, lerp(.88, .62, rut), wear), b = lerp(1, lerp(.68, .4, rut), wear);
      col[i * 3] = r; col[i * 3 + 1] = gg; col[i * 3 + 2] = b;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3));
    geo.computeVertexNormals();
    const uv = geo.attributes.uv;   // same 0.08 repeats/m as the tile, so the grass lines up
    for (let i = 0; i < uv.count; i++) uv.setXY(i, uv.getX(i) * L * .08, uv.getY(i) * W * .08);
    const m = new THREE.Mesh(geo, mats.toa_grass); m.name = 'toaster_path_ground_surface';
    m.receiveShadow = true; g.add(m);
    return g;
  },
});

const SPECIES = {
  oak: { h: 14, r: 5.5, trunk: .52, fork: .54, limbs: 7, tiers: 4, leaf: 'toa_leaf_oak', bark: 'toa_bark', cards: 52, lean: .06 },
  maple: { h: 12, r: 4.5, trunk: .42, fork: .56, limbs: 6, tiers: 4, leaf: 'toa_leaf_maple', bark: 'toa_bark', cards: 46, lean: .05 },
  pine: { h: 16, r: 3.0, trunk: .40, fork: .30, limbs: 9, tiers: 7, leaf: 'toa_leaf_pine', bark: 'toa_bark_pine', cards: 56, lean: .03 },
  /* Open-grown trees, which the belt does not have: a hedgerow oak that forked
     low and spread because nothing shaded it, and an orchard apple. Placed
     one at a time by the level file, never by the belt lattice. */
  oak_open: { h: 16, r: 7.5, trunk: .78, fork: .34, limbs: 8, tiers: 4, leaf: 'toa_leaf_oak', bark: 'toa_bark', cards: 300, core: 60, lean: .1, limbLen: .55 },
  apple: { h: 4.6, r: 2.4, trunk: .2, fork: .3, limbs: 5, tiers: 3, leaf: 'toa_leaf_maple', bark: 'toa_bark', cards: 34, core: 8, lean: .08, limbLen: .6 },
};
const BELT = ['oak', 'maple', 'pine'];
/* Fork fractions are high and the lean is small because this is closed-canopy
   second-growth hardwood, not parkland: in the street views the trunks run
   bare and near-vertical for more than half their height and the crowns only
   start where they reach the light. The first pass forked at .40 and leaned
   .10, which draws an open-grown specimen tree — the shape a hedgerow oak has
   and the shape nothing in this wood has. */

function buildTree(K, id, key) {
  const { THREE, grp, mats } = K, S = SPECIES[key], g = grp(id);
  const conifer = key === 'pine';
  const forkY = S.h * S.fork, H = hash(key.length * 7.3);

  /* Trunk + limbs, merged into ONE part. A tapered bole, then limbs that
     actually leave it below the canopy — a canopy floating over a pole is the
     silhouette that gives a billboard away when you walk up to it. */
  const cyl = [[S.trunk * .34, S.trunk * .5, forkY, 0, forkY / 2, 0, 0, 0, 7]];
  cyl.push([S.trunk * .16, S.trunk * .34, S.h - forkY, S.lean * 1.2, forkY + (S.h - forkY) / 2, 0, 0, -S.lean * .18, 6]);
  for (let i = 0; i < S.limbs; i++) {
    const a = (i / S.limbs) * Math.PI * 2 + H, t = i / S.limbs;
    const y = forkY + (S.h - forkY) * (conifer ? .12 + t * .62 : .06 + t * .5);
    const len = (conifer ? S.r * (1 - t * .55) : S.r * (.62 + hash(i, 3) * .4)) * (S.limbLen ?? 1);
    const droop = conifer ? .95 : .55 + hash(i, 9) * .5;
    cyl.push([S.trunk * .05, S.trunk * .17, len, Math.cos(a) * len * .42, y + len * .14, Math.sin(a) * len * .42,
      -Math.sin(a) * droop, Math.cos(a) * droop, 5]);
  }
  const trunk = MCyl(K, `${id}_trunk`, mats[S.bark], cyl);
  trunk.castShadow = true; g.add(trunk);

  /* Canopy, one merged part of MASK cards. Cards are hung on a lobed shell
     rather than a sphere so the outline is broken — and the LOD1 billboard is
     rendered from this same silhouette, which is what keeps the 90 m swap
     from popping.

     Card PLACEMENT radii are derived by subtracting the card's own half-size
     from the species envelope, not chosen and then hoped about. A card is a
     quad with its own extent, so a card centred at radius r reaches r + w/2:
     placing centres out at the canopy radius built an oak 16.1 m across
     against a 11 m spec and a pine 13.5 against 6, and since LOD1 is drawn
     from the SPEC radius the swap at 90 m would have jumped a third of the
     crown's width. On the most-instanced thing on the map an oversize canopy
     is also 780 bounding boxes too big for the belt's 3.5 m lattice. */
  const cards = [];
  for (let i = 0; i < S.cards; i++) {
    const t = i / S.cards;
    if (conifer) {
      const tier = Math.floor(t * S.tiers), tt = tier / S.tiers;
      const a = i * 2.399 + H;
      // rad + halfW ≤ S.r, with halfW = rad*1.05 + S.r*.18 ⇒ rad ≤ S.r*.40.
      const rad = S.r * .34 * (1 - tt * .82) * (.55 + hash(i, 2) * .6);
      const cw = rad * 2.1 + S.r * .36, chh = (rad * 1.7 + S.r * .33) / 2;
      const y = Math.min(forkY + (S.h - forkY) * (.04 + tt * .92) + hash(i, 5) * .5, S.h - chh);
      cards.push([cw, chh * 2, Math.cos(a) * rad, y, Math.sin(a) * rad, -a, -.42 - hash(i, 8) * .3,
        [.86 + hash(i, 4) * .3, .9 + hash(i, 6) * .22, .84 + hash(i, 7) * .26]]);
    } else {
      // Three lobes, not one ball: broadleaf crowns are clustered.
      const lobe = i % 3, la = lobe * 2.094 + H;
      const spread = .45 + hash(i, 1) * .55, a = i * 2.399 + H;
      const lob = S.r * .28, halfW = S.r * .36, halfH = S.r * .33;
      const cx = Math.cos(la) * lob, cz = Math.sin(la) * lob;
      // lob + rad + halfW ≤ S.r ⇒ rad ≤ S.r*.36 at full spread.
      const rad = S.r * .36 * spread;
      // Highest card CENTRE that still keeps the card's top on the species height.
      const fTop = (S.h - forkY - halfH) / (S.h - forkY);
      const y = forkY + (S.h - forkY) * (.28 + hash(i, 11) * (fTop - .28)) - (1 - spread) * .5;
      cards.push([halfW * 2, halfH * 2, cx + Math.cos(a) * rad, y, cz + Math.sin(a) * rad, -a, -.28 + hash(i, 12) * .5,
        [.84 + hash(i, 4) * .34, .88 + hash(i, 6) * .24, .8 + hash(i, 7) * .3]]);
    }
  }
  /* An open-grown crown is seen against the sky from every side, and a
     shell of cards alone reads as a see-through cloud of leaves. `core`
     packs extra, larger cards into the middle third so the crown has a
     body behind its outline. */
  for (let i = 0; i < (S.core || 0); i++) {
    const a = i * 2.399 + H + 1, rad = S.r * .3 * Math.sqrt(hash(i, 51)), hw = S.r * .42, hh = S.r * .36;
    const y = forkY + (S.h - forkY) * (.3 + hash(i, 52) * .45);
    cards.push([hw * 2, hh * 2, Math.cos(a) * rad, y, Math.sin(a) * rad, -a + hash(i, 53), -.3 + hash(i, 54) * .6,
      [.78 + hash(i, 55) * .3, .84 + hash(i, 56) * .22, .74 + hash(i, 57) * .3]]);
  }
  const canopy = MC(K, `${id}_canopy`, mats[S.leaf], cards);
  /* Calibrate the crown to the species radius. The placement maths above
     bounds the WORST case — a card whose width points straight out along its
     own radius — and that case barely happens: the cards are tangential and
     tilted, so the realised crown came out 9–12% inside spec. Under-spec is
     not safe here. LOD1 is a billboard at the species radius, so a crown 11%
     narrow is an 11% pop at the 90 m swap against a contract that allows 5%,
     and 780 belt instances each sit a little small on a 3.5 m lattice.
     So measure the crown that actually got built and scale it out to the
     radius in plan. Y is untouched — the card heights are already clamped to
     land exactly on the species height. */
  canopy.geometry.computeBoundingBox();
  const cb = canopy.geometry.boundingBox;
  const half = Math.max(Math.abs(cb.min.x), Math.abs(cb.max.x), Math.abs(cb.min.z), Math.abs(cb.max.z));
  if (half > .01) { canopy.geometry.scale(S.r / half, 1, S.r / half); canopy.geometry.computeBoundingBox(); }
  canopy.castShadow = true; g.add(canopy);
  // A crown is round in plan. Measured as a box its corners reach r√2, and
  // an oak 11 m off a lane read as standing 0.8 m from it.
  g.userData.plan = 'round';
  return g;
}

/** LOD1: a billboard cross carrying the same silhouette. One part, 4 tris.
 *
 *  Sized off a throwaway build of LOD0 rather than off the species radius.
 *  The calibration above puts the crown's REACH on the species radius, but a
 *  real crown is lopsided — it leans, and the three lobes do not balance — so
 *  its bounding box is narrower than a symmetric 2r billboard and sits off the
 *  trunk axis. Billboarding at the nominal radius was an 8% jump in outline at
 *  the 90 m swap against a contract that allows 5%. buildTree is deterministic
 *  (hashed, no RNG), so measuring it here costs one authoring-time build and
 *  makes the swap exact. */
function buildTreeLod(K, id, key) {
  const { THREE, grp, mats } = K, S = SPECIES[key], g = grp(id);
  const src = buildTree(K, `${id}__measure`, key);
  const bb = new THREE.Box3().setFromObject(src);
  const sz = bb.getSize(new THREE.Vector3()), c = bb.getCenter(new THREE.Vector3());
  const w = Math.max(sz.x, sz.z), h = sz.y;
  const m = MC(K, `${id}_card`, mats[S.leaf], [
    [w, h, c.x, h / 2, c.z, 0, 0],
    [w, h, c.x, h / 2, c.z, Math.PI / 2, 0],
  ]);
  g.add(m);
  return g;
}

for (const [key, S] of Object.entries(SPECIES)) {
  const id = `toaster_tree_${key}`;
  P({
    id, label: `Tree — ${key.replace('_', ' ')}`, size: `${S.h} m · canopy r ${S.r}`, swatch: key === 'pine' ? '#46583a' : key === 'maple' ? '#b8452a' : '#b07a33',
    instanced: true, budgetTris: 600, budgetParts: 2, lod1: `${id}_lod1`, lodSwitch: 90,
    stats: { Height: `${S.h} m`, Canopy: `r ${S.r} m`, Parts: '2 (trunk, canopy)', LOD1: '90 m' },
    note: `Summer ${key}, root collar on grade, trunk up +Y. EXACTLY two parts — \`${id}_trunk\` and \`${id}_canopy\` — because culling, visibility ranges and mesh LOD all act on a multimesh's whole bounding box, the belt is cut into 40 m cells, and every part multiplies the number of them. Canopy is MASK leaf cards off the shared 1024² atlas: blended leaves sort badly per instance and cost a transparent pass on the most-instanced thing in the game. Limbs leave the bole below the canopy — a crown floating over a pole is what gives a billboard away when you walk up to it.`,
    build(K) { return buildTree(K, id, key); },
  });
  P({
    id: `${id}_lod1`, label: `Tree — ${key} (LOD1)`, size: `${S.h} m billboard`, swatch: '#6a6350',
    instanced: true, budgetTris: 40, budgetParts: 1,
    stats: { Cards: '2 (cross)', Tris: '4', From: '90 m', Shadows: 'off' },
    note: `Distance stand-in for \`${id}\`: a billboard cross carrying the same silhouette, drawn from the same atlas so the 90 m swap does not change the outline or the colour. Casts no shadow. Without it the full tree draws at every distance — still instanced, so it runs; it just costs.`,
    build(K) { return buildTreeLod(K, `${id}_lod1`, key); },
  });
}

P({
  id: 'toaster_understory', label: 'Understory clump (4 m)', size: '4 m', swatch: '#6a6a3e',
  instanced: true, budgetTris: 300, budgetParts: 1,
  stats: { Clump: '4 m', Height: '1.4 m', Cards: 'MASK', Optional: 'degrades to nothing' },
  note: 'Shrubs and bracken for the inner edge of the tree belt, so the wood has a floor and the trunks do not stand in mown grass. One merged part of MASK cards off the shared leaf atlas, origin at ground centre. Optional — without it the belt is bare underneath.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_understory'), cards = [];
    for (let i = 0; i < 34; i++) {
      const a = i * 2.399, r = Math.pow(hash(i, 21), .5) * 1.7, h = .7 + hash(i, 22) * .9;
      cards.push([1.0 + hash(i, 23) * .8, h, Math.cos(a) * r, h / 2, Math.sin(a) * r, -a + hash(i, 24), -.1,
        [.7 + hash(i, 25) * .4, .8 + hash(i, 26) * .3, .6 + hash(i, 27) * .3]]);
    }
    g.add(MC(K, 'toaster_understory_cards', mats.toa_leaf_pine, cards));
    return g;
  },
});

/* ═══ P1 — the four buildings ══════════════════════════════════════════════

   Each building is two files: a shell (exterior and interior in one, door
   openings cut) and a roof, separate because the roof is a surface players
   stand on and the code puts a collider under it. Interior dressing is its own
   props, placed by the level file, so rooms arrange around the teleport pads
   and the sockets rather than being baked around nothing.

   Sides, resolved against `ToasterLevel.cs` rather than guessed: East = +X,
   West = −X, North = −Z, South = +Z. Upstream's `Side.North` is Godot's
   forward, which is −Z — the opposite of the layout file's "+Z north" comment,
   and the one place on this map where the two conventions disagree. Every
   opening below is stated in metres along the wall from its centre, positive
   toward +Z on an X wall and toward +X on a Z wall, which is how the manifest
   states them and how the code reads them.

   Walls are 0.3 m thick and built as solid boxes, so they have an inside face
   and an outside face for free: the shell is read from inside as often as from
   out, and a single-sided box is a room with no walls when you are standing
   in it. */

const SIDE = { E: 'E', W: 'W', N: 'N', S: 'S' };

/**
 * One wall's boxes, with rectangular openings cut by splitting the wall into
 * piers and headers rather than by subtracting geometry: three boxes where a
 * door is, one where it is not, and no CSG anywhere.
 *
 * Local frame: u runs along the wall, v is up, and the returned entries are
 * [len, height, thickness, u, v] for the caller to place.
 */
function wallPanels(L, H, openings) {
  const out = [], sorted = [...openings].sort((a, b) => a.at - b.at);
  let u = -L / 2;
  for (const o of sorted) {
    const a = o.at - o.w / 2, b = o.at + o.w / 2;
    if (a - u > .01) out.push([a - u, H, (u + a) / 2, H / 2]);
    // Header over the opening. Openings start at grade — these are door
    // openings, and a door that closes is a wall the sim cannot model.
    if (H - o.h > .01) out.push([o.w, H - o.h, (a + b) / 2, o.h + (H - o.h) / 2]);
    u = b;
  }
  if (L / 2 - u > .01) out.push([L / 2 - u, H, (u + L / 2) / 2, H / 2]);
  return out;
}

/**
 * A stepped triangular gable field, returned as box entries in the caller's
 * frame. `axis` 'x' spans in X (gable faces ±Z); 'z' spans in Z.
 *
 * Stepped rather than a true triangle because it merges into the same box
 * mesh as the walls it sits on — at six steps the stair is under 150 mm and
 * reads as a rake board, and it costs nothing extra in parts.
 */
function gableField(axis, span, base, apex, thick, cx, cz, steps = 6) {
  const out = [], rise = apex - base;
  if (rise <= .01) return out;
  for (let i = 0; i < steps; i++) {
    const t0 = i / steps, t1 = (i + 1) / steps;
    const half = (span / 2) * (1 - t1), y0 = base + rise * t0, y1 = base + rise * t1;
    if (half <= .05) continue;
    if (axis === 'x') out.push([half * 2, y1 - y0, thick, cx, (y0 + y1) / 2, cz]);
    else out.push([thick, y1 - y0, half * 2, cx, (y0 + y1) / 2, cz]);
  }
  return out;
}

/**
 * An arched opening head: voussoir boxes stepping round a segmental arch.
 * The main house's front door sits in a tall brick arch and it is the single
 * most recognisable thing on the property, so it is built rather than
 * implied by a rectangular casing.
 */
function archHead(w, springY, riseY, thick, cx, cz, axis, steps = 7) {
  const out = [], R = (w * w / 4 + riseY * riseY) / (2 * riseY), cy = springY + riseY - R;
  for (let i = 0; i < steps; i++) {
    const a0 = Math.PI * (i / steps), a1 = Math.PI * ((i + 1) / steps);
    const x0 = Math.cos(Math.PI - a0) * (w / 2), x1 = Math.cos(Math.PI - a1) * (w / 2);
    const yTop = springY + riseY, y = cy + Math.sqrt(Math.max(0, R * R - Math.pow((x0 + x1) / 2, 2)));
    const seg = Math.abs(x1 - x0) + .04, h = Math.max(.06, yTop - y + .22);
    if (axis === 'x') out.push([seg, h, thick, cx + (x0 + x1) / 2, y + h / 2 - .11, cz]);
    else out.push([thick, h, seg, cx, y + h / 2 - .11, cz + (x0 + x1) / 2]);
  }
  return out;
}

/**
 * A building shell. Walls, floor, foundation, gables, window bands and the
 * interior frame — twelve parts at the outside, one 2048² set.
 *
 * `doors` entries: { side, at, w, h }. `at` is measured along the wall from
 * its centre, exactly as the manifest states them.
 *
 * `wings` entries: { side, at, w, proj, eaves, arch }. A wing is a projecting
 * cross-gabled bay — the thing that makes these read as the houses on the
 * property rather than as extruded rectangles. Its eaves sit BELOW the main
 * eaves and its ridge rises to the authored roof height, so a 25–35° pitch is
 * bought by dropping the wing's eaves rather than by raising the ridge: the
 * roof surface, the roof sockets and the ladder heads are all authored at
 * that height upstream and none of them may move.
 */
function buildShell(K, id, o) {
  const { THREE, grp, mats } = K, g = grp(id);
  const { w, d, eaves, doors = [], skin, gable = false, wings = [], trim = 'toa_stucco', bays = 2, pitch = 28 } = o;
  const T = .3, FLOOR = .15, wall = mats[skin];
  const tanP = Math.tan(pitch * Math.PI / 180);
  // Same derivation the roof uses, so a wing's masonry gable and its roof
  // planes meet on one line instead of two that nearly agree.
  const ridgeMax = eaves + (w / bays / 2) * tanP;
  const wingRidge = (wg) => Math.min(wg.eaves + (wg.w / 2) * tanP, ridgeMax);

  // Four walls. The Z walls (north/south) run the full width; the X walls sit
  // between them, so corners meet as a mitre rather than overlapping twice.
  const boxes = [];
  const place = (side, panels) => {
    for (const [len, h, u, v] of panels) {
      if (side === 'N') boxes.push([len, h, T, u, v, -d / 2 + T / 2]);
      else if (side === 'S') boxes.push([len, h, T, u, v, d / 2 - T / 2]);
      else if (side === 'E') boxes.push([T, h, len, w / 2 - T / 2, v, u]);
      else boxes.push([T, h, len, -w / 2 + T / 2, v, u]);
    }
  };
  const byside = (s) => doors.filter((x) => x.side === s && !x.wing).map((x) => ({ at: x.at, w: x.w ?? 2.4, h: x.h ?? 2.6 }));
  place('N', wallPanels(w, eaves, byside('N')));
  place('S', wallPanels(w, eaves, byside('S')));
  place('E', wallPanels(d - T * 2, eaves, byside('E')));
  place('W', wallPanels(d - T * 2, eaves, byside('W')));
  const walls = MB(K, `${id}_walls`, wall, boxes);
  walls.castShadow = true; walls.receiveShadow = true; g.add(walls);

  /* Wings: the projecting cross-gabled bays. Each carries its own two side
     walls and a gable end, and the end wall's opening is the one the level
     file's front door lines up with. */
  const wingBrick = [], wingTrim = [];
  const SGN = { N: -1, S: 1, E: 1, W: -1 };
  for (const wg of wings) {
    const s = SGN[wg.side], onZ = wg.side === 'N' || wg.side === 'S';
    const face = (onZ ? d : w) / 2, outer = face + wg.proj, we = wg.eaves;
    const dr = doors.find((x) => x.wing === wg.id);
    const panels = wallPanels(wg.w, we, dr ? [{ at: 0, w: dr.w ?? 2.4, h: dr.h ?? 2.6 }] : []);
    if (onZ) {
      for (const sd of [-1, 1]) wingBrick.push([T, we, wg.proj, wg.at + sd * (wg.w / 2 - T / 2), we / 2, s * (face + wg.proj / 2)]);
      for (const [len, h, u, v] of panels) wingBrick.push([len, h, T, wg.at + u, v, s * (outer - T / 2)]);
      wingBrick.push(...gableField('x', wg.w, we, wingRidge(wg), T, wg.at, s * (outer - T / 2)));
      if (dr && wg.arch) wingTrim.push(...archHead(dr.w ?? 2.4, dr.h ?? 2.6, .9, T + .08, wg.at, s * (outer - T / 2), 'x'));
    } else {
      for (const sd of [-1, 1]) wingBrick.push([wg.proj, we, T, s * (face + wg.proj / 2), we / 2, wg.at + sd * (wg.w / 2 - T / 2)]);
      for (const [len, h, u, v] of panels) wingBrick.push([T, h, len, s * (outer - T / 2), v, wg.at + u]);
      wingBrick.push(...gableField('z', wg.w, we, wingRidge(wg), T, s * (outer - T / 2), wg.at));
      if (dr && wg.arch) wingTrim.push(...archHead(dr.w ?? 2.4, dr.h ?? 2.6, .9, T + .08, s * (outer - T / 2), wg.at, 'z'));
    }
  }
  if (wingBrick.length) {
    const m = MB(K, `${id}_wings`, wall, wingBrick);
    m.castShadow = true; m.receiveShadow = true; g.add(m);
  }

  // Floor: interior top at y 0.15, which is what the pads and the dressing
  // stand on. A slab, not a plane — it is seen from the doorway edge-on.
  g.add(MB(K, `${id}_floor`, mats.toa_floorboard, [[w - T * 2, FLOOR, d - T * 2, 0, FLOOR / 2, 0]]));
  // Foundation skirt: 150 mm proud of grade, 100 mm proud of the wall face,
  // so the building sits ON the ground instead of being pushed into it.
  /* Steps at every person door, in the same concrete as the skirt. A door
     sill 150 mm over grade with nothing under it is a doorway on a stage. */
  const steps = [];
  for (const dr of doors) {
    // A vehicle bay has an apron, not a step; and `nosteps` where the lane
    // runs close enough that a slab would put the shell's box inside §4.8.
    if ((dr.h ?? 2.6) > 3.4 || (dr.w ?? 2.4) >= 4 || dr.nosteps) continue;
    const wg = dr.wing ? wings.find((x) => x.id === dr.wing) : null, dw = (dr.w ?? 2.4) + .5;
    const onZ = dr.side === 'N' || dr.side === 'S', s = dr.side === 'N' || dr.side === 'W' ? -1 : 1;
    const face = (onZ ? d : w) / 2 + (wg ? wg.proj : 0);
    for (const [dep, h, ww2] of [[.9, .15, dw], [1.5, .07, dw + .5]]) {
      if (onZ) steps.push([ww2, h, dep, dr.at, h / 2, s * (face + dep / 2)]); else steps.push([dep, h, ww2, s * (face + dep / 2), h / 2, dr.at]);
    }
  }
  g.add(MB(K, `${id}_foundation`, mats.toa_concrete, [[w + .2, .18, d + .2, 0, .09, 0], ...steps]));
  // Gutter along both eaves and a downspout at each corner — the line a
  // house has at its eaves that a box with a lid does not.
  const gut = [];
  for (const sz of [-1, 1]) gut.push([w + .7, .1, .12, 0, eaves - .05, sz * (d / 2 + .3)]);
  for (const sx of [-1, 1]) for (const sz of [-1, 1]) gut.push([.08, eaves - .3, .08, sx * (w / 2 + .12), (eaves - .3) / 2 + .1, sz * (d / 2 - .8)], [.08, .08, .4, sx * (w / 2 + .12), eaves - .1, sz * (d / 2 - .6)]);
  g.add(MB(K, `${id}_gutters`, mats.toa_paint_faded, gut));

  /* Gable fields. The main mass no longer needs one — the roof file is now a
     closed gabled MASS rather than a plate, so it carries its own rake ends
     down to the eaves. What is left here are the dormer cheeks where each
     wing ridge dies into the main roof, which the roof cannot close because
     the wing belongs to one file and the wall it lands on to the other. */
  const SG = { N: -1, S: 1, E: 1, W: -1 };
  const gp = [];
  if (gable) {
    for (const sz of [-1, 1]) gp.push(...gableField('x', w - 3.6, eaves, o.top, T, 0, sz * (d / 2 - T / 2)));
  }
  for (const wg of wings) {
    const onZ = wg.side === 'N' || wg.side === 'S', s = SG[wg.side];
    const face = (onZ ? d : w) / 2;
    if (onZ) gp.push(...gableField('x', wg.w * .9, wg.eaves, wingRidge(wg), T, wg.at, s * (face - T / 2)));
    else gp.push(...gableField('z', wg.w * .9, wg.eaves, wingRidge(wg), T, s * (face - T / 2), wg.at));
  }
  if (gp.length) { const m = MB(K, `${id}_gable`, mats[trim], gp); m.castShadow = true; g.add(m); }

  /* Windows: opaque, faintly emissive panels 1 mm proud of the wall face.
     No glass geometry and no transparency anywhere in this kit — a dark pane
     with a warm inner glow reads better at ninety metres than a mirror does,
     and it is one material rather than a sorted pass.

     `windows: false` for the shed. The automatic band is a DOMESTIC band —
     1 m sill, 1.0 × 1.15 panes every 4.2 m — and running it round an
     equipment building put seventeen lit house windows on a blank brick box
     that has none. Rebuilding the shed in brick and then leaving the window
     band on it fixed the material and kept the tell. */
  const win = [], sill = 1.0, wh = 1.15, ww = 1.0, hasWin = o.windows !== false;
  const clearOf = (side, at) => !doors.some((x) => x.side === side && Math.abs(x.at - at) < ((x.w ?? 2.4) / 2 + ww / 2 + .5))
    && !wings.some((x) => x.side === side && Math.abs(x.at - at) < (x.w / 2 + ww / 2 + .4));
  const along = (L, step) => { const n = Math.max(1, Math.floor((L - 2) / step)), out = []; for (let i = 0; i < n; i++) out.push(-L / 2 + 1 + (i + .5) * ((L - 2) / n)); return out; };
  if (hasWin) {
    for (const at of along(w, 4.2)) {
      if (clearOf('N', at)) win.push([ww, wh, .05, at, sill + wh / 2, -d / 2 - .005]);
      if (clearOf('S', at)) win.push([ww, wh, .05, at, sill + wh / 2, d / 2 + .005]);
    }
    for (const at of along(d - T * 2, 4.2)) {
      if (clearOf('E', at)) win.push([.05, wh, ww, w / 2 + .005, sill + wh / 2, at]);
      if (clearOf('W', at)) win.push([.05, wh, ww, -w / 2 - .005, sill + wh / 2, at]);
    }
  }
  if (win.length) g.add(MB(K, `${id}_windows`, mats.toa_window, win));
  /* Casings are FRAMES — head, two jambs, a sill — proud of the wall face.
     The first pass drew each casing as one trim box the size of the opening
     and centred on it, so it sat IN the opening: every window read as a blank
     stucco square and every doorway as a panel, and the lit panes and the
     cut door openings were both hidden behind the thing meant to frame them. */
  const cas = [], F = .07, PD = .08;
  for (const [bw, , , x, y, z] of win) {
      if (bw > .1) {                                                        // pane in a Z wall
        const zz = z + Math.sign(z) * .02;
      cas.push([ww + 2 * F, F, PD, x, y + wh / 2 + F / 2, zz], [F, wh, PD, x - ww / 2 - F / 2, y, zz], [F, wh, PD, x + ww / 2 + F / 2, y, zz],
        [ww + 2 * F + .1, .08, .18, x, y - wh / 2 - .04, zz + Math.sign(z) * .04]);
    } else {                                                              // pane in an X wall
      const xx = x + Math.sign(x) * .02;
      cas.push([PD, F, ww + 2 * F, xx, y + wh / 2 + F / 2, z], [PD, wh, F, xx, y, z - ww / 2 - F / 2], [PD, wh, F, xx, y, z + ww / 2 + F / 2],
        [.18, .08, ww + 2 * F + .1, xx + Math.sign(x) * .04, y - wh / 2 - .04, z]);
    }
  }
  const rail = (side, at, dw2, dh2) => {                                  // sliding-door track over a vehicle bay
    const onZ = side === 'N' || side === 'S', s = side === 'N' || side === 'W' ? -1 : 1;
    if (onZ) cas.push([dw2 * 1.9, .16, .14, at, dh2 + .34, s * (d / 2 + .08)]); else cas.push([.14, .16, dw2 * 1.9, s * (w / 2 + .08), dh2 + .34, at]);
  };
  for (const dr of doors) {
    if (dr.wing) continue;
    const dw = dr.w ?? 2.4, dh = dr.h ?? 2.6, J = .12;
    const onZ = dr.side === 'N' || dr.side === 'S', s = dr.side === 'N' || dr.side === 'W' ? -1 : 1;
    if (onZ) {
      const zz = s * (d / 2 + .01);
      cas.push([J, dh + J, PD, dr.at - dw / 2 - J / 2, (dh + J) / 2, zz], [J, dh + J, PD, dr.at + dw / 2 + J / 2, (dh + J) / 2, zz], [dw + 2 * J, J, PD, dr.at, dh + J / 2, zz]);
    } else {
      const xx = s * (w / 2 + .01);
      cas.push([PD, dh + J, J, xx, (dh + J) / 2, dr.at - dw / 2 - J / 2], [PD, dh + J, J, xx, (dh + J) / 2, dr.at + dw / 2 + J / 2], [PD, J, dw + 2 * J, xx, dh + J / 2, dr.at]);
    }
    if (dh > 3.4) rail(dr.side, dr.at, dw, dh);
  }
  for (const l of o.leaves || []) if (l.h > 3.4) rail(l.side, l.at, l.w, l.h);
  cas.push(...wingTrim);
  g.add(MB(K, `${id}_casings`, mats[trim], cas));

  /* Interior frame: studs and a tie beam. Cheap, and it is the difference
     between a room and a box — every one of these buildings is walked into. */
  const st = [], sp = 2.4;
  for (let x = -w / 2 + 1.2; x <= w / 2 - 1.2; x += sp) {
    st.push([.12, eaves - FLOOR, .12, x, FLOOR + (eaves - FLOOR) / 2, -d / 2 + T + .06]);
    st.push([.12, eaves - FLOOR, .12, x, FLOOR + (eaves - FLOOR) / 2, d / 2 - T - .06]);
    st.push([.14, .2, d - T * 2, x, eaves - .12, 0]);
  }
  g.add(MB(K, `${id}_frame`, mats.toa_timber_bare, st));

  /* Closed door leaves. The shed's second bay is shut in every photograph of
     it, and a closed leaf is not an opening — it is a wall with a door drawn
     on it, which is exactly what the sim wants and what the contract's
     "door openings only" rule is about. */
  if (o.leaves && o.leaves.length) {
    const lv = [];
    for (const l of o.leaves) {
      const sg = l.side === 'N' || l.side === 'W' ? -1 : 1;
      if (l.side === 'N' || l.side === 'S') lv.push([l.w, l.h, .10, l.at, l.h / 2 + .02, sg * (d / 2 - .02)]);
      else lv.push([.10, l.h, l.w, sg * (w / 2 - .02), l.h / 2 + .02, l.at]);
    }
    g.add(MB(K, `${id}_doorleaf`, mats.toa_doorpanel, lv));
  }
  return g;
}

/**
 * A roof. Mass, deck, fascia — three parts.
 *
 * The first pass read the contract as "the roof is flat at the stated
 * height", made the whole 34 × 26 m plan one shallow plate, and that single
 * decision is most of why these buildings read as sheds with a lid. What the
 * contract actually says is that ONE flat area of at least 4 × 4 m sits at
 * the stated height — where the roof collider, the wall socket and the
 * ladder head are — and that "pitched slopes elsewhere are fine at ≤ 30°".
 *
 * So the flat area is now 6 × 6 m placed AT the ladder head rather than
 * spread over the whole plan, and the rest is a real gabled roof: ridges
 * running in Z, the plan divided into bays across X so no single span has to
 * carry an absurd ridge, at 28° — inside the limit and the pitch these houses
 * are actually built at. On the main house that is three parallel gables
 * topping out at 6.4 m over 3.4 m eaves, which is the roofline in the
 * photographs instead of a warehouse lid.
 *
 * Every one of the four ladder heads is authored at a CORNER of its building,
 * so the flat deck lands as a lower corner bay with the gables rising beside
 * it — an attached garage roof, which is also what is there in reality.
 *
 * Built as a heightfield on an explicit grid: the x samples include every bay
 * boundary, every ridge line and both deck edges, so each crease is exact and
 * the whole roof is about a hundred cells rather than a fine mesh.
 */
function buildRoof(K, id, o) {
  const { THREE, grp, mats } = K, g = grp(id);
  const { w, d, eaves, top, skin = 'toa_shingle', over = .45, wings = [], bays = 2, pitch = 28, deckAt = [0, 0] } = o;
  const tanP = Math.tan(pitch * Math.PI / 180);
  const X = w / 2 + over, Z = d / 2 + over;
  const DHW = 3.0;                                   // 6 × 6 m flat area
  const dx0 = deckAt[0] - DHW, dx1 = deckAt[0] + DHW, dz0 = deckAt[1] - DHW, dz1 = deckAt[1] + DHW;

  const bw = w / bays;
  const ridgeX = [], edgeX = [];
  for (let i = 0; i < bays; i++) { ridgeX.push(-w / 2 + (i + .5) * bw); edgeX.push(-w / 2 + i * bw); }
  const gableH = (x) => {
    let best = eaves;
    for (const cx of ridgeX) best = Math.max(best, eaves + Math.max(0, bw / 2 - Math.abs(x - cx)) * tanP);
    return best;
  };
  const inDeck = (x, z) => x > dx0 - 1e-6 && x < dx1 + 1e-6 && z > dz0 - 1e-6 && z < dz1 + 1e-6;
  const H = (x, z) => (inDeck(x, z) ? top : gableH(x));

  const uniq = (a) => [...new Set(a.map((v) => +v.toFixed(4)))].sort((p, q) => p - q);
  const xs = uniq([-X, -w / 2, ...ridgeX, ...edgeX, w / 2, X,
    ...[dx0, dx1].filter((v) => v > -X && v < X)]);
  const zs = uniq([-Z, -d / 2, 0, d / 2, Z, ...[dz0, dz1].filter((v) => v > -Z && v < Z)]);

  const pos = [], uv = [], idx = [], col = [];
  const quad = (a, b, c, e) => {
    const base = pos.length / 3;
    for (const p of [a, b, c, e]) { pos.push(p[0], p[1], p[2]); col.push(1, 1, 1); }
    uv.push(0, 0, 1, 0, 1, 1, 0, 1);
    idx.push(base, base + 1, base + 2, base, base + 2, base + 3);
  };
  const gp = [], guv = [], gidx = [], gcol = [];
  const gquad = (a, b, c, e) => {
    const base = gp.length / 3;
    for (const p of [a, b, c, e]) { gp.push(p[0], p[1], p[2]); gcol.push(1, 1, 1); }
    guv.push(a[0] * .3, a[1] * .3, b[0] * .3, b[1] * .3, c[0] * .3, c[1] * .3, e[0] * .3, e[1] * .3);
    gidx.push(base, base + 1, base + 2, base, base + 2, base + 3);
  };
  // Top surface, cell by cell. A cell is flat because every crease is a grid
  // line, so the corner heights of one cell always describe one plane.
  for (let i = 0; i < xs.length - 1; i++) {
    for (let j = 0; j < zs.length - 1; j++) {
      const x0 = xs[i], x1 = xs[i + 1], z0 = zs[j], z1 = zs[j + 1];
      const mx = (x0 + x1) / 2, mz = (z0 + z1) / 2, dk = inDeck(mx, mz);
      const hy = (x) => (dk ? top : gableH(x));
      quad([x0, hy(x0), z0], [x1, hy(x1), z0], [x1, hy(x1), z1], [x0, hy(x0), z1]);
    }
  }
  // Vertical closers: the gable ends and the step down onto the flat deck.
  const closer = (ax, ay0, ay1, az, bx, by0, by1, bz) => {
    if (Math.abs(ay1 - ay0) < .01 && Math.abs(by1 - by0) < .01) return;
    quad([ax, ay0, az], [bx, by0, bz], [bx, by1, bz], [ax, ay1, az]);
  };
  /* Gable ends. These are WALL, not roof: a masonry (or on the houses,
     dashed-stucco) triangle in the wall plane under the rake, which is the
     two-tone read these rooflines have. The first pass closed the ends with
     shingle at the overhang line and every gable end became a dark flat
     triangle of roof. Built as a second surface in its own material, cell
     by cell off the same heightfield, so where the flat deck reaches the
     edge the wall stops at the deck, not at the slope. */
  for (let i = 0; i < xs.length - 1; i++) {
    const x0 = Math.max(-w / 2, xs[i]), x1 = Math.min(w / 2, xs[i + 1]), mx = (x0 + x1) / 2;
    if (x1 - x0 < .01) continue;
    for (const [z, sgn] of [[-d / 2 - .012, -1], [d / 2 + .012, 1]]) {
      const mz = z + sgn * -.01, dk = inDeck(mx, mz);
      const h0 = dk ? top : gableH(x0), h1 = dk ? top : gableH(x1), e0 = eaves - .3;
      if (sgn < 0) gquad([x0, e0, z], [x0, h0, z], [x1, h1, z], [x1, e0, z]);
      else gquad([x1, e0, z], [x1, h1, z], [x0, h0, z], [x0, e0, z]);
    }
  }
  // Step faces round the deck, where the lower flat bay meets a gable.
  for (let j = 0; j < zs.length - 1; j++) {
    const z0 = zs[j], z1 = zs[j + 1], mz = (z0 + z1) / 2;
    for (const [x, sgn] of [[dx0, -1], [dx1, 1]]) {
      if (x <= -X || x >= X) continue;
      const inside = inDeck(x + sgn * .01, mz), outside = inDeck(x - sgn * .01, mz);
      if (inside === outside) continue;
      const hg0 = gableH(x - sgn * .01), hg1 = gableH(x + sgn * .01);
      const hi = Math.max(hg0, hg1, top), lo = top;
      if (hi - lo < .02) continue;
      if (sgn > 0) gquad([x, lo, z1], [x, hi, z1], [x, hi, z0], [x, lo, z0]);
      else gquad([x, lo, z0], [x, hi, z0], [x, hi, z1], [x, lo, z1]);
    }
  }
  for (let i = 0; i < xs.length - 1; i++) {
    const x0 = xs[i], x1 = xs[i + 1], mx = (x0 + x1) / 2;
    for (const [z, sgn] of [[dz0, -1], [dz1, 1]]) {
      if (z <= -Z || z >= Z) continue;
      const inside = inDeck(mx, z + sgn * .01), outside = inDeck(mx, z - sgn * .01);
      if (inside === outside) continue;
      const h0 = Math.max(gableH(x0), top), h1 = Math.max(gableH(x1), top);
      if (h0 - top < .02 && h1 - top < .02) continue;
      if (sgn > 0) gquad([x0, top, z], [x0, h0, z], [x1, h1, z], [x1, top, z]);
      else gquad([x1, top, z], [x1, h1, z], [x0, h0, z], [x0, top, z]);
    }
  }

  /* Wing roofs. Two planes falling from a ridge that runs OUT from the main
     mass, at the same 28° — so a projecting bay reads as a real cross gable
     against the main roof rather than as a flat nose. The ridge is derived
     from the wing's own eaves and half-width, never chosen, and is capped at
     the main ridge so a porch never out-tops the house it is attached to. */
  const SG = { N: -1, S: 1, E: 1, W: -1 };
  const ridgeMax = eaves + (bw / 2) * tanP;
  for (const wg of wings) {
    const s = SG[wg.side], onZ = wg.side === 'N' || wg.side === 'S';
    const face = (onZ ? d : w) / 2, outer = face + wg.proj + over, inner = face - .4;
    const hw = wg.w / 2 + over, we = wg.eaves;
    const ry = Math.min(we + (wg.w / 2) * tanP, ridgeMax);
    wg._ridge = ry;
    if (onZ) {
      const zo = s * outer, zi = s * inner, a = wg.at;
      quad([a - hw, we, zo], [a, ry, zo], [a, ry, zi], [a - hw, we, zi]);
      quad([a + hw, we, zi], [a, ry, zi], [a, ry, zo], [a + hw, we, zo]);
      quad([a - hw, we, zo], [a - hw, we, zo], [a, ry, zo], [a + hw, we, zo]);   // verge
    } else {
      const xo = s * outer, xi = s * inner, a = wg.at;
      quad([xo, we, a - hw], [xo, ry, a], [xi, ry, a], [xi, we, a - hw]);
      quad([xi, we, a + hw], [xi, ry, a], [xo, ry, a], [xo, we, a + hw]);
      quad([xo, we, a - hw], [xo, we, a - hw], [xo, ry, a], [xo, we, a + hw]);
    }
  }

  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
  geo.setIndex(idx); geo.computeVertexNormals();
  const mass = new THREE.Mesh(solid(THREE, geo), mats[skin]); mass.name = `${id}_slopes`;
  mass.material.side = THREE.DoubleSide;
  mass.castShadow = true; mass.receiveShadow = true; g.add(mass);
  if (gidx.length) {
    const gg = new THREE.BufferGeometry();
    gg.setAttribute('position', new THREE.Float32BufferAttribute(gp, 3));
    gg.setAttribute('uv', new THREE.Float32BufferAttribute(guv, 2));
    gg.setAttribute('color', new THREE.Float32BufferAttribute(gcol, 3));
    gg.setIndex(gidx); gg.computeVertexNormals();
    const gm = new THREE.Mesh(gg, mats[o.gableMat || skin]); gm.name = `${id}_gable`;
    gm.castShadow = true; gm.receiveShadow = true; g.add(gm);
  }

  // The walkable flat: a real slab at the ladder head, 100 mm proud so the
  // collider the code puts under it has something to sit on.
  const deck = MB(K, `${id}_deck`, mats[skin], [[DHW * 2, .12, DHW * 2, deckAt[0], top - .06, deckAt[1]]]);
  deck.castShadow = true; deck.receiveShadow = true; g.add(deck);

  const fas = [
    [w + over * 2 + .1, .22, .1, 0, eaves - .11, -Z], [w + over * 2 + .1, .22, .1, 0, eaves - .11, Z],
    [.1, .22, d + over * 2 + .1, -X, eaves - .11, 0], [.1, .22, d + over * 2 + .1, X, eaves - .11, 0],
  ];
  for (const wg of wings) {
    const s = SG[wg.side], onZ = wg.side === 'N' || wg.side === 'S';
    const face = (onZ ? d : w) / 2;
    if (onZ) fas.push([.1, .22, wg.proj + over, wg.at - wg.w / 2 - over, wg.eaves - .11, s * (face + wg.proj / 2)],
      [.1, .22, wg.proj + over, wg.at + wg.w / 2 + over, wg.eaves - .11, s * (face + wg.proj / 2)]);
    else fas.push([wg.proj + over, .22, .1, s * (face + wg.proj / 2), wg.eaves - .11, wg.at - wg.w / 2 - over],
      [wg.proj + over, .22, .1, s * (face + wg.proj / 2), wg.eaves - .11, wg.at + wg.w / 2 + over]);
  }
  g.add(MB(K, `${id}_fascia`, mats.toa_timber_bare, fas));
  /* Chimneys, through the slope to 0.9 m over the ridge line where they stand
     — in the shell's brick, because that is what they are. */
  if (o.chimneys && o.chimneys.length) {
    const ch = [];
    for (const [cx, cz] of o.chimneys) { const top = gableH(cx) + .9, base = eaves - .6; ch.push([.8, top - base, .8, cx, (top + base) / 2, cz], [.95, .12, .95, cx, top + .06, cz]); }
    const m = MB(K, `${id}_chimney`, mats[o.chimneyMat || 'toa_brick_red'], ch); m.castShadow = true; g.add(m);
  }
  // Ridge cupolas on the shed: louvred boxes under their own caps, which is
  // how a metal-roofed machine shed vents.
  if (o.cupolas && o.cupolas.length) {
    const cp = [], ry = eaves + (bw / 2) * tanP;
    for (const cz of o.cupolas) cp.push([1.4, 1.0, 1.4, 0, ry + .4, cz], [1.9, .12, 1.9, 0, ry + .96, cz], [1.3, .3, 1.3, 0, ry + 1.15, cz], [.6, .26, .6, 0, ry + 1.4, cz]);
    const m = MB(K, `${id}_cupola`, mats[skin], cp); m.castShadow = true; g.add(m);
  }
  return g;
}

const BUILDINGS = [
  {
    key: 'barn', label: 'Barn', w: 30, d: 21, eaves: 6.0, roof: 6.3, bays: 1, pitch: 12, deckAt: [12, -7.5], windows: false, cupolas: [-6, 6],
    centre: '(−108, 54)', swatch: '#8d5a46', skin: 'toa_brick_red', roofSkin: 'toa_corrugate', trim: 'toa_stucco',
    doors: [{ side: 'S', at: 0, w: 5, h: 4 }, { side: 'N', at: -8, w: 2.4, h: 2.6, nosteps: true }],
    // The second bay is shut in every photograph of the real shed, so it is a
    // white leaf on a solid wall rather than a second opening. An opening is
    // a thing enemies path through; a leaf is not.
    leaves: [{ side: 'S', at: 8.2, w: 4.6, h: 3.8 }],
    note: 'The equipment shed, 30 × 21, eaves 6.0 and roof surface 6.3 — red-brown brick under a 12° (about 2½:12) metal gable ridging at 9.2 m, with two louvred cupolas on the ridge and sliding-door rails over both bays: the 6° lid of the first pass read as a flat roof from every angle the game uses. Two vehicle bays across the +Z elevation, the left one open at 5 × 4 because the Gator is driven through it (2.9 long, 1.5 wide, 1.85 tall) and the right one a closed white overhead leaf; the person door is on −Z, 8 m left of centre. NO WINDOWS — the real building is a blank brick rectangle, and the kit’s automatic domestic band (1 m sill, panes every 4.2 m) put seventeen lit house windows on it. The one building with no teleport pad: it is the vehicle shed. Ladder to the roof goes on the +X wall, 8.5 m toward −Z (`shared_ladder`, placed by the level file).',
  },
  {
    key: 'house_buggy', label: 'Buggy house', w: 19, d: 28, eaves: 3.4, roof: 3.7, bays: 2, pitch: 28, deckAt: [-6.5, 11], skin: 'toa_brick_brown', roofSkin: 'toa_shingle_brown', trim: 'toa_stucco', chimneys: [[3, -5]],
    centre: '(−134, −7)', swatch: '#7d5742',
    doors: [{ side: 'E', at: 6, wing: 'entry' }, { side: 'W', at: -8 }],
    wings: [
      { id: 'entry', side: 'E', at: 6, w: 5.2, proj: 2.0, eaves: 2.35, arch: true },
      { side: 'E', at: -7, w: 6.4, proj: 1.2, eaves: 2.7 },
      { side: 'W', at: 9, w: 5.6, proj: 1.1, eaves: 2.7 },
    ],
    note: 'Single-storey brick ranch, 19 × 28, eaves 3.4, roof surface 3.7 — long and low, with a gabled entry bay projecting 2 m off the +X elevation and a second cross gable down the run. The front door sits IN that entry bay under a brick arch, 6 m toward +Z; back door on −X, 8 m toward −Z. Teleport pad inside at local (0, 0, −5) — placed by the level file, not baked in, so the room arranges around the pad. Ladder on the −X wall, 12 m toward +Z.',
  },
  {
    key: 'house_vehickle', label: 'Vehickle house', w: 34, d: 26, eaves: 3.4, roof: 3.7, bays: 3, pitch: 28, deckAt: [14, -10], skin: 'toa_brick_red', roofSkin: 'toa_shingle_brown', trim: 'toa_stucco_warm', chimneys: [[-8, 5], [3, -3]],
    centre: '(30, 31)', swatch: '#8d5a46',
    doors: [{ side: 'N', at: -10, w: 2.6, h: 2.7, wing: 'entry', nosteps: true }, { side: 'S', at: 12 }, { side: 'W', at: 0, w: 4.5, h: 2.8 }],
    wings: [
      { id: 'entry', side: 'N', at: -10, w: 5.6, proj: 1.8, eaves: 2.15, arch: true },
      { side: 'N', at: 2, w: 8.0, proj: 1.2, eaves: 2.55 },
      { side: 'N', at: 12.5, w: 6.2, proj: 0.8, eaves: 2.7 },
      { side: 'W', at: 7, w: 7.0, proj: 1.2, eaves: 2.65 },
      { side: 'S', at: -4, w: 7.4, proj: 1.3, eaves: 2.6 },
    ],
    note: 'The large main house, 34 × 26, with the circular drive to its north. Red-brown brick to the eaves with stucco gable fields above — the two-tone that gives this house its roofline. The −Z elevation is three stepped cross gables: a tall arched entry bay projecting 1.8 m at 10 m toward −X, a broad gable beside it, and a smaller one at the east end. Back door on +Z 12 m toward +X, and a 4.5 × 2.8 garage opening on −X centred — the quad lives in it. One storey deliberately: a second floor needs a climb, and a climb the sim cannot see is a tier the coverage rules cannot reason about. Pad at local (−7, 0, 0); ladder on +X, 11 m toward −Z.',
  },
  {
    key: 'house_grnmchn', label: 'Grnmchn house', w: 24, d: 30, eaves: 3.4, roof: 3.7, bays: 2, pitch: 28, deckAt: [-9, 12], skin: 'toa_brick_buff', roofSkin: 'toa_shingle', trim: 'toa_stucco', chimneys: [[5, -7]],
    centre: '(90, −50)', swatch: '#a8917a',
    doors: [{ side: 'W', at: 0, wing: 'entry', nosteps: true }, { side: 'E', at: 8 }],
    wings: [
      /* The lane runs 4.3 m off this house's −X face, which is also the face
         the front door is on. §4.8 wants 3 m clear of a lane measured from a
         piece's own footprint and §4.10 wants 5.5 m of headroom over one, so
         the entry bay projects 0.9 m rather than the 2.1 the elevation wants:
         on this house the arch has to carry the entrance on its own. */
      { id: 'entry', side: 'W', at: 0, w: 5.4, proj: 0.9, eaves: 2.3, arch: true },
      { side: 'W', at: -10, w: 6.0, proj: 0.7, eaves: 2.65 },
      { side: 'S', at: 5, w: 7.2, proj: 1.4, eaves: 2.6 },
      { side: 'N', at: -6, w: 6.6, proj: 1.1, eaves: 2.7 },
    ],
    note: 'The second house, south-east, facing the pond across an arched entry bay on its −X elevation; back door on +X 8 m toward +Z. Pale buff brick with stucco gables, 24 × 30, eaves 3.4, roof surface 3.7, cross gables on three elevations so it reads as a plan rather than a block from every approach. Pad at local (−4, 0, 2); ladder on the −X wall, 13 m toward +Z.',
  },
];

for (const b of BUILDINGS) {
  const sid = `toaster_${b.key}_shell`, rid = `toaster_${b.key}_roof`;
  P({
    id: sid, label: `${b.label} — shell`, size: `${b.w}×${b.d} m · eaves ${b.eaves}`, swatch: b.swatch,
    budgetTris: 7000, budgetParts: 14,
    stats: { Footprint: `${b.w}×${b.d}`, Eaves: `${b.eaves} m`, Centre: b.centre, Walls: '0.3 m', Floor: 'y 0.15' },
    note: `${b.note} Walls 0.3 m thick as solid boxes so the shell reads from inside as well as out, interior floor top at y 0.15, clear height over 3 m everywhere a player walks. DOOR OPENINGS ONLY, no doors — a door that closes is a wall, and the sim cannot model one that opens. Windows are opaque faintly-emissive panels in the wall face, not glass: no transparency anywhere in this kit. Casings are frames (head, jambs, sill) proud of the face rather than boxes in the opening; concrete steps at every person door; gutters and corner downspouts at the eaves.`,
    build(K) { return buildShell(K, sid, { w: b.w, d: b.d, eaves: b.eaves, top: b.roof, doors: b.doors, skin: b.skin, wings: b.wings, trim: b.trim, leaves: b.leaves, bays: b.bays, pitch: b.pitch, windows: b.windows }); },
  });
  P({
    id: rid, label: `${b.label} — roof`, size: `${b.w}×${b.d} m · top ${b.roof}`, swatch: b.swatch,
    budgetTris: 2000, budgetParts: 6,
    stats: { Flat: `6×6 m at y ${b.roof}`, Ridge: `${(b.eaves + (b.w / b.bays / 2) * Math.tan(b.pitch * Math.PI / 180)).toFixed(1)} m`, Pitch: `${b.pitch}° · ${b.bays} ${b.bays > 1 ? 'gables' : 'gable'}`, Gables: b.windows === false ? 'brick' : 'stucco' },
    note: `Separate file because the roof is a surface players stand on and the code puts a collider under it. The contract asks for ONE flat area of at least 4 × 4 m at ${b.roof} m — not a flat roof — and allows pitched slopes elsewhere up to 30°. So the flat is 6 × 6 m placed at the ladder head (local ${b.deckAt[0]}, ${b.deckAt[1]}), which on all four buildings is a corner, and it reads as the lower roof of an attached bay; the rest is ${b.bays > 1 ? `${b.bays} parallel gables` : 'a single gable'} at ${b.pitch}° ridging at ${(b.eaves + (b.w / b.bays / 2) * Math.tan(b.pitch * Math.PI / 180)).toFixed(1)} m over ${b.eaves} m eaves. The plan is split into bays across X so no one span has to carry an absurd ridge. Built as a heightfield whose x samples include every bay boundary, every ridge line and both deck edges, so each crease is exact and the whole roof is about a hundred cells.`,
    build(K) { return buildRoof(K, rid, { w: b.w, d: b.d, eaves: b.eaves, top: b.roof, skin: b.roofSkin, wings: b.wings, bays: b.bays, pitch: b.pitch, deckAt: b.deckAt, chimneys: b.chimneys, chimneyMat: b.skin, cupolas: b.cupolas, gableMat: b.windows === false ? b.skin : b.trim }); },
  });
}

/* ── H5: interior dressing ───────────────────────────────────────────────
   Placed by the level file rather than baked into the shells, so rooms can be
   arranged around the teleport pads and the sockets. */

P({
  id: 'toaster_dress_workbench', label: 'Dressing — workbench', size: '3×1×0.9 m', swatch: '#8a7f6d',
  budgetTris: 1500, budgetParts: 8,
  stats: { Size: '3×1×0.9', Where: 'barn, garage', Origin: 'footprint centre on the floor' },
  note: 'Timber workbench with a tool wall, a vice and clutter — the barn and the Vehickle house garage. Origin at footprint centre on the floor, so it drops onto the shell\u2019s interior slab at y 0.15.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_dress_workbench');
    g.add(MB(K, 'toaster_dress_workbench_top', mats.toa_deck, [[3, .07, 1, 0, .87, 0], [3, .04, .14, 0, .78, -.43]]));
    g.add(MB(K, 'toaster_dress_workbench_frame', mats.toa_timber_bare, [
      [.09, .84, .09, -1.4, .42, -.42], [.09, .84, .09, 1.4, .42, -.42],
      [.09, .84, .09, -1.4, .42, .42], [.09, .84, .09, 1.4, .42, .42],
      [2.9, .06, .9, 0, .28, 0], [2.9, .08, .08, 0, .74, .42],
    ]));
    g.add(MB(K, 'toaster_dress_workbench_clutter', mats.toa_rust, [
      [.22, .26, .16, -1.0, 1.03, -.1], [.4, .1, .28, .6, .95, .05], [.14, .14, .5, 1.15, .97, -.2],
      [.3, .18, .3, -.2, .37, .1],
    ]));
    g.add(MCyl(K, 'toaster_dress_workbench_vice', mats.toa_paint_faded, [
      [.09, .11, .26, 1.25, 1.03, -.3, 0, 0, 8], [.05, .05, .34, 1.25, 1.14, -.3, Math.PI / 2, 0, 6],
    ]));
    return g;
  },
});

P({
  id: 'toaster_dress_shelving', label: 'Dressing — shelving run', size: '4 m · 2.2 high', swatch: '#8e8474',
  budgetTris: 1500, budgetParts: 8,
  stats: { Run: '4 m', Height: '2.2 m', Shelves: '4' },
  note: 'A 4 m run of open shelving, 2.2 m high, stocked with boxes, tins and paint. Stands against a wall; origin at footprint centre on the floor.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_dress_shelving'), sh = [], up = [];
    for (let i = 0; i < 4; i++) sh.push([4, .04, .5, 0, .42 + i * .58, 0]);
    for (const x of [-1.95, -.65, .65, 1.95]) up.push([.08, 2.2, .5, x, 1.1, 0]);
    g.add(MB(K, 'toaster_dress_shelving_frame', mats.toa_timber_bare, up));
    g.add(MB(K, 'toaster_dress_shelving_shelves', mats.toa_deck, sh));
    const goods = [];
    for (let i = 0; i < 16; i++) {
      const s = i % 4, x = -1.8 + (i * 7 % 9) * .42, wdt = .22 + hash(i, 51) * .24;
      goods.push([wdt, .2 + hash(i, 52) * .18, .34, x, .52 + s * .58, hash(i, 53) * .1,
        0, [.7 + hash(i, 54) * .5, .7 + hash(i, 55) * .4, .6 + hash(i, 56) * .5]]);
    }
    g.add(MB(K, 'toaster_dress_shelving_goods', mats.toa_timber_bare, goods));
    return g;
  },
});

P({
  id: 'toaster_dress_furniture_living', label: 'Dressing — living set', size: '4×3 m cluster', swatch: '#6a5f52',
  budgetTris: 1500, budgetParts: 8,
  stats: { Cluster: '4×3 m', Pieces: 'sofa · table · lamp' },
  note: 'Sofa, low table and a standard lamp as one 4 × 3 m cluster — the houses are walked into, and an empty room with a teleport pad in it reads as a graybox that got a texture.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_dress_furniture_living');
    g.add(MB(K, 'toaster_dress_furniture_living_sofa', mats.toa_tarp, [
      [2.1, .38, .9, -.7, .33, -.9], [2.1, .5, .24, -.7, .72, -1.23],
      [.24, .52, .9, -1.73, .5, -.9], [.24, .52, .9, .33, .5, -.9],
    ]));
    g.add(MB(K, 'toaster_dress_furniture_living_table', mats.toa_deck, [
      [1.1, .06, .6, -.7, .44, .25], [.07, .42, .07, -1.15, .21, .02], [.07, .42, .07, -.25, .21, .02],
      [.07, .42, .07, -1.15, .21, .48], [.07, .42, .07, -.25, .21, .48],
    ]));
    g.add(MCyl(K, 'toaster_dress_furniture_living_lamp', mats.toa_timber_bare, [
      [.03, .03, 1.5, 1.2, .75, -.6, 0, 0, 6], [.24, .24, .04, 1.2, .02, -.6, 0, 0, 10],
    ]));
    g.add(MCyl(K, 'toaster_dress_furniture_living_shade', mats.toa_glow, [
      [.16, .22, .3, 1.2, 1.62, -.6, 0, 0, 10],
    ]));
    return g;
  },
});

P({
  id: 'toaster_dress_furniture_kitchen', label: 'Dressing — kitchen run', size: '4 m counter', swatch: '#9a9184',
  budgetTris: 1500, budgetParts: 8,
  stats: { Run: '4 m', Counter: '0.92 m', Cabinets: 'over and under' },
  note: 'A 4 m counter run with cabinets over and under, a sink and a splashback. Stands against a wall; origin at footprint centre on the floor.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_dress_furniture_kitchen');
    g.add(MB(K, 'toaster_dress_furniture_kitchen_base', mats.toa_clapboard, [
      [4, .84, .62, 0, .42, 0], [4, .06, .04, 0, .18, -.33],
    ]));
    g.add(MB(K, 'toaster_dress_furniture_kitchen_counter', mats.toa_deck, [[4.06, .05, .66, 0, .89, 0]]));
    g.add(MB(K, 'toaster_dress_furniture_kitchen_splash', mats.toa_interior, [[4, .5, .04, 0, 1.16, .3]]));
    const doors = [];
    for (let i = 0; i < 6; i++) doors.push([.6, .72, .03, -1.7 + i * .68, .46, -.32]);
    g.add(MB(K, 'toaster_dress_furniture_kitchen_doors', mats.toa_clapboard_warm, doors));
    g.add(MB(K, 'toaster_dress_furniture_kitchen_wall_units', mats.toa_clapboard, [
      [1.7, .7, .34, -1.1, 1.72, .15], [1.4, .7, .34, 1.2, 1.72, .15],
    ]));
    g.add(MB(K, 'toaster_dress_furniture_kitchen_sink', mats.toa_paint_faded, [
      [.7, .04, .46, .6, .9, 0], [.66, .16, .42, .6, .82, 0],
    ]));
    return g;
  },
});

P({
  id: 'toaster_terrain_scatter', label: 'Ground scatter (4 m)', size: '4 m', swatch: '#7c7448',
  instanced: true, budgetTris: 400, budgetParts: 4,
  stats: { Clump: '4 m', Placements: '~140', Parts: '4' },
  note: 'A 4 m cluster of grass tufts, a rock, a rotted fence post and a thistle — the detail the terrain tile is forbidden from carrying, delivered where it can be placed 140 times instead of 128 × 400. Same contract as `switchyard_terrain_scatter`; instanced now, so the placement cap rose from 22 to 140.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_terrain_scatter');
    const tufts = [];
    for (let i = 0; i < 22; i++) {
      const a = i * 2.399, r = Math.pow(hash(i, 31), .5) * 1.8, h = .28 + hash(i, 32) * .34;
      tufts.push([.42 + hash(i, 33) * .3, h, Math.cos(a) * r, h / 2, Math.sin(a) * r, -a, 0,
        [.9 + hash(i, 34) * .3, .86 + hash(i, 35) * .3, .7 + hash(i, 36) * .3]]);
    }
    g.add(MC(K, 'toaster_terrain_scatter_tufts', mats.toa_leaf_oak, tufts));
    g.add(MB(K, 'toaster_terrain_scatter_rock', mats.toa_riprap, [
      [.62, .34, .5, -1.1, .14, .8, .6], [.34, .22, .3, -.78, .1, 1.05, 1.2],
    ]));
    g.add(MCyl(K, 'toaster_terrain_scatter_post', mats.toa_timber_bare, [
      [.07, .09, .84, 1.3, .38, -.9, .13, .06, 5],
    ]));
    const th = [];
    for (let i = 0; i < 7; i++) th.push([.3, .5 + hash(i, 41) * .3, .6 + hash(i, 42) * .5, .34, -1.5 + hash(i, 43) * .6, i * 1.1, 0]);
    g.add(MC(K, 'toaster_terrain_scatter_thistle', mats.toa_leaf_pine, th));
    return g;
  },
});

/* ═══ P3 — gates, pond, sky, dressing ═════════════════════════════════════ */

/**
 * The warp gate. A new silhouette, and specifically NOT a flat pad.
 *
 * The sim moves an enemy from one gate to the other in a single tick. The
 * player has, on this same map, been taught that a flat 3 m hex pad means
 * *stand here and hold E* — so a flat enemy gate would say that and mean the
 * opposite, which MAP-AUTHORING §4.12 forbids outright: decoration must never
 * read as a control. Hence vertical: a 4.5 m arch on a 0.3 m plinth with a 3 m
 * clear opening, facing ±Z, which the code yaws along the route.
 */
function warpGate(K, id, active) {
  const { THREE, grp, mats } = K, g = grp(id);
  /* 4.5 m overall with a 3 m clear opening, which fixes every other number:
     the head is a semicircle of 1.67 m centreline radius (1.50 inside the
     340 mm section, so the opening is 3.00 across), springing at 2.66, and
     the crown lands at 2.66 + 1.67 + 0.17 = 4.50. Drawn as a plain semicircle
     instead, the whole gate was 2.4 m tall — an arch you step over rather
     than one a wave walks through. */
  const TH = .34, RC = 1.67, SPRING = 2.66, FOOT = .16;
  // Plinth, splayed, with a step so the arch is founded rather than stuck on.
  g.add(revolve(THREE, `${id}_plinth`, [
    [0, 0], [2.9, 0], [2.9, .18], [2.6, .22], [2.6, .30], [0, .30],
  ], mats.toa_warp_dark, { segs: 26, creaseAngle: 30, scale: .9, rot: [-Math.PI / 2, 0, 0] }));

  // The arch: a square-section ring swept in the XY plane, so its opening
  // faces ±Z. Legs first, then the head — rooted INSIDE the plinth at both
  // feet, because a sweep that stops at the surface shows its end caps as two
  // open tubes.
  const path = [];
  path.push([-RC, FOOT, 0], [-RC, SPRING * .5, 0], [-RC, SPRING, 0]);
  for (let i = 1; i < 24; i++) {
    const a = Math.PI * (1 - i / 24);
    path.push([Math.cos(a) * RC, SPRING + Math.sin(a) * RC, 0]);
  }
  path.push([RC, SPRING, 0], [RC, SPRING * .5, 0], [RC, FOOT, 0]);
  const ring = sweep(THREE, `${id}_arch`, path, (t, ang) => {
    // Superellipse: the arch is a squared section, not a bent rod.
    const c = Math.abs(Math.cos(ang)) ** 4 + Math.abs(Math.sin(ang)) ** 4;
    return (TH / 2) / Math.pow(c, .25);
  }, mats.toa_warp_frame, { radial: 12, samples: 60, up: [0, 0, 1] });
  g.add(ring);

  // Feet, keying the arch into the plinth.
  g.add(MB(K, `${id}_feet`, mats.toa_warp_frame, [
    [.62, .5, .62, -RC, .3, 0], [.62, .5, .62, RC, .3, 0],
  ]));
  // Keystone and two haunch blocks — the gate needs a top, or it reads as a
  // croquet hoop.
  g.add(MB(K, `${id}_keystone`, mats.toa_warp_dark, [
    [.5, .62, .46, 0, 4.19, 0],
    [.42, .42, .4, -1.28, SPRING + RC * .72, 0, -.9], [.42, .42, .4, 1.28, SPRING + RC * .72, 0, .9],
  ]));
  // Anchor stays: the thing is 4.5 m tall and 340 mm thick, so it is guyed.
  g.add(MCyl(K, `${id}_stays`, mats.toa_warp_frame, [
    [.05, .07, 2.6, -2.0, 1.1, -.95, .5, .38, 6], [.05, .07, 2.6, 2.0, 1.1, -.95, .5, -.38, 6],
    [.05, .07, 2.6, -2.0, 1.1, .95, -.5, .38, 6], [.05, .07, 2.6, 2.0, 1.1, .95, -.5, -.38, 6],
  ]));

  if (active) {
    /* The membrane the code pulses. One part, named exactly `warp_membrane`.

       Built to the TRUE aperture, which is a stadium — two straight legs to
       the springing, then a semicircular head — not a half-disc. The first
       pass fanned from a point well below the head's centre, so the mesh came
       out as a lopsided teardrop hanging inside the arch with daylight down
       both sides: a pink shape in a gate rather than the thing filling it.

       Each vertex is found by marching its own ray from the aperture centroid
       to the boundary, so the membrane meets the soffit exactly whatever the
       arch numbers become. It is dished 120 mm toward −Z rather than flat,
       because a plane at this scale flips between fully lit and fully dark as
       the camera passes it; a lens always has a gradient across it. */
    const RI = 1.50, CY = (FOOT + SPRING + RI) / 2;
    const inside = (x, y) => Math.abs(x) <= RI && y >= FOOT && (y <= SPRING || (x * x + (y - SPRING) * (y - SPRING)) <= RI * RI);
    const edge = (a) => {
      const dx = Math.cos(a), dy = Math.sin(a);
      let lo = 0, hi = 5;
      for (let k = 0; k < 30; k++) { const m = (lo + hi) / 2; if (inside(dx * m, CY + dy * m)) lo = m; else hi = m; }
      return (lo + hi) / 2;
    };
    const NS = 40, RINGS = 5, DEPTH = .12;
    const pos = [], uv = [], col = [], idx = [];
    pos.push(0, CY, -DEPTH); uv.push(.5, .5); col.push(1, 1, 1);
    for (let j = 1; j <= RINGS; j++) {
      const t = j / RINGS;
      for (let i = 0; i < NS; i++) {
        const a = i / NS * Math.PI * 2, r = edge(a) * t;
        pos.push(Math.cos(a) * r, CY + Math.sin(a) * r, -DEPTH * (1 - t * t));
        // Radial UVs, so the atlas's falloff lands on the aperture edge.
        uv.push(.5 + Math.cos(a) * .5 * t, .5 + Math.sin(a) * .5 * t);
        const v = 1 - t * .45;
        col.push(v, v * .92, v * .95);
      }
    }
    for (let i = 0; i < NS; i++) idx.push(0, 1 + i, 1 + (i + 1) % NS);
    for (let j = 0; j < RINGS - 1; j++) {
      const a0 = 1 + j * NS, a1 = 1 + (j + 1) * NS;
      for (let i = 0; i < NS; i++) {
        const n = (i + 1) % NS;
        idx.push(a0 + i, a1 + i, a1 + n, a0 + i, a1 + n, a0 + n);
      }
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
    geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
    geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
    geo.setIndex(idx); geo.computeVertexNormals();
    const mem = new THREE.Mesh(geo, mats.toa_warp_membrane); mem.name = 'warp_membrane';
    g.add(mem);

    /* Soffit bead. The commission says the ring is DARK on _idle, so the two
       states cannot differ only by a membrane appearing behind an unchanged
       frame — the gate has to look energised, not decorated. This is the one
       frame part that exists only when active: an emissive band round the
       inner face of the arch, on the same stadium the membrane is cut to, so
       edge and bead are the same line rather than two that nearly agree.

       A flat band rather than a swept tube: `sweep` arc-length-parametrises
       its path, and a closed loop hands it a zero-length final segment, which
       hangs. A ring of quads has no such opinion. */
    const bp = [], bu = [], bc = [], bi = [], BW = .07;
    for (let i = 0; i < NS; i++) {
      const a = i / NS * Math.PI * 2, r = edge(a), ca2 = Math.cos(a), sa2 = Math.sin(a);
      bp.push(ca2 * (r - BW), CY + sa2 * (r - BW), .01, ca2 * r, CY + sa2 * r, .01);
      bu.push(0, i / NS, 1, i / NS);
      bc.push(1, 1, 1, 1, .9, .92);
    }
    for (let i = 0; i < NS; i++) {
      const a = i * 2, b = ((i + 1) % NS) * 2;
      bi.push(a, b, b + 1, a, b + 1, a + 1);
    }
    const bg = new THREE.BufferGeometry();
    bg.setAttribute('position', new THREE.Float32BufferAttribute(bp, 3));
    bg.setAttribute('uv', new THREE.Float32BufferAttribute(bu, 2));
    bg.setAttribute('color', new THREE.Float32BufferAttribute(bc, 3));
    bg.setIndex(bi); bg.computeVertexNormals();
    const rim = new THREE.Mesh(bg, mats.toa_warp_rim); rim.name = `${id}_rim`;
    rim.material.side = THREE.DoubleSide;
    g.add(rim);

    g.userData.pulse = { part: 'warp_membrane', emissive: [0.4, 2.4], hz: 0.6 };
  }
  return g;
}

for (const state of ['idle', 'active']) {
  const id = `shared_warp_gate_${state}`;
  P({
    id, label: `Warp gate — ${state}`, size: '4.5 m · 3 m opening', swatch: state === 'active' ? '#ff3a52' : '#2b2e36', dir: 'maps/shared/',
    budgetTris: 2500, budgetParts: 12,
    stats: { Height: '4.5 m', Opening: '3 m clear', Plinth: '0.3 m', Faces: '±Z', Parts: state === 'active' ? '7' : '5' },
    note: `A NEW SILHOUETTE, and specifically not a flat pad. The sim moves an enemy from one gate to the other in a single tick; the player has been taught on this same map that a flat 3 m hex means stand-here-and-hold-E, so a flat enemy gate would say that and mean the opposite — §4.12 forbids decoration that reads as a control. So it is vertical: a 4.5 m squared-section arch on a 0.3 m splayed plinth, 3 m clear opening facing ±Z (the code yaws it along the route), guyed because a 4.5 m arch 340 mm thick would be. ${state === 'active' ? 'Carries the emissive membrane the code pulses, named exactly \`warp_membrane\`, plus a soffit bead — the one FRAME part that exists only when the gate is live, because the brief says the ring is dark on idle and two states differing only by something appearing behind an unchanged frame read as decoration rather than as a gate under power. The membrane is cut to the TRUE aperture (a stadium: straight legs to the springing, then the semicircular head) by marching each vertex’s own ray out to the boundary, so it meets the soffit exactly whatever the arch numbers become, and it is dished 120 mm rather than flat, because a plane at this scale flips between fully lit and fully dark as the camera passes it. MASK with a real alpha map of filaments and voids — alphaTest against no map tests a constant 1 and masks nothing, which is why the first pass was a hard flat silhouette.' : 'Dark — no membrane, no bead, nothing emissive.'} Six placements: three gates, two ends each. Degradation today is \`shared_spawn_portal\`, which reads as three extra spawn points, which is the reason for the ask.`,
    build(K) { return warpGate(K, id, state === 'active'); },
  });
}

/**
 * The pond. 34 m across, and drawn UPWARD rather than dug — the ground is one
 * flat slab and always will be. Open water all the way across, a low earth
 * shelf under it, and a ring of river rock laid round the margin in three
 * courses — the inner one standing in the water.
 *
 * No collision: the code rings it with a hidden bank collider at r 17, which
 * is what stops a quad doing twenty across the surface of the water.
 */
P({
  id: 'toaster_pond', label: 'Pond', size: '34 m across', swatch: '#243033',
  budgetTris: 12000, budgetParts: 4,
  stats: { Across: '36 × 30 m oval', Water: 'r 15.3 · y 0.22', Rock: '3 courses · r 14.3–17.4', Collision: 'none (code rings it)' },
  note: 'Drawn upward, not dug: the ground is one flat slab and always will be. A plain pond — OPEN WATER across the whole basin, ringed by river rock. No island: a bare disc filling two thirds of the middle read as a crater rather than as water, which is the reason for the rebuild. An elliptical plan (1.0 × 0.84, turned 34°), a low earth shelf from r 13.6 out to r 18.8 that the water laps over, and ~120 water-tumbled stones laid in three courses — a wet inner course standing in the shallows at r 14.9, a large crest course at r 16.0, a smaller outer course bedded into the grass at r 17.1, plus scatter to break the ring. Stones are rounded ellipsoids with their own displacement, randomly tumbled and tinted in vertex colour, merged to ONE mesh per course pair so the whole ring is two draw calls. Water is an opaque plane at y 0.22 with a low-amplitude swell on it so the specular breaks up instead of reading as a plastic disc — roughness ~0.1, no transparency, no refraction. Four parts: `pond_shelf`, `pond_water`, `pond_rocks`, `pond_rocks_wet`. No collision: the code rings it with a hidden bank collider at r 17, which is what stops a quad doing twenty across the surface of the water.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_pond');
    /* Plan is an ellipse, 1.0 × 0.84, turned 34° — measured off the aerial
       rather than assumed round. Applied as a vertex warp after the revolve,
       so the bank profile stays a profile and only its plan changes. */
    const EA = 34 * Math.PI / 180, SQ = .84, eca = Math.cos(EA), esa = Math.sin(EA);
    const oval = (geo) => {
      const p = geo.attributes.position;
      for (let i = 0; i < p.count; i++) {
        const x = p.getX(i), z = p.getZ(i);
        const u = x * eca + z * esa, v = (-x * esa + z * eca) * SQ;
        p.setX(i, u * eca - v * esa); p.setZ(i, u * esa + v * eca);
      }
      geo.computeVertexNormals();
    };
    // Same warp for a single point, so stones are PLACED on the ellipse
    // instead of being squashed by it.
    const ovalPt = (x, z) => {
      const u = x * eca + z * esa, v = (-x * esa + z * eca) * SQ;
      return [u * eca - v * esa, u * esa + v * eca];
    };

    /* Shelf: the ground the water lies in and the rock beds into. Starts below
       water level so the water's edge is never a visible seam, rises to a low
       margin and dies back into grade at r 18.8 — a rim, not a bund. */
    const shelf = revolve(THREE, 'pond_shelf', [
      [13.6, -.12], [14.8, .06], [15.8, .26], [16.8, .34], [17.8, .20], [18.8, .01],
    ], mats.toa_dirt, { segs: 56, creaseAngle: 34, scale: 1.6, rot: [-Math.PI / 2, 0, 0] });
    /* revolve() sweeps the profile about local Z and carries the lie-flat
       rotation on the MESH, so the position attribute's height is Z, not Y.
       Bake it before touching vertices — the noise below and oval() both read
       world axes, and warping the unbaked attribute mixes the rim height into
       the plan (a 2.7 m tilted annulus instead of a 0.5 m rim). */
    shelf.updateMatrix();
    shelf.geometry.applyMatrix4(shelf.matrix);
    shelf.rotation.set(0, 0, 0); shelf.updateMatrix();
    const sp = shelf.geometry.attributes.position;
    for (let i = 0; i < sp.count; i++) {
      const x = sp.getX(i), z = sp.getZ(i), y = sp.getY(i), a = Math.atan2(z, x);
      if (y > .04) sp.setY(i, y + (vnoise(Math.cos(a) * 9 + 3, Math.sin(a) * 9) - .5) * .14);
    }
    shelf.geometry.computeVertexNormals();
    oval(shelf.geometry);
    shelf.receiveShadow = true; g.add(shelf);

    /* Water: open across the whole basin. A low-amplitude swell rather than a
       flat plane — at 30 m the specular on a plane flips wholesale as the
       camera passes, which is what made the old disc read as sheet plastic. */
    const water = new THREE.Mesh(new THREE.CircleGeometry(15.3, 64, 1, Math.PI * 2), mats.toa_water);
    water.geometry.rotateX(-Math.PI / 2);
    const wp = water.geometry.attributes.position;
    for (let i = 0; i < wp.count; i++) {
      const x = wp.getX(i), z = wp.getZ(i);
      wp.setY(i, (vnoise(x * .22 + 4, z * .22) - .5) * .05 + (vnoise(x * .7 + 19, z * .7) - .5) * .015);
    }
    water.geometry.computeVertexNormals();
    water.position.y = .22; water.name = 'pond_water'; solid(THREE, water.geometry);
    oval(water.geometry);
    water.receiveShadow = true; g.add(water);

    /* River rock. Rounded stone, not riprap: each is an ellipsoid with its own
       low-frequency displacement, tumbled to a random attitude and tinted in
       vertex colour, so no two read alike. Three courses plus scatter, merged
       to one mesh for the dry stone and one for the wet. */
    let seed = 20260913;
    const rnd = () => ((seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
    const stones = [];
    for (let v = 0; v < 4; v++) {
      const geo = new THREE.IcosahedronGeometry(1, 1);
      const p = geo.attributes.position, ax = [.94 + rnd() * .5, .58 + rnd() * .26, .82 + rnd() * .4];
      for (let i = 0; i < p.count; i++) {
        const x = p.getX(i), y = p.getY(i), z = p.getZ(i);
        const d = 1 + (vnoise(x * 1.7 + v * 7, z * 1.7 + y) - .5) * .30;
        p.setXYZ(i, x * ax[0] * d, y * ax[1] * d, z * ax[2] * d);
      }
      geo.computeVertexNormals();
      const uv = geo.attributes.uv;
      if (uv) for (let i = 0; i < uv.count; i++) uv.setXY(i, uv.getX(i) * 1.6, uv.getY(i) * 1.6);
      stones.push(geo);
    }
    const dry = [], wet = [];
    const M = new THREE.Matrix4(), Q = new THREE.Quaternion(), E = new THREE.Euler(), S = new THREE.Vector3(), T = new THREE.Vector3();
    const lay = (r, n, sMin, sMax, y, isWet, jitR, jitA) => {
      for (let i = 0; i < n; i++) {
        const a = (i / n) * Math.PI * 2 + (rnd() - .5) * jitA;
        const rr = r + (rnd() - .5) * jitR;
        const [px, pz] = ovalPt(Math.cos(a) * rr, Math.sin(a) * rr);
        const s = sMin + rnd() * (sMax - sMin);
        E.set(rnd() * .7 - .35, rnd() * Math.PI * 2, rnd() * .7 - .35);
        Q.setFromEuler(E); S.set(s, s * (.72 + rnd() * .3), s * (.86 + rnd() * .3));
        T.set(px, y + s * (.1 + rnd() * .2), pz);
        M.compose(T, Q, S);
        const t = .84 + rnd() * .26, warm = (rnd() - .5) * .1;
        (isWet ? wet : dry).push({ geo: stones[(i + (isWet ? 1 : 0)) % stones.length], m: M.clone(), c: [t + warm, t, t - warm * .6] });
      }
    };
    lay(14.85, 34, .34, .60, -.02, true, .5, .10);   // standing in the shallows
    lay(16.00, 30, .46, .86, .10, false, .45, .10);  // the big crest course
    lay(17.10, 32, .26, .50, .02, false, .5, .12);   // bedded into the grass
    lay(15.45, 14, .22, .44, .00, true, 1.1, .5);    // scatter, wet side
    lay(16.60, 16, .20, .40, .06, false, 1.3, .5);   // scatter, dry side
    const rocks = merge(THREE, dry, 'pond_rocks', mats.toa_riverrock);
    rocks.castShadow = rocks.receiveShadow = true; g.add(rocks);
    const rocksWet = merge(THREE, wet, 'pond_rocks_wet', mats.toa_riverrock_wet);
    rocksWet.castShadow = rocksWet.receiveShadow = true; g.add(rocksWet);
    /* Declared round, because the clearance sweep measures bounding boxes and
       a 36 m circle in a 36 m square reaches 25.5 m at the corners — it read
       as sitting in a lane it is actually 3.9 m clear of. A piece that knows
       its own plan shape is measured correctly; one that does not is measured
       as the worst square that contains it. */
    g.userData.plan = 'round';
    return g;
  },
});

P({
  id: 'toaster_dock', label: 'Dock', size: '2×6 m', swatch: '#8a7f6d',
  budgetTris: 300, budgetParts: 2,
  stats: { Size: '2×6 m', Deck: 'y 0.55', Origin: 'landward end' },
  note: 'A plank jetty out over the pond. Origin at the LANDWARD end, so it is placed by pushing it off the bank rather than by solving for its centre. Optional.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_dock'), planks = [];
    for (let i = 0; i < 14; i++) planks.push([1.9, .05, .3, 0, .55, -.2 - i * .4]);
    g.add(MB(K, 'toaster_dock_deck', mats.toa_deck, planks));
    const piles = [];
    for (const z of [-.6, -3.0, -5.4]) for (const x of [-.8, .8]) piles.push([.14, .12, .8, x, .34, z, .04, .03, 5]);
    g.add(MCyl(K, 'toaster_dock_piles', mats.toa_timber_bare, piles));
    return g;
  },
});

/**
 * Skybox. Same 700 m dome as the other two maps, baked at 2048 × 1024 rather
 * than 4096 × 2048 — the existing skies are 9 MB each and a clear sky is
 * gradient and soft cloud, neither of which needs the resolution.
 *
 * Clear summer afternoon: a high sun in the south-west at 58° and scattered
 * fair-weather cumulus — flat-bottomed, bright-crowned, flattening toward the
 * horizon as perspective makes them. The part that matters on a map this wide
 * is still the TREELINE SILHOUETTE baked into the horizon band, so the belt
 * meets the sky instead of stopping at a slab edge.
 */
P({
  id: 'toaster_skybox', label: 'Skybox — clear summer', size: '700 m dome', swatch: '#63a3d8',
  budgetTris: 4000, budgetParts: 1,
  stats: { Dome: '700 m', Bake: '2048×1024', Sun: '58° · az 220°', Horizon: 'treeline baked' },
  note: 'Clear summer afternoon: high sun in the south-west at 58° elevation with a small disc, scattered fair-weather cumulus that flatten and thin toward the horizon, and a green treeline silhouette baked into the horizon band so the belt meets the sky rather than stopping. Baked at 2048 × 1024 — the existing two skies are 4096 × 2048 and 9 MB each, and a gradient-and-cumulus sky does not need it.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_skybox');
    const W = 2048, H = 1024, c = cv(W, H), x = c.getContext('2d'), R = rng(1124);
    // Vertical gradient: saturated blue at the zenith, thinning to a pale warm
    // haze at the horizon — the aerial perspective that makes a sky read deep.
    const grd = x.createLinearGradient(0, 0, 0, H);
    grd.addColorStop(0, '#1f61ae'); grd.addColorStop(.34, '#4f96d4');
    grd.addColorStop(.50, '#8ebfe2'); grd.addColorStop(.60, '#c6dbe6'); grd.addColorStop(1, '#7d8f6e');
    x.fillStyle = grd; x.fillRect(0, 0, W, H);
    const sunU = (220 / 360) * W, sunV = H * (.5 - 58 / 180);
    /* Fair-weather cumulus. Each cloud is a cluster of soft blots sitting ON a
       flat base line — that flat bottom is the whole tell — with the crowns
       bright and the underside shaded. Clusters near the horizon are wider and
       much flatter, which is perspective on a deck at one altitude, not taste. */
    for (let i = 0; i < 46; i++) {
      const cu = R() * W, t = Math.pow(R(), .75);
      const base = H * .12 + t * H * .34;               // higher on the sheet = overhead
      const persp = 1 - t;                              // 1 overhead, 0 at the horizon
      const w = (110 + R() * 200) * (.5 + persp * .8), hh = w * (.10 + persp * .24);
      const lit = 1 - Math.min(1, Math.abs(cu - sunU) / (W * .45)) * .35;
      for (let k = 0; k < 14; k++) {
        const bu = cu + (R() - .5) * w, lift = Math.pow(R(), 1.5) * hh;
        const r = hh * (.45 + R() * .7) * (1 - lift / (hh * 2.4));
        const crown = Math.min(1, lift / (hh * .55));
        const v0 = (208 + crown * 46) * lit;
        const gg = x.createRadialGradient(bu, base - lift, 0, bu, base - lift, Math.max(6, r));
        gg.addColorStop(0, `rgba(${v0 | 0},${(v0 * .99) | 0},${Math.min(255, v0 * 1.02) | 0},${.62 + crown * .32})`);
        gg.addColorStop(.6, `rgba(${v0 | 0},${v0 | 0},${Math.min(255, v0 * 1.03) | 0},${.3 + crown * .22})`);
        gg.addColorStop(1, 'rgba(255,255,255,0)');
        x.fillStyle = gg;
        x.save(); x.beginPath(); x.rect(0, 0, W, base + 2); x.clip();   // nothing below the base
        x.beginPath(); x.ellipse(bu, base - lift, r * 1.25, r, 0, 0, 6.283); x.fill();
        x.restore();
      }
    }
    // The sun: a small disc in a tight halo. On a clear day it is a disc.
    const sg = x.createRadialGradient(sunU, sunV, 0, sunU, sunV, W * .13);
    sg.addColorStop(0, 'rgba(255,250,232,.9)'); sg.addColorStop(.14, 'rgba(255,242,206,.44)');
    sg.addColorStop(.5, 'rgba(255,238,200,.13)'); sg.addColorStop(1, 'rgba(0,0,0,0)');
    x.fillStyle = sg; x.beginPath(); x.arc(sunU, sunV, W * .13, 0, 6.283); x.fill();
    x.fillStyle = 'rgba(255,253,245,.98)'; x.beginPath(); x.arc(sunU, sunV, 15, 0, 6.283); x.fill();
    // Treeline: two layers of silhouette on the horizon band, the far one
    // hazed back, so the wood has depth where the belt runs out.
    const band = (base, height, alpha, tint) => {
      x.fillStyle = tint; x.globalAlpha = alpha;
      x.beginPath(); x.moveTo(0, H);
      for (let u = 0; u <= W; u += 6) {
        const n = vnoise(u * .012, base) * .6 + vnoise(u * .05, base + 9) * .4;
        const spike = vnoise(u * .17, base + 3) > .72 ? 1.7 : 1;
        x.lineTo(u, H * base - n * height * spike);
      }
      x.lineTo(W, H); x.closePath(); x.fill(); x.globalAlpha = 1;
    };
    band(.565, 42, .5, '#8fa88c');
    band(.585, 62, .95, '#3e5936');
    const t = new THREE.CanvasTexture(c);
    t.colorSpace = THREE.SRGBColorSpace; t.mapping = THREE.EquirectangularReflectionMapping;
    const sky = new THREE.Mesh(new THREE.SphereGeometry(700, 48, 24), new THREE.MeshBasicMaterial({ map: t, side: THREE.BackSide, depthWrite: false, fog: false }));
    /* `sky_` prefix and the gizmo flag are the viewer's contract for "not part
       of the level": without them the 1.4 km dome is what the bounds readout
       measures, and the Toaster reported 1400×1400×1400 instead of its
       320×160 property. renderOrder and frustumCulled match the space sky. */
    sky.name = 'sky_toaster_dome'; sky.renderOrder = -10; sky.frustumCulled = false;
    sky.userData.sky = true; sky.userData.gizmo = true;
    g.add(sky);
    return g;
  },
});

/* ── Z: dressing ─────────────────────────────────────────────────────────
   Nothing in this table may sit within 3 m of a route, a socket, a pad, the
   spawn, the armory or a hero station — the code refuses those placements and
   logs them, so a bad coordinate is a line in the output rather than a girder
   in the roadway. */

P({
  id: 'toaster_fence_wood', label: 'Fence — post and rail', size: '4 m · 1.2 high', swatch: '#8e8474',
  instanced: true, budgetTris: 120, budgetParts: 1, run: '+X', repeat: 4.0, width: 0.2,
  stats: { Run: '+X · 4 m', Height: '1.2 m', Rails: '2' },
  note: 'Post-and-rail run, 4 m, 1.2 m high, along the drive and the field edges. One merged part and 4 m of repeat, so the post lands on the module edge and two runs meet at a shared post rather than doubling it.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_fence_wood');
    g.add(MB(K, 'toaster_fence_wood_run', mats.toa_fence, [
      [.12, 1.2, .12, -2, .6, 0], [.12, 1.2, .12, 2, .6, 0],
      [4, .09, .05, 0, 1.02, 0], [4, .09, .05, 0, .58, 0],
    ]));
    return g;
  },
});

P({
  id: 'toaster_mailbox', label: 'Mailbox', size: '0.5×1.2 m', swatch: '#8c9aa0',
  budgetTris: 200, budgetParts: 2,
  stats: { Height: '1.2 m', Placed: '3', Where: 'road head' },
  note: 'Post box at the head of each drive. Three placements — the three properties on the road.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_mailbox');
    g.add(MB(K, 'toaster_mailbox_post', mats.toa_timber_bare, [[.09, 1.05, .09, 0, .52, 0], [.3, .05, .12, 0, 1.03, .06]]));
    g.add(revolve(THREE, 'toaster_mailbox_box', [
      [0, -.24], [.14, -.24], [.16, -.21], [.16, .21], [.14, .24], [0, .24],
    ], mats.toa_paint_faded, { segs: 12, creaseAngle: 40, scale: 2, pos: [0, 1.16, 0], rot: [0, Math.PI / 2, 0] }));
    return g;
  },
});

P({
  id: 'toaster_woodpile', label: 'Woodpile', size: '3×1.4×1.2 m', swatch: '#7c6448',
  budgetTris: 600, budgetParts: 2,
  stats: { Size: '3×1.4×1.2', Placed: '2' },
  note: 'Split logs stacked under the edge of a tarp. Two placements.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_woodpile'), logs = [];
    // Four courses of five at five sides each, which is what a 600-triangle
    // budget buys. A rounder log costs 25% more for a silhouette nobody reads
    // from the lane.
    for (let r = 0; r < 4; r++) for (let i = 0; i < 5; i++) {
      // Logs run ALONG the pile and stack ACROSS it. The first pass stepped
      // them along their own axis as well, which laid a 2.8 m log out over a
      // 2.2 m run and made a 3 m woodpile 4.96 m long.
      const y = .15 + r * .26, z = -.50 + i * .25 + (r % 2) * .10;
      logs.push([.26, .26, 2.8, 0, y, z, 0, Math.PI / 2, 5,
        [.8 + hash(i, r * 3) * .4, .78 + hash(i, r * 5) * .34, .7 + hash(i, r * 7) * .3]]);
    }
    g.add(MCyl(K, 'toaster_woodpile_logs', mats.toa_bark, logs));
    g.add(MB(K, 'toaster_woodpile_tarp', mats.toa_tarp, [
      [2.9, .04, .9, 0, 1.12, -.18, 0], [2.9, .44, .04, 0, .92, -.62],
    ]));
    return g;
  },
});

P({
  id: 'toaster_hay_bale', label: 'Hay bale', size: '1.5 dia × 1.2', swatch: '#b49a55',
  instanced: true, budgetTris: 150, budgetParts: 1,
  stats: { Size: '1.5 dia × 1.2', Placed: '~25 instanced', Field: 'east' },
  note: 'Round bale lying on its side in the east field, instanced about twenty-five times. One part, twelve sides — at this size a rounder cylinder is triangles spent on a silhouette nobody reads.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_hay_bale');
    g.add(MCyl(K, 'toaster_hay_bale_roll', mats.toa_hay, [[.75, .75, 1.2, 0, .75, 0, 0, Math.PI / 2, 12]]));
    return g;
  },
});

P({
  id: 'toaster_propane_tank', label: 'Propane tank', size: '3.2×1.0×1.4 m', swatch: '#cfcdc4',
  budgetTris: 500, budgetParts: 3,
  stats: { Length: '3.2 m', Placed: '3 (one per house)' },
  note: 'Horizontal propane tank on saddle legs, one behind each house. Revolved, because a tank is a turned form with dished ends — a capsule of two hemispheres and a cylinder is the thing this kit is replacing.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_propane_tank');
    g.add(revolve(THREE, 'toaster_propane_tank_shell', [
      [0, -1.55], [.30, -1.52], [.44, -1.42], [.49, -1.22], [.50, -1.0], [.50, 1.0], [.49, 1.22], [.44, 1.42], [.30, 1.52], [0, 1.55],
    ], mats.toa_tank, { segs: 20, creaseAngle: 40, scale: 1.2, pos: [0, .86, 0], rot: [0, Math.PI / 2, 0] }));
    g.add(MB(K, 'toaster_propane_tank_saddles', mats.toa_paint_faded, [
      [.24, .5, .8, -1.0, .25, 0], [.24, .5, .8, 1.0, .25, 0],
      [.2, .06, .9, -1.0, .03, 0], [.2, .06, .9, 1.0, .03, 0],
    ]));
    g.add(MB(K, 'toaster_propane_tank_dome', mats.toa_paint_faded, [[.42, .2, .38, 0, 1.4, 0]]));
    return g;
  },
});

P({
  id: 'toaster_wreck_pickup', label: 'Wrecked pickup', size: '5.2×2.0×1.8 m', swatch: '#7a4228',
  budgetTris: 4000, budgetParts: 6,
  stats: { Length: '5.2 m', Placed: '1 (behind the barn)', Rig: 'none' },
  note: 'Rusted pickup on flat tyres with the hood up, behind the barn. DRESSING, NOT A VEHICLE — no rig, no named wheel nodes, nothing the vehicle code could bind to. It sits 120 mm lower than it should because the tyres are flat, which is most of what makes a wreck read as a wreck rather than as a car you cannot drive.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_wreck_pickup');
    g.add(MB(K, 'toaster_wreck_pickup_body', mats.toa_rust, [
      [2.3, .62, 1.85, -1.3, .78, 0], [1.9, .5, 1.8, .95, .72, 0],
      [.12, .5, 1.8, 1.92, .72, 0], [2.0, .1, .14, .95, .98, -.86], [2.0, .1, .14, .95, .98, .86],
    ]));
    g.add(MB(K, 'toaster_wreck_pickup_cab', mats.toa_rust, [
      [1.5, .72, 1.7, -.35, 1.42, 0], [.1, .7, 1.6, .38, 1.42, 0],
    ]));
    g.add(MB(K, 'toaster_wreck_pickup_glass', mats.toa_window, [
      [.06, .52, 1.46, .36, 1.46, 0], [1.2, .5, .06, -.35, 1.48, -.84], [1.2, .5, .06, -.35, 1.48, .84],
    ]));
    // Hood, propped open on its stay.
    g.add(MB(K, 'toaster_wreck_pickup_hood', mats.toa_rust, [[1.8, .07, 1.7, -2.05, 1.62, 0, 0]]));
    g.add(MCyl(K, 'toaster_wreck_pickup_engine', mats.toa_paint_faded, [
      [.34, .36, .56, -2.1, .98, 0, 0, 0, 8], [.1, .1, .5, -2.1, 1.3, .3, 0, 0, 6],
    ]));
    // Flat tyres: squashed, and the rim is nearly down on the rubber.
    const wh = [];
    for (const x of [-2.0, 1.3]) for (const z of [-.82, .82]) {
      wh.push([.38, .38, .26, x, .26, z, 0, Math.PI / 2, 10, [.5, .48, .46]]);
    }
    g.add(MCyl(K, 'toaster_wreck_pickup_wheels', mats.toa_warp_dark, wh));
    return g;
  },
});

P({
  id: 'toaster_leaf_pile', label: 'Cut-grass drift', size: '2×0.3 m', swatch: '#6e9a40',
  instanced: true, budgetTris: 60, budgetParts: 1,
  stats: { Size: '2×0.3 m', Cards: 'MASK', Placed: 'instanced, belt edge' },
  note: 'A drift of cut grass and brush at the inner edge of the belt, where the mower stops and the wood begins. One merged part of MASK cards.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_leaf_pile'), cards = [];
    for (let i = 0; i < 9; i++) {
      const a = i * 2.399, r = hash(i, 61) * .8;
      cards.push([1.1 + hash(i, 62) * .7, .34, Math.cos(a) * r, .1 + hash(i, 63) * .12, Math.sin(a) * r, -a, -1.15,
        [1.05, .84, .6]]);
    }
    g.add(MC(K, 'toaster_leaf_pile_cards', mats.toa_leaf_oak, cards));
    return g;
  },
});

P({
  id: 'vfx_teleport_burst', label: 'VFX — teleport burst', size: '1.2 × 3 m', swatch: '#7ad6d0', dir: 'vfx/',
  budgetTris: 400, budgetParts: 2,
  stats: { Height: '3 m', Tint: 'by code', Authored: 'standing on grade' },
  note: 'A short column of light for a player arriving or leaving on a pad, and for an enemy coming through a warp gate. One file, tinted by the code — teal for a player, red for an enemy — so it is authored white and standing on the ground. Stand-in today is the Nova impact ring, which is flat, and a flat ring on a flat pad is the §4.12 problem again.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('vfx_teleport_burst');
    const col = revolve(THREE, 'vfx_teleport_burst_column', [
      [.62, 0], [.52, .6], [.46, 1.4], [.5, 2.2], [.42, 3.0],
    ], mats.toa_warp_membrane, { segs: 18, scale: 1.2, rot: [-Math.PI / 2, 0, 0], inward: false });
    col.material = mats.toa_glow; g.add(col);
    const ring = revolve(THREE, 'vfx_teleport_burst_ring', [
      [.9, .04], [1.25, .10], [1.5, .04],
    ], mats.toa_glow, { segs: 22, scale: 2, rot: [-Math.PI / 2, 0, 0] });
    g.add(ring);
    return g;
  },
});

/* ═══ P4 — the working farm ══════════════════════════════════════════════════════
   What the first drop left out. The property had four buildings, a pond and
   a lawn, and nothing that said anyone farmed it: no line dividing the land
   into fields, nothing stored, nothing that pumps or feeds or dries. These
   are the pieces a rural property has BETWEEN its buildings — each placed a
   handful of times or instanced along a rule, and each built to the same
   budget arithmetic as the rest of the kit. */

/** Merged arbitrary geometries. Entries: [geo, x, y, z, rx?, ry?, rz?, colour?]. */
function MG(K, name, mat, list) {
  const { THREE } = K, items = [], e = new THREE.Euler();
  for (const [geo, x, y, z, rx = 0, ry = 0, rz = 0, c] of list) {
    e.set(rx, ry, rz, 'XYZ');
    const m = new THREE.Matrix4().makeRotationFromEuler(e); m.setPosition(x, y, z);
    items.push({ geo, m, c });
  }
  return merge(THREE, items, name, mat);
}
/** Merged cylinders between two points — braces, wires, tubes. Entries: [a, b, r0, r1?, segs?, colour?]. */
function MSeg(K, name, mat, list) {
  const { THREE } = K, items = [], up = new THREE.Vector3(0, 1, 0);
  for (const [a, b, r0, r1 = r0, segs = 6, c] of list) {
    const A = new THREE.Vector3(...a), B = new THREE.Vector3(...b), d = B.clone().sub(A), len = d.length();
    if (len < 1e-4) continue;
    const m = new THREE.Matrix4().makeRotationFromQuaternion(new THREE.Quaternion().setFromUnitVectors(up, d.normalize()));
    m.setPosition(A.lerp(B, .5));
    items.push({ geo: new THREE.CylinderGeometry(r1, r0, len, segs, 1, false), m, c });
  }
  return merge(THREE, items, name, mat);
}

P({
  id: 'toaster_fence_wire', label: 'Fence — wire on T-posts', size: '4 m · 1.3 high', swatch: '#5c6658',
  instanced: true, budgetTris: 120, budgetParts: 2, run: '+X', repeat: 4.0, width: 0.1,
  stats: { Run: '+X · 4 m', Height: '1.3 m', Strands: '5', Post: 'steel T @ 4 m' },
  note: 'Five-strand wire on a steel T-post, 4 m of it — the fence a working field has, where post-and-rail is the fence a lawn has. Post on the module end so runs share posts. Instanced round the pasture, the orchard, the crop field and the hayfield, about 180 placements, and it is what turns one 320 m lawn into fields.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_fence_wire');
    g.add(MB(K, 'toaster_fence_wire_post', mats.toa_tpost, [[.05, 1.55, .05, -2, .78, 0], [.14, .05, .04, -2, 1.5, 0], [.05, .4, .12, -2, .15, 0]]));
    const strands = [];
    for (const y of [.32, .56, .8, 1.04, 1.28]) strands.push([4, .014, .014, 0, y, 0]);
    g.add(MB(K, 'toaster_fence_wire_strands', mats.toa_galv, strands));
    return g;
  },
});

P({
  id: 'toaster_gate_farm', label: 'Farm gate (tube)', size: '4 m · 1.3 high', swatch: '#9da19c',
  budgetTris: 900, budgetParts: 2, run: '+X', repeat: 4.0, width: 0.2,
  stats: { Span: '4 m', Height: '1.3 m', Rails: '6 tube', Posts: 'timber' },
  note: 'A 4 m galvanised tube gate between two timber posts — the same module length as the wire, so it drops into a fence run in place of one module. Six rails, two uprights and a diagonal brace, as the real thing is welded. Hung shut: a gate that stands open is an opening the sim does not know about.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_gate_farm');
    g.add(MCyl(K, 'toaster_gate_farm_posts', mats.toa_timber_bare, [[.1, .12, 1.7, -2.05, .85, 0, 0, 0, 7], [.1, .12, 1.7, 2.05, .85, 0, 0, 0, 7]]));
    const segs = [];
    for (const y of [.28, .48, .68, .88, 1.08, 1.28]) segs.push([[-1.92, y, 0], [1.92, y, 0], .022, .022, 6]);
    for (const x of [-1.92, -.3, 1.3, 1.92]) segs.push([[x, .28, 0], [x, 1.28, 0], .022, .022, 6]);
    segs.push([[-1.92, .28, 0], [1.3, 1.28, 0], .018, .018, 5]);
    g.add(MSeg(K, 'toaster_gate_farm_tubes', mats.toa_galv, segs));
    return g;
  },
});

P({
  id: 'toaster_utility_pole', label: 'Utility pole', size: '10 m', swatch: '#9a9184',
  budgetTris: 600, budgetParts: 3,
  stats: { Height: '10 m', Crossarm: '2.2 m · 3 insulators', Variants: '2 (v1 carries a transformer)', Placed: '~14' },
  note: 'A 10 m treated pole with a single crossarm and three insulators, laid down the county road and the south road at 36 m intervals, on the verge. The conductors between them are built by the level (`buildToasterWires`) from the poles that were actually placed, with real sag, so a refused pole never leaves a wire hanging in air. v1 adds a pole-mounted transformer at the drops to the houses. Poles and wires are what say a road is a road that goes somewhere.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { grp, mats } = K, g = grp('toaster_utility_pole');
    g.add(MCyl(K, 'toaster_utility_pole_pole', mats.toa_timber_bare, [[.11, .17, 10, 0, 5, 0, 0, 0, 8]]));
    g.add(MB(K, 'toaster_utility_pole_arm', mats.toa_timber_bare, [[2.2, .1, .1, 0, 9.35, 0], [.05, .55, .05, -.65, 9.05, .1], [.05, .55, .05, .65, 9.05, .1]]));
    const fit = [[.05, .06, .22, -.95, 9.51, 0, 0, 0, 8], [.05, .06, .22, .95, 9.51, 0, 0, 0, 8], [.05, .06, .22, 0, 10.1, 0, 0, 0, 8]];
    if (v % 2) fit.push([.27, .27, .8, .48, 8.3, .3, 0, 0, 12, [.55, .58, .6]], [.02, .02, 1.2, .3, 9.0, .15, 0, .4, 4, [.2, .2, .2]]);
    g.add(MCyl(K, 'toaster_utility_pole_fittings', mats.toa_doorpanel, fit));
    return g;
  },
});

function grainBin(K, R, H) {
  const { THREE, grp, mats } = K, g = grp('toaster_grain_bin');
  // Corrugated wall in stiffener rings every 950 mm, then the 30° cone, the
  // eave flashing and the fill cap — one turned profile.
  const pts = [[0, 0], [R, 0]];
  for (let y = .95; y < H - .3; y += .95) pts.push([R, y - .05], [R + .06, y], [R, y + .05]);
  const apex = H + (R - .4) * .55;
  pts.push([R, H], [R + .18, H + .02], [R + .18, H + .1], [.4, apex], [.4, apex + .25], [.28, apex + .32], [0, apex + .32]);
  const shell = revolve(THREE, 'toaster_grain_bin_shell', pts, mats.toa_galv, { segs: 30, creaseAngle: 35, rot: [-Math.PI / 2, 0, 0] });
  shell.castShadow = true; g.add(shell);
  const lad = [[.05, H + .6, .05, R + .12, (H + .6) / 2, -.22], [.05, H + .6, .05, R + .12, (H + .6) / 2, .22]];
  for (let y = .3; y < H + .5; y += .3) lad.push([.03, .03, .44, R + .12, y, 0]);
  // Unloading auger tube out of the base, and the short ladder onto the cone.
  lad.push([.16, .16, R + 1.6, R * .4, .5, 0, 0], [.6, .5, .6, .0, .45, 0]);
  g.add(MB(K, 'toaster_grain_bin_fittings', mats.toa_galv, lad));
  g.add(revolve(THREE, 'toaster_grain_bin_pad', [[0, 0], [R + .5, 0], [R + .5, .12], [0, .12]], mats.toa_concrete, { segs: 30, rot: [-Math.PI / 2, 0, 0] }));
  return g;
}
P({
  id: 'toaster_grain_bin', label: 'Grain bin', size: '5.5 dia · 6.5 m', swatch: '#9da19c',
  budgetTris: 3000, budgetParts: 3,
  stats: { Diameter: '5.5 m (v1: 7.3)', Eave: '5.2 m (v1: 7.0)', Roof: '30° cone', Variants: '2' },
  note: 'Corrugated galvanised grain bin on a concrete pad: stiffener rings every 950 mm, a 30° cone roof with eave flashing and a fill cap, a side ladder and the unloading auger out of the base. Turned as one profile, so the rings are in the silhouette rather than painted. Two sizes (5.5 and 7.3 m) beside the shed, where a farm keeps them. The single most legible thing on a farm horizon after the barn itself.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) { return v % 2 ? grainBin(K, 3.65, 7.0) : grainBin(K, 2.75, 5.2); },
});

P({
  id: 'toaster_windmill', label: 'Windmill (water pump)', size: '10 m · 2.5 m wheel', swatch: '#9da19c',
  budgetTris: 5000, budgetParts: 6,
  stats: { Tower: '10 m · 4-leg lattice', Wheel: '2.5 m · 18 blades', Spin: 'userData.spin', Placed: '1, pasture' },
  note: 'Aermotor-pattern pumping mill: a four-leg galvanised lattice tower 10 m tall with girts and X-bracing at 2 m panels, a 2.5 m 18-blade wheel on a rim, and the tail vane that keeps it in the wind. Stands over the stock tank in the pasture. The wheel is ONE part named `toaster_windmill_wheel` and the group declares `userData.spin = { part, axis, rpm }` so a driver can turn it without knowing its geometry — the moving-parts contract from the weapons applied to a prop.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_windmill');
    const HT = 10, B = 1.5, TW = .42, C = [[-1, -1], [1, -1], [1, 1], [-1, 1]];
    const at = (sx, sz, y) => { const r = B + (TW - B) * (y / HT); return [sx * r, y, sz * r]; };
    const segs = [];
    for (const [sx, sz] of C) segs.push([at(sx, sz, -.2), at(sx, sz, HT), .045, .03, 6]);
    for (let y = 2; y <= 8; y += 2) for (let i = 0; i < 4; i++) segs.push([at(...C[i], y), at(...C[(i + 1) % 4], y), .02, .02, 5]);
    for (let y = 0; y < HT; y += 2) for (let i = 0; i < 4; i++) segs.push([at(...C[i], y), at(...C[(i + 1) % 4], y + 2), .013, .013, 4], [at(...C[(i + 1) % 4], y), at(...C[i], y + 2), .013, .013, 4]);
    segs.push([[0, .4, 0], [0, HT + .1, 0], .03, .03, 6]);   // pump rod
    const tower = MSeg(K, 'toaster_windmill_tower', mats.toa_galv, segs); tower.castShadow = true; g.add(tower);
    g.add(MB(K, 'toaster_windmill_head', mats.toa_galv, [[1.3, .06, 1.3, 0, HT - .55, 0], [.5, .45, .7, 0, HT + .3, -.05], [.44, .12, .44, 0, HT, 0]]));
    g.add(MB(K, 'toaster_windmill_footings', mats.toa_concrete, C.map(([sx, sz]) => [.55, .3, .55, sx * B, .15, sz * B])));
    // The wheel: 18 blades on a rim, pitched 28° to the plane of rotation.
    const items = [], N = 18, R0 = 1.25;
    for (let i = 0; i < N; i++) {
      const m = new THREE.Matrix4().makeRotationZ(i / N * Math.PI * 2).multiply(new THREE.Matrix4().makeTranslation(0, .84, 0)).multiply(new THREE.Matrix4().makeRotationY(.49));
      items.push({ geo: new THREE.PlaneGeometry(.36, .74), m });
    }
    items.push({ geo: new THREE.TorusGeometry(R0 - .04, .022, 4, 40), m: new THREE.Matrix4() }, { geo: new THREE.TorusGeometry(.48, .02, 4, 20), m: new THREE.Matrix4() },
      { geo: new THREE.CylinderGeometry(.09, .09, .34, 10).rotateX(Math.PI / 2), m: new THREE.Matrix4() });
    for (let i = 0; i < 6; i++) items.push({ geo: new THREE.BoxGeometry(.028, R0 * 2 - .1, .028), m: new THREE.Matrix4().makeRotationZ(i * Math.PI / 6) });
    const wheel = merge(THREE, items, 'toaster_windmill_wheel', mats.toa_galv_ds);
    wheel.position.set(0, HT + .38, .78); wheel.castShadow = true; g.add(wheel);
    g.add(MG(K, 'toaster_windmill_tail', mats.toa_galv_ds, [
      [new THREE.CylinderGeometry(.025, .025, 1.7, 6), 0, HT + .34, -1.2, Math.PI / 2, 0, 0],
      [new THREE.PlaneGeometry(1.0, .82), 0, HT + .5, -1.85, 0, Math.PI / 2, 0],
    ]));
    g.userData.spin = { part: 'toaster_windmill_wheel', axis: [0, 0, 1], rpm: 14 };
    return g;
  },
});

P({
  id: 'toaster_stock_tank', label: 'Stock tank', size: '2.4 dia × 0.66 m', swatch: '#9da19c',
  budgetTris: 900, budgetParts: 3,
  stats: { Diameter: '2.4 m', Height: '0.66 m', Water: 'y 0.5', Placed: '1, under the mill' },
  note: 'Round galvanised stock tank with a rolled rim, standing water in it, and the fill pipe from the mill. Turned, like the bins, from one profile with the rim in it.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_stock_tank');
    g.add(revolve(THREE, 'toaster_stock_tank_wall', [[0, 0], [1.22, 0], [1.22, .04], [1.2, .04], [1.2, .6], [1.25, .63], [1.25, .67], [1.15, .67], [1.15, .05], [0, .05]], mats.toa_galv, { segs: 28, creaseAngle: 35, rot: [-Math.PI / 2, 0, 0] }));
    g.add(MCyl(K, 'toaster_stock_tank_water', mats.toa_water, [[1.14, 1.14, .02, 0, .5, 0, 0, 0, 28]]));
    g.add(MSeg(K, 'toaster_stock_tank_pipe', mats.toa_galv, [[[1.75, -.1, 0], [1.75, .95, 0], .03, .03, 6], [[1.75, .95, 0], [.9, .95, 0], .03, .03, 6]]));
    return g;
  },
});

P({
  id: 'toaster_shed_small', label: 'Small shed / coop', size: '3.6×2.4×2.4 m', swatch: '#9a9184',
  budgetTris: 600, budgetParts: 2,
  stats: { Footprint: '3.6×2.4', Roof: 'lean-to, corrugated', Door: '+Z, ajar', Placed: '2' },
  note: 'Board shed under a corrugated lean-to — a hen house behind the south-east house, a garden shed behind the main house. Grey unpainted board, door hung ajar on the +Z side, a nest box on the flank. The kind of outbuilding every property has two of and no plan ever draws.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_shed_small');
    const W = 3.6, D = 2.4, HF = 2.4, HB = 1.95, T = .08;
    const walls = [
      [2.4, HF, T, -.6, HF / 2, D / 2 - T / 2], [.8, HF - 1.9, T, 1.0, 1.9 + (HF - 1.9) / 2, D / 2 - T / 2], [.4, HF, T, 1.6, HF / 2, D / 2 - T / 2],
      [W, HB, T, 0, HB / 2, -D / 2 + T / 2],
      [T, HB, D, -W / 2 + T / 2, HB / 2, 0], [T, HB, D, W / 2 - T / 2, HB / 2, 0],
      [W + .1, .12, D + .1, 0, .06, 0],
      [.76, 1.84, .05, .55, .98, D / 2 + .32, .75],                            // door leaf, ajar
      [.9, .5, .45, W / 2 + .2, 1.05, -.3], [.98, .04, .55, W / 2 + .22, 1.34, -.3],  // nest box and its lid
    ];
    for (let i = 0; i < 4; i++) { const z0 = -D / 2 + i * D / 4, h = HB + (HF - HB) * (i + 1) / 4; walls.push([T, h - HB, D / 4, -W / 2 + T / 2, HB + (h - HB) / 2, z0 + D / 8], [T, h - HB, D / 4, W / 2 - T / 2, HB + (h - HB) / 2, z0 + D / 8]); }
    const m = MB(K, 'toaster_shed_small_walls', mats.toa_timber_bare, walls); m.castShadow = true; g.add(m);
    const roof = MG(K, 'toaster_shed_small_roof', mats.toa_corrugate, [[new THREE.BoxGeometry(W + .5, .05, Math.hypot(D + .5, HF - HB)), 0, (HF + HB) / 2 + .04, .02, -Math.atan2(HF - HB, D), 0, 0]]);
    roof.castShadow = true; g.add(roof);
    return g;
  },
});

P({
  id: 'toaster_hedgerow', label: 'Hedgerow (4 m)', size: '4 m · 2.4 high', swatch: '#4f7a34',
  instanced: true, budgetTris: 400, budgetParts: 3, run: '+X', repeat: 4.0, width: 1.8,
  stats: { Run: '+X · 4 m', Height: '1.6–2.6 m', Cards: 'MASK', Placed: '~18' },
  note: 'Four metres of overgrown field hedge — hawthorn and bramble that grew up along a fence line and was never cut back — with a few bare stems showing through. MASK cards off the shared leaf atlas in two greens, so the run has depth. Along the hayfield frontage with open-grown oaks standing in it, which is how a hedgerow tree got there.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_hedgerow'), a = [], b = [];
    for (let i = 0; i < 44; i++) {
      const x = -2 + (i / 44) * 4 + (hash(i, 71) - .5) * .4, z = (hash(i, 72) - .5) * 1.6, h = 1.6 + hash(i, 73) * 1.0;
      (i % 3 ? a : b).push([1.1 + hash(i, 74) * .7, h, x, h / 2 - .1, z, hash(i, 75) * 3.1, -.08 + hash(i, 76) * .16,
        [.72 + hash(i, 77) * .4, .8 + hash(i, 78) * .3, .6 + hash(i, 79) * .3]]);
    }
    g.add(MC(K, 'toaster_hedgerow_cards', mats.toa_leaf_edge, a));
    g.add(MC(K, 'toaster_hedgerow_cards_dark', mats.toa_leaf_oak, b));
    const st = [];
    for (let i = 0; i < 7; i++) st.push([.015, .04, 1.8 + hash(i, 81) * .9, -1.7 + i * .55, 1.0, (hash(i, 82) - .5) * .8, (hash(i, 83) - .5) * .5, (hash(i, 84) - .5) * .5, 4]);
    g.add(MCyl(K, 'toaster_hedgerow_stems', mats.toa_bark, st));
    return g;
  },
});

P({
  id: 'toaster_crop_rows', label: 'Row crop (10 m)', size: '10×10 m', swatch: '#6f8f3a',
  instanced: true, budgetTris: 700, budgetParts: 2,
  stats: { Module: '10×10 m', Rows: '13 @ 760 mm', Plants: '260 MASK cards', Placed: '12' },
  note: 'Ten metres square of row crop — soybeans a month in — as 13 ridged rows on 760 mm centres along +X with the plants as MASK cards and the ridges in a shaded soil plane. Twelve modules fill the fenced field north of the core yard, and from the drive the rows read as the striped block a crop field is from a distance. Instanced.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_crop_rows');
    const S = 10, geo = new THREE.PlaneGeometry(S, S, 2, 52); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position, col = new Float32Array(p.count * 3);
    for (let i = 0; i < p.count; i++) {
      const z = p.getZ(i), ridge = Math.cos(z / .76 * Math.PI * 2) * .5 + .5;
      p.setY(i, .02 + ridge * .07);
      const v = .7 + ridge * .42; col[i * 3] = v; col[i * 3 + 1] = v * .94; col[i * 3 + 2] = v * .84;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(col, 3)); geo.computeVertexNormals();
    const soil = new THREE.Mesh(geo, mats.toa_dirt); soil.name = 'toaster_crop_rows_soil'; soil.receiveShadow = true; g.add(soil);
    const cards = [];
    for (let r = 0; r < 13; r++) {
      const z = -4.56 + r * .76;
      for (let k = 0; k < 20; k++) {
        const x = -4.75 + k * .5, h = .48 + hash(r, k) * .22;
        cards.push([.56, h, x, h / 2 + .06, z, (k % 2 ? .55 : -.55) + (hash(k, r) - .5) * .5, 0, [.82 + hash(r, k, 3) * .3, .9 + hash(r, k, 4) * .2, .66 + hash(r, k, 5) * .3]]);
      }
    }
    g.add(MC(K, 'toaster_crop_rows_plants', mats.toa_leaf_maple, cards));
    return g;
  },
});

P({
  id: 'toaster_reeds', label: 'Reeds (cattail clump)', size: '2 m · 1.9 high', swatch: '#5a6e3a',
  instanced: true, budgetTris: 200, budgetParts: 2,
  stats: { Clump: '2 m', Height: '1.4–2.0 m', Heads: '12', Placed: '8, pond margin' },
  note: 'A clump of cattails for the pond\u2019s wet shelf, standing in the inner rock course: blade cards with brown seed heads. Eight round the margin, none on the dock side or where the lane passes. A pond with a bare stone rim is a reservoir; reeds are what make it a farm pond.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_reeds'), cards = [], heads = [];
    for (let i = 0; i < 34; i++) {
      const a = i * 2.399, r = Math.pow(hash(i, 91), .5) * .95, h = 1.4 + hash(i, 92) * .6;
      cards.push([.12, h, Math.cos(a) * r, h / 2, Math.sin(a) * r, hash(i, 93) * 3.1, (hash(i, 94) - .5) * .2, [.75 + hash(i, 95) * .3, .85 + hash(i, 96) * .2, .55]]);
      if (i % 3 === 0) heads.push([.025, .025, .2, Math.cos(a) * r, h + .05, Math.sin(a) * r, 0, 0, 5]);
    }
    g.add(MC(K, 'toaster_reeds_blades', mats.toa_leaf_pine, cards));
    g.add(MCyl(K, 'toaster_reeds_heads', mats.toa_bark, heads));
    return g;
  },
});

P({
  id: 'toaster_fuel_tank', label: 'Farm fuel tank', size: '2.3×1.0 · 2.6 m', swatch: '#8a2c22',
  budgetTris: 900, budgetParts: 3,
  stats: { Tank: '1.0 dia × 2.3 m', Stand: '1.75 m steel', Placed: '1, by the shed' },
  note: 'Gravity diesel tank on a steel stand beside the shed — red enamel, rust on the stand, the hose and nozzle hung off the end. Turned like the propane tank; the stand is the same welded angle every farm builds.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_fuel_tank');
    const t = revolve(THREE, 'toaster_fuel_tank_shell', [[0, -1.15], [.3, -1.12], [.44, -1.02], [.5, -.85], [.5, .85], [.44, 1.02], [.3, 1.12], [0, 1.15]], mats.toa_red_paint, { segs: 20, creaseAngle: 40, pos: [0, 2.2, 0], rot: [0, Math.PI / 2, 0] });
    t.castShadow = true; g.add(t);
    const st = [];
    for (const x of [-.8, .8]) for (const z of [-.42, .42]) st.push([.06, 1.75, .06, x, .88, z]);
    st.push([1.9, .06, .06, 0, 1.72, -.42], [1.9, .06, .06, 0, 1.72, .42], [.06, .06, 1.0, -.8, 1.72, 0], [.06, .06, 1.0, .8, 1.72, 0],
      [1.9, .06, .06, 0, .55, -.42], [1.9, .06, .06, 0, .55, .42], [.12, .3, .9, -.8, 1.87, 0], [.12, .3, .9, .8, 1.87, 0]);
    g.add(MB(K, 'toaster_fuel_tank_stand', mats.toa_rust, st));
    g.add(MCyl(K, 'toaster_fuel_tank_fittings', mats.toa_warp_dark, [[.03, .03, .9, 1.05, 1.5, .35, .35, 0, 6], [.05, .05, .26, 1.18, 1.08, .5, Math.PI / 2, 0, 6], [.06, .06, .12, 0, 2.76, 0, 0, 0, 8]]));
    return g;
  },
});

P({
  id: 'toaster_hay_wagon', label: 'Hay wagon', size: '2.4×7.2×2.9 m', swatch: '#8a2c22',
  budgetTris: 1800, budgetParts: 4,
  stats: { Deck: '2.4×5.0 at 1.0 m', Load: '5 round bales', Placed: '1, hayfield' },
  note: 'Flat running-gear wagon, tongue down, loaded with five round bales and left in the hayfield where the baler stopped. Red running gear under a grey plank deck, tyres on the axles. DRESSING, not a vehicle — no rig, nothing the vehicle code binds to.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_hay_wagon');
    g.add(MB(K, 'toaster_hay_wagon_deck', mats.toa_deck, [[2.4, .08, 5.0, 0, 1.0, 0], [.1, .35, 5.0, -1.12, .82, 0], [.1, .35, 5.0, 1.12, .82, 0]]));
    g.add(MB(K, 'toaster_hay_wagon_gear', mats.toa_red_paint, [[.12, .14, 4.6, -.6, .84, 0], [.12, .14, 4.6, .6, .84, 0], [2.1, .1, .1, 0, .48, -1.7], [2.1, .1, .1, 0, .48, 1.7], [.08, .08, 2.0, 0, .42, 3.5], [.5, .08, .3, 0, .3, 4.45]]));
    const wh = [];
    for (const z of [-1.7, 1.7]) for (const x of [-1.2, 1.2]) wh.push([.42, .42, .2, x, .44, z, 0, Math.PI / 2, 12], [.14, .14, .24, x, .44, z, 0, Math.PI / 2, 8, [.6, .6, .6]]);
    g.add(MCyl(K, 'toaster_hay_wagon_wheels', mats.toa_warp_dark, wh));
    const bales = [];
    for (const z of [-1.3, 1.3]) for (const x of [-.64, .64]) bales.push([.72, .72, 1.2, x, 1.78, z, 0, Math.PI / 2, 12]);
    bales.push([.72, .72, 1.2, 0, 3.02, 0, 0, Math.PI / 2, 12]);
    g.add(MCyl(K, 'toaster_hay_wagon_bales', mats.toa_hay, bales));
    return g;
  },
});

P({
  id: 'toaster_bale_feeder', label: 'Bale feeder', size: '2.5 dia × 1.3 m', swatch: '#9da19c',
  budgetTris: 1200, budgetParts: 2,
  stats: { Ring: '2.4 m · 14 bars', Bale: 'standing, inside', Placed: '2, pasture' },
  note: 'Round tube bale feeder with a bale stood in it — the one object that says a pasture is grazed without putting an animal in the scene. Two in the south-west pasture.',
  build(K) {
    const { THREE, grp, mats } = K, g = grp('toaster_bale_feeder'), items = [];
    for (const y of [.32, 1.22]) items.push([new THREE.TorusGeometry(1.2, .03, 5, 36), 0, y, 0, Math.PI / 2, 0, 0]);
    for (let i = 0; i < 14; i++) { const a = i / 14 * Math.PI * 2; items.push([new THREE.CylinderGeometry(.022, .022, 1.36, 5), Math.cos(a) * 1.2, .74, Math.sin(a) * 1.2]); }
    g.add(MG(K, 'toaster_bale_feeder_ring', mats.toa_galv, items));
    g.add(MCyl(K, 'toaster_bale_feeder_bale', mats.toa_hay, [[.72, .74, 1.15, 0, .58, 0, 0, 0, 12]]));
    return g;
  },
});

P({
  id: 'toaster_garden_plot', label: 'Vegetable garden', size: '8×7.4 m', swatch: '#6b5a44',
  budgetTris: 900, budgetParts: 4,
  stats: { Beds: '4 raised, 1.2×6', Fence: 'low wire', Placed: '2, behind houses' },
  note: 'Four timber raised beds with a low rabbit wire round them, behind the two houses that are lived in. Plants as MASK cards on the soil. Origin at footprint centre on grade.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_garden_plot'), beds = [], soil = [], plants = [];
    for (let i = 0; i < 4; i++) {
      const x = -2.7 + i * 1.8;
      beds.push([1.2, .28, .05, x, .14, -3], [1.2, .28, .05, x, .14, 3], [.05, .28, 6.05, x - .6, .14, 0], [.05, .28, 6.05, x + .6, .14, 0]);
      soil.push([1.1, .2, 5.95, x, .13, 0]);
      for (let k = 0; k < 11; k++) { const z = -2.6 + k * .52, h = .25 + hash(i, k) * .35; plants.push([.6, h, x, .23 + h / 2, z, hash(k, i) * 3, 0, [.8 + hash(i, k, 2) * .4, .85 + hash(i, k, 3) * .3, .68 + hash(i, k, 4) * .3]]); }
    }
    g.add(MB(K, 'toaster_garden_plot_beds', mats.toa_timber_bare, beds));
    g.add(MB(K, 'toaster_garden_plot_soil', mats.toa_dirt, soil));
    g.add(MC(K, 'toaster_garden_plot_plants', mats.toa_leaf_maple, plants));
    const fence = [];
    for (const [x, z] of [[-4, -3.7], [0, -3.7], [4, -3.7], [-4, 3.7], [0, 3.7], [4, 3.7], [-4, 0], [4, 0]]) fence.push([.05, 1.0, .05, x, .5, z]);
    for (const y of [.45, .92]) fence.push([8, .012, .012, 0, y, -3.7], [8, .012, .012, 0, y, 3.7], [.012, .012, 7.4, -4, y, 0], [.012, .012, 7.4, 4, y, 0]);
    g.add(MB(K, 'toaster_garden_plot_fence', mats.toa_galv, fence));
    return g;
  },
});

P({
  id: 'toaster_clothesline', label: 'Clothesline', size: '8 m · 2.2 high', swatch: '#e6e2d8',
  budgetTris: 400, budgetParts: 3,
  stats: { Span: '8 m', Lines: '3, sagging', Washing: '3 sheets', Placed: '2' },
  note: 'Two steel T-posts, three lines with sag in them, and washing on the middle one. Cheap, and it is the one prop that says the house is lived in today rather than abandoned.',
  build(K) {
    const { grp, mats } = K, g = grp('toaster_clothesline');
    g.add(MB(K, 'toaster_clothesline_posts', mats.toa_galv, [[.06, 2.2, .06, -4, 1.1, 0], [.06, 2.2, .06, 4, 1.1, 0], [.05, .05, 1.8, -4, 2.15, 0], [.05, .05, 1.8, 4, 2.15, 0]]));
    const segs = [];
    for (const z of [-.8, 0, .8]) { let prev = [-4, 2.15, z]; for (let k = 1; k <= 4; k++) { const t = k / 4, p = [-4 + 8 * t, 2.15 - .5 * t * (1 - t), z]; segs.push([prev, p, .008, .008, 4]); prev = p; } }
    g.add(MSeg(K, 'toaster_clothesline_lines', mats.toa_galv, segs));
    g.add(MC(K, 'toaster_clothesline_washing', mats.toa_linen, [[1.6, 1.1, -1.8, 1.5, 0, 0, 0], [1.0, .9, .6, 1.6, 0, 0, 0, [.72, .78, .9]], [1.3, 1.0, 2.2, 1.55, 0, 0, 0, [.95, .88, .78]]]));
    return g;
  },
});

export const TOASTER_BUDGETS = TOASTER.filter((p) => p.budgetTris).map((p) => ({ id: p.id, tris: p.budgetTris, parts: p.budgetParts, instanced: !!p.instanced }));

/* ═══ assembly helpers ═════════════════════════════════════════════════════
   The belt and the roads are laid by rule rather than listed in levels.js:
   about 780 trees and ninety road modules are not a `place` array anybody can
   read, and upstream lays both from the same rules. Keeping the rule here
   rather than in the viewer means the kit and the client cannot drift. */

/**
 * The treeline, as ONE merged mesh per species per LOD rather than 780 scene
 * subtrees — the viewer's equivalent of the multimesh the client uses. Same
 * hashed lattice as `BuildTreeBelt`: no RNG anywhere, so every build of this
 * map draws the identical wood.
 */
export function buildToasterBelt(K, o) {
  const { THREE, grp, mats } = K, g = grp('toaster_treebelt');
  const { halfX, halfZ, depth = 12, keepOut = () => false, step = 3.5 } = o;
  const keys = BELT, spots = Object.fromEntries(keys.map((k) => [k, []]));
  let i = 0;
  for (let x = -halfX + 2; x <= halfX - 2; x += step) {
    for (let z = -halfZ + 2; z <= halfZ - 2; z += step, i++) {
      if (Math.abs(x) < halfX - depth && Math.abs(z) < halfZ - depth) continue;
      const jx = (i * 31 % 17) / 17 * 2.4 - 1.2, jz = (i * 43 % 13) / 13 * 2.4 - 1.2;
      const px = x + jx, pz = z + jz;
      if (keepOut(px, pz)) continue;
      const sc = .85 + (i * 7 % 10) * .04;
      // Positive modulo: half this map is at negative x.
      const pick = ((i * 3 + Math.floor(x)) % keys.length + keys.length) % keys.length;
      spots[keys[pick]].push({ x: px, z: pz, ry: (i * 47 % 360) * Math.PI / 180, sc });
    }
  }
  let n = 0;
  for (const key of keys) {
    if (!spots[key].length) continue;
    n += spots[key].length;
    for (const lod of [0, 1]) {
      const src = lod ? buildTreeLod(K, `t_${key}`, key) : buildTree(K, `t_${key}`, key);
      src.updateMatrixWorld(true);
      const tmpl = [];
      src.traverse((m) => { if (m.isMesh) tmpl.push({ geo: m.geometry.clone().applyMatrix4(m.matrixWorld), mat: m.material }); });
      const byMat = new Map();
      for (const t of tmpl) { if (!byMat.has(t.mat)) byMat.set(t.mat, []); byMat.get(t.mat).push(t.geo); }
      let part = 0;
      for (const [mat, geos] of byMat) {
        const list = [];
        for (const s of spots[key]) {
          const m = new THREE.Matrix4().makeRotationY(s.ry);
          m.scale(new THREE.Vector3(s.sc, s.sc, s.sc));
          m.setPosition(s.x, 0, s.z);
          for (const geo of geos) list.push({ geo, m });
        }
        const mesh = merge(THREE, list, `toaster_treebelt_${key}${lod ? '_lod1' : ''}_${part++}`, mat);
        mesh.castShadow = !lod;
        // LOD1 is the 90 m stand-in; in a static viewer only one of the pair
        // should be visible or every tree is drawn twice.
        mesh.visible = !lod;
        g.add(mesh);
      }
    }
  }
  g.userData.trees = n;
  return g;
}

/**
 * The made roads: 4 m modules laid along the authored polylines.
 *
 * The polyline is the gameplay — vehicle handling is read off these same
 * centre lines — so the art is laid ON the line by construction rather than
 * fitted to it afterwards. Asphalt sits a centimetre proud of gravel, so a
 * crossing is a crossing rather than a z-fight.
 */
export function buildToasterRoads(K, roads, ALL) {
  const { THREE, grp } = K, g = grp('toaster_roads');
  for (const r of roads) {
    const M = ALL[r.kind === 'asphalt' ? 'toaster_road_asphalt' : r.kind === 'track' ? 'toaster_path_ground' : 'toaster_road_gravel'];
    if (!M) continue;
    /* Wider than the module: lay rows across it. The shed apron is 8 m of the
       same gravel the drive is made of, not a wider module — and an `apron`
       sits 15 mm under the drive that crosses it so the two do not fight. */
    const mw = M.width || 4, rows = Math.max(1, Math.round((r.width ?? mw) / mw)), drop = r.kind === 'apron' ? .015 : 0;
    for (let i = 0; i < r.pts.length - 1; i++) {
      const a = new THREE.Vector3(...r.pts[i]), b = new THREE.Vector3(...r.pts[i + 1]);
      const d = b.clone().sub(a), len = d.length();
      if (len < 1) continue;
      const n = Math.max(1, Math.round(len / 4)), perp = new THREE.Vector3(-d.z, 0, d.x).normalize();
      for (let k = 0; k < n; k++) for (let q = 0; q < rows; q++) {
        const p = M.build(K);
        p.position.copy(a).addScaledVector(d, (k + .5) / n).addScaledVector(perp, (q - (rows - 1) / 2) * mw);
        p.position.y -= drop;
        // The module runs along its own +X, so it is laid on the LINE, not
        // across it. A quarter-turn out here is a road at right angles to the
        // one the vehicles are driving on.
        p.rotation.y = Math.atan2(-d.z, d.x);
        p.scale.x = (len / n) / 4;
        g.add(p);
      }
    }
    /* Fillet every interior bend. Two straight runs butting at a vertex each
       carry their own heading, so the outside of the corner is a wedge of
       bare grass as wide as the turn is sharp — on a 6 m road through a 40°
       bend that is a two-metre notch bitten out of the carriageway. One
       extra module on the bisector, squared to the road width, covers it. */
    for (let i = 1; i < r.pts.length - 1; i++) {
      const a = new THREE.Vector3(...r.pts[i - 1]), b = new THREE.Vector3(...r.pts[i]), c = new THREE.Vector3(...r.pts[i + 1]);
      const h0 = Math.atan2(-(b.z - a.z), b.x - a.x), h1 = Math.atan2(-(c.z - b.z), c.x - b.x);
      let turn = h1 - h0;
      while (turn > Math.PI) turn -= Math.PI * 2;
      while (turn < -Math.PI) turn += Math.PI * 2;
      if (Math.abs(turn) < .06) continue;            // effectively straight
      const p = M.build(K);
      p.position.copy(b);
      p.rotation.y = h0 + turn / 2;
      p.scale.x = (r.width ?? 6) / 4; p.scale.z = rows; p.position.y -= drop;
      g.add(p);
    }
  }
  return g;
}

/** The circular drive: twelve 30° arcs off one centre of curvature. */
export function buildToasterDrive(K, centre, ALL) {
  const { grp } = K, g = grp('toaster_drive');
  const M = ALL.toaster_road_arc; if (!M) return g;
  for (let i = 0; i < 12; i++) {
    const a = M.build(K);
    a.position.set(...centre);
    a.rotation.y = -i * Math.PI / 6;
    g.add(a);
  }
  return g;
}

/**
 * Power lines: three conductors between consecutive PLACED poles, with sag.
 * `lines` is what the level's pole generator actually managed to place —
 * [x, z, yaw] per pole — so a pole refused for clearance shortens a span
 * instead of leaving a wire in the air over nothing. Attachment points are the
 * pole's own: outer insulators ±0.95 m along the crossarm at 9.6, the pin at
 * the pole top at 10.2. Sag follows span (w L² / 8T), clamped, so a 36 m span
 * dips about 0.9 m and nothing dips into a lane's 5.5 m headroom.
 */
export function buildToasterWires(K, lines) {
  const { grp, mats } = K, g = grp('toaster_wires'), segs = [];
  for (const line of lines) for (let i = 0; i < line.length - 1; i++) {
    const [ax, az, ary] = line[i], [bx, bz, bry] = line[i + 1];
    const span = Math.hypot(bx - ax, bz - az), sag = Math.min(1.8, .00065 * span * span + .25);
    for (const [off, y] of [[-.95, 9.6], [.95, 9.6], [0, 10.2]]) {
      const A = [ax + Math.cos(ary) * off, y, az - Math.sin(ary) * off], B = [bx + Math.cos(bry) * off, y, bz - Math.sin(bry) * off];
      let prev = A;
      for (let k = 1; k <= 6; k++) { const t = k / 6, P = [A[0] + (B[0] - A[0]) * t, y - sag * 4 * t * (1 - t), A[2] + (B[2] - A[2]) * t]; segs.push([prev, P, .016, .016, 4]); prev = P; }
    }
  }
  if (segs.length) g.add(MSeg(K, 'toaster_wires_conductors', mats.toa_warp_dark, segs));
  return g;
}
