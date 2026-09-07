/**
 * Deep Field 3D — level atmosphere for the map viewer.
 * Applies per-map fog + lighting to the stage, and adds to the assembly group:
 * point lights on every light source, volumetric cones + ground pools under
 * the floods, GPU particle fields (embers / ash), drifting ground-haze cards.
 * Everything added here is named `__atmo_*` and lives in the group, so it
 * disappears with the model; stage lighting is restored on `clear()`.
 */
const PRESETS = {
  foundry: {
    fog: 0x08060a, density: .006, exposure: 1.05,
    hemi: [0x6a4634, 0x120c0a, .7], key: [0xffb27a, .85, [-40, 18, 26]], fill: [0x2a3550, .35],
    particles: { kind: 'ember', count: 1800, color: 0xff8a2a, size: 5, height: 14, speed: .9 },
    haze: { color: 0x2a1610, layers: [[.35, .16], [.9, .11], [1.7, .07]] },
    lights: [
      { p: [-32, 3.6, -18], c: 0xff7a1a, i: 140, d: 30 }, { p: [-32, .3, -20.6], c: 0xff5a00, i: 60, d: 8 },   // crucible + pour puddle
      { p: [40, 3.2, 6], c: 0x22d3ee, i: 160, d: 30 }, { p: [-40, 2.2, 0], c: 0xff2e4a, i: 120, d: 26 },      // core, spawn portal
      { p: [-6, 3.0, -24], c: 0xf0c83a, i: 60, d: 14 },                                                        // armory sign
      { p: [0, 1.3, -6.5], c: 0xffd9a0, i: 30, d: 9 }, { p: [0, 1.3, -1], c: 0xffd9a0, i: 22, d: 8 },          // vent tunnel lamps
      { p: [-6, 5.4, -12], c: 0xffe2a8, i: 24, d: 9 }, { p: [6, 5.4, -12], c: 0xffe2a8, i: 24, d: 9 },        // deck edge lamps
      { p: [-46, .4, -30], c: 0xff5a00, i: 18, d: 7 }, { p: [-46, .4, 30], c: 0xff5a00, i: 18, d: 7 }, { p: [46, .4, -30], c: 0xff5a00, i: 18, d: 7 }, { p: [-20, .4, 32], c: 0xff5a00, i: 18, d: 7 }, // lava cracks
    ],
    floods: [ // light-rig heads: position, aim (yaw), warm
      { p: [-20, 6.6, -26], yaw: 0, c: 0xffe2a8, i: 90, d: 30 }, { p: [30, 6.6, -18], yaw: 2.4, c: 0xffe2a8, i: 90, d: 30 }, { p: [30, 6.6, 22], yaw: -2.2, c: 0xffe2a8, i: 90, d: 30 },
    ],
    hotspots: [{ p: [-32, 3.5, -18], r: 2.6, c: 0xff7a1a }], // crucible mouth glow sprite
  },
  switchyard: {
    fog: 0x05070e, density: .0065, exposure: 1.2,
    hemi: [0x3a4668, 0x14161c, .95], key: [0x9fb4ff, 1.0, [20, 34, -14]], fill: [0x6a4a1c, .4],
    particles: { kind: 'ash', count: 1400, color: 0xb9c2d0, size: 4, height: 12, speed: .35 },
    haze: { color: 0x222838, layers: [[.3, .18], [.8, .12], [1.6, .07]] },
    lights: [
      { p: [44, 3.2, 8], c: 0x22d3ee, i: 160, d: 30 }, { p: [-45, 2.2, -10], c: 0xff2e4a, i: 120, d: 26 },
      { p: [6, 3.0, -26], c: 0xf0c83a, i: 60, d: 14 },
      { p: [-14, 3.6, 1.2], c: 0xbfd4ff, i: 40, d: 12 },                                                       // cut lamp post
      { p: [-36, 7.0, -22], c: 0xff2e4a, i: 20, d: 8 }, { p: [-36, 6.3, -22], c: 0x7fe65a, i: 14, d: 7 },      // signal towers
      { p: [-34, 7.0, 20], c: 0xff2e4a, i: 20, d: 8 }, { p: [26, 7.0, 16], c: 0xff2e4a, i: 20, d: 8 }, { p: [26, 6.3, 16], c: 0x7fe65a, i: 14, d: 7 },
      { p: [-14, 4.3, -14.3], c: 0xbfd4ff, i: 30, d: 10 }, { p: [-2, 4.3, -14.3], c: 0xbfd4ff, i: 30, d: 10 }, // mid-deck lamps
      { p: [-50, 1.9, 30], c: 0xff2e4a, i: 10, d: 5 }, { p: [-50, 1.9, -30], c: 0xff2e4a, i: 10, d: 5 }, { p: [50, 1.9, 30], c: 0xff2e4a, i: 10, d: 5 }, // buffer lamps
    ],
    floods: [ // catwalk floods hang under the walkway at y 9 and point straight down
      { p: [-2, 9.0, 3], yaw: 0, down: true, c: 0xdce8ff, i: 70, d: 22 }, { p: [6, 9.0, 3], yaw: 0, down: true, c: 0xdce8ff, i: 70, d: 22 }, { p: [13, 9.0, 3], yaw: 0, down: true, c: 0xdce8ff, i: 70, d: 22 },
      { p: [-14, 6.2, -20], yaw: 0, c: 0xbfd4ff, i: 60, d: 26 }, { p: [1.3, 6.2, -20], yaw: 0, c: 0xbfd4ff, i: 60, d: 26 }, // retaining-wall lamps over the mid deck
    ],
    hotspots: [],
  },
};

let saved = null, loopers = [];

function radialTexture(THREE, inner = 'rgba(255,255,255,1)', outer = 'rgba(255,255,255,0)', n = 256) {
  const c = document.createElement('canvas'); c.width = c.height = n; const x = c.getContext('2d');
  const g = x.createRadialGradient(n / 2, n / 2, 0, n / 2, n / 2, n / 2); g.addColorStop(0, inner); g.addColorStop(1, outer); x.fillStyle = g; x.fillRect(0, 0, n, n);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}
function hazeTexture(THREE, n = 512) {
  const c = document.createElement('canvas'); c.width = c.height = n; const x = c.getContext('2d');
  let s = 99; const r = () => (s = (s * 16807) % 2147483647) / 2147483647;
  for (let i = 0; i < 40; i++) { const cx = r() * n, cy = r() * n, rad = 60 + r() * 120; const g = x.createRadialGradient(cx, cy, 0, cx, cy, rad); g.addColorStop(0, `rgba(255,255,255,${.25 + r() * .25})`); g.addColorStop(1, 'rgba(255,255,255,0)'); x.fillStyle = g; x.fillRect(0, 0, n, n); }
  const t = new THREE.CanvasTexture(c); t.wrapS = t.wrapT = THREE.RepeatWrapping; return t;
}

function particles(THREE, P, W, H) {
  const n = P.count, pos = new Float32Array(n * 3), seed = new Float32Array(n);
  for (let i = 0; i < n; i++) { pos[i * 3] = (Math.random() - .5) * W; pos[i * 3 + 1] = 0; pos[i * 3 + 2] = (Math.random() - .5) * H; seed[i] = Math.random(); }
  const geo = new THREE.BufferGeometry(); geo.setAttribute('position', new THREE.BufferAttribute(pos, 3)); geo.setAttribute('aSeed', new THREE.BufferAttribute(seed, 1));
  const ember = P.kind === 'ember';
  const mat = new THREE.ShaderMaterial({
    uniforms: { uTime: { value: 0 }, uSize: { value: P.size }, uHeight: { value: P.height }, uSpeed: { value: P.speed }, uColor: { value: new THREE.Color(P.color) }, uEmber: { value: ember ? 1 : 0 } },
    transparent: true, depthWrite: false, blending: ember ? THREE.AdditiveBlending : THREE.NormalBlending, fog: false,
    vertexShader: `uniform float uTime,uSize,uHeight,uSpeed,uEmber; attribute float aSeed; varying float vA; varying float vS;
      void main(){ vec3 p=position; float s=aSeed; float life=fract(uTime*uSpeed*(0.25+0.75*fract(s*7.1))*0.08+s);
        float y = uEmber>0.5 ? life*uHeight : uHeight*(1.0-life);
        p.x += sin(uTime*0.6+s*40.0)*(0.6+s)+ (uEmber>0.5?0.0:uTime*0.4); p.z += cos(uTime*0.45+s*23.0)*(0.6+s); p.y = y;
        vA = smoothstep(0.0,0.12,life)*(1.0-smoothstep(0.7,1.0,life)); vS=s;
        vec4 mv=modelViewMatrix*vec4(p,1.0); float dist=-mv.z; vA *= smoothstep(1.5,7.0,dist);
        gl_PointSize = clamp(uSize*(0.5+fract(s*3.3))*(40.0/max(1.0,dist)) + 1.5, 0.0, 9.0); gl_Position=projectionMatrix*mv; }`,
    fragmentShader: `uniform vec3 uColor; uniform float uTime,uEmber; varying float vA; varying float vS;
      void main(){ float d=length(gl_PointCoord-0.5); float a=smoothstep(0.5,0.08,d)*vA; if(a<0.01) discard;
        float flick = uEmber>0.5 ? 0.6+0.4*sin(uTime*9.0+vS*60.0) : 0.35; vec3 c = uEmber>0.5 ? mix(uColor, vec3(1.0,0.85,0.5), smoothstep(0.3,0.0,d)) : uColor;
        gl_FragColor=vec4(c, a*flick*(uEmber>0.5?1.0:0.55)); }`,
  });
  const pts = new THREE.Points(geo, mat); pts.name = '__atmo_particles'; pts.frustumCulled = false; pts.userData.gizmo = true;
  loopers.push((t) => { mat.uniforms.uTime.value = t; });
  return pts;
}

function flood(THREE, F, g) {
  const grp = new THREE.Group(); grp.name = '__atmo_flood'; grp.position.set(...F.p);
  const light = new THREE.SpotLight(F.c, F.i, F.d, F.down ? .6 : .55, .6, 1.6); light.position.set(0, 0, 0); light.castShadow = false;
  const tgt = new THREE.Object3D(); const aim = F.down ? [0, -1, 0] : [Math.sin(F.yaw) * 6, -F.p[1], -Math.cos(F.yaw) * 6];
  tgt.position.set(...aim); grp.add(tgt); light.target = tgt; grp.add(light);
  const len = Math.hypot(...aim), coneMat = new THREE.MeshBasicMaterial({ color: F.c, transparent: true, opacity: F.down ? .05 : .04, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.DoubleSide, fog: false });
  const cone = new THREE.Mesh(new THREE.ConeGeometry(F.down ? 4.5 : 5.5, len, 24, 1, true), coneMat); cone.position.set(aim[0] / 2, aim[1] / 2, aim[2] / 2);
  cone.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), new THREE.Vector3(...aim).normalize().negate()); grp.add(cone);
  const inner = new THREE.Mesh(new THREE.ConeGeometry(F.down ? 2.2 : 2.8, len * .95, 20, 1, true), coneMat.clone()); inner.material.opacity = .05; inner.position.copy(cone.position); inner.quaternion.copy(cone.quaternion); grp.add(inner);
  const pool = new THREE.Mesh(new THREE.CircleGeometry(F.down ? 5 : 6.5, 32), new THREE.MeshBasicMaterial({ map: radialTexture(THREE), color: F.c, transparent: true, opacity: .22, blending: THREE.AdditiveBlending, depthWrite: false, fog: false }));
  pool.rotation.x = -Math.PI / 2; pool.position.set(aim[0], -F.p[1] + .06, aim[2]); grp.add(pool);
  const glare = new THREE.Sprite(new THREE.SpriteMaterial({ map: radialTexture(THREE), color: F.c, transparent: true, opacity: .8, blending: THREE.AdditiveBlending, depthWrite: false, fog: false })); glare.scale.setScalar(1.8); grp.add(glare);
  grp.traverse((o) => { o.userData.gizmo = true; });
  g.add(grp);
}

export function applyAtmosphere(stage, THREE, kitId, group, W, H) {
  const P = PRESETS[kitId]; if (!P) return;
  const scene = stage._scene, renderer = stage._renderer;
  const hemi = scene.children.find((o) => o.isHemisphereLight), key = stage._key, fill = scene.children.find((o) => o.isDirectionalLight && o !== key);
  if (!saved) saved = { fog: scene.fog, bg: stage.style.getPropertyValue('--stage-bg'), tone: renderer.toneMapping, exp: renderer.toneMappingExposure, hemi: [hemi.color.getHex(), hemi.groundColor.getHex(), hemi.intensity], key: [key.color.getHex(), key.intensity, key.position.toArray()], fill: fill ? [fill.color.getHex(), fill.intensity] : null };
  scene.fog = new THREE.FogExp2(P.fog, P.density);
  stage.style.setProperty('--stage-bg', '#' + new THREE.Color(P.fog).getHexString());
  renderer.toneMapping = THREE.ACESFilmicToneMapping; renderer.toneMappingExposure = P.exposure;
  hemi.color.setHex(P.hemi[0]); hemi.groundColor.setHex(P.hemi[1]); hemi.intensity = P.hemi[2];
  key.color.setHex(P.key[0]); key.intensity = P.key[1]; key.position.set(...P.key[2]); key.shadow.bias = -.0005; key.shadow.normalBias = .04;
  if (fill) { fill.color.setHex(P.fill[0]); fill.intensity = P.fill[1]; }
  // Point lights.
  for (const L of P.lights) { const pl = new THREE.PointLight(L.c, L.i, L.d, 2); pl.position.set(...L.p); pl.name = '__atmo_light'; pl.userData.gizmo = true; group.add(pl); }
  for (const F of P.floods) flood(THREE, F, group);
  for (const Hs of P.hotspots) { const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: radialTexture(THREE), color: Hs.c, transparent: true, opacity: .9, blending: THREE.AdditiveBlending, depthWrite: false, fog: false })); s.position.set(...Hs.p); s.scale.setScalar(Hs.r * 2); s.name = '__atmo_hotspot'; s.userData.gizmo = true; group.add(s); }
  // Particles + haze.
  group.add(particles(THREE, P.particles, W + 10, H + 10));
  const hz = hazeTexture(THREE);
  P.haze.layers.forEach(([y, op], i) => {
    const t = hz.clone(); t.needsUpdate = true; t.repeat.set(3 + i, 2 + i);
    const m = new THREE.Mesh(new THREE.PlaneGeometry(W + 20, H + 20), new THREE.MeshBasicMaterial({ map: t, color: P.haze.color, transparent: true, opacity: op, depthWrite: false, fog: false, blending: THREE.NormalBlending }));
    m.rotation.x = -Math.PI / 2; m.position.y = y; m.name = '__atmo_haze' + i; m.userData.gizmo = true; group.add(m);
    loopers.push((tm) => { t.offset.set(tm * .004 * (i + 1), tm * .0015 * (i % 2 ? -1 : 1)); });
  });
  if (!applyAtmosphere._raf) { const t0 = performance.now(); const tick = () => { const t = (performance.now() - t0) / 1000; for (const f of loopers) f(t); applyAtmosphere._raf = requestAnimationFrame(tick); }; applyAtmosphere._raf = requestAnimationFrame(tick); }
}

export function clearAtmosphere(stage, THREE) {
  loopers = [];
  if (!saved) return;
  const scene = stage._scene, renderer = stage._renderer;
  const hemi = scene.children.find((o) => o.isHemisphereLight), key = stage._key, fill = scene.children.find((o) => o.isDirectionalLight && o !== key);
  scene.fog = saved.fog || null; stage.style.setProperty('--stage-bg', saved.bg || '#0D1119');
  renderer.toneMapping = saved.tone; renderer.toneMappingExposure = saved.exp;
  hemi.color.setHex(saved.hemi[0]); hemi.groundColor.setHex(saved.hemi[1]); hemi.intensity = saved.hemi[2];
  key.color.setHex(saved.key[0]); key.intensity = saved.key[1]; key.position.set(...saved.key[2]);
  if (fill && saved.fill) { fill.color.setHex(saved.fill[0]); fill.intensity = saved.fill[1]; }
}
