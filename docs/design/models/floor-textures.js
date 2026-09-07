/**
 * Deep Field 3D — procedural floor textures.
 * Bakes albedo / roughness / normal / emissive maps on a canvas at build time
 * so the floor carries real surface detail (plates, rivets, rust, scorch,
 * gravel, oil, puddles) with no external files, and still exports into the
 * GLB (GLTFExporter embeds CanvasTextures as PNG). One texture set per kit,
 * generated once per THREE instance and shared by every tile.
 *
 * Scale: a 20 m tile maps to the full 1024² texture → ~2 cm per texel.
 * The engine constraint (§1) holds: no baked lighting, just surface albedo
 * that tolerates runtime tint.
 */
const cache = new WeakMap();

function rng(seed) { let s = seed >>> 0; return () => { s += 0x6D2B79F5; let t = Math.imul(s ^ (s >>> 15), 1 | s); t ^= t + Math.imul(t ^ (t >>> 7), 61 | t); return ((t ^ (t >>> 14)) >>> 0) / 4294967296; }; }
const cv = (n = 1024) => { const c = document.createElement('canvas'); c.width = c.height = n; return c; };
const rgba = (r, g, b, a = 1) => `rgba(${r | 0},${g | 0},${b | 0},${a})`;

/** Grain: many tiny alpha rects — cheap noise that survives downsampling. */
function grain(x, n, count, lo, hi, alpha, R) {
  for (let i = 0; i < count; i++) { const v = lo + R() * (hi - lo); x.fillStyle = rgba(v, v, v, alpha); x.fillRect(R() * n, R() * n, 1 + R() * 2, 1 + R() * 2); }
}
function blot(x, cx, cy, r, color, alpha, inner = 0) {
  const g = x.createRadialGradient(cx, cy, r * inner, cx, cy, r); g.addColorStop(0, color.replace(/[\d.]+\)$/, alpha + ')')); g.addColorStop(1, color.replace(/[\d.]+\)$/, '0)'));
  x.fillStyle = g; x.beginPath(); x.arc(cx, cy, r, 0, Math.PI * 2); x.fill();
}
function jag(x, x0, y0, x1, y1, amp, steps, R) {
  x.beginPath(); x.moveTo(x0, y0);
  for (let i = 1; i < steps; i++) { const t = i / steps; x.lineTo(x0 + (x1 - x0) * t + (R() - .5) * amp, y0 + (y1 - y0) * t + (R() - .5) * amp); }
  x.lineTo(x1, y1); x.stroke();
}
/** Sobel height → tangent-space normal map. */
function normalFromHeight(hc, strength = 2.5) {
  const n = hc.width, src = hc.getContext('2d').getImageData(0, 0, n, n).data, out = cv(n), ox = out.getContext('2d'), img = ox.createImageData(n, n), d = img.data;
  const h = (x, y) => src[(((y + n) % n) * n + ((x + n) % n)) * 4] / 255;
  for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) {
    const dx = (h(x + 1, y - 1) + 2 * h(x + 1, y) + h(x + 1, y + 1)) - (h(x - 1, y - 1) + 2 * h(x - 1, y) + h(x - 1, y + 1));
    const dy = (h(x - 1, y + 1) + 2 * h(x, y + 1) + h(x + 1, y + 1)) - (h(x - 1, y - 1) + 2 * h(x, y - 1) + h(x + 1, y - 1));
    let nx = -dx * strength, ny = -dy * strength, nz = 1; const l = Math.hypot(nx, ny, nz); nx /= l; ny /= l; nz /= l;
    const i = (y * n + x) * 4; d[i] = (nx * .5 + .5) * 255; d[i + 1] = (ny * .5 + .5) * 255; d[i + 2] = (nz * .5 + .5) * 255; d[i + 3] = 255;
  }
  ox.putImageData(img, 0, 0); return out;
}
function tex(THREE, canvas, srgb = false) {
  const t = new THREE.CanvasTexture(canvas); t.wrapS = t.wrapT = THREE.RepeatWrapping; t.anisotropy = 8;
  if (srgb) t.colorSpace = THREE.SRGBColorSpace; t.needsUpdate = true; return t;
}

/* ── Foundry: cast-iron floor plates ───────────────────────────────── */
function bakeFoundry(THREE) {
  const N = 1024, R = rng(7331), P = 8, S = N / P; // 8 plates of 2.5 m
  const a = cv(N), ax = a.getContext('2d'), r = cv(N), rx = r.getContext('2d'), h = cv(N), hx = h.getContext('2d'), e = cv(N), ex = e.getContext('2d');
  // Base iron + grain.
  ax.fillStyle = '#2b2c33'; ax.fillRect(0, 0, N, N); grain(ax, N, 26000, 20, 80, .35, R);
  rx.fillStyle = '#c9c9c9'; rx.fillRect(0, 0, N, N); grain(rx, N, 8000, 150, 240, .5, R);
  hx.fillStyle = '#808080'; hx.fillRect(0, 0, N, N); grain(hx, N, 6000, 110, 150, .5, R);
  ex.fillStyle = '#000'; ex.fillRect(0, 0, N, N);
  // Plates: tint variation, slight dome in height, worn centre in roughness.
  for (let i = 0; i < P; i++) for (let j = 0; j < P; j++) {
    const x0 = i * S, y0 = j * S, v = R();
    ax.fillStyle = v < .12 ? rgba(70, 48, 40, .55) : v < .2 ? rgba(30, 30, 36, .6) : rgba(40 + R() * 14, 42 + R() * 12, 50 + R() * 12, .55); ax.fillRect(x0 + 3, y0 + 3, S - 6, S - 6);
    const g = hx.createRadialGradient(x0 + S / 2, y0 + S / 2, 0, x0 + S / 2, y0 + S / 2, S * .7); g.addColorStop(0, rgba(150, 150, 150, .5)); g.addColorStop(1, rgba(150, 150, 150, 0)); hx.fillStyle = g; hx.fillRect(x0, y0, S, S);
    blot(rx, x0 + S / 2, y0 + S / 2, S * .45, 'rgba(120,120,120,1)', .5);
    // tread ridges on some plates
    if (v > .55) { ax.strokeStyle = rgba(18, 19, 24, .5); ax.lineWidth = 2; hx.strokeStyle = rgba(60, 60, 60, .6); for (let k = 12; k < S - 8; k += 14) { ax.beginPath(); ax.moveTo(x0 + 10, y0 + k); ax.lineTo(x0 + S - 10, y0 + k + 6); ax.stroke(); hx.beginPath(); hx.moveTo(x0 + 10, y0 + k); hx.lineTo(x0 + S - 10, y0 + k + 6); hx.stroke(); } }
  }
  // Seams: recessed dark gap with a lit bevel.
  for (let k = 0; k <= P; k++) {
    const p = k * S;
    ax.fillStyle = '#121318'; ax.fillRect(p - 3, 0, 6, N); ax.fillRect(0, p - 3, N, 6);
    ax.fillStyle = rgba(120, 128, 140, .28); ax.fillRect(p + 3, 0, 1, N); ax.fillRect(0, p + 3, N, 1);
    hx.fillStyle = '#2a2a2a'; hx.fillRect(p - 3, 0, 6, N); hx.fillRect(0, p - 3, N, 6);
    rx.fillStyle = '#f0f0f0'; rx.fillRect(p - 3, 0, 6, N); rx.fillRect(0, p - 3, N, 6);
  }
  // Rivets at plate corners (inset), with rust bleed below some of them.
  for (let i = 0; i < P; i++) for (let j = 0; j < P; j++) for (const [dx, dy] of [[12, 12], [S - 12, 12], [12, S - 12], [S - 12, S - 12]]) {
    const x = i * S + dx, y = j * S + dy;
    ax.fillStyle = '#1a1b21'; ax.beginPath(); ax.arc(x, y, 4.5, 0, Math.PI * 2); ax.fill();
    ax.fillStyle = '#6a7280'; ax.beginPath(); ax.arc(x - .8, y - .8, 3, 0, Math.PI * 2); ax.fill();
    hx.fillStyle = '#e0e0e0'; hx.beginPath(); hx.arc(x, y, 3.2, 0, Math.PI * 2); hx.fill();
    rx.fillStyle = '#707070'; rx.beginPath(); rx.arc(x, y, 3.5, 0, Math.PI * 2); rx.fill();
    if (R() < .25) { const g = ax.createLinearGradient(x, y, x, y + 40 + R() * 40); g.addColorStop(0, rgba(150, 80, 40, .5)); g.addColorStop(1, rgba(150, 80, 40, 0)); ax.fillStyle = g; ax.fillRect(x - 3, y, 6 + R() * 3, 80); }
  }
  // Soot blotches + rust patches (albedo), wet slag glass (roughness low, dark albedo).
  for (let i = 0; i < 26; i++) blot(ax, R() * N, R() * N, 60 + R() * 140, 'rgba(10,10,14,1)', .35 + R() * .3);
  for (let i = 0; i < 18; i++) { const x = R() * N, y = R() * N, rad = 30 + R() * 80; blot(ax, x, y, rad, 'rgba(140,70,35,1)', .3 + R() * .3); blot(rx, x, y, rad, 'rgba(255,255,255,1)', .6); }
  for (let i = 0; i < 9; i++) { const x = R() * N, y = R() * N, rad = 24 + R() * 40; ax.save(); ax.translate(x, y); ax.rotate(R() * Math.PI); ax.scale(1, .55 + R() * .3); blot(ax, 0, 0, rad, 'rgba(6,6,9,1)', .9, .4); ax.restore(); rx.save(); rx.translate(x, y); rx.scale(1, .6); blot(rx, 0, 0, rad, 'rgba(40,40,40,1)', .95, .5); rx.restore(); }
  // Scorch rings with an ember fringe, and cooled-lava cracks; some cracks still glow (emissive).
  for (let i = 0; i < 7; i++) { const x = R() * N, y = R() * N, rad = 40 + R() * 70; blot(ax, x, y, rad, 'rgba(8,7,9,1)', .75, .2); blot(ax, x, y, rad * 1.15, 'rgba(180,70,20,1)', .16, .8); }
  ax.lineCap = 'round';
  for (let i = 0; i < 14; i++) {
    const x0 = R() * N, y0 = R() * N, x1 = x0 + (R() - .5) * 300, y1 = y0 + (R() - .5) * 300, glow = R() < .4;
    ax.strokeStyle = '#0b0b0e'; ax.lineWidth = 3 + R() * 2; jag(ax, x0, y0, x1, y1, 24, 9, R);
    hx.strokeStyle = '#303030'; hx.lineWidth = 3; jag(hx, x0, y0, x1, y1, 24, 9, rng(i));
    if (glow) { ex.strokeStyle = '#ff7a1a'; ex.lineWidth = 2.2; ex.shadowColor = '#ff5a00'; ex.shadowBlur = 8; jag(ex, x0, y0, x1, y1, 24, 9, rng(i)); ex.shadowBlur = 0; ax.strokeStyle = rgba(255, 120, 40, .35); ax.lineWidth = 1.2; jag(ax, x0, y0, x1, y1, 24, 9, rng(i)); }
  }
  // Scratches + ash drift along seams.
  ax.lineWidth = 1; for (let i = 0; i < 400; i++) { const x = R() * N, y = R() * N, l = 10 + R() * 60, an = R() * Math.PI; ax.strokeStyle = rgba(150, 160, 175, .08 + R() * .1); ax.beginPath(); ax.moveTo(x, y); ax.lineTo(x + Math.cos(an) * l, y + Math.sin(an) * l); ax.stroke(); }
  for (let i = 0; i < 20; i++) { const k = Math.floor(R() * P) * S, along = R() * N; ax.save(); if (R() < .5) ax.translate(k, along); else { ax.translate(along, k); ax.rotate(Math.PI / 2); } ax.scale(1, 3.5); blot(ax, 0, 0, 14 + R() * 10, 'rgba(120,118,112,1)', .35); ax.restore(); }
  // Stencilled bay numbers, faded.
  ax.font = 'bold 44px Oxanium, sans-serif'; ax.textAlign = 'center'; ax.fillStyle = rgba(200, 155, 60, .22);
  for (let i = 0; i < 4; i++) { ax.save(); ax.translate(S * (1 + Math.floor(R() * 6)) + S / 2, S * (1 + Math.floor(R() * 6)) + S / 2); ax.rotate(Math.floor(R() * 4) * Math.PI / 2); ax.fillText(`F-${String(Math.floor(R() * 40)).padStart(2, '0')}`, 0, 14); ax.restore(); }
  // Hazard chevrons along one seam.
  { const y = S * 4; for (let x = 0; x < N; x += 28) { ax.fillStyle = rgba(200, 155, 60, .18); ax.beginPath(); ax.moveTo(x, y - 8); ax.lineTo(x + 14, y - 8); ax.lineTo(x + 24, y + 8); ax.lineTo(x + 10, y + 8); ax.closePath(); ax.fill(); } }
  return { map: tex(THREE, a, true), roughnessMap: tex(THREE, r), normalMap: tex(THREE, normalFromHeight(h, 3)), emissiveMap: tex(THREE, e, true) };
}

/* ── Switchyard: ballast, oil and standing water ────────────────────── */
function bakeSwitchyard(THREE) {
  const N = 1024, R = rng(4242);
  const a = cv(N), ax = a.getContext('2d'), r = cv(N), rx = r.getContext('2d'), h = cv(N), hx = h.getContext('2d'), e = cv(N), ex = e.getContext('2d');
  ax.fillStyle = '#4a4640'; ax.fillRect(0, 0, N, N);
  rx.fillStyle = '#f2f2f2'; rx.fillRect(0, 0, N, N);
  hx.fillStyle = '#707070'; hx.fillRect(0, 0, N, N);
  ex.fillStyle = '#000'; ex.fillRect(0, 0, N, N);
  // Compacted dark bands where the sidings run (z ±6 → v .2 / .8) and a centre drain.
  for (const v of [.2, .8]) { const g = ax.createLinearGradient(0, N * v - 90, 0, N * v + 90); g.addColorStop(0, rgba(30, 28, 26, 0)); g.addColorStop(.5, rgba(30, 28, 26, .7)); g.addColorStop(1, rgba(30, 28, 26, 0)); ax.fillStyle = g; ax.fillRect(0, N * v - 90, N, 180); }
  { const g = ax.createLinearGradient(0, N / 2 - 20, 0, N / 2 + 20); g.addColorStop(0, rgba(20, 20, 22, 0)); g.addColorStop(.5, rgba(20, 20, 22, .8)); g.addColorStop(1, rgba(20, 20, 22, 0)); ax.fillStyle = g; ax.fillRect(0, N / 2 - 20, N, 40); hx.fillStyle = '#404040'; hx.fillRect(0, N / 2 - 10, N, 20); }
  // Gravel: ~30k pebbles with a lit top-left edge; each also a bump in height.
  const pal = [[112, 108, 100], [86, 82, 76], [132, 126, 116], [70, 66, 62], [146, 132, 116], [98, 96, 94], [116, 102, 86]];
  for (let i = 0; i < 30000; i++) {
    const x = R() * N, y = R() * N, rad = 1.2 + R() * 2.6, c = pal[Math.floor(R() * pal.length)], k = .85 + R() * .3;
    ax.fillStyle = rgba(c[0] * k, c[1] * k, c[2] * k, .9); ax.beginPath(); ax.ellipse(x, y, rad, rad * (.6 + R() * .4), R() * Math.PI, 0, Math.PI * 2); ax.fill();
    ax.fillStyle = rgba(c[0] * k + 40, c[1] * k + 40, c[2] * k + 40, .35); ax.beginPath(); ax.arc(x - rad * .3, y - rad * .3, rad * .45, 0, Math.PI * 2); ax.fill();
    hx.fillStyle = rgba(130 + rad * 18, 130 + rad * 18, 130 + rad * 18, .8); hx.beginPath(); hx.arc(x, y, rad, 0, Math.PI * 2); hx.fill();
  }
  // Oil stains (dark, slightly glossy), rust dust near rails, coal fines.
  for (let i = 0; i < 14; i++) { const x = R() * N, y = R() * N, rad = 30 + R() * 70; blot(ax, x, y, rad, 'rgba(12,11,12,1)', .7, .1); blot(rx, x, y, rad, 'rgba(90,90,90,1)', .8, .2); }
  for (const v of [.2, .8]) for (let i = 0; i < 40; i++) blot(ax, R() * N, N * v + (R() - .5) * 120, 10 + R() * 30, 'rgba(120,60,30,1)', .25);
  for (let i = 0; i < 20; i++) blot(ax, R() * N, R() * N, 40 + R() * 100, 'rgba(18,18,20,1)', .3);
  // Puddles: flat, dark, near-mirror (roughness ~.1), with a pale rim of dried silt; recessed in height.
  for (let i = 0; i < 7; i++) {
    const x = R() * N, y = R() * N, rad = 40 + R() * 90, sy = .5 + R() * .4, an = R() * Math.PI;
    for (const [ctx, col, al, inner] of [[ax, 'rgba(160,150,130,1)', .35, .9], [ax, 'rgba(14,18,26,1)', .92, .85], [rx, 'rgba(25,25,25,1)', 1, .85], [hx, 'rgba(70,70,70,1)', .9, .8]]) { ctx.save(); ctx.translate(x, y); ctx.rotate(an); ctx.scale(1, sy); blot(ctx, 0, 0, rad * (al === .35 ? 1.12 : 1), col, al, inner); ctx.restore(); }
  }
  // Weeds in the margins: small dry-grass strokes.
  ax.lineCap = 'round'; for (let i = 0; i < 260; i++) { const x = R() * N, y = R() < .5 ? R() * 90 : N - R() * 90; for (let k = 0; k < 5; k++) { ax.strokeStyle = rgba(90 + R() * 30, 96 + R() * 30, 60, .55); ax.lineWidth = 1 + R(); ax.beginPath(); ax.moveTo(x, y); ax.lineTo(x + (R() - .5) * 12, y - 6 - R() * 10); ax.stroke(); } }
  // Painted yard markings: a faded white edge line, a yellow keep-clear box, stencilled track numbers.
  ax.fillStyle = rgba(220, 220, 210, .16); ax.fillRect(0, N * .35, N, 5); ax.fillRect(0, N * .65, N, 5);
  ax.strokeStyle = rgba(216, 161, 58, .28); ax.lineWidth = 6; ax.setLineDash([26, 16]); ax.strokeRect(N * .1, N * .4, N * .25, N * .2); ax.setLineDash([]);
  ax.font = 'bold 60px Oxanium, sans-serif'; ax.textAlign = 'center'; ax.fillStyle = rgba(220, 220, 210, .2);
  for (const [t, u, v] of [['07', .55, .27], ['12', .55, .77], ['SY', .85, .5]]) { ax.save(); ax.translate(N * u, N * v); ax.fillText(t, 0, 20); ax.restore(); }
  // A couple of dropped signal-lamp glints (emissive) — reflections of the yard lights in the puddles.
  for (let i = 0; i < 3; i++) blot(ex, R() * N, R() * N, 8, 'rgba(255,190,90,1)', .5);
  return { map: tex(THREE, a, true), roughnessMap: tex(THREE, r), normalMap: tex(THREE, normalFromHeight(h, 2.2)), emissiveMap: tex(THREE, e, true) };
}

/* ── Foundry: refractory brick (boundary wall, buttresses) ───────────── */
/* 1024² = 2.5 × 2.5 m: 10 stretcher courses of 250 × 75 mm brick, 8 px mortar, half-brick stagger. */
function bakeBrick(THREE) {
  const N = 1024, R = rng(9137), BW = N / 4, BH = N / 10, J = 8;
  const a = cv(N), ax = a.getContext('2d'), r = cv(N), rx = r.getContext('2d'), h = cv(N), hx = h.getContext('2d');
  ax.fillStyle = '#6a625a'; ax.fillRect(0, 0, N, N); grain(ax, N, 9000, 60, 130, .35, R);      // mortar
  rx.fillStyle = '#f4f4f4'; rx.fillRect(0, 0, N, N);
  hx.fillStyle = '#404040'; hx.fillRect(0, 0, N, N);
  const pal = [[122, 62, 44], [140, 74, 50], [104, 52, 40], [150, 88, 60], [88, 44, 36], [132, 70, 48], [160, 104, 72]];
  for (let row = 0; row < 10; row++) {
    const off = row % 2 ? BW / 2 : 0;
    for (let col = -1; col < 4; col++) {
      const x0 = col * BW + off + J / 2, y0 = row * BH + J / 2, w = BW - J, hh = BH - J;
      const c = pal[Math.floor(R() * pal.length)], k = .85 + R() * .3, glazed = R() < .08, sooty = row > 7 && R() < .6;
      const base = sooty ? [c[0] * .45, c[1] * .45, c[2] * .5] : [c[0] * k, c[1] * k, c[2] * k];
      ax.fillStyle = rgba(...base, 1); ax.fillRect(x0, y0, w, hh);
      // fired-clay mottle + a lit top edge / dark bottom edge (bevel)
      for (let i = 0; i < 40; i++) blot(ax, x0 + R() * w, y0 + R() * hh, 6 + R() * 14, `rgba(${base[0] * (.7 + R() * .6) | 0},${base[1] * (.7 + R() * .6) | 0},${base[2] * (.7 + R() * .6) | 0},1)`, .35);
      ax.fillStyle = rgba(255, 240, 220, .18); ax.fillRect(x0, y0, w, 3); ax.fillStyle = rgba(0, 0, 0, .35); ax.fillRect(x0, y0 + hh - 3, w, 3);
      // height: brick face proud, slight per-brick tilt; roughness: glazed bricks are shiny, soot is dull
      const g = hx.createLinearGradient(x0, y0, x0 + w, y0 + hh); const lv = 150 + R() * 40; g.addColorStop(0, rgba(lv, lv, lv, 1)); g.addColorStop(1, rgba(lv - 30, lv - 30, lv - 30, 1)); hx.fillStyle = g; hx.fillRect(x0, y0, w, hh);
      rx.fillStyle = glazed ? '#505050' : sooty ? '#ffffff' : rgba(190 + R() * 40, 190 + R() * 40, 190 + R() * 40, 1); rx.fillRect(x0, y0, w, hh);
      if (glazed) { ax.fillStyle = rgba(30, 20, 24, .55); ax.fillRect(x0, y0, w, hh); }
      if (R() < .12) { ax.strokeStyle = rgba(20, 14, 12, .6); ax.lineWidth = 1.5; jag(ax, x0 + R() * w, y0, x0 + R() * w, y0 + hh, 10, 5, rng(row * 7 + col)); } // crazing crack
    }
  }
  // Soot wash rising from the ground courses, heat scorch bands, salt bloom near the top.
  { const g = ax.createLinearGradient(0, N, 0, N * .55); g.addColorStop(0, rgba(8, 6, 8, .7)); g.addColorStop(1, rgba(8, 6, 8, 0)); ax.fillStyle = g; ax.fillRect(0, 0, N, N); }
  for (let i = 0; i < 6; i++) blot(ax, R() * N, N * .3 + R() * N * .5, 90 + R() * 120, 'rgba(60,20,10,1)', .3);
  for (let i = 0; i < 40; i++) blot(ax, R() * N, R() * N * .3, 10 + R() * 30, 'rgba(230,225,215,1)', .18);
  for (let i = 0; i < 300; i++) { const x = R() * N, y = R() * N; ax.fillStyle = rgba(200, 200, 210, .1 + R() * .1); ax.fillRect(x, y, 1 + R() * 2, 1 + R() * 2); }
  return { map: tex(THREE, a, true), roughnessMap: tex(THREE, r), normalMap: tex(THREE, normalFromHeight(h, 2.4)) };
}

export function floorMaterials(THREE) {
  if (cache.has(THREE)) return cache.get(THREE);
  const f = bakeFoundry(THREE), s = bakeSwitchyard(THREE), b = bakeBrick(THREE);
  const brick = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 1, metalness: 0, ...b, normalScale: new THREE.Vector2(1, 1) }); brick.name = 'refractory_brick';
  const foundry = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 1, metalness: .2, ...f, emissive: new THREE.Color(0xff6a00), emissiveIntensity: 1.2, normalScale: new THREE.Vector2(.9, .9) }); foundry.name = 'floor_foundry';
  const switchyard = new THREE.MeshStandardMaterial({ color: 0xffffff, roughness: 1, metalness: .05, ...s, emissive: new THREE.Color(0xffb060), emissiveIntensity: .8, normalScale: new THREE.Vector2(.7, .7) }); switchyard.name = 'floor_switchyard';
  const puddle = new THREE.MeshStandardMaterial({ color: 0x0c1119, roughness: .08, metalness: .3 }); puddle.name = 'puddle';
  const slag_glass = new THREE.MeshStandardMaterial({ color: 0x0a0a0d, roughness: .12, metalness: .2, emissive: new THREE.Color(0x3a1206), emissiveIntensity: .5 }); slag_glass.name = 'slag_glass';
  const out = { foundry, switchyard, puddle, slag_glass, brick }; cache.set(THREE, out); return out;
}
