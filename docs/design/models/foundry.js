/**
 * Deep Field 3D — Foundry environment kit (DESIGN-BRIEF §3.6).
 *
 * Rebuilt onto the hero standard (CLAUDE.md) on the shared environment
 * vocabulary in `env-kit.js`. The generation this replaces was box primitives
 * glued to constant sections: a beam that was a solid box, grating that was
 * fourteen fat bars at twenty times the real pitch, a handrail of 100 mm posts
 * carrying an 80 mm rail, a ladle made of two cylinders and a torus. Every
 * member here is a REAL ROLLED SECTION at table dimensions, every repeated
 * small part is merged rather than glued, and the detail is cut into the
 * profile instead of stacked on the face.
 *
 * What was measured against the real class (rule 2):
 *   deck primaries   UB 305×165×54  — 10 m span at walkway loading
 *   deck joists      UB 152×89×16   — 4 m span at 1 m centres
 *   walkway stringers PFC 200×75     — light walkway, toes inward
 *   floor grating    30 × 5 mm bearing bars @ 41 mm, cross rods @ 100 mm
 *   handrail         48.3 OD top @ 1100, 42.4 mid @ 550, 100 mm toe board,
 *                    posts at 1.27 m centres (code wants ≤ 1.5)
 *   road plate       25 mm cast plate, 1.3 % crossfall to a centre channel
 *   column           built-up riveted box: 4 × L150×150×15 + 12 mm web plates
 *   refractory       230 × 76 mm firebrick, 10 mm joints
 *   ladle            20 t lip-pour: 3 m over the rim, 150 mm lining, trunnion
 *                    ring at the balance line
 *
 * The deck's honest structural depth comes out at 0.49 m (30 grating + 152
 * joist + 310 primary), leaving the lane 5.51 m of clear headroom under it —
 * ten millimetres over the §4.10 minimum, and the primaries are precambered
 * UP so live load cannot eat that margin. Ten millimetres is not a crossing,
 * which is why the gantry bridge is still its own 0.34 m walkway piece.
 *
 * Metres, Y-up, origin at footprint centre on the ground. Pieces are modular;
 * the map viewer and GameRoot.cs instance them at the levels.js coordinates.
 */
import { buildSpaceSky } from './space-sky.js';
import { floorMaterials } from './floor-textures.js';
import {
  envSurfaces, SECT, member, bar, grating, meshInfill, rivets, bolts, merge,
  stain, foundrySoot, ep, bake, loft, sweep, revolve, solid, clamp, smooth, lerp,
} from './env-kit.js';

export const FOUNDRY = [];
const P = (o) => { FOUNDRY.push({ ...o, file: o.file || o.id + '.glb', swatch: o.swatch || '#8d99ad', dir: 'maps/foundry/' }); };

/* Rolled sections used across the kit, resolved once. */
const UB305 = SECT.i(.3102, .1665, .0076, .0137, .0089);
const UB152 = SECT.i(.1524, .0889, .0046, .0077, .0076);
const PFC200 = SECT.channel(.2032, .0762, .0076, .0125);
const PFC100 = SECT.channel(.1016, .0508, .0051, .0085);
const L150 = SECT.angle(.150, .015);
const L90 = SECT.angle(.090, .010);
const L75 = SECT.angle(.075, .008);

export function makeFoundryMats(THREE, mats) {
  /* Legacy names. Other kits (Switchyard, Spire, Shared) and levels.js still
     reference these, and they are plain non-vertex-colour materials, so they
     stay exactly as they were rather than being aliased onto the env family. */
  const mat = (name, color, o = {}) => { const m = new THREE.MeshStandardMaterial({ color, roughness: .85, metalness: .1, flatShading: false, ...o }); m.name = name; mats[name] = m; return m; };
  mat('slag', 0x2a2a30, { roughness: .95 });
  mat('slag_pale', 0x3c3a40, { roughness: .95 });
  mat('road_plate', 0x4a4e58, { roughness: .7, metalness: .35 });
  mat('road_seam', 0x22252c, { roughness: .8, metalness: .2 });
  mat('deck_grate', 0x555c6b, { roughness: .6, metalness: .45 });
  mat('brick', 0x5a3a2e, { roughness: .95 });
  mat('molten', 0xff8a1f, { roughness: .3, metalness: 0, emissive: new THREE.Color(0xff6a00), emissiveIntensity: 1.4 });
  mat('ember_glow', 0xe8622b, { roughness: .4, emissive: new THREE.Color(0xe8622b), emissiveIntensity: .9 });
  mat('lamp_warm', 0xfff0c8, { roughness: .3, emissive: new THREE.Color(0xffe2a8), emissiveIntensity: 1.2 });
  mat('steam', 0xd9dde6, { roughness: 1, metalness: 0, transparent: true, opacity: .35, depthWrite: false });
  mat('sky_deep', 0x0d1119, { roughness: 1, metalness: 0, side: THREE.BackSide });
  mat('sky_glow', 0x3a1f14, { roughness: 1, metalness: 0, emissive: new THREE.Color(0x6a2c12), emissiveIntensity: .6, side: THREE.BackSide });
  return envSurfaces(THREE, mats);
}

/* ── shared subassemblies ──────────────────────────────────────────────────
   Built here rather than in env-kit because they are Foundry furniture, not
   vocabulary: a handrail run and a caged lamp recur across four pieces each,
   and duplicating them is how the old kit drifted out of step with itself. */

/**
 * Code-compliant handrail run along X. Posts are capped tube on a bolted base
 * plate, rails are real 48.3/42.4 OD, and the toe board is 100 mm — the old
 * 150 mm plate under an 80 mm rail is what made the deck read as toy-scale.
 */
function handrail(K, name, len, o = {}) {
  const { THREE, mats, grp } = K, g = grp(name, o.pos || [0, 0, 0]);
  if (o.rot) g.rotation.set(...o.rot);
  const span = 1.27, n = Math.max(2, Math.round(len / span));
  const xs = [];
  for (let i = 0; i <= n; i++) xs.push(-len / 2 + len * i / n);
  const list = [], post = member(THREE, 'p', SECT.tube(.0242, 12), 1.10, mats.env_galv, { axis: 'y' });
  for (const x of xs) list.push({ geo: bake(post), m: new THREE.Matrix4().makeTranslation(x, .55, 0) });
  g.add(merge(THREE, list, name + '_posts', mats.env_galv));
  // Domed post caps — an open tube end is the tell of an unfinished handrail.
  g.add(rivets(THREE, name + '_post_caps', [xs[0], 1.10, 0], [xs[xs.length - 1], 1.10, 0], xs.length, .0242, mats.env_galv, [0, 1, 0]));
  g.add(bar(THREE, name + '_rail_top', [-len / 2 - .03, 1.10, 0], [len / 2 + .03, 1.10, 0], .0242, mats.env_galv));
  g.add(bar(THREE, name + '_rail_mid', [-len / 2, .55, 0], [len / 2, .55, 0], .0212, mats.env_galv));
  // Base plates + holding-down bolts, so the rail is fixed to something.
  const plate = member(THREE, 'b', SECT.plate(.12, .010), .12, mats.env_mill, { axis: 'x' });
  g.add(merge(THREE, xs.map((x) => ({ geo: bake(plate), m: new THREE.Matrix4().makeTranslation(x, .005, 0) })), name + '_bases', mats.env_mill));
  g.add(bolts(THREE, name + '_base_bolts', xs.flatMap((x) => [[x, .010, -.042], [x, .010, .042]]), .011, .014, mats.env_mill));
  // Toe board on the outer face, hazard-banded.
  g.add(member(THREE, name + '_toe', SECT.plate(.100, .006), len, mats.env_haz, { axis: 'x', spin: Math.PI / 2, pos: [0, .053, -.030], uScale: 1.2, vScale: 1.2 }));
  if (o.infill) g.add(meshInfill(THREE, name + '_infill', len - .06, .50, mats.env_mesh, { pos: [0, .80, -.004] }));
  return g;
}

/** Caged bulkhead lamp: hood, lens, and a guard of real bent wire. */
function cagedLamp(K, name, pos, rot = [0, 0, 0], lit = true) {
  const { THREE, mats, grp } = K, g = grp(name, pos);
  g.rotation.set(...rot);
  g.add(revolve(THREE, name + '_body', [
    [0, -.070], [.048, -.070], [.052, -.058], [.052, -.016], [.062, -.008], [.062, .004],
  ], mats.env_cast, { segs: 20, creaseAngle: 26, scale: 6 }));
  g.add(revolve(THREE, name + '_lens', [
    [.058, .004], [.052, .030], [.034, .048], [0, .054],
  ], lit ? mats.env_lamp : mats.env_lamp_off, { segs: 20, scale: 6 }));
  // Guard: four bent ribs rooted in the body rim, plus a ring — real wire, so
  // it casts the striped shadow a caged lamp is recognised by.
  for (let i = 0; i < 4; i++) {
    const a = i / 4 * Math.PI * 2, c = Math.cos(a), s = Math.sin(a);
    g.add(sweep(THREE, `${name}_guard_${i}`, [
      [c * .056, s * .056, -.004], [c * .064, s * .064, .026], [c * .030, s * .030, .062], [0, 0, .070],
    ], () => .0035, mats.env_galv, { radial: 6, samples: 18, up: [0, 0, 1] }));
  }
  // Rings of the guard, as real square-section wire rather than a one-sided
  // band — a two-point profile has no back, so it vanishes from below.
  g.add(revolve(THREE, name + '_guard_ring', [[.050, -.0035], [.0535, -.0035], [.0535, .0035], [.050, .0035], [.050, -.0035]],
    mats.env_galv, { segs: 18, pos: [0, 0, .034], scale: 8 }));
  /* Lens toward local +Z; the final quarter-turn points that at the FLOOR.
     The first pass turned it the other way and hung every lamp in the kit
     upside down, shining at the sky. */
  g.rotateX(Math.PI / 2);
  return g;
}

/** Pipe run with real weld-neck flanges, proud weld beads and saddle clamps. */
function pipeRun(K, name, len, r, mat, o = {}) {
  const { THREE, mats, grp } = K, g = grp(name, o.pos || [0, 0, 0]);
  g.add(member(THREE, name + '_pipe', SECT.tube(r, 18), len, mat, { axis: o.axis || 'x', uScale: 2, vScale: 2 }));
  const f = o.flanges ?? 2, list = [];
  // A flange is a hub taper out to a raised face, not a disc — the taper is
  // most of what makes pipework read as pipework.
  const fl = revolve(THREE, 'f', [
    [r, -.052], [r * 1.10, -.048], [r * 1.42, -.020], [r * 1.62, -.014], [r * 1.62, .014], [r * 1.42, .020], [r * 1.10, .048], [r, .052],
  ], mats.env_mill, { segs: 22, creaseAngle: 26, scale: 5 });
  const bead = revolve(THREE, 'w', [[r, -.006], [r * 1.07, -.002], [r * 1.07, .002], [r, .006]], mats.env_mill, { segs: 20, scale: 8 });
  const ax = o.axis === 'z' ? 'z' : 'x';
  for (let i = 0; i < f; i++) {
    const t = f === 1 ? 0 : -len / 2 + .10 + (len - .20) * i / (f - 1);
    const m = new THREE.Matrix4().makeRotationY(ax === 'x' ? Math.PI / 2 : 0);
    m.setPosition(ax === 'x' ? t : 0, 0, ax === 'x' ? 0 : t);
    list.push({ geo: bake(fl), m });
  }
  for (const t of o.welds || []) {
    const m = new THREE.Matrix4().makeRotationY(ax === 'x' ? Math.PI / 2 : 0);
    m.setPosition(ax === 'x' ? t : 0, 0, ax === 'x' ? 0 : t);
    list.push({ geo: bake(bead), m });
  }
  if (list.length) g.add(merge(THREE, list, name + '_fittings', mats.env_mill));
  return g;
}

/* ══ terrain ══════════════════════════════════════════════════════════════ */

P({ id: 'foundry_terrain', label: 'Terrain tile (20 m)', size: '20 m', swatch: '#2a2a30', stats: { Tile: '20×20', Grid: '6×4 tiles', Relief: '±0.12 m', Details: 'gully · hatch · conduit · slag pool' },
  note: 'Tileable cast-iron floor: 8×8 riveted plates baked into a 1024² albedo/roughness/normal set (soot, rust bleed, scorch rings, tread ridges, stencilled bay numbers) with an emissive map for the seams still running molten. Four variants — rotation plus a different set of real details: a gully with a cast angle frame and 41/100 grating over a tapered sump, a bolted access cover with a keyway and a bent lifting bar, a conduit on saddle clamps with swaged couplings, and a cooled slag pool with a crust rim. Flat edges so tiles seam; nothing stands above 0.16 m.',
  build(K) { return this.buildVariant(K, 0); },
  buildVariant(K, v = 0) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_terrain'), F = floorMaterials(THREE);
    const geo = new THREE.PlaneGeometry(20, 20, 24, 24); geo.rotateX(-Math.PI / 2);
    const p = geo.attributes.position;
    for (let i = 0; i < p.count; i++) {
      const x = p.getX(i), z = p.getZ(i), edge = Math.abs(x) > 9.9 || Math.abs(z) > 9.9;
      p.setY(i, edge ? 0 : (Math.sin(x * 1.3 + v) * Math.cos(z * .9 - v) * .09 + Math.sin(x * 3.1 + z * 2.2) * .025));
    }
    geo.computeVertexNormals();
    const ground = ep(K, 'terrain_ground', geo, F.foundry); ground.rotation.y = v * Math.PI / 2; ground.receiveShadow = true; g.add(ground);

    /* Gully: a cast angle frame set flush, a tapered sump cut in below it, and
       a grating panel dropped into the rebate. The old version glued a box
       frame and seven fat bars on top of the floor. */
    const gully = (name, x, z) => {
      const d = grp(name, [x, 0, z]);
      for (let i = 0; i < 4; i++) d.add(member(THREE, `${name}_frame_${i}`, L90, 1.34, mats.env_cast,
        { axis: 'x', spin: i * Math.PI / 2, pos: [0, -.045, 0], rot: [0, i * Math.PI / 2, 0] }));
      // Sump: walls batter inward so it reads as a cast pit with depth.
      d.add(loft(THREE, name + '_sump', [-.46, -.30, -.02].map((y, i) => ({
        z: y, pts: SECT.sq(1.16 - i * .14, .05).map(([a, b]) => [a, b]),
      })), mats.env_slag, { rot: [-Math.PI / 2, 0, 0], uScale: 1.4, vScale: 1.4 }));
      d.add(ep(K, name + '_glow', new THREE.PlaneGeometry(1.0, 1.0), mats.env_molten_skin, [0, -.44, 0], [-Math.PI / 2, 0, 0]));
      d.add(grating(THREE, name + '_grate', 1.10, 1.10, mats.env_cast, { pos: [0, -.012, 0], depth: .034 }));
      return d;
    };
    /* Access cover: a real frame profile with a seating rebate, a dished lid
       with a keyway, and a lifting bar bent from round stock — rooted into the
       lid so it is not a torus floating over a disc. */
    const hatch = (name, x, z) => {
      const d = grp(name, [x, 0, z]);
      d.add(revolve(THREE, name + '_frame', [
        [.72, -.10], [.98, -.10], [.98, -.02], [.90, -.02], [.90, .04], [.80, .04], [.80, -.005], [.72, -.005],
      ], mats.env_cast, { segs: 26, creaseAngle: 24, scale: 3, rot: [-Math.PI / 2, 0, 0] }));
      d.add(revolve(THREE, name + '_lid', [
        [0, .030], [.52, .034], [.70, .046], [.78, .052], [.80, .038], [.80, .000], [.60, -.004], [0, -.004],
      ], mats.env_tread, { segs: 30, creaseAngle: 22, scale: 3, rot: [-Math.PI / 2, 0, 0],
        wear: (r) => smooth(.10, .60, r) * .5 }));
      const ring = [];
      for (let i = 0; i < 14; i++) { const a = i / 14 * Math.PI * 2; ring.push([Math.cos(a) * .86, .024, Math.sin(a) * .86]); }
      d.add(bolts(THREE, name + '_frame_bolts', ring, .018, .016, mats.env_cast));
      d.add(sweep(THREE, name + '_lift_bar', [
        [-.17, .020, 0], [-.15, .072, 0], [0, .092, 0], [.15, .072, 0], [.17, .020, 0],
      ], () => .014, mats.env_mill, { radial: 8, samples: 26, up: [0, 0, 1] }));
      return d;
    };
    const conduit = (name, x, z, len, rot) => {
      const d = grp(name, [x, 0, z]); d.rotation.y = rot;
      d.add(member(THREE, name + '_pipe', SECT.tube(.070, 16), len, mats.env_brass, { axis: 'x', pos: [0, .105, 0], uScale: 2, vScale: 2 }));
      const cpl = revolve(THREE, 'c', [[.070, -.036], [.082, -.030], [.082, .030], [.070, .036]], mats.env_brass, { segs: 18, scale: 6 });
      const sad = member(THREE, 's', L75, .16, mats.env_mill, { axis: 'z' });
      const list = [];
      for (let i = 0; i < Math.floor(len / 2.4); i++) {
        const x0 = -len / 2 + 1.2 + i * 2.4;
        list.push({ geo: bake(cpl), m: new THREE.Matrix4().makeRotationY(Math.PI / 2).setPosition(x0, .105, 0) });
        list.push({ geo: bake(sad), m: new THREE.Matrix4().makeTranslation(x0, .045, 0) });
      }
      if (list.length) d.add(merge(THREE, list, name + '_fittings', mats.env_mill));
      return d;
    };
    /* Cooled pool: a crust rim that rolled up as it froze, a vitreous skin, and
       veins still live under it. Revolved, so the rim is a real lip. */
    const pool = (name, x, z, r) => {
      const d = grp(name, [x, 0, z]);
      d.add(revolve(THREE, name + '_crust', [
        [r * .55, .014], [r * .88, .022], [r * 1.02, .048], [r * 1.10, .036], [r * 1.06, .008], [r * .92, .002],
      ], mats.env_clinker, { segs: 26, creaseAngle: 30, scale: 2.2, rot: [-Math.PI / 2, 0, 0] }));
      d.add(revolve(THREE, name + '_skin', [[0, .020], [r * .45, .019], [r * .80, .014], [r * .96, .012]],
        F.slag_glass, { segs: 26, scale: 2.2, rot: [-Math.PI / 2, 0, 0] }));
      for (let i = 0; i < 3; i++) d.add(loft(THREE, `${name}_vein_${i}`,
        [0, .35, .7, 1].map((t) => ({ z: -r + 2 * r * t, pts: SECT.plate(.052 * (1 - Math.abs(t - .5)), .008) })),
        mats.env_molten, { rot: [-Math.PI / 2, i * 1.1, 0], pos: [0, .022, 0], uScale: 3, vScale: 3 }));
      return d;
    };
    [
      () => { g.add(gully('terrain_gully_a', -6.2, 3.8)); g.add(hatch('terrain_hatch', 5.5, -5.5)); },
      () => { g.add(gully('terrain_gully_a', 4.2, 6.5)); g.add(conduit('terrain_conduit', -3, -7.4, 12, 0)); g.add(pool('terrain_pool', -6.8, 2.0, .9)); },
      () => { g.add(hatch('terrain_hatch', -4.5, -4.5)); g.add(gully('terrain_gully_a', 6.8, -1.2)); g.add(gully('terrain_gully_b', 6.8, 1.4)); },
      () => { g.add(pool('terrain_pool', 3.5, 4.5, 1.2)); g.add(conduit('terrain_conduit', 7.5, 0, 14, Math.PI / 2)); },
    ][v % 4]();
    return stain(THREE, g, foundrySoot({ reach: 4 }));
  } });

P({ id: 'foundry_terrain_scatter', label: 'Terrain scatter (boulders + crack)', size: '6 m', swatch: '#3c3a40', stats: { Boulders: '5', Crack: 'lofted, molten core', Place: 'off-route' },
  note: 'Slag boulder cluster and a cooled-lava seam. The boulders are noise-displaced solids rather than regular polyhedra — a dodecahedron reads as a dice at any distance — and the crack is a lofted fissure that narrows and shallows along its length with clinker lips and a molten core set down inside it, not an emissive plate lying on the floor. Scatter by hand in levels.js, never within 2.5 m of a route, socket or placed piece.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_terrain_scatter');
    /* A fissure: lips standing proud, walls falling away inside, and the glow
       down in the bottom where it belongs. */
    const crack = (name, len, w0, rot, pos) => {
      const st = [];
      for (let i = 0; i <= 10; i++) {
        const t = i / 10, taper = Math.sin(Math.PI * (.12 + .88 * t)) ** .6;
        const w = w0 * taper, dep = -.10 * taper;
        st.push({ z: -len / 2 + len * t, pts: [
          [-w, .012], [-w * .55, .020], [-w * .22, dep], [0, dep * 1.15], [w * .22, dep], [w * .55, .020], [w, .012],
          [w * .92, -.004], [-w * .92, -.004],
        ] });
      }
      const d = grp(name, pos); d.rotation.y = rot;
      d.add(loft(THREE, name + '_lips', st, mats.env_clinker, { creaseAngle: 34, uScale: 2.2, vScale: 2.2 }));
      d.add(loft(THREE, name + '_core', st.map((s) => ({ z: s.z, pts: SECT.plate(Math.abs(s.pts[3][0]) * .7 + .02, .014) })),
        mats.env_molten, { pos: [0, -.075, 0], uScale: 3, vScale: 3 }));
      return d;
    };
    g.add(crack('scatter_crack', 5, .17, .4, [0, 0, 0]));
    g.add(crack('scatter_crack_b', 3, .12, -.6, [1.2, 0, -1.5]));
    /* Boulders: an icosahedron pushed around by its own noise field, so the
       silhouette is irregular and the facets are not a regular solid's. */
    [[-2, .6], [-.6, .4], [1.5, .75], [2.4, .35], [.4, .5]].forEach(([x, r], i) => {
      const geo = new THREE.IcosahedronGeometry(r, 2), pp = geo.attributes.position, V = new THREE.Vector3();
      for (let k = 0; k < pp.count; k++) {
        V.fromBufferAttribute(pp, k);
        const n = .68 + .52 * Math.abs(Math.sin(V.x * 6.1 + i) * Math.cos(V.z * 5.3 - i) + .4 * Math.sin(V.y * 9.7));
        V.multiplyScalar(clamp(n, .62, 1.24)); pp.setXYZ(k, V.x, V.y * .78, V.z);
      }
      geo.computeVertexNormals();
      g.add(ep(K, 'scatter_boulder' + i, geo, i % 2 ? mats.env_clinker : mats.env_slag,
        [x, r * .70, i % 2 ? 1.2 : -1.0], [i * .4, i * .7, 0]));
    });
    return stain(THREE, g, foundrySoot({ heat: [0, -.1, 0], reach: 3 }));
  } });

/* ══ roadway ══════════════════════════════════════════════════════════════ */

P({ id: 'foundry_path_ground', label: 'Roadway plate (4 m)', size: '4×3.4 m', swatch: '#4a4e58', stats: { Lane: '3.4 m', Plate: '25 mm cast', Crossfall: '1.3 % to centre', Drain: '41/100 slot' },
  note: 'One 4 m run of the 3.4 m ground lane, built as a real cast bay: 25 mm plate with a 1.3 % crossfall falling to a centre drain channel (a foundry bay drains to the middle, so the camber runs inward, not outward), a slot grating over the channel, and battered kerbs both sides with a hazard band on the outer face. The teardrop tread is a 35 mm-pitch pattern in the height map, worn flat down the middle of the run where traffic goes — 40 000 proud teardrops per bay is not a model. Bays lap at a bolted joint strap so consecutive runs interlock instead of butting.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_path_ground');
    /* Lane cross-section. Profile X runs across the lane, Y is height. */
    const top = (x) => {
      const a = Math.abs(x);
      if (a < .080) return .058;                       // channel invert
      if (a < .100) return lerp(.058, .078, (a - .080) / .020);
      return lerp(.078, .112, smooth(.100, 1.560, a)); // crossfall inward
    };
    const half = [];
    for (let i = 0; i <= 22; i++) { const x = 1.66 * i / 22; half.push([x, top(x)]); }
    const lip = [[1.66, .112], [1.70, .104], [1.70, 0]];
    const slab = [
      ...half.slice().reverse().map(([x, y]) => [-x, y]), ...half.slice(1), ...lip,
      [-1.70, 0], [-1.70, .104], [-1.66, .112],
    ];
    // Reorder into one closed contour: bottom edge, then the top face back.
    const face = [...half.slice().reverse().map(([x, y]) => [-x, y]), ...half.slice(1)];
    const contour = [[-1.70, 0], [1.70, 0], [1.70, .104], ...face.slice().reverse(), [-1.70, .104]];
    g.add(member(THREE, 'road_plate', contour, 4, mats.env_tread, {
      axis: 'x', stations: 4, uScale: 1.4, vScale: 1.4,
      // Cast bays sag a little between their bearings and the arrises wear down.
      at: (t, pts) => pts.map(([x, y]) => [x, y - Math.sin(Math.PI * t) * .0035 * smooth(1.70, .2, Math.abs(x))]),
      wear: (x, y) => smooth(.10, .95, Math.abs(x)) * smooth(.06, .11, y) * .45,
    }));
    /* Kerb: battered outer face, chamfered top arris, rooted at the bedding so
       there is no daylight between kerb and slab (rule 3). Mirrored for the
       far side rather than spun about the run — spinning it turned the kerb
       upside down and buried 180 mm of it. */
    const kerb = [[-.080, 0], [.080, 0], [.080, .150], [.056, .180], [-.062, .180], [-.080, .120]];
    const mirrorX = (p) => p.map(([x, y]) => [-x, y]);
    for (const s of [-1, 1]) {
      g.add(member(THREE, `road_kerb${s > 0 ? '_p' : '_n'}`, s > 0 ? kerb : mirrorX(kerb), 4, mats.env_cast,
        { axis: 'x', pos: [0, 0, s * 1.78], uScale: 1.6, vScale: 1.6 }));
      g.add(member(THREE, `road_kerb_haz${s > 0 ? '_p' : '_n'}`, SECT.plate(.11, .004), 3.9, mats.env_haz,
        { axis: 'x', spin: Math.PI / 2, pos: [0, .112, s * 1.862], uScale: 1.1, vScale: 1.1 }));
    }
    g.add(grating(THREE, 'road_drain', 3.96, .160, mats.env_cast, { pos: [0, .076, 0], depth: .034, pitch: .041 }));
    /* Bolted joint strap at the −X end: the lap that makes bays interlock. */
    g.add(member(THREE, 'road_joint_strap', SECT.plate(.180, .012), 3.36, mats.env_mill,
      { axis: 'z', spin: Math.PI / 2, pos: [-1.985, .100, 0], rot: [0, 0, 0] }));
    const jb = [];
    for (let i = 0; i < 9; i++) jb.push([-1.985, .112, -1.50 + i * .375]);
    g.add(bolts(THREE, 'road_joint_bolts', jb, .016, .014, mats.env_mill));
    // Rivet rows down the plate edges, inboard of the kerbs.
    for (const s of [-1, 1]) g.add(rivets(THREE, `road_rivets${s > 0 ? '_p' : '_n'}`,
      [-1.86, .110, s * 1.59], [1.86, .110, s * 1.59], 13, .017, mats.env_cast, [0, 1, 0]));
    return stain(THREE, g, foundrySoot({ reach: 3, track: { z: 0, w: 1.25, y: .10 } }));
  } });

/* ══ upper deck ═══════════════════════════════════════════════════════════ */

P({ id: 'foundry_deck', label: 'Upper deck section (4 m)', size: '4×10 m', swatch: '#555c6b', stats: { Top: 'y +6.00', Soffit: 'y 5.51', Primaries: 'UB 305×165', Joists: 'UB 152×89 @ 1 m' },
  note: 'One 4 m bay of the 28×10 m upper deck, walking surface at y 6.00. Two UB 305×165 primaries span the 10 m along Z, precambered 6 mm UP so live load cannot eat the headroom below; eleven UB 152×89 joists cross them at 1 m centres on bolted cleat angles; 41/100 open grating spans the 1 m bays, so you see the lane through the deck and the light falls through in stripes. Fascia plate and hazard band on the free edges, riveted flange rows, and a caged lamp on a bracket under the edge. Structural depth is 0.49 m, so the soffit sits at 5.51 — ten millimetres over the §4.10 minimum, which is margin, not a crossing: the gantry bridge stays its own 0.34 m walkway. Instance 7 along X at x −12…12 (centre x 2, z −17).',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_deck');
    const TOP = 6.00, GR = .030, JT = TOP - GR, JC = JT - .0762, PT = JT - .1524, PC = PT - .1551;
    for (const s of [-1, 1]) {
      g.add(member(THREE, `deck_primary${s > 0 ? '_p' : '_n'}`, UB305, 10, mats.env_mill,
        { axis: 'z', pos: [s * 1.94, PC, 0], sag: -.006 }));
      g.add(rivets(THREE, `deck_primary_rivets${s > 0 ? '_p' : '_n'}`,
        [s * 1.94, PT - .022, -4.8], [s * 1.94, PT - .022, 4.8], 22, .015, mats.env_mill, [0, 1, 0]));
    }
    const zs = [];
    for (let i = 0; i < 11; i++) zs.push(-5 + i);
    const joist = member(THREE, 'j', UB152, 3.88, mats.env_mill, { axis: 'x' });
    const cleat = member(THREE, 'c', L90, .130, mats.env_mill, { axis: 'y' });
    const jl = [], cl = [];
    for (const z of zs) {
      jl.push({ geo: bake(joist), m: new THREE.Matrix4().makeTranslation(0, JC, z) });
      for (const s of [-1, 1]) cl.push({ geo: bake(cleat), m: new THREE.Matrix4().makeTranslation(s * 1.86, JC, z) });
    }
    g.add(merge(THREE, jl, 'deck_joists', mats.env_mill));
    g.add(merge(THREE, cl, 'deck_cleats', mats.env_mill));
    g.add(bolts(THREE, 'deck_cleat_bolts', zs.flatMap((z) => [[-1.90, JC + .035, z], [-1.90, JC - .035, z], [1.90, JC + .035, z], [1.90, JC - .035, z]]), .011, .012, mats.env_mill, [1, 0, 0]));
    // Grating in 10 panels, one per joist bay — real panels have real joints.
    for (let i = 0; i < 10; i++) g.add(grating(THREE, 'deck_grating' + i, 3.86, 1.0, mats.env_galv, { pos: [0, TOP, -4.5 + i] }));
    /* Free edges: fascia plate, toe angle, hazard band. This is the edge read
       from the lane below, so it carries the piece's silhouette. */
    for (const s of [-1, 1]) {
      const sg = s > 0 ? '_p' : '_n';
      g.add(member(THREE, 'deck_fascia' + sg, SECT.plate(.300, .008), 4, mats.env_mill,
        { axis: 'x', spin: Math.PI / 2, pos: [0, TOP - .150, s * 4.96] }));
      g.add(member(THREE, 'deck_fascia_haz' + sg, SECT.plate(.140, .003), 3.9, mats.env_haz,
        { axis: 'x', spin: Math.PI / 2, pos: [0, TOP - .075, s * 4.968], uScale: 1.1, vScale: 1.1 }));
      g.add(member(THREE, 'deck_edge_angle' + sg, L90, 4, mats.env_mill,
        { axis: 'x', spin: s > 0 ? 0 : Math.PI, pos: [0, TOP - .038, s * 4.92] }));
    }
    g.add(member(THREE, 'deck_end_beam', UB152, 9.9, mats.env_mill, { axis: 'z', pos: [-1.99, JC, 0] }));
    // Service lamp on a bracket under the edge.
    g.add(member(THREE, 'deck_lamp_bracket', L75, .46, mats.env_mill, { axis: 'z', pos: [0, TOP - .26, 4.80] }));
    g.add(cagedLamp(K, 'deck_lamp', [0, TOP - .30, 5.02], [0, 0, 0]));
    return stain(THREE, g, foundrySoot({ lift: 4.2, track: { z: 0, w: 3.2, y: TOP } }));
  } });

P({ id: 'foundry_deck_rail', label: 'Deck guard rail (4 m)', size: '4 m', swatch: '#8d99ad', stats: { Height: '1.10 m', Rails: '48.3 / 42.4 OD', Posts: '4 @ 1.27 m', Toe: '100 mm' },
  note: 'One 4 m run of deck rail to real handrail geometry: capped 48.3 OD tube posts at 1.27 m centres on bolted base plates, a 48.3 top rail at 1100 and a 42.4 mid rail at 550, and a 100 mm toe board with a hazard band. The old run put an 80 mm rail on 100 mm square posts over a 150 mm plate, which is what made the deck read a size too small for the player. Sits on the deck’s +Z edge at z −11.2; leave bays open where the ladder and zipline anchor land.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_deck_rail');
    g.add(handrail(K, 'rail', 4));
    // Knee braces at the run ends — a 4 m run needs them and they read.
    for (const s of [-1, 1]) g.add(bar(THREE, `rail_brace${s > 0 ? '_p' : '_n'}`,
      [s * 1.90, 1.02, 0], [s * 1.62, .14, -.20], .016, mats.env_galv));
    return stain(THREE, g, foundrySoot({ lift: 4.4 }));
  } });

P({ id: 'foundry_gantry_walk', label: 'Gantry walkway (4 m)', size: '4×4 m', swatch: '#6b7484', stats: { Deck: '41/100 grating', Depth: '0.34 m', Stringers: 'PFC 200×75', Rails: 'both edges' },
  note: 'The bridge that crosses the lane, authored with its walking surface AT the mount point so it sits at the height the pads are on. Two PFC 200×75 channel stringers with toes inward carry 41/100 grating; PFC 100 transoms under them at 1 m bring the total to 0.34 m from walking surface to underside — a deck bay is 0.69 m of real section and could not both sit flush at y 6 and leave the lane its 5.5 m of headroom, which is why this is a separate light walkway. Handrail with weldmesh infill on both long edges, hazard nosing angles at both ends. Square, so it tiles either way.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_gantry_walk');
    const GR = .030, ST = -GR, SC = ST - .1016, TR = SC - .1016 - .0508;
    for (const s of [-1, 1]) g.add(member(THREE, `walk_stringer${s > 0 ? '_p' : '_n'}`, PFC200, 4, mats.env_mill,
      { axis: 'z', pos: [s * 1.93, SC, 0], spin: s > 0 ? Math.PI : 0 }));
    const tr = member(THREE, 't', PFC100, 3.86, mats.env_mill, { axis: 'x' });
    g.add(merge(THREE, [-1.5, -.5, .5, 1.5].map((z) => ({ geo: bake(tr), m: new THREE.Matrix4().makeTranslation(0, TR, z) })), 'walk_transoms', mats.env_mill));
    for (let i = 0; i < 4; i++) g.add(grating(THREE, 'walk_grating' + i, 3.80, 1.0, mats.env_galv, { pos: [0, 0, -1.5 + i * 1.0] }));
    for (const s of [-1, 1]) {
      g.add(handrail(K, `walk_rail${s > 0 ? '_p' : '_n'}`, 4, { pos: [0, 0, s * 1.95], rot: [0, s > 0 ? 0 : Math.PI, 0], infill: true }));
      g.add(member(THREE, `walk_nose${s > 0 ? '_p' : '_n'}`, L90, 3.86, mats.env_haz,
        { axis: 'x', spin: s > 0 ? -Math.PI / 2 : Math.PI / 2, pos: [0, -.006, s * 1.955], uScale: 1.2, vScale: 1.2 }));
    }
    g.add(rivets(THREE, 'walk_stringer_rivets_n', [-1.90, SC, -1.8], [-1.90, SC, 1.8], 13, .014, mats.env_mill, [-1, 0, 0]));
    g.add(rivets(THREE, 'walk_stringer_rivets_p', [1.90, SC, -1.8], [1.90, SC, 1.8], 13, .014, mats.env_mill, [1, 0, 0]));
    return stain(THREE, g, foundrySoot({ lift: 4.4, track: { z: 0, w: 1.4, y: 0 } }));
  } });

/* ══ column ═══════════════════════════════════════════════════════════════ */

P({ id: 'foundry_pillar', label: 'Deck pillar', size: '1.2×5.51 m', swatch: '#3e4a63', stats: { At: 'x −10, 14 · z −17', Head: 'y 5.51 (deck soffit)', Section: 'built-up riveted box' },
  note: 'A built-up riveted box column the way they were actually made: four L150×150×15 corner angles with 12 mm web plates between them, cover-plate laps stepping the section at 1.3 m intervals, and riveted rows up every corner. Cap plate on four gusset brackets, base plate with holding-down bolts on a grout plinth, and a conduit run on saddle clamps up one face into a junction box with a lit lens. The old column was a plain box with three bands glued round it. The head lands at 5.51, which is the deck soffit; the two instances under the gantry walkway (soffit 5.67) take a 160 mm bearing packer.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_pillar');
    const H = 5.167, BASE = .30;
    g.add(member(THREE, 'pillar_plinth', SECT.plate(1.95, .14), 1.95, mats.env_refract_burnt, { axis: 'x', pos: [0, .07, 0], uScale: 1.2, vScale: 1.2 }));
    g.add(member(THREE, 'pillar_base_plate', SECT.plate(1.80, .045), 1.80, mats.env_mill, { axis: 'x', pos: [0, .1625, 0] }));
    g.add(bolts(THREE, 'pillar_base_bolts', [[-.78, .185, -.78], [.78, .185, -.78], [-.78, .185, .78], [.78, .185, .78],
      [0, .185, -.80], [0, .185, .80], [-.80, .185, 0], [.80, .185, 0]], .026, .038, mats.env_mill));
    /* Four corner angles at the CORNERS and four web plates on the FACES — the
       two families sit on different radii and different angles, and putting
       the angles on the face centres (as the first pass did) pushed a web
       plate a metre out of the column. */
    for (let i = 0; i < 4; i++) {
      const a = Math.PI / 4 + i * Math.PI / 2, cx = Math.cos(a) * .742, cz = Math.sin(a) * .742;
      g.add(member(THREE, `pillar_angle_${i}`, L150, H, mats.env_mill,
        { axis: 'y', spin: a + Math.PI * .75, pos: [cx, BASE + H / 2, cz] }));
      const f = i * Math.PI / 2;
      g.add(member(THREE, `pillar_web_${i}`, SECT.plate(.90, .012), H, mats.env_mill,
        { axis: 'y', spin: f, pos: [Math.sin(f) * .594, BASE + H / 2, Math.cos(f) * .594] }));
      // Rivet rows: one up each corner, on the outer face of the angle.
      g.add(rivets(THREE, `pillar_rivets_${i}`, [cx * .92, BASE + .18, cz * .92],
        [cx * .92, BASE + H - .18, cz * .92], 18, .018, mats.env_mill, [Math.cos(a), 0, Math.sin(a)]));
    }
    /* Cover-plate laps: the section genuinely steps out where plates overlap. */
    const lap = member(THREE, 'l', SECT.plate(1.26, .014), 1.26, mats.env_mill, { axis: 'x' });
    g.add(merge(THREE, [1.30, 2.60, 3.90].map((y) => ({ geo: bake(lap), m: new THREE.Matrix4().makeTranslation(0, BASE + y, 0) })), 'pillar_laps', mats.env_mill));
    for (const y of [1.30, 2.60, 3.90]) g.add(rivets(THREE, 'pillar_lap_rivets' + Math.round(y * 10),
      [-.56, BASE + y + .009, .60], [.56, BASE + y + .009, .60], 9, .016, mats.env_mill, [0, 0, 1]));
    /* Cap on gusset brackets — a bearing that shows how the load gets in. */
    g.add(member(THREE, 'pillar_cap', SECT.plate(1.80, .040), 1.80, mats.env_mill, { axis: 'x', pos: [0, BASE + H + .02, 0] }));
    for (let i = 0; i < 4; i++) {
      const a = i * Math.PI / 2;
      g.add(member(THREE, `pillar_gusset_${i}`, [[-.012, -.28], [.012, -.28], [.012, .28], [-.012, .28]], .40, mats.env_mill,
        { axis: 'z', rot: [0, a, 0], pos: [Math.cos(a) * .44, BASE + H - .26, Math.sin(a) * .44],
          at: (t, pts) => pts.map(([x, y]) => [x, y * (1 - .55 * t)]) }));
    }
    // Conduit + junction box up the +Z face.
    g.add(member(THREE, 'pillar_conduit', SECT.tube(.032, 14), H - .30, mats.env_galv, { axis: 'y', pos: [.40, BASE + H / 2, .655], uScale: 2.4, vScale: 2.4 }));
    const sad = member(THREE, 's', L75, .12, mats.env_mill, { axis: 'x' });
    g.add(merge(THREE, [.9, 2.0, 3.1, 4.2].map((y) => ({ geo: bake(sad), m: new THREE.Matrix4().makeTranslation(.40, BASE + y, .625) })), 'pillar_conduit_clamps', mats.env_mill));
    g.add(member(THREE, 'pillar_junction', [[-.15, -.11], [.15, -.11], [.15, .11], [-.15, .11]], .13, mats.env_cast,
      { axis: 'z', pos: [.40, BASE + 2.60, .695], at: (t, pts) => pts.map(([x, y]) => [x * (1 - .10 * t), y * (1 - .10 * t)]) }));
    g.add(revolve(THREE, 'pillar_junction_lens', [[0, .010], [.026, .010], [.030, .002], [.026, -.004]], mats.env_ember,
      { segs: 16, pos: [.40, BASE + 2.60, .762], rot: [0, 0, 0], scale: 8 }));
    return stain(THREE, g, foundrySoot({ lift: 1.2 }));
  } });

/* ══ vent tunnel ══════════════════════════════════════════════════════════ */

P({ id: 'foundry_vent_tunnel', label: 'Vent tunnel (14 m)', size: '3.6×1.5×14 m', swatch: '#3e4a63', stats: { At: 'x 0 · z −6.5', Roof: 'y 1.52', Walls: 'corrugated 76/19' },
  note: 'The hero-only shortcut under the deck. The duct walls are genuinely fluted — the corrugation is the lofted cross-section at the real 76 mm pitch × 19 mm depth, not a flat box with ribs glued on — with L75 angle ring frames every 1.8 m, seven 2 m grating panels down the middle so the floor has real joints, a caged lamp every 3.5 m, a pipe run on saddle clamps along one wall, and hazard nosing angles at both mouths. Mount `shared_vent_grate` at each end.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_vent_tunnel');
    for (const s of [-1, 1]) g.add(member(THREE, `tunnel_wall${s > 0 ? '_p' : '_n'}`, SECT.corrug(1.40, .012), 14, mats.env_mill,
      { rot: [0, 0, Math.PI / 2], pos: [s * 1.80, .70, 0], uScale: 1.5, vScale: 1.5 }));
    g.add(member(THREE, 'tunnel_roof', SECT.corrug(3.62, .016, .11, .026), 14, mats.env_mill,
      { axis: 'z', pos: [0, 1.52, 0], uScale: 1.5, vScale: 1.5 }));
    /* Ring frames: two stanchions and a rafter of real angle per ring. */
    const st = member(THREE, 'a', L75, 1.40, mats.env_mill, { axis: 'y' });
    const rf = member(THREE, 'b', L75, 3.70, mats.env_mill, { axis: 'x' });
    const list = [];
    for (let i = 0; i < 8; i++) {
      const z = -6.3 + i * 1.8;
      list.push({ geo: bake(rf), m: new THREE.Matrix4().makeTranslation(0, 1.435, z) });
      for (const s of [-1, 1]) list.push({ geo: bake(st), m: new THREE.Matrix4().makeRotationY(s > 0 ? 0 : Math.PI).setPosition(s * 1.865, .70, z) });
    }
    g.add(merge(THREE, list, 'tunnel_ring_frames', mats.env_mill));
    for (let i = 0; i < 7; i++) g.add(grating(THREE, 'tunnel_floor' + i, 1.20, 2.0, mats.env_galv, { pos: [0, .034, -6 + i * 2] }));
    for (let i = 0; i < 4; i++) g.add(cagedLamp(K, 'tunnel_lamp' + i, [0, 1.40, -5.25 + i * 3.5], [Math.PI / 2, 0, 0]));
    for (let i = 0; i < 3; i++) g.add(pipeRun(K, 'tunnel_pipe' + i, 14, .072 - i * .016, mats.env_brass,
      { axis: 'z', pos: [1.54, .52 + i * .30, 0], flanges: 3, welds: [-4.4, 2.1] }));
    const sad = member(THREE, 's', L75, .16, mats.env_mill, { axis: 'x' });
    g.add(merge(THREE, [-5.4, -1.8, 1.8, 5.4].map((z) => ({ geo: bake(sad), m: new THREE.Matrix4().makeTranslation(1.66, .70, z) })), 'tunnel_pipe_clamps', mats.env_mill));
    for (const z of [-7, 7]) g.add(member(THREE, 'tunnel_lip' + (z > 0 ? '_p' : '_n'), L90, 3.60, mats.env_haz,
      { axis: 'x', spin: Math.PI / 4, pos: [0, 1.56, z], uScale: 1.2, vScale: 1.2 }));
    return stain(THREE, g, foundrySoot({ reach: 5, track: { z: 0, w: .55, y: .034 } }));
  } });

/* ══ boundary wall ════════════════════════════════════════════════════════ */

P({ id: 'foundry_wall_boundary', label: 'Boundary wall (10 m)', size: '10×6 m', swatch: '#5a3a2e', stats: { Perimeter: '110×80', Height: '6 m', Brick: '230×76 @ 10 mm', Arch: 'segmental, 13 voussoirs' },
  note: 'Refractory brick wall on a baked 2.5 m brick set (stretcher courses, glazed and soot-blackened bricks, crazing, salt bloom, mortar joints in the normal map). Real geometry breaks the face: a stepped stone plinth, three BATTERED buttresses — lofted so they genuinely narrow with height, which a box cannot do — a corbelled cornice under a coping with a drip throat cut into its underside, iron pattress plates with dome bolts, spalled bricks standing proud, weep pipes, a slag-chute flow lofted down the face with a crust lip where it spilled over the plinth, and a furnace window with a splayed reveal, a segmental arch of thirteen voussoirs and a cast frame, with the molten set back so it lights the reveal instead of glowing flat. Repeat around the 110×80 m playfield.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_wall_boundary'), F = floorMaterials(THREE);
    /* Box-projected UVs in metres so the 2.5 m brick tile never stretches —
       applied to ANY geometry, so the battered buttresses get brick too. */
    const brickUV = (mesh, off = [0, 0, 0]) => {
      const ge = mesh.geometry, p = ge.attributes.position, n = ge.attributes.normal, uv = ge.attributes.uv;
      if (!n) ge.computeVertexNormals();
      for (let i = 0; i < p.count; i++) {
        const ax = Math.abs(n.getX(i)), ay = Math.abs(n.getY(i));
        const u = ax > .5 ? p.getZ(i) : p.getX(i), v = ay > .5 ? p.getZ(i) : p.getY(i);
        uv.setXY(i, (u + off[0]) / 2.5, (v + off[1]) / 2.5);
      }
      uv.needsUpdate = true; return mesh;
    };
    const brick = (name, w, h, d, pos) => brickUV(ep(K, name, new THREE.BoxGeometry(w, h, d), F.brick, pos), pos);
    g.add(brick('wall_body', 10, 5.6, 1.1, [0, 3.2, 0]));
    g.add(member(THREE, 'wall_plinth', SECT.plate(1.50, .50, .04), 10.2, mats.env_refract_burnt, { axis: 'x', pos: [0, .25, 0], uScale: 1.2, vScale: 1.2 }));
    g.add(member(THREE, 'wall_plinth_step', SECT.plate(1.30, .16, .03), 10.1, mats.env_clinker, { axis: 'x', pos: [0, .57, 0], uScale: 1.4, vScale: 1.4 }));
    g.add(brick('wall_corbel_a', 10.05, .24, 1.25, [0, 6.05, 0]));
    g.add(brick('wall_corbel_b', 10.1, .20, 1.40, [0, 6.27, 0]));
    /* Coping with a drip throat: the groove under the overhang that stops water
       running back to the brick. It is cut into the profile. */
    g.add(member(THREE, 'wall_cap', [
      [-.80, -.11], [-.62, -.11], [-.60, -.075], [-.56, -.075], [-.54, -.11], [.54, -.11], [.56, -.075],
      [.60, -.075], [.62, -.11], [.80, -.11], [.80, .07], [.74, .11], [-.74, .11], [-.80, .07],
    ], 10.2, mats.env_cast, { axis: 'x', pos: [0, 6.48, 0], uScale: 1.4, vScale: 1.4 }));
    g.add(rivets(THREE, 'wall_cap_rivets', [-4.2, 6.60, .70], [4.2, 6.60, .70], 15, .020, mats.env_cast, [0, 1, 0]));
    /* Battered buttresses — wider at the foot, narrowing with height. */
    for (const x of [-5, 0, 5]) {
      const b = member(THREE, 'wall_buttress' + (x < 0 ? 'n5' : x ? 'p5' : '0'), SECT.plate(.98, .92, .05), 4.20, F.brick,
        { axis: 'y', pos: [x, 2.35, .95], stations: 5, taper: .74, uScale: 1, vScale: 1 });
      g.add(brickUV(b, [x, 2.35, 0]));
      g.add(member(THREE, 'wall_buttress_foot' + (x < 0 ? 'n5' : x ? 'p5' : '0'), SECT.plate(1.10, .50, .04), 1.00, mats.env_refract_burnt,
        { axis: 'z', pos: [x, .25, .95], uScale: 1.3, vScale: 1.3 }));
      // Chamfered stone weathering cap. Built as a tapered square member: a
      // 4-segment revolve turned its diagonal onto the axes and came out half
      // a metre wider than the buttress it caps.
      g.add(member(THREE, 'wall_buttress_cap' + (x < 0 ? 'n5' : x ? 'p5' : '0'), SECT.sq(1.06, .05), .30, mats.env_refract_burnt,
        { axis: 'y', pos: [x, 4.55, .95], stations: 2, taper: .46, uScale: 1.2, vScale: 1.2 }));
    }
    // Iron pattress plates with a real dome bolt at the crossing.
    for (const [x, y] of [[-3.6, 3.9], [-1.3, 2.3], [1.6, 4.3], [3.7, 2.9]]) {
      const tag = `${x}`.replace('.', '').replace('-', 'n');
      for (const rz of [.785, -.785]) g.add(member(THREE, `wall_tie_${tag}${rz > 0 ? 'a' : 'b'}`, SECT.plate(.10, .012), .70, mats.env_rust,
        { axis: 'x', spin: Math.PI / 2, pos: [x, y, .565], rot: [0, Math.PI / 2, rz], uScale: 2, vScale: 2 }));
      g.add(rivets(THREE, `wall_tie_bolt_${tag}`, [x, y, .575], [x, y, .575], 1, .046, mats.env_rust, [0, 0, 1]));
    }
    for (const [x, y] of [[-4.3, 1.1], [-2.1, 4.9], [-.4, 3.4], [2.2, 1.5], [3.9, 5.2], [4.6, 2.4]])
      g.add(brick(`wall_spall_${`${x}`.replace('.', '').replace('-', 'n')}`, .25, .075, .12, [x, y, .60]));
    for (const x of [-2.5, 2.5]) g.add(revolve(THREE, 'wall_weep' + (x < 0 ? 'n' : 'p'), [
      [0, -.15], [.050, -.15], [.050, .13], [.044, .15], [0, .15],
    ], mats.env_cast, { segs: 14, pos: [x, .85, .62], rot: [Math.PI / 2, 0, 0], scale: 8 }));
    /* Furnace window: splayed reveal, segmental arch of voussoirs, cast frame,
       molten set back 0.14 m so the reveal is what you see lit. */
    const WX = 2.5, WY = 3.5;
    g.add(member(THREE, 'wall_window_reveal', [
      [-.95, -.75], [.95, -.75], [.95, .75], [-.95, .75],
    ], .34, F.brick, { axis: 'z', pos: [WX, WY, .46], at: (t, pts) => pts.map(([a, b]) => [a * (1 - .18 * t), b * (1 - .18 * t)]), uScale: 1, vScale: 1 }));
    const vou = [];
    for (let i = 0; i < 13; i++) {
      const a = Math.PI * (.14 + .72 * i / 12), R = 1.28;
      const m = new THREE.Matrix4().makeRotationZ(a - Math.PI / 2);
      m.setPosition(WX - Math.cos(a) * R, WY + .52 + Math.sin(a) * R * .52, .60);
      vou.push({ geo: new THREE.BoxGeometry(.13, .30, .34), m });
    }
    g.add(brickUV(merge(THREE, vou, 'wall_window_arch', F.brick), [WX, WY, 0]));
    g.add(member(THREE, 'wall_window_frame', L90, 1.90, mats.env_rust, { axis: 'x', spin: Math.PI / 4, pos: [WX, WY - .78, .62], uScale: 2, vScale: 2 }));
    g.add(member(THREE, 'wall_window_sill', SECT.plate(.50, .120, .02), 2.10, mats.env_refract_burnt, { axis: 'x', pos: [WX, WY - .80, .70], uScale: 1.3, vScale: 1.3 }));
    g.add(ep(K, 'wall_window_melt', new THREE.PlaneGeometry(1.50, 1.20), mats.env_molten, [WX, WY, .48]));
    const bars = [];
    for (let i = 0; i < 5; i++) bars.push({ geo: new THREE.BoxGeometry(.030, 1.10, .030), m: new THREE.Matrix4().makeTranslation(WX - .60 + i * .30, WY, .74) });
    g.add(merge(THREE, bars, 'wall_window_bars', mats.env_rust));
    /* Slag chute: a flow that ran down the face and crusted over the plinth. */
    g.add(member(THREE, 'wall_stain', SECT.plate(1.30, .030), 3.20, mats.env_slag,
      { axis: 'y', pos: [-2.5, 2.10, .570], stations: 5,
        at: (t, pts) => pts.map(([x, y]) => [x * (.55 + .55 * t), y * (1 + 1.9 * (1 - t))]), uScale: 1.6, vScale: 1.6 }));
    g.add(revolve(THREE, 'wall_stain_crust', [
      [0, .10], [.34, .09], [.58, .05], [.72, -.02], [.66, -.07], [.30, -.08], [0, -.08],
    ], mats.env_clinker, { segs: 16, creaseAngle: 30, scale: 2, pos: [-2.5, .72, .84], rot: [-Math.PI / 2, 0, 0] }));
    g.add(member(THREE, 'wall_lamp_bracket', L75, .80, mats.env_mill, { axis: 'z', pos: [-2.5, 5.62, 1.00] }));
    g.add(cagedLamp(K, 'wall_lamp', [-2.5, 5.48, 1.32], [.5, 0, 0]));
    return stain(THREE, g, foundrySoot({ heat: [WX, WY, .8], reach: 4.5 }));
  } });

P({ id: 'foundry_skybox', label: 'Skybox', size: '1.4 km', swatch: '#7a3a18', stats: { Type: 'deep space (shader)', Body: 'rust gas giant + moon', Stars: 'black-body tinted' },
  note: 'Realistic deep-space sky: black-body starfield in three magnitude layers, Milky Way band with dust lanes, faint nebulosity, sun disc with corona aligned to the key light, and a banded rust gas giant with soft terminator, Fresnel atmosphere and limb glow. Full sphere, unlit, fog-exempt. GLB stand-in for the HDR bake.',
  build(K) { return buildSpaceSky(K.THREE, 'foundry'); } });

/* ══ dressing ═════════════════════════════════════════════════════════════ */

P({ id: 'foundry_dress_crucible', label: 'Dressing — crucible', size: '6.2×11.8 m', swatch: '#ff8a1f', stats: { Class: '20 t lip-pour ladle', Lining: '150 mm', Flow: '−Z, 8 m', Stream: 'necks as it falls' },
  note: 'A 20 t lip-pour ladle caught mid-tip, rebuilt as turned parts: a rolled-rim shell on a real taper, a 150 mm refractory lining with the top course showing and burnt darker than the rest, a trunnion ring at the balance line with cast bosses and pins, a spout trough rooted INSIDE the shell rim, and a tilting gear rim with merged teeth. The pour stream narrows as it accelerates — a falling stream necks down, and modelling it as a constant tube is the single tell that gives away a fake pour — into a splash crown, a slag-banked channel and a puddle field of revolved pools with crust rims, hot cores and cold cousins. The map’s biggest light source; place the pour side away from route sightlines.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_dress_crucible');
    /* Trunnion frame: real I-section standards on bolted base plates. */
    for (const s of [-1, 1]) {
      g.add(member(THREE, `crucible_standard${s > 0 ? '_p' : '_n'}`, SECT.i(.46, .30, .018, .028, .018), 3.20, mats.env_mill,
        { axis: 'y', pos: [s * 2.20, 1.60, 0], spin: Math.PI / 2 }));
      g.add(member(THREE, `crucible_standard_base${s > 0 ? '_p' : '_n'}`, SECT.plate(1.20, .06), 1.20, mats.env_mill, { axis: 'x', pos: [s * 2.20, .03, 0] }));
      g.add(bolts(THREE, `crucible_base_bolts${s > 0 ? '_p' : '_n'}`,
        [[s * 2.20 - .44, .06, -.44], [s * 2.20 + .44, .06, -.44], [s * 2.20 - .44, .06, .44], [s * 2.20 + .44, .06, .44]], .030, .044, mats.env_mill));
    }
    g.add(revolve(THREE, 'crucible_trunnion_shaft', [[0, -2.75], [.10, -2.75], [.10, 2.75], [0, 2.75]], mats.env_mill,
      { segs: 20, pos: [0, 3.00, 0], rot: [0, Math.PI / 2, 0], scale: 2 }));

    /* The tip, and the lip it puts the metal over. Derived once and reused by
       the melt surface, the stream and the splash — three hard-coded guesses
       is how the first pass ended up pouring from mid-air.
       Lip = the shell rim (r 1.54 at y .90 in the vessel's own frame) carried
       through the −0.62 rad rotation. */
    const TIP = -.62, ct = Math.cos(TIP), stp = Math.sin(TIP);
    const lipY = 3.00 + (.90 * ct + 1.54 * stp), lipZ = .90 * stp - 1.54 * ct;

    const b = grp('crucible_body', [0, 3.00, 0]); b.rotation.x = TIP;
    /* Shell: rolled top rim, taper, knuckle to a flat bottom. A ladle is a
       turned form, so it is revolved — the old two-cylinder stack could not
       have a rim or a knuckle at all.

       revolve() spins its profile about +Z, so every vessel part here carries
       rot [−π/2, 0, 0] to stand the axis up in Y. Without it the whole ladle
       lies on its side and reads as a stack of loose rings. */
    const UP = [-Math.PI / 2, 0, 0];
    b.add(revolve(THREE, 'crucible_shell', [
      [0, -1.52], [.82, -1.52], [.94, -1.44], [1.02, -1.28], [1.18, -.60], [1.34, .30], [1.42, .78],
      [1.50, .84], [1.54, .90], [1.50, .96], [1.40, .94], [1.38, .86],
    ], mats.env_mill, { segs: 44, creaseAngle: 26, scale: 1.6, rot: UP, wear: (r, z) => smooth(1.30, 1.54, r) * .5 }));
    b.add(revolve(THREE, 'crucible_lining', [
      [0, -1.36], [.80, -1.36], [.92, -1.24], [1.04, -.56], [1.20, .32], [1.26, .82],
    ], mats.env_refract, { segs: 40, creaseAngle: 30, scale: 2.2, rot: UP, inward: true }));
    /* Burnt top course: the same continuous lining surface as above, just a
       different material where the heat got at it \u2014 so it takes the same
       `inward` facing, and runs out to r 1.38 to close on the shell's inner
       rim lip instead of stopping short and leaving the rim as a notch. */
    b.add(revolve(THREE, 'crucible_lining_burnt', [[1.26, .82], [1.32, .845], [1.38, .86]],
      mats.env_refract_burnt, { segs: 40, scale: 4, rot: UP, inward: true }));
    b.add(revolve(THREE, 'crucible_trunnion_ring', [
      [1.28, -.16], [1.46, -.16], [1.50, -.10], [1.50, .10], [1.46, .16], [1.28, .16],
    ], mats.env_cast, { segs: 40, creaseAngle: 24, scale: 2, rot: UP }));
    for (const s of [-1, 1]) b.add(revolve(THREE, `crucible_boss${s > 0 ? '_p' : '_n'}`, [
      [0, .30], [.26, .30], [.30, .22], [.30, -.10], [.24, -.16], [0, -.16],
    ], mats.env_cast, { segs: 22, creaseAngle: 24, scale: 2.4, pos: [s * 1.40, 0, 0], rot: [0, s * Math.PI / 2, 0] }));
    /* Lifting bail: a real bent bar over the mouth, rooted into the bosses. */
    b.add(sweep(THREE, 'crucible_bail', [
      [-1.52, .06, 0], [-1.30, 1.10, 0], [0, 1.52, 0], [1.30, 1.10, 0], [1.52, .06, 0],
    ], () => .062, mats.env_mill, { radial: 12, samples: 40, up: [0, 0, 1] }));
    /* Spout: a trough formed in the rim. Rooted inside the shell so its end cap
       never shows as an open tube (rule 3). */
    b.add(loft(THREE, 'crucible_spout', [0, .3, .6, 1].map((t) => ({
      z: -1.10 - t * .70,
      pts: [[-.46 + t * .10, .62 - t * .30], [-.30 + t * .06, .40 - t * .26], [.30 - t * .06, .40 - t * .26],
        [.46 - t * .10, .62 - t * .30], [.52 - t * .10, .74 - t * .30], [-.52 + t * .10, .74 - t * .30]],
    })), mats.env_refract_burnt, { creaseAngle: 30, uScale: 1.8, vScale: 1.8 }));
    g.add(b);

    /* Melt surface. LEVEL — and therefore not a child of the tipped body.
       A liquid in a tilted vessel stays horizontal and meets the wall in an
       ellipse, so the boundary is solved per azimuth against the lining
       profile rather than approximated with a disc: for each direction round
       the vessel's own axis, find the height at which that wall lands on the
       world plane y = lipY. Rotating the melt with the shell (as the first
       pass did) put a 35° slope on free iron and left 1.4 m of it standing
       above the lip it was pouring over. */
    {
      const LIN = [[0, -1.36], [.80, -1.36], [.92, -1.24], [1.04, -.56], [1.20, .32], [1.26, .82]];
      const rAt = (z) => {
        if (z <= LIN[0][1]) return LIN[1][0];
        for (let i = 1; i < LIN.length; i++) if (z <= LIN[i][1]) {
          const [r0p, z0] = LIN[i - 1], [r1, z1] = LIN[i];
          return z1 === z0 ? r1 : lerp(r0p, r1, (z - z0) / (z1 - z0));
        }
        return LIN[LIN.length - 1][0];
      };
      const D = lipY - 3.00, N = 48, pos = [], uv = [], idx = [];
      // Centre sits where the vessel's own axis crosses the level plane.
      pos.push(0, lipY, (D / ct) * stp); uv.push(.5, .5);
      for (let i = 0; i < N; i++) {
        const a = i / N * Math.PI * 2, sa = Math.sin(a);
        // f(z) = z·ct − r(z)·sin(a)·stp − D, monotonic in z over the profile.
        let lo = LIN[0][1], hi = LIN[LIN.length - 1][1];
        for (let k = 0; k < 28; k++) {
          const mid = (lo + hi) / 2;
          if (mid * ct - rAt(mid) * sa * stp - D < 0) lo = mid; else hi = mid;
        }
        const z = (lo + hi) / 2, r = rAt(z) - .02;
        pos.push(r * Math.cos(a), lipY, z * stp + r * sa * ct);
        uv.push(.5 + Math.cos(a) * .5, .5 + sa * .5);
      }
      for (let i = 0; i < N; i++) idx.push(0, 1 + (i + 1) % N, 1 + i);
      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
      geo.setAttribute('uv', new THREE.Float32BufferAttribute(uv, 2));
      geo.setIndex(idx); geo.computeVertexNormals();
      const melt = new THREE.Mesh(solid(THREE, geo), mats.env_molten_hot);
      melt.name = 'crucible_melt';
      g.add(melt);
    }

    /* Tilting gear: a rim with real merged teeth, driven off the trunnion. */
    g.add(revolve(THREE, 'crucible_gear_rim', [
      [.28, -.10], [.60, -.10], [.62, -.06], [.62, .06], [.60, .10], [.28, .10],
    ], mats.env_brass, { segs: 32, creaseAngle: 24, scale: 2.4, pos: [2.60, 3.00, 0], rot: [0, Math.PI / 2, 0] }));
    const teeth = [];
    for (let i = 0; i < 18; i++) {
      const a = i / 18 * Math.PI * 2, m = new THREE.Matrix4().makeRotationX(-a);
      m.setPosition(2.60, 3.00 + Math.cos(a) * .655, Math.sin(a) * .655);
      teeth.push({ geo: new THREE.BoxGeometry(.17, .085, .13), m });
    }
    g.add(merge(THREE, teeth, 'crucible_gear_teeth', mats.env_brass));

    /* The pour. A free stream accelerates, so continuity narrows it: r ∝ v^−½.
       This is the detail that makes a pour read as liquid metal. */
    const path = [[0, lipY, lipZ], [0, lipY - .55, lipZ - .26], [0, lipY - 1.45, lipZ - .50],
      [0, .34, lipZ - .64], [0, .10, lipZ - .68]];
    const r0 = .118, drop = lipY - .10;
    g.add(sweep(THREE, 'crucible_stream', path, (t) => r0 / Math.sqrt(1 + 3.1 * t * drop / 2), mats.env_molten,
      { radial: 16, samples: 54, up: [1, 0, 0], cap: false }));
    g.add(sweep(THREE, 'crucible_stream_core', path, (t) => r0 * .42 / Math.sqrt(1 + 3.1 * t * drop / 2), mats.env_molten_hot,
      { radial: 12, samples: 54, up: [1, 0, 0], cap: false }));
    g.add(revolve(THREE, 'crucible_splash', [
      [.06, .26], [.22, .16], [.40, .05], [.54, .01], [.50, -.02], [.30, -.01], [.10, .04],
    ], mats.env_molten, { segs: 26, creaseAngle: 34, scale: 3, pos: [0, .10, lipZ - .68], rot: UP }));

    /* Channel and puddle field. */
    const ch = grp('crucible_channel', [0, 0, lipZ - .84]);
    ch.add(member(THREE, 'channel_bed', [
      [-.62, 0], [.62, 0], [.62, .20], [.34, .20], [.22, .07], [-.22, .07], [-.34, .20], [-.62, .20],
    ], 3.20, mats.env_clinker, { axis: 'z', pos: [0, 0, -1.60], stations: 4,
      at: (t, pts) => pts.map(([x, y]) => [x * (1 + .22 * t), y]), uScale: 1.6, vScale: 1.6 }));
    ch.add(member(THREE, 'channel_melt', SECT.plate(.42, .030), 3.10, mats.env_molten, { axis: 'z', pos: [0, .085, -1.60], uScale: 3, vScale: 3 }));
    for (let i = 0; i < 5; i++) ch.add(revolve(THREE, 'channel_ripple' + i, [[0, .012], [.17, .010], [.21, .004]],
      mats.env_molten_hot, { segs: 14, pos: [Math.sin(i * 2.1) * .07, .098, -.40 - i * .60], rot: [-Math.PI / 2, 0, 0], scale: 4 }));
    /* Pools: crust rim, oxide skin, hot core. Cold ones are the same form with
       the heat gone out of them — same geometry, different surface (rule 4). */
    const pool = (n, x, z, r, sy = .75, rot = 0, hot = true) => {
      const p = grp(n, [x, 0, z]); p.rotation.y = rot; p.scale.z = sy;
      p.add(revolve(THREE, n + '_crust', [
        [r * .62, .020], [r * .92, .034], [r * 1.06, .072], [r * 1.16, .050], [r * 1.12, .010], [r * .98, .004],
      ], mats.env_clinker, { segs: 24, creaseAngle: 30, scale: 2, rot: [-Math.PI / 2, 0, 0] }));
      p.add(revolve(THREE, n + '_skin', [[0, .034], [r * .5, .032], [r * .86, .026], [r * 1.0, .022]],
        hot ? mats.env_molten_skin : mats.env_slag, { segs: 24, scale: 2.2, rot: [-Math.PI / 2, 0, 0] }));
      if (hot) {
        p.add(revolve(THREE, n + '_core', [[0, .040], [r * .40, .038], [r * .58, .032]], mats.env_molten_hot, { segs: 20, scale: 3, rot: [-Math.PI / 2, 0, 0] }));
        for (let i = 0; i < 3; i++) p.add(member(THREE, `${n}_vein_${i}`, SECT.plate(.052, .012), r * 1.7, mats.env_molten,
          { axis: 'z', rot: [0, i * 1.1, 0], pos: [Math.cos(i * 2.1) * r * .3, .036, 0], uScale: 3, vScale: 3 }));
      }
      return p;
    };
    ch.add(pool('pool_main', 0, -4.0, 1.5, .75));
    ch.add(pool('pool_a', -1.7, -4.9, .8, .8, .5));
    ch.add(pool('pool_b', 1.6, -5.2, .95, .7, -.4));
    ch.add(pool('pool_c', .4, -6.1, .6, .9, .9));
    ch.add(pool('pool_cold_a', -2.4, -3.3, .5, .8, .2, false));
    ch.add(pool('pool_cold_b', 2.5, -4.0, .45, .7, -.7, false));
    const lumps = [];
    for (let i = 0; i < 9; i++) {
      const a = i / 9 * Math.PI * 2, r = 2.1 + (i % 3) * .3;
      const geo = new THREE.IcosahedronGeometry(.30 + (i % 2) * .12, 1), pp = geo.attributes.position, V = new THREE.Vector3();
      for (let k = 0; k < pp.count; k++) { V.fromBufferAttribute(pp, k); V.multiplyScalar(.72 + .5 * Math.abs(Math.sin(V.x * 8 + i) * Math.cos(V.z * 7))); pp.setXYZ(k, V.x, V.y * .7, V.z); }
      geo.computeVertexNormals();
      lumps.push({ geo, m: new THREE.Matrix4().makeRotationY(i).setPosition(Math.cos(a) * r, .12, -4.2 + Math.sin(a) * r * .75) });
    }
    ch.add(merge(THREE, lumps, 'slag_crust', mats.env_clinker));
    g.add(ch);
    return stain(THREE, g, foundrySoot({ heat: [0, 1.2, lipZ - 2.4], reach: 7 }));
  } });

P({ id: 'foundry_dress_pipes', label: 'Dressing — pipe run (8 m)', size: '8 m', swatch: '#b08a3e', stats: { Height: '3 m', Lines: '3', Valves: '2', Elbow: '1.5D long-radius' },
  note: 'Overhead pipe rack: three lines of differing gauge on two goalpost frames of real I-section, each with weld-neck flanges — a hub taper out to a raised face, not a disc — and proud butt-weld beads between them. The bypass is a 1.5D long-radius elbow swept along its centreline, and the valves are turned bodies with a bonnet, gland nut and a real handwheel rim on merged spokes. Runs along X.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_dress_pipes');
    for (const x of [-3.5, 3.5]) {
      const tag = x < 0 ? '_n' : '_p';
      for (const z of [-.8, .8]) g.add(member(THREE, `pipes_post${tag}${z < 0 ? 'a' : 'b'}`, UB152, 3.00, mats.env_prime,
        { axis: 'y', pos: [x, 1.50, z], spin: Math.PI / 2 }));
      g.add(member(THREE, 'pipes_head' + tag, PFC200, 2.00, mats.env_prime, { axis: 'z', pos: [x, 3.06, 0], spin: Math.PI }));
      g.add(member(THREE, 'pipes_foot' + tag, SECT.plate(.60, .035), 2.20, mats.env_mill, { axis: 'z', pos: [x, .0175, 0] }));
      g.add(bolts(THREE, 'pipes_foot_bolts' + tag, [[x - .22, .035, -.94], [x + .22, .035, -.94], [x - .22, .035, .94], [x + .22, .035, .94]], .024, .032, mats.env_mill));
    }
    [[-.60, .220, 'env_mill'], [0, .150, 'env_brass'], [.60, .100, 'env_copper']].forEach(([z, r, m], i) => {
      g.add(pipeRun(K, 'pipes_line' + i, 8, r, mats[m], { axis: 'x', pos: [0, 3.20 + r, z], flanges: 3, welds: [-2.1, 1.4, 3.0] }));
      // Pipe shoes: the line does not rest directly on the steel.
      const shoe = member(THREE, 's', PFC100, .18, mats.env_mill, { axis: 'z' });
      g.add(merge(THREE, [-3.5, 3.5].map((x) => ({ geo: bake(shoe), m: new THREE.Matrix4().makeTranslation(x, 3.20 - .05, z) })), `pipes_shoe${i}`, mats.env_mill));
    });
    /* Long-radius elbow: swept along the arc, so the bend is a real bend. */
    const R = .225, arcPts = [];
    for (let i = 0; i <= 8; i++) { const a = Math.PI / 2 * i / 8; arcPts.push([1.00 + Math.sin(a) * R, 3.35 - (1 - Math.cos(a)) * R, 0]); }
    g.add(sweep(THREE, 'pipes_elbow', arcPts, () => .150, mats.env_brass, { radial: 18, samples: 28, up: [0, 0, 1], cap: false }));
    g.add(pipeRun(K, 'pipes_drop', 1.60, .150, mats.env_brass, { axis: 'z', pos: [1.225, 2.55, 0], flanges: 1, welds: [-.6] }));
    for (const x of [-2.2, 2.6]) {
      const tag = x < 0 ? '_n' : '_p';
      g.add(revolve(THREE, 'pipes_valve_body' + tag, [
        [.150, -.18], [.190, -.16], [.200, -.06], [.170, 0], [.170, .10], [.200, .14], [.190, .20], [.150, .22],
      ], mats.env_cast, { segs: 24, creaseAngle: 26, scale: 3, pos: [x, 3.55, -.60], rot: [Math.PI / 2, 0, 0] }));
      g.add(revolve(THREE, 'pipes_valve_bonnet' + tag, [
        [0, .34], [.040, .34], [.048, .28], [.075, .26], [.075, .22], [.120, .20], [.120, .14],
      ], mats.env_brass, { segs: 20, creaseAngle: 26, scale: 4, pos: [x, 3.55, -.60], rot: [-Math.PI / 2, 0, 0] }));
      g.add(revolve(THREE, 'pipes_valve_wheel' + tag, [
        [.200, -.016], [.250, -.020], [.262, 0], [.250, .020], [.200, .016],
      ], mats.env_haz, { segs: 26, scale: 4, pos: [x, 3.89, -.60], rot: [-Math.PI / 2, 0, 0] }));
      const sp = [];
      for (let i = 0; i < 4; i++) { const a = i / 4 * Math.PI * 2, m = new THREE.Matrix4().makeRotationY(a); m.setPosition(x + Math.cos(a) * .125, 3.89, -.60 + Math.sin(a) * .125); sp.push({ geo: new THREE.BoxGeometry(.230, .016, .022), m }); }
      g.add(merge(THREE, sp, 'pipes_valve_spokes' + tag, mats.env_haz));
    }
    g.add(revolve(THREE, 'pipes_gauge', [
      [0, -.030], [.130, -.030], [.150, -.010], [.150, .020], [.135, .030], [.115, .032], [0, .032],
    ], mats.env_brass, { segs: 22, creaseAngle: 24, scale: 5, pos: [-.50, 3.70, -.60], rot: [0, Math.PI, 0] }));
    // Dial on a vertical plane facing out of the rack — a gauge pointing at the
    // sky is a disc, not an instrument.
    g.add(revolve(THREE, 'pipes_gauge_face', [[0, .034], [.112, .034]], mats.env_lamp, { segs: 20, pos: [-.50, 3.70, -.60], rot: [0, Math.PI, 0], scale: 5 }));
    for (let i = 0; i < 5; i++) g.add(ep(K, 'pipes_steam' + i, new THREE.IcosahedronGeometry(.10 + i * .06, 1), mats.env_steam,
      [2.60 + i * .07, 4.05 + i * .22, -.60 + Math.sin(i) * .05], [i, i * .8, 0]));
    return stain(THREE, g, foundrySoot({ lift: .8 }));
  } });

P({ id: 'foundry_dress_gantry', label: 'Dressing — gantry crane (12 m)', size: '13×9.9 m', swatch: '#c9a04a', stats: { Span: '12 m', Girder: 'box, 900 deep', Hook: 'forged, swept' },
  note: 'Overhead travelling crane: lattice legs of real angle bracing between angle chords, a 900 mm box girder with diaphragm stiffeners stepping its section, and a rail on top the trolley wheels actually sit on. The trolley carries a rope drum with the helical groove cut in, a sheave block, and a forged hook swept along its own curve with a section that thickens at the bend and tapers to the point — a hook is the one part on a crane everybody recognises, so it cannot be a torus. Wire rope is swept with a visible lay. Hazard-striped end stops and a cab with a lit window. Spans the yard at 9 m; clear of the 8 m air lane only where placed off-route.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_dress_gantry');
    const L120 = SECT.angle(.120, .012);
    for (const x of [-6, 6]) {
      const tag = x < 0 ? '_n' : '_p';
      const chord = member(THREE, 'c', L120, 8.50, mats.env_prime, { axis: 'y' });
      const list = [];
      for (let i = 0; i < 4; i++) {
        const a = i * Math.PI / 2;
        list.push({ geo: bake(chord), m: new THREE.Matrix4().makeRotationY(a).setPosition(x + Math.cos(a) * .34, 4.25, Math.sin(a) * 1.14) });
      }
      // Lattice: real diagonals at a real angle, both faces.
      const dia = member(THREE, 'd', L75, 2.45, mats.env_prime, { axis: 'y' });
      for (let i = 0; i < 6; i++) for (const s of [-1, 1]) {
        const m = new THREE.Matrix4().makeRotationX(s * .78);
        m.setPosition(x + s * .34, .95 + i * 1.35, 0);
        list.push({ geo: bake(dia), m });
      }
      g.add(merge(THREE, list, 'gantry_leg' + tag, mats.env_prime));
      g.add(member(THREE, 'gantry_foot' + tag, SECT.plate(1.20, .05), 3.20, mats.env_mill, { axis: 'z', pos: [x, .025, 0] }));
      g.add(member(THREE, 'gantry_stop' + tag, SECT.plate(.50, .40, .03), .95, mats.env_haz, { axis: 'x', spin: Math.PI / 2, pos: [x, 9.30, 1.00], uScale: 1.2, vScale: 1.2 }));
    }
    /* Box girder: top and bottom flange plates over twin webs, and the section
       steps where the diaphragms are — a welded box girder is not a solid bar. */
    g.add(member(THREE, 'gantry_girder', [
      [-.50, -.45], [.50, -.45], [.50, -.41], [.46, -.41], [.46, .41], [.50, .41], [.50, .45], [-.50, .45],
      [-.50, .41], [-.46, .41], [-.46, -.41], [-.50, -.41],
    ], 13, mats.env_prime, { axis: 'x', pos: [0, 8.90, 0], sag: .012, uScale: 1.4, vScale: 1.4 }));
    const dph = member(THREE, 'p', SECT.plate(.96, .014), .90, mats.env_prime, { axis: 'y' });
    g.add(merge(THREE, [-5.5, -3.3, -1.1, 1.1, 3.3, 5.5].map((x) => ({ geo: bake(dph), m: new THREE.Matrix4().makeRotationY(Math.PI / 2).setPosition(x, 8.90, 0) })), 'gantry_diaphragms', mats.env_prime));
    g.add(member(THREE, 'gantry_rail', [[-.035, -.030], [.035, -.030], [.045, .012], [.022, .020], [.022, .048], [-.022, .048], [-.022, .020], [-.045, .012]],
      13, mats.env_galv, { axis: 'x', pos: [0, 9.38, .35], uScale: 2.4, vScale: 2.4 }));
    g.add(rivets(THREE, 'gantry_girder_rivets', [-6.2, 9.34, .50], [6.2, 9.34, .50], 34, .016, mats.env_prime, [0, 1, 0]));

    const t = grp('gantry_trolley', [-2, 9.42, 0]);
    t.add(member(THREE, 'gantry_trolley_frame', [[-.80, -.20], [.80, -.20], [.80, .20], [-.80, .20]], 1.40, mats.env_haz,
      { axis: 'z', pos: [0, .22, 0], uScale: 1.2, vScale: 1.2 }));
    for (const x of [-.62, .62]) for (const z of [.35]) t.add(revolve(THREE, `gantry_wheel${x < 0 ? '_n' : '_p'}`, [
      [.070, -.038], [.115, -.038], [.115, -.014], [.098, 0], [.115, .014], [.115, .038], [.070, .038],
    ], mats.env_mill, { segs: 22, creaseAngle: 26, scale: 4, pos: [x, -.02, z], rot: [0, Math.PI / 2, 0] }));
    /* Rope drum with the helix cut in as displacement on the profile. */
    const drumPts = [];
    for (let i = 0; i <= 40; i++) { const z = -.50 + i * .025; drumPts.push([.300 + Math.sin(i * 1.6) * .010, z]); }
    t.add(revolve(THREE, 'gantry_drum', [[0, -.52], [.300, -.52], ...drumPts, [.300, .52], [0, .52]], mats.env_mill,
      { segs: 30, creaseAngle: 44, scale: 3, pos: [0, .22, 0], rot: [0, Math.PI / 2, 0] }));
    t.add(revolve(THREE, 'gantry_motor', [
      [0, .24], [.16, .24], [.19, .20], [.19, -.20], [.16, -.24], [0, -.24],
    ], mats.env_cast, { segs: 22, creaseAngle: 24, scale: 3, pos: [.74, .22, -.30], rot: [0, Math.PI / 2, 0] }));
    /* Wire rope: swept with a lay, so it reads as stranded rope. */
    t.add(sweep(THREE, 'gantry_rope', [[0, .10, 0], [0, -1.4, 0], [0, -2.9, 0], [0, -4.15, 0]],
      (u, ang) => .020 * (1 + .16 * Math.sin(ang * 3 + u * 110)), mats.env_mill, { radial: 12, samples: 60, up: [1, 0, 0] }));
    t.add(member(THREE, 'gantry_block', [[-.16, -.26], [.16, -.26], [.16, .26], [-.16, .26]], .34, mats.env_mill,
      { axis: 'z', pos: [0, -4.34, 0], at: (tt, pts) => pts.map(([x, y]) => [x, y * (1 - .18 * tt)]) }));
    t.add(revolve(THREE, 'gantry_sheave', [[.05, -.03], [.19, -.03], [.19, -.010], [.16, 0], [.19, .010], [.19, .03], [.05, .03]],
      mats.env_galv, { segs: 20, creaseAngle: 26, scale: 5, pos: [0, -4.22, 0], rot: [0, Math.PI / 2, 0] }));
    /* Forged hook: the section thickens through the bend and tapers to a point.
       Swept along the hook's own curve. */
    t.add(sweep(THREE, 'gantry_hook', [
      [0, -4.56, 0], [0, -4.76, .02], [.10, -4.96, .02], [.26, -5.02, .02], [.36, -4.88, .02], [.32, -4.72, .02],
    ], (u) => .052 * (1 + .45 * Math.sin(Math.PI * clamp(u * 1.25, 0, 1))) * (1 - .55 * smooth(.72, 1, u)),
    mats.env_mill, { radial: 12, samples: 40, up: [0, 0, 1] }));
    g.add(t);
    g.add(member(THREE, 'gantry_cab', [
      [-.80, -.70], [.80, -.70], [.80, .46], [.44, .70], [-.80, .70],
    ], 1.20, mats.env_prime, { axis: 'z', pos: [4.50, 8.10, 1.20], uScale: 1.3, vScale: 1.3 }));
    g.add(ep(K, 'gantry_cab_window', new THREE.PlaneGeometry(1.20, .70), mats.env_lamp, [4.50, 8.20, .585]));
    g.add(cagedLamp(K, 'gantry_lamp', [0, 8.36, 0], [Math.PI / 2, 0, 0]));
    return stain(THREE, g, foundrySoot({ lift: 2.0 }));
  } });

P({ id: 'foundry_dress_lightrig', label: 'Dressing — light rig', size: '7 m', swatch: '#fff0c8', stats: { Heads: '4', Mast: 'lattice, 7 m', Reflector: 'paraboloid' },
  note: 'Yard floodlight mast: three tube chords with zigzag bracing, tapering toward the top, on a bolted base. The cross-arm is real channel, and each head is a revolved paraboloid reflector — a floodlight hood is a dish, and modelling it as a box is why the old heads read as crates — with a lens, a hinged visor, a wire guard and a knuckle mount that shows how it aims. Ladder rungs, hazard collar, junction box and a beacon. Point heads down-route to justify the level lighting.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_dress_lightrig');
    g.add(member(THREE, 'light_base', SECT.plate(1.20, .04), 1.20, mats.env_mill, { axis: 'x', pos: [0, .02, 0] }));
    g.add(bolts(THREE, 'light_base_bolts', [[-.44, .04, -.44], [.44, .04, -.44], [-.44, .04, .44], [.44, .04, .44]], .026, .034, mats.env_mill));
    /* Lattice mast: three chords on a triangle, tapering, with real bracing. */
    const chord = member(THREE, 'c', SECT.tube(.034, 12), 6.50, mats.env_galv, { axis: 'y' });
    const list = [];
    for (let i = 0; i < 3; i++) {
      const a = i / 3 * Math.PI * 2 + .5;
      list.push({ geo: bake(chord), m: new THREE.Matrix4().makeTranslation(Math.cos(a) * .22, 3.30, Math.sin(a) * .22) });
    }
    const brace = member(THREE, 'b', SECT.tube(.016, 8), .78, mats.env_galv, { axis: 'y' });
    for (let i = 0; i < 3; i++) {
      const a = i / 3 * Math.PI * 2 + .5;
      for (let k = 0; k < 9; k++) {
        const m = new THREE.Matrix4().makeRotationZ(k % 2 ? .82 : -.82);
        m.setPosition(Math.cos(a) * .19, .42 + k * .70, Math.sin(a) * .19);
        list.push({ geo: bake(brace), m });
      }
    }
    g.add(merge(THREE, list, 'light_mast', mats.env_galv));
    const rung = member(THREE, 'r', SECT.tube(.019, 8), .48, mats.env_galv, { axis: 'x' });
    g.add(merge(THREE, Array.from({ length: 10 }, (_, i) => ({ geo: bake(rung), m: new THREE.Matrix4().makeTranslation(0, 1.0 + i * .55, .23) })), 'light_rungs', mats.env_galv));
    g.add(member(THREE, 'light_collar', SECT.plate(.90, .120, .02), .04, mats.env_haz, { axis: 'z', spin: Math.PI / 2, pos: [0, 1.60, .20], uScale: 1.2, vScale: 1.2 }));
    g.add(member(THREE, 'light_box', [[-.20, -.25], [.20, -.25], [.20, .25], [-.20, .25]], .30, mats.env_cast,
      { axis: 'z', pos: [.30, 2.20, 0], at: (t, pts) => pts.map(([x, y]) => [x * (1 - .08 * t), y * (1 - .08 * t)]) }));
    g.add(revolve(THREE, 'light_box_lens', [[0, .012], [.030, .012], [.034, .002], [.030, -.004]], mats.env_ember,
      { segs: 16, pos: [.30, 2.32, -.16], rot: [0, Math.PI, 0], scale: 8 }));
    g.add(member(THREE, 'light_arm', PFC100, 3.20, mats.env_galv, { axis: 'x', pos: [0, 6.70, 0], spin: -Math.PI / 2 }));
    for (let i = 0; i < 4; i++) {
      const x = -1.2 + i * .8, h = grp('light_head' + i, [x, 6.52, .10]);
      h.rotation.x = -.90;
      // Reflector: a real paraboloid, so the hood is a dish.
      const par = [];
      for (let k = 0; k <= 10; k++) { const r = .30 * k / 10; par.push([r, -(r * r) / .30 * .9]); }
      h.add(revolve(THREE, `light_reflector${i}`, [...par, [.30, .055], [.285, .060], [0, -.005]], mats.env_lamp_off,
        { segs: 26, creaseAngle: 30, scale: 3, inward: true }));
      h.add(revolve(THREE, `light_hood${i}`, [[.300, .055], [.316, .040], [.316, -.180], [.300, -.200]], mats.env_cast, { segs: 26, scale: 3 }));
      h.add(revolve(THREE, `light_lens${i}`, [[0, .042], [.18, .038], [.272, .026], [.292, .016]], mats.env_lamp, { segs: 26, scale: 3 }));
      for (let k = 0; k < 3; k++) h.add(revolve(THREE, `light_guard${i}_${k}`,
        [[.255 - k * .08, -.004], [.263 - k * .08, -.004], [.263 - k * .08, .004], [.255 - k * .08, .004], [.255 - k * .08, -.004]],
        mats.env_galv, { segs: 20, pos: [0, 0, .055], scale: 10 }));
      h.add(member(THREE, `light_visor${i}`, SECT.plate(.30, .006), .64, mats.env_cast, { axis: 'x', pos: [0, .22, .02], rot: [.5, 0, 0] }));
      h.add(revolve(THREE, `light_knuckle${i}`, [[0, -.10], [.040, -.10], [.046, -.06], [.046, -.02], [.030, 0]], mats.env_mill, { segs: 16, scale: 6, pos: [0, -.20, 0], rot: [Math.PI / 2, 0, 0] }));
      h.rotateX(0);
      g.add(h);
    }
    g.add(revolve(THREE, 'light_cap', [[0, .20], [.10, .19], [.19, .10], [.22, 0], [.22, -.04], [0, -.04]], mats.env_cast,
      { segs: 20, creaseAngle: 26, scale: 3, pos: [0, 6.86, 0], rot: [-Math.PI / 2, 0, 0] }));
    g.add(revolve(THREE, 'light_beacon', [[0, .12], [.055, .10], [.075, .04], [.075, -.02], [0, -.02]], mats.env_ember,
      { segs: 18, scale: 5, pos: [0, 7.02, 0], rot: [-Math.PI / 2, 0, 0] }));
    return stain(THREE, g, foundrySoot({ lift: 1.0 }));
  } });

P({ id: 'foundry_dress_steamvent', label: 'Dressing — steam vent', size: '2 m · 4.8 m plume', swatch: '#d9dde6', stats: { Grate: '1.2 m', Stack: 'swaged, flap lid', Solid: 'tops at 1.47 m' },
  note: 'Floor vent: a turned collar with a seating rebate and a bolt ring, 41/100 grating dropped into it, and a stub stack with a swaged joint and a flap lid propped open on a real barrel hinge. The plume is nine translucent puffs widening, thinning and shearing downwind with a hot core at the mouth, rising to 4.8 m — a stand-in the client swaps for particles. Safe filler along the lane edges: the solid envelope tops out at 1.47 m, propped lid included, so it clears under anything.',
  build(K) {
    const { grp, mats, THREE } = K; envSurfaces(THREE, mats);
    const g = grp('foundry_dress_steamvent');
    g.add(revolve(THREE, 'vent_collar', [
      [.62, 0], [.96, 0], [.96, .16], [.88, .20], [.80, .20], [.80, .12], [.62, .12],
    ], mats.env_cast, { segs: 28, creaseAngle: 24, scale: 2.6, rot: [-Math.PI / 2, 0, 0] }));
    const ring = [];
    for (let i = 0; i < 10; i++) { const a = i / 10 * Math.PI * 2; ring.push([Math.cos(a) * .88, .20, Math.sin(a) * .88]); }
    g.add(bolts(THREE, 'vent_collar_bolts', ring, .020, .018, mats.env_cast));
    g.add(grating(THREE, 'vent_grate', 1.36, 1.36, mats.env_cast, { pos: [0, .155, 0], depth: .034 }));
    g.add(revolve(THREE, 'vent_stack', [
      [.36, 0], [.36, .40], [.40, .44], [.40, .48], [.36, .52], [.36, .78], [.40, .82],
    ], mats.env_mill, { segs: 24, creaseAngle: 26, scale: 2.2, pos: [0, .10, -.60], rot: [-Math.PI / 2, 0, 0] }));
    g.add(revolve(THREE, 'vent_stack_band', [[.400, -.036], [.424, -.030], [.424, .030], [.400, .036]], mats.env_mill,
      { segs: 24, scale: 4, pos: [0, .58, -.60], rot: [-Math.PI / 2, 0, 0] }));
    /* Lid propped at 0.75 rad, hinged just under the mouth: the whole solid
       envelope has to close under 1.5 m for the note's placement guarantee to
       be true, and the plate's own radius is most of what spends that budget. */
    const lid = grp('vent_lid', [0, .94, -.95]); lid.rotation.x = -.75;
    lid.add(revolve(THREE, 'vent_lid_plate', [[0, .010], [.36, .010], [.40, 0], [.36, -.014], [0, -.014]], mats.env_mill,
      { segs: 24, scale: 3, pos: [0, 0, .36], rot: [-Math.PI / 2, 0, 0] }));
    lid.add(revolve(THREE, 'vent_lid_hinge', [[0, -.10], [.028, -.10], [.028, .10], [0, .10]], mats.env_mill,
      { segs: 14, scale: 6, rot: [0, Math.PI / 2, 0] }));
    g.add(lid);
    const N = 9, plume = grp('vent_plume');
    const puffMat = (i) => { const m = mats.env_steam.clone(); m.name = 'env_steam_' + i; m.opacity = .46 * Math.pow(1 - i / N, 1.4) + .03; m.color.lerp(new THREE.Color(0x8a8f99), i / N * .6); return m; };
    for (let i = 0; i < N; i++) {
      const t = i / (N - 1), r = .22 + t * .62, y = 1.12 + t * 2.3, x = t * t * .9, z = -.60 + Math.sin(i * 2.1) * .08 * t;
      plume.add(ep(K, 'vent_puff' + i, new THREE.IcosahedronGeometry(r, 2), puffMat(i), [x, y, z], [i * .7, i * 1.3, 0]));
      if (i > 2 && i % 2) plume.add(ep(K, 'vent_wisp' + i, new THREE.IcosahedronGeometry(r * .45, 1), puffMat(i + 2), [x + r * .8, y + r * .3, z + (i % 4 ? .3 : -.3)]));
    }
    plume.add(ep(K, 'vent_steam_core', new THREE.SphereGeometry(.18, 12, 8), mats.env_lamp, [0, 1.06, -.60]));
    g.add(plume);
    g.add(ep(K, 'vent_scorch', new THREE.RingGeometry(.96, 1.34, 24), mats.env_clinker, [0, .008, 0], [-Math.PI / 2, 0, 0]));
    g.add(ep(K, 'vent_glow', new THREE.RingGeometry(.62, .70, 16), mats.env_ember, [0, .148, 0], [-Math.PI / 2, 0, 0]));
    return stain(THREE, g, foundrySoot({ heat: [0, .2, 0], reach: 2.4 }));
  } });
