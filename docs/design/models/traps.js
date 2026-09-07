/**
 * Deep Field 3D — trap state meshes (DESIGN-BRIEF §3.3). Every state is its own
 * file because state IS gameplay information. All sit on the 1.5 m trap-socket
 * plate; origin at plate centre, y = 0.
 */
export const TRAPS = [];

function plate(K, id, lampMat) {
  const { part, cyl, grp, mats, D } = K;
  const g = grp(id);
  g.add(part(id + '_plate', cyl(.75, .78, .08, 8), mats.chassis, [0, .04, 0]));
  g.add(part(id + '_plate_rim', cyl(.70, .70, .02, 8), mats.steel_plate, [0, .09, 0]));
  g.add(K.hazardStripes(id + '_hazard', 1.0, .05, [0, .08, .74], [0, 0, 0], 6));
  for (let i = 0; i < 4; i++) {
    const a = (i / 4) * Math.PI * 2 + 45 * D;
    g.add(part(id + '_lamp' + i, cyl(.035, .035, .02, 8), lampMat, [Math.cos(a) * .66, .105, Math.sin(a) * .66]));
  }
  return g;
}

const SPIKE = (state) => ({
  id: 'trap_spike_' + state, file: `trap_spike_${state}.glb`, label: `Spike — ${state}`, swatch: '#c3ccd8',
  stats: { Cost: '45', Damage: '18', Charges: '3', Rearm: '6 s' },
  note: { armed: 'Tips just proud of the plate: a threat you can see but a Mote might not.', triggered: 'Spikes fully extended, hazard glowing — the burst frame.', spent: 'Retracted, lamps off. Three charges gone; reads as safe until rearmed.' }[state],
  build(K) {
    const { part, cyl, mats, THREE } = K;
    const g = plate(K, 'trap_spike', state === 'spent' ? mats.trim : mats.energy_fuse);
    const h = { armed: .16, triggered: .62, spent: .04 }[state];
    let n = 0;
    for (let r = 0; r < 2; r++) for (let i = 0; i < (r ? 8 : 4); i++) {
      const a = (i / (r ? 8 : 4)) * Math.PI * 2 + (r ? 0 : Math.PI / 4), rad = r ? .50 : .22;
      g.add(part('trap_spike_socket' + n, cyl(.07, .08, .04, 8), mats.steel_hull, [Math.cos(a) * rad, .12, Math.sin(a) * rad]));
      g.add(part('trap_spike_tip' + (n++), new THREE.ConeGeometry(.05, h, 6), state === 'spent' ? mats.trim : mats.chrome, [Math.cos(a) * rad, .12 + h / 2, Math.sin(a) * rad]));
    }
    if (state === 'triggered') g.add(part('trap_spike_flash', new THREE.RingGeometry(.30, .62, 8), mats.energy_fuse, [0, .11, 0], [-Math.PI / 2, 0, 0]));
    return g;
  },
});
TRAPS.push(SPIKE('armed'), SPIKE('triggered'), SPIKE('spent'));

const TAR = (state) => ({
  id: 'trap_tar_' + state, file: `trap_tar_${state}.glb`, label: `Tar — ${state}`, swatch: '#4fc0e8',
  stats: { Cost: '35', Applies: 'chill', Charges: '6', Rearm: '4 s' },
  note: { full: 'A deep dark pool with a chill-blue rim. Wet = active.', depleted: 'Thin cracked residue, rim dark. Same footprint so players learn the shape.' }[state],
  build(K) {
    const { part, cyl, mats, THREE } = K;
    const g = plate(K, 'trap_tar', state === 'full' ? mats.energy_flak : mats.trim);
    if (state === 'full') {
      g.add(part('trap_tar_pool', cyl(.60, .64, .10, 24), mats.rubber, [0, .14, 0]));
      g.add(part('trap_tar_sheen', new THREE.RingGeometry(.52, .60, 24), mats.energy_flak, [0, .195, 0], [-Math.PI / 2, 0, 0]));
      for (let i = 0; i < 5; i++) {
        const a = i * 1.9, r = .15 + i * .08;
        g.add(part('trap_tar_bubble' + i, new THREE.SphereGeometry(.05 - i * .005, 8, 6), mats.rubber, [Math.cos(a) * r, .20, Math.sin(a) * r]));
      }
    } else {
      g.add(part('trap_tar_residue', cyl(.60, .62, .02, 24), mats.trim, [0, .11, 0]));
      for (let i = 0; i < 6; i++) g.add(part('trap_tar_crack' + i, new THREE.BoxGeometry(.03, .01, .40), mats.chassis, [Math.cos(i * 1.05) * .28, .125, Math.sin(i * 1.05) * .28], [0, i * 1.05 + .5, 0]));
    }
    return g;
  },
});
TRAPS.push(TAR('full'), TAR('depleted'));

const LAUNCHER = (state) => ({
  id: 'trap_launcher_' + state, file: `trap_launcher_${state}.glb`, label: `Launcher — ${state}`, swatch: '#ff6f1a',
  stats: { Cost: '55', Knockback: '8 m ÷ mass', Charges: '2', Rearm: '8 s' },
  note: { charged: 'Piston primed, pad raised a hand, buff-orange charge ring lit.', fired: 'Pad thrown to full extension and tilted down-route (+Z): the recoil frame.', rearming: 'Pad flat, ring dark, hazard lamps only. Reads as "not yet".' }[state],
  build(K) {
    const { part, cyl, box, mats, D } = K;
    const g = plate(K, 'trap_launcher', state === 'rearming' ? mats.trim : mats.energy_buff);
    const ext = { charged: .22, fired: .90, rearming: .04 }[state];
    const tilt = state === 'fired' ? -28 * D : 0;
    g.add(part('trap_launcher_cylinder', cyl(.20, .22, .30, 16), mats.steel_hull, [0, .23, 0]));
    g.add(part('trap_launcher_piston', cyl(.11, .11, ext + .02, 12), mats.chrome, [0, .38 + ext / 2, 0]));
    g.add(part('trap_launcher_pad', cyl(.56, .60, .08, 8), mats.steel_plate, [0, .40 + ext, state === 'fired' ? .18 : 0], [tilt, 0, 0]));
    g.add(K.hazardStripes('trap_launcher_pad_hazard', .9, .06, [0, .40 + ext, (state === 'fired' ? .18 : 0) + .56], [tilt, 0, 0], 5));
    g.add(part('trap_launcher_ring', cyl(.30, .30, .03, 24), state === 'rearming' ? mats.trim : mats.energy_buff, [0, .16, 0]));
    for (const s of [-1, 1]) g.add(part('trap_launcher_guide' + (s < 0 ? '_l' : '_r'), box(.06, .70, .06), mats.trim, [s * .62, .43, -.30]));
    return g;
  },
});
TRAPS.push(LAUNCHER('charged'), LAUNCHER('fired'), LAUNCHER('rearming'));
