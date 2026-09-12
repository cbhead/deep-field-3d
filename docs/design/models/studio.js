/**
 * Deep Field 3D — studio image-based lighting for every viewer.
 *
 * The stage ships with lights but no environment, so PBR metal has nothing to
 * reflect. The first version of this file fixed that with five flat, uniformly
 * lit rectangles — which is better than nothing, but a uniform emitter gives
 * metal a flat featureless highlight, and flat highlights are most of why a
 * correct model still photographs as CG.
 *
 * A real studio has structure: sources with falloff across their own surface,
 * a few small very bright kickers that throw the crisp glints metal needs,
 * dark gaps between the sources, and a graded floor-to-sky wrap. That
 * structure is what a curved metal surface smears along its length — it is
 * what makes a barrel look turned and a receiver look anodised.
 *
 * Idempotent per stage.
 */

/** A soft source: radial falloff across the panel, so it reflects as a real
 *  softbox with a hot centre and feathered edges instead of a white slab. */
function softTex(THREE, feather = .55, squash = 1) {
  const c = document.createElement('canvas'); c.width = c.height = 128;
  const x = c.getContext('2d');
  const g = x.createRadialGradient(64, 64, 0, 64, 64, 64);
  g.addColorStop(0, '#fff');
  g.addColorStop(feather, 'rgba(255,255,255,.82)');
  g.addColorStop(1, 'rgba(255,255,255,0)');
  x.fillStyle = g;
  x.save(); x.translate(64, 64); x.scale(1, squash); x.translate(-64, -64);
  x.fillRect(0, 0, 128, 128); x.restore();
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace; return t;
}

/** Vertical gradient wrap — dark underfoot, lifting through a horizon band to
 *  a cool sky. Gives every curved surface a direction to grade along. */
function skyTex(THREE) {
  const c = document.createElement('canvas'); c.width = 8; c.height = 256;
  const x = c.getContext('2d');
  const g = x.createLinearGradient(0, 0, 0, 256);
  g.addColorStop(0, '#4d586b');      // zenith
  g.addColorStop(.34, '#2b3242');
  g.addColorStop(.49, '#11151d');
  g.addColorStop(.52, '#1a2029');    // horizon break
  g.addColorStop(.75, '#0a0d13');
  g.addColorStop(1, '#05070a');      // floor
  x.fillStyle = g; x.fillRect(0, 0, 8, 256);
  const t = new THREE.CanvasTexture(c); t.colorSpace = THREE.SRGBColorSpace;
  t.mapping = THREE.EquirectangularReflectionMapping; return t;
}

export function applyStudio(stage, THREE, o = {}) {
  if (stage.__studio) return stage.__studio;
  const r = stage._renderer, sc = stage._scene;
  const es = new THREE.Scene();

  // graded wrap instead of a flat black void
  const sky = skyTex(THREE);
  es.add(new THREE.Mesh(
    new THREE.SphereGeometry(30, 32, 24),
    new THREE.MeshBasicMaterial({ map: sky, side: THREE.BackSide }),
  ));

  const soft = softTex(THREE, .5), softWide = softTex(THREE, .38, .62);
  const lamp = (w, h, c, i, pos, rot, map) => {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({
      color: new THREE.Color(c).multiplyScalar(i), side: THREE.DoubleSide,
      map: map || null, transparent: !!map, depthWrite: false,
    }));
    m.position.set(...pos); m.rotation.set(...rot); es.add(m);
  };

  // Key: large feathered box above and slightly forward.
  lamp(7, 3.2, 0xfff6ec, 9.2, [0, 3.4, -.8], [Math.PI / 2, 0, 0], softWide);
  // Broad frontal wrap on the side the viewer actually orbits from.
  lamp(6, 4.2, 0xeef3ff, 2.9, [-5.2, 1.4, 1.6], [0, 1.14, .1], softWide);
  // Fill: tall cool strip camera-left, soft.
  lamp(1.6, 5.5, 0xd7e6ff, 3.4, [-4.2, 1.1, .4], [0, Math.PI / 2, .18], soft);
  // Warm bounce low camera-right — separates the two blacks by temperature.
  lamp(2.2, 3.2, 0xffe2c2, 2.1, [3.9, -.5, .8], [0, -Math.PI / 2, -.12], soft);
  // Kickers: small and very bright. These are what throw the crisp glints
  // along a machined edge; large soft sources alone never will.
  lamp(.34, 1.5, 0xffffff, 26, [2.4, 2.1, -2.6], [0, -2.3, .5], soft);
  lamp(.26, 1.1, 0xeaf2ff, 20, [-2.8, 1.7, -2.4], [0, 2.3, -.45], soft);
  lamp(.5, .5, 0xffffff, 14, [.6, -1.4, 3.2], [0, Math.PI, 0], soft);
  // Dark occluders: gaps in the reflection field. Without somewhere dark to
  // reflect, metal loses its contrast range and reads as plastic.
  const dark = new THREE.MeshBasicMaterial({ color: 0x000000, side: THREE.DoubleSide });
  for (const [w, h, p, rt] of [[3, 6, [-1.9, 1.2, -4.4], [0, .3, 0]], [2.4, 6, [2.6, 1.4, -4.2], [0, -.35, 0]], [9, .8, [0, 1.9, 0], [Math.PI / 2, 0, 0]]]) {
    const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), dark); m.position.set(...p); m.rotation.set(...rt); es.add(m);
  }
  // Graded floor bounce.
  lamp(14, 14, 0x2c3444, .55, [0, -2.6, 0], [-Math.PI / 2, 0, 0], soft);

  const pm = new THREE.PMREMGenerator(r);
  pm.compileEquirectangularShader();
  const tex = pm.fromScene(es, .02).texture;   // low sigma keeps the kickers sharp
  pm.dispose();

  sc.environment = tex; sc.environmentIntensity = o.intensity ?? 1.0;
  r.toneMapping = THREE.ACESFilmicToneMapping; r.toneMappingExposure = o.exposure ?? 1.05;
  r.setPixelRatio(Math.min(2, window.devicePixelRatio || 1));
  /* Contact darkening. Image-based light alone leaves every junction — guard to
     receiver, handguard to barrel nut, magazine to magwell — equally lit, and
     parts that never darken where they meet read as decals on each other. The
     stage's key ships with a scene-sized shadow frustum, which at weapon scale
     quantises to nothing; tightening it to a 60 cm box buys real self-shadowing
     at the same map size. */
  if (stage._key) {
    stage._key.castShadow = true;
    const sh = stage._key.shadow;
    sh.mapSize.set(2048, 2048);
    sh.camera.near = .02; sh.camera.far = 3.2;
    sh.camera.left = -.36; sh.camera.right = .36;
    sh.camera.top = .36; sh.camera.bottom = -.36;
    sh.bias = -.00035; sh.normalBias = .0012;
    sh.camera.updateProjectionMatrix();
    sh.needsUpdate = true;
  }
  r.shadowMap.enabled = true; r.shadowMap.type = THREE.PCFShadowMap;
  sc.traverse((n) => { if (n.isMesh) { n.castShadow = true; n.receiveShadow = true; } });
  if (stage._key) stage._key.intensity = o.key ?? 1.1;
  sc.traverse((n) => { if (n.isHemisphereLight) n.intensity = o.hemi ?? .3; });
  return (stage.__studio = { texture: tex });
}
