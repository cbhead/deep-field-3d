/**
 * Deep Field 3D — studio image-based lighting for every viewer.
 * The stage ships with lights but no environment, so PBR metal has nothing to
 * reflect and reads as flat 2008-era plastic. This builds a small emissive
 * light-box scene, pre-filters it with PMREM and installs it as
 * scene.environment, then rebalances the direct lights and switches the
 * renderer to ACES filmic. Idempotent per stage.
 */
export function applyStudio(stage, THREE, o = {}) {
  if (stage.__studio) return stage.__studio;
  const r = stage._renderer, sc = stage._scene;
  const es = new THREE.Scene(); es.background = new THREE.Color(0x07090d);
  const lamp = (w, h, c, i, pos, rot) => { const m = new THREE.Mesh(new THREE.PlaneGeometry(w, h), new THREE.MeshBasicMaterial({ color: new THREE.Color(c).multiplyScalar(i), side: THREE.DoubleSide })); m.position.set(...pos); m.rotation.set(...rot); es.add(m); };
  lamp(6, 2.4, 0xffffff, 6, [0, 3, 0], [Math.PI / 2, 0, 0]);          // soft top box
  lamp(1.2, 5, 0xffe7c8, 4, [-4, 1, 0], [0, Math.PI / 2, .2]);          // warm left strip
  lamp(1.2, 5, 0xbfe4ff, 3.5, [4, .6, -1], [0, -Math.PI / 2, -.2]);   // cool right strip
  lamp(8, 1, 0x9fb7ff, 1.2, [0, .2, 5], [0, Math.PI, 0]);              // rim from behind
  lamp(12, 12, 0x2a3140, .6, [0, -2.5, 0], [-Math.PI / 2, 0, 0]);        // dark floor bounce
  const pm = new THREE.PMREMGenerator(r), tex = pm.fromScene(es, .05).texture; pm.dispose();
  sc.environment = tex; sc.environmentIntensity = o.intensity ?? 1.0;
  r.toneMapping = THREE.ACESFilmicToneMapping; r.toneMappingExposure = o.exposure ?? 1.05;
  if (stage._key) stage._key.intensity = o.key ?? 1.1;
  sc.traverse((n) => { if (n.isHemisphereLight) n.intensity = o.hemi ?? .3; });
  return (stage.__studio = { texture: tex });
}
