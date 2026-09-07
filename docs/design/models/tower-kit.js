/**
 * Deep Field 3D — tower model kit.
 *
 * Shared materials + the greeble helpers every tower model is built from.
 * Call makeKit(THREE) once per page; hand the returned kit to a model
 * module's build(kit, levels).
 *
 * Conventions every model must hold to:
 *  - metres, y-up, base resting at y = 0, centred on the socket origin
 *  - rig groups named <id>_yaw / <id>_pitch, rest pose bore-level along +Z
 *  - projectile spawn marker named <id>_muzzle
 *  - upgrade attachments named <id>_up_<path>_l<level> for EVERY level 2–10,
 *    hidden until earned. L4 / L7 / L10 are the breakpoint rungs (mechanic
 *    changes, bigger hardware); the rest are smaller additive increments.
 */
export function makeKit(THREE) {
  const D = Math.PI / 180;
  const mats = {};
  const mat = (name, color, o = {}) => {
    const m = new THREE.MeshStandardMaterial({ color, roughness: .62, metalness: .3, ...o });
    m.name = name; mats[name] = m; return m;
  };
  mat('steel_hull',  0x606a7c, { roughness: .45, metalness: .75 });
  mat('steel_plate', 0x9aa6b7, { roughness: .38, metalness: .8 });
  mat('chassis',     0x2b3a5c, { roughness: .55, metalness: .5 });
  mat('trim',        0x1b2436, { roughness: .6, metalness: .4 });
  mat('chrome',      0xc3ccd8, { roughness: .18, metalness: .95 });
  mat('brass',       0xb08a3e, { roughness: .3, metalness: .9 });
  mat('rubber',      0x14181f, { roughness: .95, metalness: .0 });
  mat('hazard',      0xd8a13a, { roughness: .5, metalness: .2 });
  mat('optic_glass', 0x14314a, { roughness: .08, metalness: .2 });
  const energy = (name, hex) => mat(name, hex, {
    roughness: .3, metalness: .06, emissive: new THREE.Color(hex), emissiveIntensity: .95,
  });
  // One hue per tower, spaced ~25–60° apart so projectiles/effects stay
  // tellable at a glance once they inherit these colours.
  energy('energy_rail',  0x2b5cff);   // Lance      — cobalt        228°
  energy('energy_flak',  0x22d3ee);   // Skywatch   — cyan          188°
  energy('energy_scan',  0x7fe65a);   // Detector   — green         105°
  energy('energy_fuse',  0xf0c83a);   // Nova       — gold           48°
  energy('energy_buff',  0xff6f1a);   // Overclock  — orange         22°
  energy('energy_beam',  0xff2e4a);   // Filament   — crimson       350°
  energy('energy_arc',   0xf05ae6);   // Arc        — plasma magenta 305°
  energy('energy_field', 0x9b5be8);   // Singularity— violet        265°
  mat('ceramic',     0xd9d2c3, { roughness: .38, metalness: .04 });
  mat('concrete',    0x7a7f88, { roughness: .92, metalness: .02 });

  /* ── primitive refinement ─────────────────────────────────────────────
   * Every mesh built through part() is upgraded on the way in: boxes become
   * chamfered blocks (26 faces, highlights catch on every edge), full round
   * cylinders/cones become turned parts with a chamfered rim at 32 segments,
   * spheres and tori get real curvature, and anything deliberately low-poly
   * (≤ 6 radial segments: hex bolts, hex plates, polyhedra) keeps crisp flat
   * facets without needing flatShading on the material. */
  const refined = new WeakMap();
  const flat = (geo) => { const g = geo.index ? geo.toNonIndexed() : geo; g.computeVertexNormals(); return g; };
  function chamferBox(w, h, d) {
    const b = Math.min(w, h, d) * .18;
    const s = new THREE.Shape();
    s.moveTo(-w / 2 + b, -h / 2); s.lineTo(w / 2 - b, -h / 2); s.lineTo(w / 2, -h / 2 + b); s.lineTo(w / 2, h / 2 - b);
    s.lineTo(w / 2 - b, h / 2); s.lineTo(-w / 2 + b, h / 2); s.lineTo(-w / 2, h / 2 - b); s.lineTo(-w / 2, -h / 2 + b); s.closePath();
    const g = new THREE.ExtrudeGeometry(s, { depth: d - 2 * b, bevelEnabled: true, bevelThickness: b, bevelSize: b, bevelSegments: 1, curveSegments: 1 });
    g.translate(0, 0, -(d / 2 - b)); return g;
  }
  function turned(rt, rb, h) {
    const c = Math.min(rt || rb, rb || rt, h) * .14, pts = [new THREE.Vector2(0, -h / 2)];
    if (rb > 0) pts.push(new THREE.Vector2(Math.max(rb - c, 0), -h / 2), new THREE.Vector2(rb, -h / 2 + c)); else pts.push(new THREE.Vector2(0, -h / 2));
    if (rt > 0) pts.push(new THREE.Vector2(rt, h / 2 - c), new THREE.Vector2(Math.max(rt - c, 0), h / 2));
    pts.push(new THREE.Vector2(0, h / 2));
    return new THREE.LatheGeometry(pts, 32);
  }
  function refine(geo) {
    if (refined.has(geo)) return refined.get(geo);
    const p = geo.parameters || {}, t = geo.type; let out = geo;
    const full = (p.thetaLength === undefined) || Math.abs(p.thetaLength - Math.PI * 2) < 1e-6;
    if (t === 'BoxGeometry') { const m = Math.min(p.width, p.height, p.depth); out = m > .0008 && (p.widthSegments || 1) === 1 ? chamferBox(p.width, p.height, p.depth) : geo; }
    else if (t === 'CylinderGeometry' || t === 'ConeGeometry') {
      const rt = t === 'ConeGeometry' ? 0 : p.radiusTop, rb = p.radiusBottom, rs = p.radialSegments ?? 32;
      if (!full) out = geo;
      else if (rs <= 6) out = flat(geo);
      else if (p.openEnded) out = new THREE.CylinderGeometry(rt, rb, p.height, 32, 1, true);
      else out = turned(rt, rb, p.height);
    }
    else if (t === 'SphereGeometry') out = new THREE.SphereGeometry(p.radius, Math.max(p.widthSegments, 24), Math.max(p.heightSegments, 16), p.phiStart, p.phiLength, p.thetaStart, p.thetaLength);
    else if (t === 'TorusGeometry') out = new THREE.TorusGeometry(p.radius, p.tube, Math.max(p.radialSegments, 12), Math.max(p.tubularSegments, 48), p.arc);
    else if (t === 'CapsuleGeometry') out = new THREE.CapsuleGeometry(p.radius, p.length, Math.max(p.capSegments, 4), Math.max(p.radialSegments, 16));
    refined.set(geo, out); return out;
  }

  const part = (name, geo, material, pos = [0, 0, 0], rot = [0, 0, 0]) => {
    const m = new THREE.Mesh(refine(geo), material);
    m.name = name; m.position.set(...pos); m.rotation.set(...rot);
    return m;
  };
  const box = (w, h, d) => new THREE.BoxGeometry(w, h, d);
  const cyl = (rt, rb, h, s = 24) => new THREE.CylinderGeometry(rt, rb, h, s);
  const tor = (r, t, rs = 8, ts = 28) => new THREE.TorusGeometry(r, t, rs, ts);
  const grp = (name, pos = [0, 0, 0]) => {
    const g = new THREE.Group(); g.name = name; g.position.set(...pos); return g;
  };

  const BOLT = new THREE.CylinderGeometry(.045, .045, .034, 6);
  const TOOTH = new THREE.BoxGeometry(.052, .05, .085);

  function boltRing(name, radius, count, y = 0, material = mats.chrome) {
    const g = grp(name, [0, y, 0]);
    for (let i = 0; i < count; i++) {
      const a = (i / count) * Math.PI * 2;
      const m = new THREE.Mesh(BOLT, material);
      m.name = name + '_' + i;
      m.position.set(Math.cos(a) * radius, 0, Math.sin(a) * radius);
      g.add(m);
    }
    return g;
  }

  /** Slew ring gear — what makes a yaw axis legible as machinery. */
  function ringGear(name, radius, teeth) {
    const g = grp(name);
    g.add(part(name + '_race', cyl(radius, radius, .07, 40), mats.brass));
    for (let i = 0; i < teeth; i++) {
      const a = (i / teeth) * Math.PI * 2;
      const m = new THREE.Mesh(TOOTH, mats.brass);
      m.name = name + '_tooth' + i;
      m.position.set(Math.cos(a) * (radius + .03), 0, Math.sin(a) * (radius + .03));
      m.rotation.y = -a;
      g.add(m);
    }
    return g;
  }

  /** Hydraulic ram with clevis ends, chromed rod, rubber wiper boot.
   *  Keep both ends inside ONE rig group so it never stretches at a pivot. */
  function hydraulic(name, from, to, gauge = 1) {
    const a = new THREE.Vector3(...from), b = new THREE.Vector3(...to);
    const len = a.distanceTo(b);
    const g = grp(name, from);
    g.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize());
    const body = len * .58, rod = len * .5;
    g.add(part(name + '_clevis_a', box(.09 * gauge, .07, .07), mats.steel_plate, [0, .03, 0]));
    g.add(part(name + '_body', cyl(.055 * gauge, .055 * gauge, body, 16), mats.chassis, [0, .05 + body / 2, 0]));
    g.add(part(name + '_boot', cyl(.062 * gauge, .062 * gauge, .07, 12), mats.rubber, [0, .05 + body, 0]));
    g.add(part(name + '_rod', cyl(.03 * gauge, .03 * gauge, rod, 12), mats.chrome, [0, .05 + body + rod / 2, 0]));
    g.add(part(name + '_clevis_b', box(.08 * gauge, .06, .06), mats.steel_plate, [0, len - .02, 0]));
    return g;
  }

  const cableRun = (name, points, radius = .022, material = mats.rubber) => part(
    name,
    new THREE.TubeGeometry(new THREE.CatmullRomCurve3(points.map((p) => new THREE.Vector3(...p))), 20, radius, 7, false),
    material);

  function finStack(name, count, spacing, w, h, thickness = .014, material = mats.steel_plate) {
    const g = grp(name);
    const geo = box(w, h, thickness);
    for (let i = 0; i < count; i++) {
      const m = new THREE.Mesh(geo, material);
      m.name = name + '_' + i; m.position.z = i * spacing;
      g.add(m);
    }
    return g;
  }

  function louvres(name, w, h, count, material = mats.trim) {
    const g = grp(name);
    const slat = box(w, h / count * .7, .03);
    for (let i = 0; i < count; i++) {
      const m = new THREE.Mesh(slat, material);
      m.name = name + '_' + i;
      m.position.y = -h / 2 + (i + .5) * (h / count);
      m.rotation.x = 28 * D;
      g.add(m);
    }
    return g;
  }

  const panelPlate = (name, w, h, pos, rot = [0, 0, 0], material = mats.steel_plate) =>
    part(name, box(w, h, .012), material, pos, rot);

  function hazardStripes(name, w, h, pos, rot = [0, 0, 0], count = 4) {
    const g = grp(name, pos);
    g.rotation.set(...rot);
    const stripe = box(w / count * .5, h, .01);
    for (let i = 0; i < count; i++) {
      const m = new THREE.Mesh(stripe, mats.hazard);
      m.name = name + '_' + i;
      m.position.x = -w / 2 + (i + .5) * (w / count);
      m.rotation.z = 22 * D;
      g.add(m);
    }
    return g;
  }

  /** Projectile spawn / tracer origin. The bind layer reads this node. */
  function muzzle(id, pos) {
    const g = grp(id + '_muzzle', pos);
    g.userData.role = 'projectileSpawn';
    const ring = part(id + '_muzzle_marker', tor(.13, .018, 6, 20), mats.steel_plate);
    ring.visible = false; ring.userData.gizmo = true;
    g.add(ring);
    return g;
  }

  /** Socket foot — static, never rotates. Sized to the 2.1 m socket pads. */
  function foot(id, energyMat) {
    const g = grp(id + '_foot');
    g.add(part(id + '_baseplate', cyl(1.02, 1.08, .12, 6), mats.chassis, [0, .06, 0]));
    g.add(part(id + '_baseplate_bevel', cyl(.94, 1.02, .07, 6), mats.steel_hull, [0, .155, 0]));
    g.add(part(id + '_flange', cyl(.80, .80, .06, 32), mats.steel_plate, [0, .215, 0]));
    g.add(boltRing(id + '_flange_bolts', .72, 12, .255));
    g.add(part(id + '_collar', cyl(.64, .74, .14, 24), mats.steel_hull, [0, .30, 0]));
    g.add(part(id + '_seam', tor(.90, .03, 8, 40), energyMat, [0, .155, 0], [90 * D, 0, 0]));
    for (let i = 0; i < 3; i++) {
      const a = (i / 3) * Math.PI * 2 + 30 * D, x = Math.cos(a) * .90, z = Math.sin(a) * .90;
      g.add(part(id + '_anchor' + i, cyl(.10, .12, .20, 10), mats.trim, [x, .10, z]));
      g.add(part(id + '_anchor_washer' + i, cyl(.15, .15, .028, 10), mats.steel_plate, [x, .205, z]));
      g.add(part(id + '_anchor_nut' + i, cyl(.062, .062, .05, 6), mats.chrome, [x, .243, z]));
    }
    g.add(part(id + '_junction', box(.26, .20, .16), mats.chassis, [.60, .16, -.44]));
    g.add(part(id + '_junction_hatch', box(.16, .11, .012), mats.steel_plate, [.60, .17, -.36]));
    g.add(cableRun(id + '_conduit', [[.60, .25, -.44], [.44, .29, -.35], [.26, .23, -.23]], .026));
    g.add(part(id + '_gland', cyl(.05, .062, .06, 10), mats.brass, [.26, .25, -.23]));
    return g;
  }

  /** Deployable stabiliser outriggers — the shared "this thing got heavier"
   *  cue, used by more than one tower's range path. */
  function outriggers(name, reach = .52) {
    const g = grp(name);
    for (let i = 0; i < 3; i++) {
      const a = (i / 3) * Math.PI * 2 + 30 * D;
      const leg = grp(name + '_leg' + i);
      leg.rotation.y = -a;
      leg.add(part(name + '_arm' + i, box(.13, .10, reach), mats.steel_hull, [0, .22, reach / 2]));
      leg.add(part(name + '_pad' + i, cyl(.17, .20, .07, 12), mats.trim, [0, .05, reach]));
      leg.add(part(name + '_jack' + i, cyl(.045, .045, .22, 10), mats.chrome, [0, .14, reach]));
      leg.position.set(Math.cos(a) * .62, 0, Math.sin(a) * .62);
      leg.rotation.y = Math.atan2(Math.sin(a), Math.cos(a)) * -1 + Math.PI / 2;
      g.add(leg);
    }
    return g;
  }

  return {
    THREE, D, mats, part, box, cyl, tor, grp, refine,
    boltRing, ringGear, hydraulic, cableRun, finStack, louvres,
    panelPlate, hazardStripes, muzzle, foot, outriggers,
  };
}

/** Level → visibility. Every upgrade group is built once and toggled, so a
 *  fully-levelled tower and a fresh one are the same mesh set in memory. */
export function applyLevels(model, tower, levels) {
  for (const path of tower.paths) {
    const lvl = levels[path.id] ?? 1;
    for (let m = 2; m <= 10; m++) {
      const g = model.getObjectByName(`${tower.id}_up_${path.id}_l${m}`);
      if (g) g.visible = lvl >= m;
    }
  }
  tower.cue?.(model, levels);
}

/** Attach an upgrade increment: builds the named group under `parent`.
 *  Every rung must carry the tower's accent so progress reads as growing
 *  colour, not just growing metal: if the filled group has no energy-material
 *  part, a small powered status stud is added on its largest piece. */
export function makeUp(K, id, accent) {
  const { THREE } = K;
  return (path, lvl, parent, pos, fill) => {
    const g = K.grp(`${id}_up_${path}_l${lvl}`, pos);
    fill(g);
    if (accent) {
      let hasAccent = false, biggest = null, vol = -1;
      g.traverse((o) => {
        if (!o.isMesh) return;
        if (o.material?.name?.startsWith('energy_')) hasAccent = true;
        o.geometry.computeBoundingBox();
        const b = o.geometry.boundingBox, s = new THREE.Vector3(); b.getSize(s);
        const v = s.x * s.y * s.z;
        if (v > vol) { vol = v; biggest = o; }
      });
      if (!hasAccent && biggest) {
        g.updateMatrixWorld(true);
        const b = new THREE.Box3().setFromObject(biggest);
        const c = b.getCenter(new THREE.Vector3());
        const ring = /Torus|Tube/.test(biggest.geometry.type);
        const p = ring ? new THREE.Vector3(b.max.x, c.y, c.z) : new THREE.Vector3(c.x, b.max.y, c.z);
        g.worldToLocal(p);
        const stud = K.part(`${id}_up_${path}_l${lvl}_stud`, new THREE.CylinderGeometry(.028, .028, .022, 8), accent, p.toArray());
        if (ring) stud.rotation.z = Math.PI / 2;
        g.add(stud);
      }
    }
    parent.add(g);
    return g;
  };
}

export function triCount(obj) {
  let tris = 0;
  obj.traverse((o) => {
    if (!o.isMesh || o.userData.gizmo || !o.visible) return;
    let p = o.parent, shown = true;
    while (p) { if (!p.visible) { shown = false; break; } p = p.parent; }
    if (!shown) return;
    const g = o.geometry;
    tris += g.index ? g.index.count / 3 : g.attributes.position.count / 3;
  });
  return Math.round(tris);
}

export function countMeshes(obj, visibleOnly = true) {
  let n = 0;
  obj.traverse((o) => {
    if (!o.isMesh || o.userData.gizmo) return;
    if (visibleOnly) {
      let p = o; while (p) { if (!p.visible) return; p = p.parent; }
    }
    n++;
  });
  return n;
}
