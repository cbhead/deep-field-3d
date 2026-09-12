/**
 * Deep Field 3D — The Toaster's vehicles (docs/FORWARD-MANIFEST-toaster.md P2).
 *
 * New prefix `vehicle_` → `game/assets/vehicles/`, registered upstream in
 * `AssetLibrary.cs` 49–50.
 *
 * THE RIG CONTRACT, and why these four are the only pieces on this map that
 * are not instanced: named nodes have to stay nodes. A multimesh has no
 * children to move, so a wheel that spins and a wheel that is instanced are
 * different things. Twenty-four parts is therefore a ceiling, not a target.
 *
 * The code moves named nodes and nothing is animated in the file:
 *
 *   seat_driver / seat_passenger   empty at the seat cushion; the rider's eye
 *                                  sits at the stated height above the
 *                                  vehicle's ORIGIN, and their body is carried
 *                                  by this node
 *   wheel_fl/fr/rl/rr              separate meshes, PIVOT AT THE AXLE CENTRE,
 *   (trike: wheel_0/1/2)           axle along local X; spun about X at
 *                                  speed ÷ radius
 *   steer_wheel / steer_bars /     the thing the driver holds, turned with the
 *   steer_lever_l / steer_lever_r  steering input
 *
 * Origin at the GROUND CONTACT POINT under the chassis centre, and the nose
 * along −Z — the integration contract's forward, and the direction this
 * controller drives on. Every other kit piece with a front faces +Z and is
 * turned by a yaw helper; a vehicle is not, because forward here is a
 * direction of travel rather than a thing to point at something. Getting this
 * backwards would not fail a check: it would drive in reverse everywhere.
 *
 * `userData.vehicle` carries the manifest block the export emits, in the same
 * spirit as a tower's `rig`. `hull` is the collision box the code already
 * builds — a delivered model that does not fit inside it clips the world, so
 * every body below is measured against it rather than drawn and hoped for.
 */
import { makeToasterMats } from './toaster.js';
import { merge, revolve, sweep, solid, lerp } from './env-kit.js';

export const VEHICLES = [];
const P = (o) => { VEHICLES.push({ ...o, file: o.file || o.id + '.glb', swatch: o.swatch || '#8c9aa0', dir: 'vehicles/' }); };

export function makeVehicleMats(THREE, mats) {
  makeToasterMats(THREE, mats);
  if (mats.veh_tyre) return mats;
  const std = (name, o) => { const m = new THREE.MeshStandardMaterial({ vertexColors: true, roughness: .8, metalness: 0, envMapIntensity: .4, ...o }); m.name = name; mats[name] = m; return m; };
  /* Two blacks that differ by SURFACE, not by lightness (CLAUDE.md rule 4):
     tyre rubber is matte and takes almost nothing from the environment, the
     trike's plastic drift sleeves are slick and take a lot. That difference is
     the whole reason the trike slides and the quad grips, so it should be
     visible. */
  std('veh_tyre', { color: 0x1a1a1c, roughness: .95, metalness: 0, envMapIntensity: .06 });
  std('veh_sleeve', { color: 0x232428, roughness: .24, metalness: .05, envMapIntensity: .7 });
  std('veh_rim', { color: 0xb9bcc0, roughness: .42, metalness: .7, envMapIntensity: .8 });
  std('veh_chrome', { color: 0xd6d9dd, roughness: .18, metalness: .95, envMapIntensity: 1 });
  std('veh_steel', { color: 0x6f747b, roughness: .5, metalness: .6, envMapIntensity: .7 });
  std('veh_glass', { color: 0x2a3138, roughness: .16, metalness: .1, envMapIntensity: .9 });
  std('veh_seat', { color: 0x3a332c, roughness: .88, metalness: 0, envMapIntensity: .08 });
  // The Gator's bench is yellow vinyl — the one bright thing on a drab work
  // vehicle, and what you pick it out by across a field.
  std('veh_seat_yellow', { color: 0xd8a82c, roughness: .62, metalness: .02, envMapIntensity: .3 });
  std('veh_lamp', { color: 0xffeccb, roughness: .22, emissive: new THREE.Color(0xffd79a), emissiveIntensity: .5, envMapIntensity: .6 });
  std('veh_lamp_red', { color: 0xc8322c, roughness: .3, emissive: new THREE.Color(0xb01a16), emissiveIntensity: .35, envMapIntensity: .5 });
  std('veh_plate', { color: 0xd8d4c6, roughness: .6, envMapIntensity: .3 });
  // Paints. Faded, farm-kept, never showroom — these live outdoors.
  std('veh_paint_buggy', { color: 0x9c4034, roughness: .62, metalness: .06, envMapIntensity: .7 });
  std('veh_paint_gator', { color: 0x6d7a4a, roughness: .66, metalness: .06, envMapIntensity: .5 });
  std('veh_paint_trike', { color: 0x4e8c3a, roughness: .5, metalness: .04, envMapIntensity: .6 });
  std('veh_paint_quad', { color: 0x2f5f96, roughness: .42, metalness: .05, envMapIntensity: .8 });
  return mats;
}

/* ── local helpers ─────────────────────────────────────────────────────── */

const hash = (x, y = 0) => { const h = Math.sin(x * 127.1 + y * 311.7) * 43758.5453; return h - Math.floor(h); };

/**
 * A lofted hull: one continuous skin through cross-sections that CHANGE.
 *
 * This is the difference between a vehicle and a pile of crates. The first
 * pass drew the saloon as thirteen stacked boxes and the quad as six, and no
 * amount of detail on top of that reads as anything but boxes — a car's
 * section changes at every millimetre of its length, and a stack of prisms
 * says the opposite at every joint.
 *
 * Each station is a superellipse with SEPARATE exponents above and below the
 * waist, which is how one primitive covers a crowned roof (n ~ 2.4, nearly an
 * ellipse) and a flat floor pan (n ~ 5, nearly a rectangle) on the same body.
 * `w` is the half-width, `yb`/`yt` the bottom and top of the section, `cx` a
 * lateral offset. Stations are given at explicit z and interpolated with a
 * smoothstep — linear blending creases the skin at every station, which is
 * the stacked-box tell coming back by another route.
 */
function hull(K, name, mat, stations, o = {}) {
  const { THREE } = K, N = o.radial ?? 22, STEP = o.step ?? .13;
  const at = (z) => {
    let a = stations[0], b = stations[stations.length - 1];
    for (let i = 0; i < stations.length - 1; i++) {
      if (z >= stations[i].z && z <= stations[i + 1].z) { a = stations[i]; b = stations[i + 1]; break; }
    }
    const span = b.z - a.z, t = span > 1e-6 ? (z - a.z) / span : 0;
    const u = t * t * (3 - 2 * t), m = (p, q) => p + (q - p) * u;
    return {
      w: m(a.w, b.w), yb: m(a.yb, b.yb), yt: m(a.yt, b.yt),
      nt: m(a.nt ?? 2.5, b.nt ?? 2.5), nb: m(a.nb ?? 4.2, b.nb ?? 4.2), cx: m(a.cx ?? 0, b.cx ?? 0),
    };
  };
  const zs = [];
  for (let z = stations[0].z; z < stations[stations.length - 1].z - 1e-6; z += STEP) zs.push(z);
  zs.push(stations[stations.length - 1].z);

  const pos = [], uv = [], col = [], idx = [];
  const rings = zs.map((z) => {
    const s = at(z), yc = (s.yb + s.yt) / 2, hT = s.yt - yc, hB = yc - s.yb, out = [];
    for (let j = 0; j < N; j++) {
      const th = j / N * Math.PI * 2, c = Math.cos(th), sn = Math.sin(th);
      const n = sn >= 0 ? s.nt : s.nb, h = sn >= 0 ? hT : hB;
      out.push([
        s.cx + s.w * Math.sign(c) * Math.pow(Math.abs(c), 2 / n),
        yc + h * Math.sign(sn) * Math.pow(Math.abs(sn), 2 / n),
        z,
      ]);
    }
    return out;
  });
  const z0 = zs[0], zl = zs[zs.length - 1];
  for (let i = 0; i < rings.length; i++) {
    const v = (zs[i] - z0) / (zl - z0 || 1);
    for (let j = 0; j < N; j++) { const p = rings[i][j]; pos.push(p[0], p[1], p[2]); uv.push(j / N, v); col.push(1, 1, 1); }
  }
  /* Winding. Ring points advance counter-clockwise in XY (x = cos th,
     y = sin th) while z INCREASES, so the naive quad order (a, c, d / a, d, b)
     faces inward and every lofted surface in the file renders as the inside
     of its own far wall: the buggy's roof went near-black however much it was
     rounded, the trike's seat read as a pale slab, the Gator's yellow bench
     barely showed. Second time this class of bug has landed here — env-kit's
     revolve() had it too — so the check is now part of the build: assert the
     mean face normal points AWAY from the hull's own axis before returning.
     A backwards surface is invisible from the only side anyone looks at it
     from, which is exactly the kind of defect a screenshot does not name. */
  for (let i = 0; i < rings.length - 1; i++) {
    for (let j = 0; j < N; j++) {
      const a = i * N + j, b = i * N + (j + 1) % N, c = (i + 1) * N + j, d = (i + 1) * N + (j + 1) % N;
      idx.push(a, d, c, a, b, d);
    }
  }
  for (const [i, sgn] of [[0, -1], [rings.length - 1, 1]]) {
    const s = at(zs[i]), base = pos.length / 3;
    pos.push(s.cx, (s.yb + s.yt) / 2, zs[i]); uv.push(.5, .5); col.push(1, 1, 1);
    for (let j = 0; j < N; j++) {
      const a = i * N + j, b = i * N + (j + 1) % N;
      if (sgn < 0) idx.push(base, a, b); else idx.push(base, b, a);
    }
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
  geo.setIndex(idx); geo.computeVertexNormals();
  outward(THREE, geo);
  const m = new THREE.Mesh(solid(THREE, geo), mat); m.name = name;
  m.castShadow = true; m.receiveShadow = true;
  return m;
}

/**
 * Force a closed surface's faces to point away from its own centroid.
 *
 * Cheap insurance rather than a fix: the winding above is correct, but this
 * is the second inside-out loft in this project and the failure mode is
 * silent — the mesh builds, measures and budgets identically, and only the
 * one face nobody renders is showing. Sampling the mean dot of vertex normal
 * against outward direction costs nothing at authoring time and makes the
 * mistake impossible to ship.
 */
function outward(THREE, geo) {
  const p = geo.attributes.position, n = geo.attributes.normal;
  const b = new THREE.Box3().setFromBufferAttribute(p), c = b.getCenter(new THREE.Vector3());
  let acc = 0, cnt = 0;
  const V = new THREE.Vector3(), D = new THREE.Vector3();
  for (let i = 0; i < p.count; i += Math.max(1, Math.floor(p.count / 240))) {
    D.set(p.getX(i), p.getY(i), p.getZ(i)).sub(c);
    if (D.lengthSq() < 1e-9) continue;
    V.set(n.getX(i), n.getY(i), n.getZ(i));
    acc += V.dot(D.normalize()); cnt++;
  }
  if (cnt && acc / cnt < 0) {
    const idx = geo.index;
    for (let i = 0; i < idx.count; i += 3) {
      const t = idx.getX(i + 1); idx.setX(i + 1, idx.getX(i + 2)); idx.setX(i + 2, t);
    }
    idx.needsUpdate = true; geo.computeVertexNormals();
  }
}

/**
 * A curved band over a wheel — mudguards and fenders. Boxes stepped round an
 * arc leave a facet at every joint and read as a staircase; this is a real
 * strip, so a guard follows its tyre.
 */
function arcBand(K, name, mat, arcs) {
  const { THREE } = K;
  const list = Array.isArray(arcs) ? arcs : [arcs];
  const pos = [], uv = [], col = [], idx = [];
  for (const o of list) {
    const { cx = 0, cy = 0, cz = 0, r, width, a0, a1, thick = .03, segs = 10 } = o;
    const hw = width / 2, base = pos.length / 3;
    for (let i = 0; i <= segs; i++) {
      const t = i / segs, a = a0 + (a1 - a0) * t;
      for (const [ro, sx] of [[r, -hw], [r, hw], [r + thick, hw], [r + thick, -hw]]) {
        pos.push(cx + sx, cy + Math.sin(a) * ro, cz + Math.cos(a) * ro);
        uv.push(t, sx > 0 ? 1 : 0); col.push(1, 1, 1);
      }
    }
    for (let i = 0; i < segs; i++) {
      const a = base + i * 4, b = base + (i + 1) * 4;
      for (const [p, q] of [[0, 1], [1, 2], [2, 3], [3, 0]]) idx.push(a + p, b + p, b + q, a + p, b + q, a + q);
    }
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
  geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
  geo.setAttribute('color', new THREE.Float32BufferAttribute(col, 3));
  geo.setIndex(idx); geo.computeVertexNormals();
  const m = new THREE.Mesh(solid(THREE, geo), mat); m.name = name;
  m.castShadow = true; return m;
}

function MB(K, name, mat, list) {
  const { THREE } = K, items = [];
  for (const [w, h, d, x, y, z, ry = 0, c] of list) {
    const m = new THREE.Matrix4().makeRotationY(ry); m.setPosition(x, y, z);
    items.push({ geo: new THREE.BoxGeometry(w, h, d), m, c });
  }
  return merge(THREE, items, name, mat);
}
function MCyl(K, name, mat, list) {
  const { THREE } = K, items = [], e = new THREE.Euler();
  for (const [rt, rb, h, x, y, z, rx = 0, rz = 0, segs = 8, c] of list) {
    e.set(rx, 0, rz, 'ZYX');
    const m = new THREE.Matrix4().makeRotationFromEuler(e); m.setPosition(x, y, z);
    items.push({ geo: new THREE.CylinderGeometry(rt, rb, h, segs, 1, false), m, c });
  }
  return merge(THREE, items, name, mat);
}

/**
 * A road wheel as its own node.
 *
 * PIVOT AT THE AXLE CENTRE and the axle along local X, which is the whole
 * contract — the code spins this about X at speed ÷ radius, so an origin
 * anywhere but the axle makes the wheel orbit instead of turn, and that is not
 * something a screenshot review catches.
 *
 * Built from a revolved carcass rather than a cylinder: a tyre has a shoulder
 * and a sidewall bulge, and the rim is inset inside the bead, not glued to the
 * outside of a disc.
 */
function wheel(K, name, r, width, o = {}) {
  const { THREE, grp, mats } = K, g = grp(name, o.pos || [0, 0, 0]);
  const hw = width / 2, knobbly = o.knobbly, sleeve = o.sleeve;
  const tyreMat = sleeve ? mats.veh_sleeve : mats.veh_tyre;
  /* On a knobbly tyre the TREAD is the outer surface, so the carcass is built
     inside the stated radius and the lugs bring it back out to r. Built the
     other way the lugs stand proud of the radius the code drives on, and the
     whole vehicle sinks 45 mm into the ground — which measures as a bounding
     box below y 0, not as anything you would see. */
  const rc = knobbly ? r * .94 : r;
  /* Carcass: bead → sidewall → shoulder → tread → back again, revolved about
     its own axis, then laid over so the axis runs along X. */
  const car = revolve(THREE, `${name}_tyre`, [
    [rc * .52, -hw * .58], [rc * .78, -hw * .86], [rc * .94, -hw * 1.0], [rc, -hw * .82],
    [rc, hw * .82], [rc * .94, hw * 1.0], [rc * .78, hw * .86], [rc * .52, hw * .58],
  ], tyreMat, { segs: o.segs ?? 16, creaseAngle: 34, scale: 3.2 });
  car.rotation.y = Math.PI / 2; g.add(car);
  // Rim, inset inside the bead.
  const rim = revolve(THREE, `${name}_rim`, [
    [0, -hw * .5], [rc * .30, -hw * .56], [rc * .52, -hw * .58], [rc * .52, hw * .58], [rc * .30, hw * .56], [0, hw * .5],
  ], mats.veh_rim, { segs: o.segs ?? 16, creaseAngle: 40, scale: 3.2 });
  rim.rotation.y = Math.PI / 2; g.add(rim);
  if (knobbly) {
    // Tread blocks cut as real geometry only here, where the tyre is 300 mm
    // across and read from two metres — on the saloon they are in the texture.
    const blocks = [], N = 12;
    for (let i = 0; i < N; i++) {
      const a = i / N * Math.PI * 2, side = i % 2 ? 1 : -1;
      blocks.push([width * .34, r * .1, r * .12, side * width * .26, Math.sin(a) * r * .94, Math.cos(a) * r * .94, 0]);
    }
    const items = blocks.map(([w, h, d, x, y, z]) => {
      const ang = Math.atan2(y, z);
      const m = new THREE.Matrix4().makeRotationX(-ang); m.setPosition(x, y, z);
      return { geo: new THREE.BoxGeometry(w, h, d), m };
    });
    g.add(merge(THREE, items, `${name}_tread`, tyreMat));
  }
  g.userData.wheel = { radius: r, axis: [1, 0, 0] };
  return g;
}

/** An empty at a seat cushion. Not a mesh — it carries a rider, not geometry. */
function seat(K, name, pos) {
  const { grp } = K, g = grp(name, pos);
  g.userData.seat = { eye: null };
  return g;
}

/* ═══ V1 — the Buggy ═══════════════════════════════════════════════════════ */

P({
  id: 'vehicle_buggy', label: 'Buggy — saloon', size: '1.55 × 1.5 × 4.1 m', swatch: '#9c4034',
  budgetTris: 8000, budgetParts: 24,
  rig: { seats: 2, wheels: 4, steer: 'steer_wheel' },
  stats: { Hull: '1.55×1.5×4.1', Wheelbase: '2.4 m', Wheel: 'r 0.29', Eye: '1.15 m', Nose: '−Z' },
  note: 'Rear-engined saloon, two seats abreast, in FADED RED. One continuous lofted skin from nose to tail with a crowned roof and tumblehome at the waist — not the stack of thirteen boxes it started as, which announced a flat at every joint. The cabin is the glass: a rounded dark volume with a painted crown over it and slim pillars down its corners, which is how a saloon actually reads, rather than a painted box with window holes cut in it. One dented wing — farm-kept rather than restored: it is the thing that lives under the carport and gets driven to the end of the road. Origin at the ground contact point under the chassis centre, nose along −Z. Wheels pivot at the axle centre with the axle along local X, which is what lets the code spin them at speed ÷ radius.',
  build(K) {
    const { THREE, grp, mats } = K; makeVehicleMats(THREE, mats);
    const g = grp('vehicle_buggy'), R = .29, WB = 2.4, HW = .66;
    const paint = mats.veh_paint_buggy;

    /* Body. A saloon's section changes everywhere — tumblehome at the waist,
       a crowned roof, haunches over the arches — so it is ONE lofted skin,
       not the thirteen stacked boxes the first pass used. Stacked prisms
       announce a flat at every joint; nothing about a car is flat.

       Every station is clamped so the drawn body fits INSIDE the
       1.55 × 1.5 × 4.1 hull the code already builds: a model that does not
       fit its own collision box clips the world, and the box is not
       negotiable. Widest station is 0.75 half — 1.50 across against 1.55. */
    g.add(hull(K, 'vehicle_buggy_body', paint, [
      { z: -2.00, w: .46, yb: .38, yt: .60, nt: 2.3, nb: 3.0 },
      { z: -1.72, w: .62, yb: .30, yt: .66, nt: 2.4 },
      { z: -1.24, w: .71, yb: .26, yt: .74, nt: 2.5 },
      { z: -0.76, w: .75, yb: .25, yt: .82, nt: 2.6, nb: 4.6 },
      { z: -0.10, w: .74, yb: .25, yt: .86, nt: 2.7, nb: 5.0 },
      { z:  0.55, w: .75, yb: .25, yt: .86, nt: 2.7, nb: 5.0 },
      { z:  1.10, w: .74, yb: .26, yt: .82, nt: 2.6, nb: 4.6 },
      { z:  1.58, w: .66, yb: .29, yt: .74, nt: 2.4 },
      { z:  1.90, w: .50, yb: .36, yt: .62, nt: 2.3, nb: 3.0 },
    ], { radial: 24 }));

    /* Greenhouse. The cabin IS the glass — one rounded dark volume — with a
       painted shell capping its crown and slim pillars down its corners.
       Drawing it the other way round (a painted box with window holes) is
       what made the first pass a shed on wheels; a saloon reads as a dark
       band under a body-colour roof, so that is what is modelled. */
    g.add(hull(K, 'vehicle_buggy_glass', mats.veh_glass, [
      { z: -0.92, w: .50, yb: .78, yt: .96, nt: 2.2 },
      { z: -0.58, w: .60, yb: .78, yt: 1.16, nt: 2.3 },
      { z: -0.05, w: .645, yb: .78, yt: 1.24, nt: 2.5 },
      { z:  0.52, w: .64, yb: .78, yt: 1.24, nt: 2.5 },
      { z:  1.02, w: .57, yb: .78, yt: 1.12, nt: 2.3 },
      { z:  1.34, w: .45, yb: .78, yt: .96, nt: 2.2 },
    ], { radial: 22 }));
    // The roof itself: a painted shell over the crown of that same volume,
    // wider and taller than the glass by enough to read as a separate
    // surface — at 15 mm it just looked like the glass had gone matte.
    g.add(hull(K, 'vehicle_buggy_roof', paint, [
      { z: -0.74, w: .505, yb: 1.08, yt: 1.24, nt: 1.95 },
      { z: -0.24, w: .625, yb: 1.12, yt: 1.42, nt: 2.05 },
      { z:  0.26, w: .672, yb: 1.12, yt: 1.45, nt: 2.10 },
      { z:  0.78, w: .635, yb: 1.12, yt: 1.42, nt: 2.05 },
      { z:  1.18, w: .475, yb: 1.08, yt: 1.23, nt: 1.95 },
    ], { radial: 22 }));
    // A and C pillars, standing proud of the glass so the cabin has structure.
    g.add(MB(K, 'vehicle_buggy_pillars', paint, [
      [.10, .52, .13, -.60, 1.06, -.72, .10], [.10, .52, .13, .60, 1.06, -.72, -.10],
      [.10, .48, .13, -.57, 1.04, 1.14, -.08], [.10, .48, .13, .57, 1.04, 1.14, .08],
    ]));

    /* Wheel arches, as curved bands over each tyre rather than box lips — and
       all four merged into ONE part, because a part is a draw call and four
       guards that never move independently have no business being four.
       0.22 m across, not 0.30: at 0.30 the outer lip stood at 0.81 from the
       centre line and the car measured 1.62 across its own 1.55 hull. */
    g.add(arcBand(K, 'vehicle_buggy_arches', paint,
      [[-HW, -WB / 2], [HW, -WB / 2], [-HW, WB / 2], [HW, WB / 2]].map(([x, z]) => ({
        cx: x, cy: R, cz: z, r: R + .055, width: .22, a0: -Math.PI * .18, a1: Math.PI * 1.18, thick: .035, segs: 9,
      }))));

    g.add(MB(K, 'vehicle_buggy_sills', paint, [
      [1.40, .16, 2.5, 0, .30, .1],
      // The dent: one front wing sits proud and skewed. Kept inside \u00b10.775 \u2014
      // a 0.30 box at x \u22120.62 with 0.07 of yaw reached \u22120.796 and put the car
      // 16 mm outside the hull the code collides with.
      [.24, .22, .72, -.615, .60, -.95, .07],
    ]));
    g.add(MB(K, 'vehicle_buggy_trim', mats.veh_chrome, [
      [.90, .07, .11, 0, .48, -1.93], [.92, .07, .11, 0, .50, 1.88],
      // Waist strip, kept to the parallel middle of the body: run the full
      // 2.9 m and the ends hang in air where the wings tuck in.
      [.04, .045, 2.0, -.745, .66, .10], [.04, .045, 2.0, .745, .66, .10],
    ]));
    g.add(MB(K, 'vehicle_buggy_grille', mats.veh_steel, [
      [.78, .17, .09, 0, .64, -1.92],
    ]));
    g.add(MB(K, 'vehicle_buggy_lamps', mats.veh_lamp, [
      [.20, .15, .08, -.44, .66, -1.93], [.20, .15, .08, .44, .66, -1.93],
    ]));
    g.add(MB(K, 'vehicle_buggy_lamps_rear', mats.veh_lamp_red, [
      [.20, .13, .08, -.46, .68, 1.88], [.20, .13, .08, .46, .68, 1.88],
    ]));
    g.add(MB(K, 'vehicle_buggy_plate', mats.veh_plate, [[.40, .11, .03, 0, .45, -1.99], [.40, .11, .03, 0, .47, 1.93]]));
    g.add(MB(K, 'vehicle_buggy_interior', mats.veh_seat, [
      [.48, .12, .48, -.34, .78, .12], [.48, .48, .12, -.34, 1.00, .34],
      [.48, .12, .48, .34, .78, .12], [.48, .48, .12, .34, 1.00, .34],
      [1.24, .10, .44, 0, .80, 1.00], [1.24, .42, .10, 0, 1.00, 1.20],
      [1.22, .06, .28, 0, .96, -.64],
    ]));
    /* The wheel the driver holds. Rim, spokes and hub as one node so the code
       has a single thing to turn. */
    const sw = grp('steer_wheel', [-.34, 1.06, -.5]);
    sw.rotation.x = -1.12;
    sw.add(revolve(THREE, 'steer_wheel_rim', [
      [.155, -.016], [.175, -.012], [.181, 0], [.175, .012], [.155, .016],
    ], mats.veh_seat, { segs: 18, creaseAngle: 40, scale: 6 }));
    sw.add(MCyl(K, 'steer_wheel_spokes', mats.veh_steel, [
      [.012, .012, .3, 0, 0, 0, 0, Math.PI / 2, 6],
      [.012, .012, .3, 0, 0, 0, 0, Math.PI / 6, 6],
      [.012, .012, .3, 0, 0, 0, 0, -Math.PI / 6, 6],
      [.036, .04, .05, 0, 0, 0, Math.PI / 2, 0, 10],
    ]));
    g.add(sw);
    g.add(MCyl(K, 'vehicle_buggy_column', mats.veh_steel, [[.022, .026, .38, -.34, .93, -.36, 1.12, 0, 6]]));

    g.add(seat(K, 'seat_driver', [-.34, .82, .1]));
    g.add(seat(K, 'seat_passenger', [.34, .82, .1]));
    for (const [n, x, z, st] of [['wheel_fl', -HW, -WB / 2, 1], ['wheel_fr', HW, -WB / 2, 1], ['wheel_rl', -HW, WB / 2, 0], ['wheel_rr', HW, WB / 2, 0]]) {
      g.add(wheel(K, n, R, .2, { pos: [x, R, z] }));
    }
    g.userData.vehicle = {
      seats: ['seat_driver', 'seat_passenger'],
      wheels: [
        { node: 'wheel_fl', radius: R, steers: true }, { node: 'wheel_fr', radius: R, steers: true },
        { node: 'wheel_rl', radius: R, steers: false }, { node: 'wheel_rr', radius: R, steers: false },
      ],
      steer: 'steer_wheel', steerMaxDeg: 32, hull: [1.55, 1.5, 4.1], eye: 1.15,
    };
    return g;
  },
});

/* ═══ V2 — the Gator ═══════════════════════════════════════════════════════ */

P({
  id: 'vehicle_dagator', label: 'Gator — utility', size: '1.5 × 1.85 × 2.9 m', swatch: '#6d7a4a',
  budgetTris: 8000, budgetParts: 24,
  rig: { seats: 2, wheels: 4, steer: 'steer_wheel' },
  stats: { Hull: '1.5×1.85×2.9', Wheel: 'r 0.31', Eye: '1.0 m', Cage: 'roll bar', Bed: 'open' },
  note: 'Utility vehicle with a YELLOW vinyl bench — the one bright thing on a drab work machine and what you pick it out by across a field. The bonnet and scuttle are one lofted pressing that crowns over the engine and falls to the grille, and the fenders are real curved bands over the knobblies: a work vehicle is still a pressing, and four flat-topped crates is the whole of why it read as primitive. Roll cage, open cargo bed with a strapped tarp, light bar. Eye height 1.0 — low and workmanlike. Lives in the barn and is driven through its 5 × 4 door, which is why that door is 5 × 4. Knobbly tread is cut as real geometry here because the tyre is 300 mm across and read from two metres; on the saloon the same detail is in the texture.',
  build(K) {
    const { THREE, grp, mats } = K; makeVehicleMats(THREE, mats);
    const g = grp('vehicle_dagator'), R = .31, WB = 1.9, HW = .58;
    const paint = mats.veh_paint_gator;

    g.add(MB(K, 'vehicle_dagator_chassis', mats.veh_steel, [
      [1.16, .1, 2.7, 0, .30, 0], [.1, .16, 2.6, -.56, .38, 0], [.1, .16, 2.6, .56, .38, 0],
      [1.02, .24, .07, 0, .58, -1.39], [1.10, .08, .12, 0, .42, -1.37],   // grille + bumper
    ]));

    /* Bonnet and scuttle as one lofted nose. A work vehicle is still a
       PRESSING — the first pass drew it as four flat-topped crates and that
       is the whole of why it read as primitive. The section crowns over the
       engine and falls away to the grille. */
    g.add(hull(K, 'vehicle_dagator_nose', paint, [
      { z: -1.40, w: .54, yb: .40, yt: .68, nt: 2.6, nb: 4.0 },
      { z: -1.12, w: .61, yb: .38, yt: .76, nt: 2.8, nb: 4.6 },
      { z: -0.74, w: .64, yb: .38, yt: .80, nt: 3.0, nb: 5.0 },
      { z: -0.42, w: .645, yb: .40, yt: .84, nt: 3.2, nb: 5.0 },
      { z: -0.24, w: .64, yb: .44, yt: .86, nt: 3.4, nb: 5.0 },
    ], { radial: 20, step: .11 }));

    /* Body and bed are ONE part. They are the same pressing in the same
       paint and they never move apart, and a part is a draw call — the piece
       was 27 against a 24 budget purely from splitting things that are one
       thing. Tailgate at z 1.41, not 1.44: at 1.44 its 70 mm thickness put
       the vehicle 50 mm outside the 2.9 m hull the code collides with. */
    g.add(MB(K, 'vehicle_dagator_body', paint, [
      [1.3, .5, .12, 0, .78, .44],             // seat back panel
      [1.24, .06, .9, 0, .56, .0],             // floor
      [1.28, .30, .10, 0, .70, -.20],          // dash face
      [1.3, .06, 1.04, 0, .66, .90],           // bed floor
      [.07, .34, 1.04, -.62, .86, .90], [.07, .34, 1.04, .62, .86, .90],
      [1.3, .34, .07, 0, .86, 1.38],           // tailgate
      [1.3, .3, .06, 0, .84, .4],              // bulkhead
    ]));

    g.add(MB(K, 'vehicle_dagator_tarp', mats.toa_tarp, [
      [1.14, .16, .86, 0, 1.06, .9], [1.2, .05, .1, 0, 1.1, .52], [1.2, .05, .1, 0, 1.1, 1.28],
    ]));
    /* Fenders: real curved bands over the knobblies, all four merged into one
       part. A utility vehicle is mostly fender from the side, and a flat lip
       over a round tyre is the cheapest possible lie about the shape. */
    g.add(arcBand(K, 'vehicle_dagator_fenders', paint,
      [[-HW, -WB / 2], [HW, -WB / 2], [-HW, WB / 2], [HW, WB / 2]].map(([x, z]) => ({
        cx: x, cy: R, cz: z, r: R + .07, width: .34, a0: -Math.PI * .14, a1: Math.PI * 1.14, thick: .035, segs: 9,
      }))));
    /* Roll cage from real tube, rooted INSIDE the body: a cage that stops at
       the surface shows four open tube ends. */
    g.add(MCyl(K, 'vehicle_dagator_cage', mats.veh_steel, [
      [.028, .028, 1.15, -.58, 1.18, .44, 0, .06, 8], [.028, .028, 1.15, .58, 1.18, .44, 0, -.06, 8],
      [.028, .028, 1.12, 0, 1.74, .44, 0, Math.PI / 2, 8],
      [.026, .026, 1.35, -.56, 1.36, -.1, .82, 0, 8], [.026, .026, 1.35, .56, 1.36, -.1, .82, 0, 8],
      [.026, .026, .9, -.57, 1.3, .88, -.6, 0, 8], [.026, .026, .9, .57, 1.3, .88, -.6, 0, 8],
      [.022, .022, .74, 0, 1.5, .44, 0, Math.PI / 2, 6],
    ]));
    g.add(MB(K, 'vehicle_dagator_lightbar', mats.veh_lamp, [
      [.9, .1, .12, 0, 1.76, -.62], [.14, .12, .1, -.5, 1.7, -.66], [.14, .12, .1, .5, 1.7, -.66],
    ]));
    /* The bench, in YELLOW vinyl — the one bright thing on the vehicle and
       what you pick it out by across a field. Cushion and squab are lofted
       rather than boxed: a bench seat sags in the middle and rolls at its
       front edge, and those two facts are most of what says “seat”. */
    g.add(hull(K, 'vehicle_dagator_seat', mats.veh_seat_yellow, [
      { z: -0.14, w: .58, yb: .64, yt: .78, nt: 2.2, nb: 3.4 },
      { z:  0.10, w: .59, yb: .64, yt: .80, nt: 2.6, nb: 4.4 },
      { z:  0.30, w: .59, yb: .64, yt: .82, nt: 2.6, nb: 4.4 },
      { z:  0.38, w: .58, yb: .70, yt: 1.06, nt: 3.0, nb: 3.0 },
      { z:  0.46, w: .57, yb: .74, yt: 1.20, nt: 2.4, nb: 3.2 },
    ], { radial: 18, step: .07 }));

    g.add(MB(K, 'vehicle_dagator_lamps', mats.veh_lamp, [
      [.17, .13, .07, -.40, .72, -1.41], [.17, .13, .07, .40, .72, -1.41],
    ]));
    const sw = grp('steer_wheel', [-.32, 1.02, -.3]);
    sw.rotation.x = -1.0;
    sw.add(revolve(THREE, 'steer_wheel_rim', [
      [.14, -.015], [.158, -.011], [.164, 0], [.158, .011], [.14, .015],
    ], mats.veh_seat, { segs: 16, creaseAngle: 40, scale: 6 }));
    sw.add(MCyl(K, 'steer_wheel_spokes', mats.veh_steel, [
      [.011, .011, .28, 0, 0, 0, 0, Math.PI / 2, 6],
      [.011, .011, .28, 0, 0, 0, 0, -Math.PI / 5, 6],
      [.034, .038, .05, 0, 0, 0, Math.PI / 2, 0, 10],
    ]));
    g.add(sw);
    g.add(MCyl(K, 'vehicle_dagator_column', mats.veh_steel, [[.02, .024, .34, -.32, .88, -.2, 1.0, 0, 6]]));

    g.add(seat(K, 'seat_driver', [-.32, .79, .12]));
    g.add(seat(K, 'seat_passenger', [.32, .79, .12]));
    for (const [n, x, z] of [['wheel_fl', -HW, -WB / 2], ['wheel_fr', HW, -WB / 2], ['wheel_rl', -HW, WB / 2], ['wheel_rr', HW, WB / 2]]) {
      g.add(wheel(K, n, R, .3, { pos: [x, R, z], knobbly: true, segs: 14 }));
    }
    g.userData.vehicle = {
      seats: ['seat_driver', 'seat_passenger'],
      wheels: [
        { node: 'wheel_fl', radius: R, steers: true }, { node: 'wheel_fr', radius: R, steers: true },
        { node: 'wheel_rl', radius: R, steers: false }, { node: 'wheel_rr', radius: R, steers: false },
      ],
      steer: 'steer_wheel', steerMaxDeg: 34, hull: [1.5, 1.85, 2.9], eye: 1.0,
    };
    return g;
  },
});

/* ═══ V3 — the Grnmchn ═════════════════════════════════════════════════════ */

P({
  id: 'vehicle_grnmchn', label: 'Grnmchn — drift trike', size: '0.9 × 0.9 × 1.9 m', swatch: '#4e8c3a',
  budgetTris: 8000, budgetParts: 24,
  rig: { seats: 1, wheels: 3, steer: 'steer_fork' },
  stats: { Hull: '0.9×0.9×1.9', Front: 'wheel_0 r 0.25', Rear: 'r 0.13 sleeved', Eye: '0.75 m' },
  note: 'Motorised drift trike, and the lowest thing on the map at 0.75 m eye height, which is most of the joke. Front `wheel_0` r 0.25 on a fork; rear `wheel_1` and `wheel_2` at r 0.13 wearing PLASTIC DRIFT SLEEVES, which is why it slides — the sleeves are a slick, high-envMap material against the matte rubber everywhere else, so the reason it drifts is visible rather than just simulated. It steers by pulling TWO LEVERS, not by turning a wheel, and that is why it can spin on the spot. The levers and the front wheel are children of one `steer_fork` node so a single steering input turns all three together; if upstream would rather bind the levers directly, the node names are there for it.',
  build(K) {
    const { THREE, grp, mats } = K; makeVehicleMats(THREE, mats);
    const g = grp('vehicle_grnmchn'), RF = .25, RR = .13;
    const paint = mats.veh_paint_trike;
    /* The fork's pivot is the head stock at y 0.52; the front wheel hangs
       0.27 m below it, which puts the wheel centre at 0.25 and its contact
       patch on the ground. Solved rather than eyeballed: the first pass hung
       it off a 0.62 pivot and buried 90 mm of the front wheel in the field. */
    const RAKE = .28;

    // Backbone: one tube from the head stock back to the rear axle, with the
    // seat pan slung off it. A trike is a frame, not a body.
    g.add(MCyl(K, 'vehicle_grnmchn_frame', paint, [
      [.034, .04, 1.25, 0, .32, .1, 1.42, 0, 8],
      [.028, .028, .56, 0, .28, .66, 0, Math.PI / 2, 8],
      [.024, .024, .44, -.15, .21, .44, .3, .5, 8], [.024, .024, .44, .15, .21, .44, .3, -.5, 8],
    ]));
    /* Bucket seat, lofted. Two boxes is what a seat looks like from a metre
       away and nothing like one from the saddle — a trike seat is a scooped
       pan with rolled sides, and on a vehicle this small it is a third of
       what you see. */
    g.add(hull(K, 'vehicle_grnmchn_seat', mats.veh_seat, [
      { z: 0.22, w: .15, yb: .33, yt: .40, nt: 2.0, nb: 2.6 },
      { z: 0.42, w: .18, yb: .33, yt: .41, nt: 2.4, nb: 3.2 },
      { z: 0.58, w: .18, yb: .34, yt: .48, nt: 2.2, nb: 3.0 },
      { z: 0.66, w: .17, yb: .36, yt: .64, nt: 2.6, nb: 2.6 },
    ], { radial: 16, step: .07 }));
    g.add(MB(K, 'vehicle_grnmchn_engine', mats.veh_steel, [
      [.24, .22, .28, 0, .32, .7], [.28, .06, .18, 0, .19, .7],
    ]));
    g.add(MCyl(K, 'vehicle_grnmchn_exhaust', mats.veh_chrome, [
      [.024, .028, .42, .18, .4, .72, .1, Math.PI / 2 - .2, 6],
    ]));
    g.add(MB(K, 'vehicle_grnmchn_plate', mats.veh_plate, [[.2, .1, .02, 0, .48, .86]]));
    /* A small moulded nose fairing over the head stock. The trike was all
       bare tube and read as a frame with parts bolted to it; one curved
       plastic panel is what makes it a MACHINE, and it is the only place on
       this vehicle with a surface big enough to carry the green. */
    g.add(hull(K, 'vehicle_grnmchn_fairing', paint, [
      { z: -0.70, w: .07, yb: .40, yt: .50, nt: 2.0, nb: 2.2 },
      { z: -0.58, w: .13, yb: .36, yt: .56, nt: 2.2, nb: 2.6 },
      { z: -0.44, w: .15, yb: .34, yt: .58, nt: 2.4, nb: 3.0 },
      { z: -0.32, w: .13, yb: .34, yt: .54, nt: 2.2, nb: 2.8 },
    ], { radial: 16, step: .07 }));

    /* The fork IS the steering node: the front wheel and both levers hang off
       it, so one input turns the lot. Its pivot is the head stock, laid back
       at the rake the geometry actually has — a fork pivoting about the wheel
       centre steers like a shopping trolley. */
    /* `steer_fork` is the node the code YAWS, so it carries no rake of its
       own: the rake is drawn into the legs inside it. Raking the node itself
       tilted the front wheel with it and put 53 mm of tyre under the field —
       a wheel must stay upright whatever its fork is doing. */
    const fork = grp('steer_fork', [0, .52, -.52]);
    fork.add(MCyl(K, 'steer_fork_legs', paint, [
      [.019, .022, .44, -.1, -.16, -.05, RAKE, .05, 8], [.019, .022, .44, .1, -.16, -.05, RAKE, -.05, 8],
      [.026, .03, .18, 0, .04, .01, RAKE, 0, 8],
    ]));
    // Two lever handles, pivoting about Y at the fork. Kept under the hull's
    // 0.9 m roof — the grips are the highest thing on the vehicle, and at the
    // first pass's height they stood 100 mm out through it.
    const lever = (name, side) => {
      const l = grp(name, [side * .09, .08, 0]);
      l.add(MCyl(K, `${name}_arm`, mats.veh_steel, [
        [.014, .016, .3, side * .04, .11, -.05, -.2, side * .18, 6],
      ]));
      l.add(MCyl(K, `${name}_grip`, mats.veh_seat, [
        [.02, .02, .12, side * .075, .23, -.09, -.2, side * .18, 8],
      ]));
      return l;
    };
    fork.add(lever('steer_lever_l', -1));
    fork.add(lever('steer_lever_r', 1));
    fork.add(wheel(K, 'wheel_0', RF, .07, { pos: [0, -.27, -.10], segs: 14 }));
    g.add(fork);

    for (const [n, x] of [['wheel_1', -.31], ['wheel_2', .31]]) {
      g.add(wheel(K, n, RR, .14, { pos: [x, RR, .74], sleeve: true, segs: 12 }));
    }
    g.add(MCyl(K, 'vehicle_grnmchn_axle', mats.veh_steel, [[.016, .016, .72, 0, RR, .74, 0, Math.PI / 2, 6]]));
    g.add(seat(K, 'seat_driver', [0, .41, .42]));

    g.userData.vehicle = {
      seats: ['seat_driver'],
      wheels: [
        { node: 'wheel_0', radius: RF, steers: true },
        { node: 'wheel_1', radius: RR, steers: false }, { node: 'wheel_2', radius: RR, steers: false },
      ],
      steer: 'steer_fork', steerLevers: ['steer_lever_l', 'steer_lever_r'],
      steerMaxDeg: 60, hull: [0.9, 0.9, 1.9], eye: 0.75,
    };
    return g;
  },
});

/* ═══ V4 — the Vehickle ════════════════════════════════════════════════════ */

P({
  id: 'vehicle_vehickle', label: 'Vehickle — sport quad', size: '1.16 × 1.13 × 1.85 m', swatch: '#2f5f96',
  budgetTris: 8000, budgetParts: 24,
  rig: { seats: 1, wheels: 4, steer: 'steer_bars' },
  stats: { Hull: '1.16×1.13×1.85', Wheel: 'r 0.28', Eye: '1.05 m', Top: '22 m/s' },
  note: 'Sport quad in BLUE, and the sleekest thing on the map: one lofted plastic shell whose first station is a 0.05 m half-width, so the nose is a beak that starts at a knife edge and flares back over the front arches rather than the four flat slabs it was. Saddle lofted to narrow at the tank so the rider’s knees have somewhere to go; guards are curved bands over the crown of each tyre, all four merged to one part. The fastest thing on the map at 22 m/s and it handles like it. `steer_bars` is the node the code turns and the node the `bars` hand pose grips, so the grips are 0.7 m apart to match it.',
  build(K) {
    const { THREE, grp, mats } = K; makeVehicleMats(THREE, mats);
    const g = grp('vehicle_vehickle'), R = .28, WB = 1.22, HW = .44;
    const paint = mats.veh_paint_quad;

    g.add(MB(K, 'vehicle_vehickle_chassis', mats.veh_steel, [
      [.5, .08, 1.5, 0, .34, 0], [.08, .12, 1.4, -.3, .4, 0], [.08, .12, 1.4, .3, .4, 0],
      [.8, .06, .5, 0, .3, .34],
    ]));

    /* Bodywork: one lofted plastic shell running the length of the machine,
       drawn to a POINT at the nose. A sport quad's front is a beak — a
       wedge that starts narrow at the bumper and flares back over the front
       arches — and the first pass drew it as four flat slabs, which is why
       it had no front at all. The first station is 0.05 half-width: near
       enough a knife edge that the shell closes on itself, with the taper
       carried over half a metre rather than faked with one chamfer. */
    g.add(hull(K, 'vehicle_vehickle_body', paint, [
      { z: -0.92, w: .05, yb: .54, yt: .62, nt: 2.0, nb: 2.0 },
      { z: -0.76, w: .17, yb: .50, yt: .68, nt: 2.1, nb: 2.4 },
      { z: -0.54, w: .31, yb: .46, yt: .76, nt: 2.2, nb: 2.8 },
      { z: -0.30, w: .40, yb: .44, yt: .82, nt: 2.4, nb: 3.4 },
      { z: -0.02, w: .43, yb: .42, yt: .74, nt: 2.6, nb: 4.0 },
      { z:  0.34, w: .44, yb: .42, yt: .72, nt: 2.6, nb: 4.0 },
      { z:  0.66, w: .43, yb: .44, yt: .74, nt: 2.5, nb: 3.6 },
      { z:  0.90, w: .34, yb: .48, yt: .70, nt: 2.2, nb: 2.8 },
    ], { radial: 22, step: .10 }));

    /* Mudguards: curved bands over all four knobblies, merged to one part.
       On a quad the guards ARE the silhouette at distance, so a flat lip is
       the one thing that cannot be there. */
    g.add(arcBand(K, 'vehicle_vehickle_guards', paint,
      [[-HW, -WB / 2], [HW, -WB / 2], [-HW, WB / 2], [HW, WB / 2]].map(([x, z]) => ({
        // 0.20π → 0.80π, not −0.10π → 1.10π: a guard wrapping past the tyre's
        // equator reached 0.98 m fore and aft and stood the quad 110 mm out
        // through its own 1.85 m hull. A sport quad's guards cover the crown.
        cx: x, cy: R, cz: z, r: R + .06, width: .28, a0: Math.PI * .20, a1: Math.PI * .80, thick: .03, segs: 8,
      }))));

    /* Saddle: a lofted seat that narrows at the tank so the rider's knees
       have somewhere to go, which is the shape of every sport quad and none
       of the boxes that were here. */
    g.add(hull(K, 'vehicle_vehickle_saddle', mats.veh_seat, [
      { z: -0.30, w: .11, yb: .74, yt: .84, nt: 2.0, nb: 2.6 },
      { z: -0.02, w: .15, yb: .74, yt: .86, nt: 2.2, nb: 3.0 },
      { z:  0.34, w: .19, yb: .74, yt: .90, nt: 2.4, nb: 3.4 },
      { z:  0.60, w: .18, yb: .76, yt: .94, nt: 2.2, nb: 3.0 },
    ], { radial: 16, step: .09 }));
    g.add(MB(K, 'vehicle_vehickle_board', mats.veh_plate, [
      [.30, .26, .03, 0, .86, -.50, 0],
    ]));
    g.add(MB(K, 'vehicle_vehickle_lamps', mats.veh_lamp, [
      [.11, .08, .06, -.13, .74, -.70], [.11, .08, .06, .13, .74, -.70],
    ]));
    g.add(MCyl(K, 'vehicle_vehickle_exhaust', mats.veh_chrome, [
      [.032, .036, .62, .22, .58, .5, .12, Math.PI / 2 - .1, 8],
    ]));
    g.add(MB(K, 'vehicle_vehickle_rack', mats.veh_steel, [
      [.5, .04, .04, 0, .82, .82], [.04, .04, .3, -.22, .82, .7], [.04, .04, .3, .22, .82, .7],
    ]));
    /* Handlebars. The grips sit 0.7 m apart because that is the dimension the
       `bars` hand pose is authored to — a bar the hands do not land on is
       worse than no hands at all. */
    const bars = grp('steer_bars', [0, 1.06, -.36]);
    bars.rotation.x = -.18;
    bars.add(MCyl(K, 'steer_bars_tube', mats.veh_steel, [
      [.014, .014, .7, 0, 0, 0, 0, Math.PI / 2, 8],
      [.016, .018, .16, 0, -.08, .02, .5, 0, 8],
    ]));
    bars.add(MCyl(K, 'steer_bars_grips', mats.veh_seat, [
      [.017, .017, .13, -.29, 0, 0, 0, Math.PI / 2, 8],
      [.017, .017, .13, .29, 0, 0, 0, Math.PI / 2, 8],
    ]));
    g.add(bars);
    g.add(MCyl(K, 'vehicle_vehickle_forks', mats.veh_steel, [
      [.02, .022, .5, -.05, .78, -.44, -.2, .04, 8], [.02, .022, .5, .05, .78, -.44, -.2, -.04, 8],
    ]));

    g.add(seat(K, 'seat_driver', [0, .89, .16]));
    for (const [n, x, z] of [['wheel_fl', -HW, -WB / 2], ['wheel_fr', HW, -WB / 2], ['wheel_rl', -HW, WB / 2], ['wheel_rr', HW, WB / 2]]) {
      g.add(wheel(K, n, R, .24, { pos: [x, R, z], knobbly: true, segs: 14 }));
    }
    g.userData.vehicle = {
      seats: ['seat_driver'],
      wheels: [
        { node: 'wheel_fl', radius: R, steers: true }, { node: 'wheel_fr', radius: R, steers: true },
        { node: 'wheel_rl', radius: R, steers: false }, { node: 'wheel_rr', radius: R, steers: false },
      ],
      steer: 'steer_bars', steerMaxDeg: 38, hull: [1.16, 1.13, 1.85], eye: 1.05,
    };
    return g;
  },
});
