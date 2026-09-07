/**
 * Deep Field 3D — projectiles, muzzle and impact meshes (DESIGN-BRIEF §3.2).
 * Projectiles fly along −Z from their origin; muzzle/impact effects are static
 * meshes the client scales/fades. Each inherits its tower's energy hue so the
 * shot, the flash and the hit are tellable from across the map.
 */
export const PROJECTILES = [];
const E = { lance: 'energy_rail', nova: 'energy_fuse', arc: 'energy_arc', skywatch: 'energy_flak' };
const SW = { lance: '#2b5cff', nova: '#f0c83a', arc: '#f05ae6', skywatch: '#22d3ee', filament: '#ff2e4a' };

/** What each tower fires — the viewer's live-fire table. speed m/s, rate shots/s, tier follows the damage path (L7 → T2, L10 → T3). */
export const ORDNANCE = {
  lance: { kind: 'bolt', proj: ['proj_lance_bolt', 'proj_lance_bolt_t2', 'proj_lance_bolt_t3'], muzzle: 'vfx_muzzle_lance', impact: 'vfx_impact_lance', speed: 70, rate: 1.6, tierPath: 'damage' },
  skywatch: { kind: 'bolt', proj: ['proj_skywatch_bolt', 'proj_skywatch_bolt_t2', 'proj_skywatch_bolt_t3'], muzzle: 'vfx_muzzle_skywatch', impact: 'vfx_impact_skywatch', speed: 45, rate: 3.0, tierPath: 'damage' },
  nova: { kind: 'lob', proj: ['proj_nova_shell', 'proj_nova_shell_t2', 'proj_nova_shell_t3'], muzzle: 'vfx_muzzle_nova', impact: 'vfx_impact_nova', speed: 16, rate: .5, tierPath: 'damage' },
  arc: { kind: 'chain', proj: ['proj_arc_beam'], muzzle: 'vfx_muzzle_arc', impact: 'vfx_impact_arc', rate: 1.2, life: .16, hop: 2.6, source: 'arc_spin' },
  filament: { kind: 'ray', proj: ['proj_filament_beam'], rate: 0, ramp: 3.0 },
};

function bolt(tower, tier) {
  return {
    id: `proj_${tower}_bolt${tier > 1 ? '_t' + tier : ''}`, file: `proj_${tower}_bolt${tier > 1 ? '_t' + tier : ''}.glb`,
    label: `${tower[0].toUpperCase() + tower.slice(1)} bolt${tier > 1 ? ' · T' + tier : ''}`, swatch: SW[tower],
    stats: { Tier: tier === 1 ? 'L1–6' : tier === 2 ? 'L7' : 'L10', Forward: '−Z' },
    note: tower === 'lance' ? 'A rail slug: long, thin, one bright core. T2 gains fins; T3 stretches and gains a pierce ring — armour-ignore reads as a longer needle.'
      : 'Flak tracer: short, fat, cyan. T2 doubles the tracer; T3 adds a proximity-fuse ring.',
    build(K) {
      const { part, cyl, grp, mats, THREE, D } = K;
      const g = grp(this.id);
      const em = mats[E[tower]];
      const L = tower === 'lance' ? .60 + tier * .15 : .28 + tier * .04, R = tower === 'lance' ? .035 : .06;
      g.add(part(this.id + '_core', cyl(R, R * .5, L, 8), em, [0, 0, 0], [-90 * D, 0, 0]));
      g.add(part(this.id + '_tip', new THREE.ConeGeometry(R * 1.3, R * 3, 8), mats.chrome, [0, 0, -L / 2 - R * 1.5], [-90 * D, 0, 0]));
      if (tier >= 2) for (let i = 0; i < 3; i++) {
        const a = (i / 3) * Math.PI * 2;
        g.add(part(this.id + '_fin' + i, new THREE.BoxGeometry(R * .3, R * 2.2, L * .25), mats.steel_plate, [Math.cos(a) * R * 1.2, Math.sin(a) * R * 1.2, L * .35], [0, 0, a + Math.PI / 2]));
      }
      if (tier >= 3) g.add(part(this.id + '_ring', new THREE.TorusGeometry(R * 2.4, R * .35, 6, 16), em, [0, 0, -L * .1]));
      return g;
    },
  };
}
for (const t of ['lance', 'skywatch']) for (const tier of [1, 2, 3]) PROJECTILES.push(bolt(t, tier));

for (const tier of [1, 2, 3]) PROJECTILES.push({
  id: `proj_nova_shell${tier > 1 ? '_t' + tier : ''}`, file: `proj_nova_shell${tier > 1 ? '_t' + tier : ''}.glb`,
  label: `Nova shell${tier > 1 ? ' · T' + tier : ''}`, swatch: SW.nova, stats: { Tier: tier === 1 ? 'L1–6' : tier === 2 ? 'L7' : 'L10', Arc: 'sim-side' },
  note: 'Brass mortar shell with a lit fuse cap — visible on the arc. T2 is heavier with a burn band; T3 carries three bomblets for the cluster breakpoint.',
  build(K) {
    const { part, cyl, grp, mats, THREE, D } = K;
    const g = grp(this.id);
    const r = .11 + tier * .015;
    g.add(part(this.id + '_body', cyl(r, r, .34, 12), mats.brass, [0, 0, .04], [-90 * D, 0, 0]));
    g.add(part(this.id + '_nose', new THREE.ConeGeometry(r, .16, 12), mats.steel_plate, [0, 0, -.21], [-90 * D, 0, 0]));
    g.add(part(this.id + '_fuse', cyl(.03, .05, .08, 8), mats.energy_fuse, [0, 0, .25], [-90 * D, 0, 0]));
    g.add(part(this.id + '_band', new THREE.TorusGeometry(r + .005, .012, 6, 16), mats.chrome, [0, 0, .16]));
    if (tier >= 2) g.add(part(this.id + '_burn_band', new THREE.TorusGeometry(r + .008, .02, 6, 16), mats.hazard, [0, 0, -.06]));
    if (tier >= 3) for (let i = 0; i < 3; i++) {
      const a = (i / 3) * Math.PI * 2;
      g.add(part(this.id + '_bomblet' + i, new THREE.SphereGeometry(.05, 8, 6), mats.brass, [Math.cos(a) * (r + .04), Math.sin(a) * (r + .04), .06]));
    }
    return g;
  },
});

PROJECTILES.push({
  id: 'proj_arc_beam', file: 'proj_arc_beam.glb', label: 'Arc beam', swatch: SW.arc, stats: { Segments: 'source → target → hop', Length: '1 m unit' },
  note: 'A unit-length jagged beam along −Z with empty nodes `arc_beam_source`, `arc_beam_target`, `arc_beam_hop`. The client scales Z to the hit distance and instances a second copy for the hop.',
  build(K) {
    const { part, grp, mats, THREE } = K;
    const g = grp('proj_arc_beam');
    const pts = [];
    for (let i = 0; i <= 8; i++) pts.push(new THREE.Vector3((i && i < 8) ? (i % 2 ? .06 : -.05) : 0, (i && i < 8) ? (i % 3 ? .04 : -.05) : 0, -i / 8));
    g.add(part('arc_beam_core', new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 24, .022, 6, false), mats.energy_arc));
    g.add(part('arc_beam_halo', new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 24, .045, 6, false), mats.shield_bubble ?? mats.energy_arc));
    for (const [n, z] of [['arc_beam_source', 0], ['arc_beam_target', -1], ['arc_beam_hop', -1]]) { const m = grp(n, [0, 0, z]); m.userData.role = n.split('_')[2]; g.add(m); }
    return g;
  },
});

PROJECTILES.push({
  id: 'proj_filament_beam', file: 'proj_filament_beam.glb', label: 'Filament beam', swatch: '#ff2e4a', stats: { Kind: 'ramp beam', Length: '1 m unit', Ramp: '`filament_beam_ramp` emissive' },
  note: 'A unit-length continuous beam along −Z: a thin white-hot core, a red sheath that the client brightens as heat ramps (drive `filament_beam_ramp` emissive 0.3 → 2.0), a coil of three helix strands that spin while firing, and `filament_beam_source` / `filament_beam_target` nodes. Scale Z to the hit distance every frame.',
  build(K) {
    const { part, grp, mats, THREE } = K, g = grp('proj_filament_beam');
    g.add(part('filament_beam_core', new THREE.CylinderGeometry(.012, .012, 1, 8), mats.chrome, [0, 0, -.5], [Math.PI / 2, 0, 0]));
    const ramp = grp('filament_beam_ramp');
    const sheath = new THREE.MeshStandardMaterial({ color: 0xff2e4a, emissive: new THREE.Color(0xff2e4a), emissiveIntensity: .6, roughness: .3, metalness: .05, transparent: true, opacity: .75 }); sheath.name = 'filament_sheath';
    ramp.add(part('filament_beam_sheath', new THREE.CylinderGeometry(.035, .035, 1, 10, 1, true), sheath, [0, 0, -.5], [Math.PI / 2, 0, 0]));
    for (let i = 0; i < 3; i++) {
      const pts = []; for (let k = 0; k <= 16; k++) { const a = k / 16 * Math.PI * 4 + i * Math.PI * 2 / 3; pts.push(new THREE.Vector3(Math.cos(a) * .06, Math.sin(a) * .06, -k / 16)); }
      ramp.add(part('filament_beam_helix' + i, new THREE.TubeGeometry(new THREE.CatmullRomCurve3(pts), 32, .008, 5, false), mats.energy_beam || sheath));
    }
    g.add(ramp);
    for (const [n, z] of [['filament_beam_source', 0], ['filament_beam_target', -1]]) { const m = grp(n, [0, 0, z]); m.userData.role = n.split('_')[2]; g.add(m); }
    return g;
  },
});

for (const t of ['lance', 'nova', 'arc', 'skywatch']) {
  PROJECTILES.push({
    id: `vfx_muzzle_${t}`, file: `vfx_muzzle_${t}.glb`, label: `Muzzle — ${t}`, swatch: SW[t], stats: { Placed: 'at <id>_muzzle', Life: '~80 ms' },
    note: 'Flat petal flash plus a ring, facing −Z. Client scales from 0 and fades.',
    build(K) {
      const { part, cyl, grp, mats, THREE, D } = K;
      const g = grp(this.id), em = mats[E[t]];
      const petals = t === 'nova' ? 8 : t === 'arc' ? 3 : 5, len = t === 'nova' ? .55 : .40;
      for (let i = 0; i < petals; i++) {
        const a = (i / petals) * Math.PI * 2;
        g.add(part(this.id + '_petal' + i, new THREE.ConeGeometry(.06, len, 4), em, [Math.cos(a) * .12, Math.sin(a) * .12, -len / 2], [-90 * D, 0, 0]).rotateOnAxis(new THREE.Vector3(Math.sin(a), -Math.cos(a), 0), 22 * D));
      }
      g.add(part(this.id + '_ring', new THREE.TorusGeometry(.16, .025, 6, 20), em, [0, 0, -.05]));
      g.add(part(this.id + '_core', cyl(.08, .02, .20, 8), mats.chrome, [0, 0, -.12], [-90 * D, 0, 0]));
      return g;
    },
  });
  PROJECTILES.push({
    id: `vfx_impact_${t}`, file: `vfx_impact_${t}.glb`, label: `Impact — ${t}`, swatch: SW[t], stats: { Placed: 'at hit point', Life: '~250 ms' },
    note: t === 'nova' ? 'Splash: a wide ground ring plus low shards — the 3.2 m radius should read from the ring alone.' : 'Shard burst around a bright core; the client scales up and fades.',
    build(K) {
      const { part, grp, mats, THREE, D } = K;
      const g = grp(this.id), em = mats[E[t]];
      const n = t === 'nova' ? 12 : 8, r = t === 'nova' ? .9 : .30;
      g.add(part(this.id + '_core', new THREE.IcosahedronGeometry(t === 'nova' ? .30 : .12, 0), em));
      for (let i = 0; i < n; i++) {
        const a = (i / n) * Math.PI * 2, el = t === 'nova' ? 12 * D : (i % 2 ? 40 : -20) * D;
        const d = new THREE.Vector3(Math.cos(a) * Math.cos(el), Math.sin(el), Math.sin(a) * Math.cos(el));
        const s = part(this.id + '_shard' + i, new THREE.ConeGeometry(.04, r * .6, 4), i % 3 ? em : mats.chrome, d.clone().multiplyScalar(r * .7).toArray());
        s.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), d);
        g.add(s);
      }
      if (t === 'nova') g.add(part(this.id + '_ring', new THREE.RingGeometry(r * .9, r * 1.05, 24), em, [0, .01, 0], [-90 * D, 0, 0]));
      return g;
    },
  });
}
