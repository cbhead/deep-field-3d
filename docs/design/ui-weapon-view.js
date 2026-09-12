/**
 * Deep Field 3D — live weapon render for UI screens.
 * Any element with `data-weapon="<platform id>"` (and optional
 * `data-build="slot:attachment,slot:attachment"`), `data-attachment="<id>"`
 * or `data-ammo="<id>"` gets a transparent WebGL canvas showing the real model — the same build code the Gunsmith
 * viewer uses, so the armory shows the gun you actually built.
 */
import * as THREE from 'three';
import { makeWeaponKit, WEAPONS, ATTACHMENTS, AMMO } from './models/weapons.js';
import { makeHeroKit, HEROES } from './models/heroes.js';
import { applyStudio } from './models/studio.js';

export function assembleWeapon(K, id, build = {}) {
  const w = WEAPONS.find((x) => x.id === id), model = w.build(K);
  for (const [slot, attId] of Object.entries(build)) {
    const att = ATTACHMENTS.find((a) => a.id === attId); if (!att) continue;
    const hide = (n) => { const o = model.getObjectByName(n); if (o) o.visible = false; };
    if (slot === 'barrel') hide(`${id}_barrel`);
    if (slot === 'magazine') { hide(`${id}_magazine`); hide(`${id}_mag_tube`); }
    if (slot === 'stock') hide(`${id}_stock`);
    let mnt = model.getObjectByName(`${id}_mount_${slot}`);
    if (slot === 'muzzle' && build.barrel) mnt = model.getObjectByName('attach_mount_muzzle') || mnt;
    mnt?.add(att.build(K, id));
  }
  return model;
}

let renderer, K, env = null, active = [];
function ctx() {
  if (renderer) return;
  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true, powerPreference: 'high-performance' });
  renderer.setPixelRatio(Math.min(2, devicePixelRatio || 1)); renderer.setClearColor(0x000000, 0);
  renderer.shadowMap.enabled = true;
  K = makeWeaponKit(THREE); Object.assign(K.mats, makeHeroKit(THREE).mats);
}

export function unmountWeaponViews() { for (const v of active) { cancelAnimationFrame(v.raf); v.canvas.remove(); } active = []; }

export function mountWeaponViews(root) {
  unmountWeaponViews(); ctx();
  for (const host of root.querySelectorAll('[data-weapon],[data-attachment],[data-ammo],[data-hero]')) {
    const build = Object.fromEntries((host.dataset.build || '').split(',').filter(Boolean).map((p) => p.split(':')));
    const kind = host.dataset.weapon ? 'weapon' : host.dataset.attachment ? 'attachment' : host.dataset.hero ? 'hero' : 'ammo';
    const scene = new THREE.Scene(), cam = new THREE.PerspectiveCamera(28, 1, .01, 20);
    const key = new THREE.DirectionalLight(0xfff1dc, 1.6); key.position.set(-1.2, 1.6, .8); key.castShadow = true; key.shadow.mapSize.set(1024, 1024);
    const fill = new THREE.DirectionalLight(0xbfd4ff, .5); fill.position.set(1.2, .4, -1.0);
    scene.add(key, fill, new THREE.HemisphereLight(0x9fb4ff, 0x14171d, .3));
    if (!env) env = applyStudio({ _renderer: renderer, _scene: scene, _key: key }, THREE, { intensity: .9, exposure: 1.1 }).texture; // one PMREM for every view
    scene.environment = env; scene.environmentIntensity = .9;
    const model = kind === 'weapon' ? assembleWeapon(K, host.dataset.weapon, build)
      : kind === 'attachment' ? ATTACHMENTS.find((a) => a.id === host.dataset.attachment).build(K, host.dataset.host || 'rifle')
      : kind === 'hero' ? HEROES.find((h) => h.id === 'hero_' + host.dataset.hero).build(K)
      : AMMO.find((a) => a.id === 'ammo_' + host.dataset.ammo).build(K);
    if (kind === 'ammo') model.rotation.x = Math.PI / 2; // rounds are turned along −Z (the bore); stand them tip-up for the card
    model.traverse((o) => { if (o.isMesh) { o.castShadow = true; o.receiveShadow = true; } });
    const box = new THREE.Box3().setFromObject(model), c = box.getCenter(new THREE.Vector3()), sz = box.getSize(new THREE.Vector3());
    const pivot = new THREE.Group(); pivot.position.copy(c); model.position.sub(c); pivot.add(model); scene.add(pivot);
    const canvas = renderer.domElement.cloneNode(false); // one shared GL context, blitted per view
    Object.assign(canvas.style, { position: 'absolute', inset: '0', width: '100%', height: '100%', pointerEvents: 'none' });
    host.appendChild(canvas); const c2 = canvas.getContext('2d');
    const view = { canvas, raf: 0 };
    const draw = (t) => {
      const w = host.clientWidth || 700, h = host.clientHeight || 250, pr = renderer.getPixelRatio();
      if (canvas.width !== w * pr) { canvas.width = w * pr; canvas.height = h * pr; }
      renderer.setSize(w, h, false); cam.aspect = w / h;
      const vfov = cam.fov * Math.PI / 180, hfov = 2 * Math.atan(Math.tan(vfov / 2) * cam.aspect);
      // Left three-quarter, slightly above; a slow sway keeps the highlights moving.
      const spin = kind === 'weapon' ? Math.sin(t * .00035) * .22 : kind === 'ammo' ? Math.sin(t * .0005) * .35 : kind === 'hero' ? Math.sin(t * .0003) * .3 : t * .0004, yaw = spin;
      const dir = (kind === 'weapon' ? new THREE.Vector3(-1.4, .5, -.55) : kind === 'ammo' ? new THREE.Vector3(-.3, .55, -1) : kind === 'hero' ? new THREE.Vector3(.35, .18, -1) : new THREE.Vector3(-1.0, .55, -1.0)).normalize().applyAxisAngle(new THREE.Vector3(0, 1, 0), yaw);
      // Fit the projected footprint (long thin object), not the bounding sphere.
      const a = Math.atan2(Math.abs(dir.x), Math.abs(dir.z)), ext = sz.z * Math.sin(a) + sz.x * Math.cos(a) + sz.y * .2;
      const d = kind === 'hero' ? Math.max(sz.y * .5 * 1.12 / Math.tan(vfov / 2), Math.hypot(sz.x, sz.z) * .5 * 1.2 / Math.tan(hfov / 2)) + .3
        : kind === 'weapon' ? Math.max((ext * .5 * .98) / Math.tan(hfov / 2), (sz.y * .5 * 1.7) / Math.tan(vfov / 2)) + sz.x * .4
        : Math.max(Math.hypot(sz.x, sz.z) * .5 * 1.05 / Math.tan(hfov / 2), sz.y * .5 * 1.1 / Math.tan(vfov / 2)) + Math.max(sz.x, sz.y, sz.z) * .15;
      cam.position.copy(c).addScaledVector(dir, d); cam.lookAt(c); cam.updateProjectionMatrix();
      renderer.render(scene, cam);
      c2.clearRect(0, 0, canvas.width, canvas.height); c2.drawImage(renderer.domElement, 0, 0, w * pr, h * pr, 0, 0, canvas.width, canvas.height);
      view.raf = requestAnimationFrame(draw);
    };
    view.raf = requestAnimationFrame(draw); active.push(view);
  }
}
