/**
 * Deep Field 3D — §5 production UI, v2: AUGUR.
 *
 * One direction: a projected holographic HUD inside a suit helmet. Emissive
 * line-work, no fills, canted onto the visor's curve.
 *
 * The helmet is built in five layers, back to front, so the fiction holds:
 *   1  world      — the Spire atrium behind the glass
 *   2  depth()    — how far the glass sits from the eye: vignette, inner-lip light
 *   3  <HUD>      — the projection itself, canted so it lies ON the glass
 *   4  optics()   — glass artefacts: dust, hairline scratches, chromatic fringe,
 *                   calibration ticks, and the HUD's own glow bouncing back
 *   5  shell()    — the physical aperture: brow, cheek plates, chin guard, nose
 *                   bridge. OPAQUE and on top, so panels tuck under the brow.
 *   6  breath()   — the body in the suit: condensation at the cold corners
 *
 * Layer 5 is the one that makes it land. A frame drawn behind the HUD reads as
 * a border; a shell drawn over it reads as a helmet you are looking out of.
 *
 * Every number is lifted from sim/Sim.Core/Content (Maps.Spire, Waves["spire"],
 * Towers, Factions, Balance, Gunsmith). Nothing here is invented for looks.
 */

const ic = (n, cls = '', size = 0) => `<span class="icon ${cls}"${size ? ` style="width:${size}px;height:${size}px"` : ''}><img src="game/assets/ui/icon_${n}.svg" alt=""></span>`;
const A = (style, body = '') => `<div class="abs" style="${style}">${body}</div>`;
const lab = (t, extra = '') => `<span class="label" style="${extra}">${t}</span>`;
const rule = (T, extra = '') => `<div style="height:1px;background:${T.hairline};${extra}"></div>`;
const vrule = (T, h = 28) => `<div style="width:1px;height:${h}px;background:${T.hairline}"></div>`;
/** Canted onto the visor's curve. Panels are projections, not stickers. */
const cant = (deg, origin, body, extra = '') => `<div style="transform:perspective(1100px) rotateY(${deg}deg)${extra};transform-origin:${origin}">${body}</div>`;

/* ── match state: Spire, wave 6 of 12 ──────────────────────────────────
   Waves["spire"][5] = monolith ×1 (stair) · drifter ×6 (escape, +40 s)
   · skiff ×3 (air, +90 s). Maps.Spire: 12 waves, Night W8, Fog W12,
   five hero stations from Lobby 0 m to Roof 40 m, core on the roof. */
const M = {
  map: 'Spire', sector: 'Sector 03', wave: 6, total: 12, phase: 'ACTIVE', timer: '1:12',
  lives: 17, maxLives: 20,              // Balance.StartingLives = 20
  credits: 412,                          // Balance.StartingMoney 250 + wave income
  scrap: { alloy: 9, flux: 5, plating: 3, gravium: 1 },
  income: 180, teamShare: 50,            // Balance.ScrapTeamShare = 0.5
  alive: 10,
  lanes: [
    { id: 'stair', label: 'Stair', layer: 'ground', tier: 10, lead: 'monolith', count: 1, eta: 'contact', note: 'corked at flight 2' },
    { id: 'escape', label: 'Fire escape', layer: 'ground', tier: 0, lead: 'drifter', count: 6, eta: '0:22', note: 'converges at 20 m' },
    { id: 'air', label: 'Air spiral', layer: 'air', tier: 20, lead: 'skiff', count: 3, eta: '1:08', note: 'skips every floor' },
  ],
  floors: [['Roof', 40], ['Upper', 30], ['Mid', 20], ['Mezz', 10], ['Lobby', 0]],
  station: 'midFloor',
  schedule: [[8, 'night'], [12, 'fog']],
  built: [
    { id: 'lance', socket: 'g4', lvl: 3, hp: 100 },
    { id: 'nova', socket: 'g5', lvl: 2, hp: 100 },
    { id: 'skywatch', socket: 'w13', lvl: 2, hp: 78 },
    { id: 'arc', socket: 'w7', lvl: 1, hp: 100 },
    { id: 'detector', socket: 'g11', lvl: 1, hp: 100 },
    { id: 'filament', socket: 'w15', lvl: 3, hp: 92 },
  ],
  team: [
    { faction: 'forge', name: 'Kestrel', hp: 84, you: true, lvl: 3 },
    { faction: 'tempest', name: 'Vane', hp: 41, station: 'mezzanine' },
    { faction: 'specter', name: 'Aldis', hp: 100, station: 'roof' },
  ],
  you: { hp: 84, armor: 25, faction: 'forge', ability: 'Overdrive', cd: 38, cdMax: 30, lvl: 3 },
  suit: { o2: 94, seal: 'OK', temp: 31, pwr: 78, visor: 'DF-7 · OPT-3' },
  weapon: { id: 'rifle', mag: 18, magMax: 24, reserve: 96, ammo: 'ap', melee: 'wrench' },
  attach: [['longbarrel', 'Barrel'], ['compensator', 'Muzzle'], ['rangefinder', 'Optic'], ['drumfeed', 'Mag'], ['bracestock', 'Stock'], ['stabilizer', 'Under'], ['embercoil', 'Infusion']],
  feed: [
    ['<b>Vane</b> · Chain Surge — 4 struck', 'arc'],
    ['<b>Thermal Shock</b> ×3 · 84 dmg', 'brass'],
    ['<b>Monolith</b> corked the stair', 'dan'],
    ['<b>Aldis</b> built Filament · w15', ''],
  ],
};

/* Towers.cs — real costs and target layers. The shipped screens carry stale
   numbers (Lance 100 / Nova 140 / Arc 160); these are the sim's. */
const PLACE = [
  { id: 'lance', name: 'Lance', cost: 75, tag: 'ground', paths: 3 },
  { id: 'nova', name: 'Nova', cost: 115, tag: 'ground', paths: 3 },
  { id: 'singularity', name: 'Singularity', cost: 110, tag: 'ground · air', paths: 2 },
  { id: 'skywatch', name: 'Skywatch', cost: 90, tag: 'air only', paths: 3 },
  { id: 'arc', name: 'Arc', cost: 90, tag: 'ground · air', paths: 3 },
  { id: 'detector', name: 'Detector', cost: 70, tag: 'ground · air', paths: 2 },
  { id: 'filament', name: 'Filament', cost: 125, tag: 'ground · air', paths: 3 },
  { id: 'barricade', name: 'Barricade', cost: 60, tag: 'barricade', paths: 0 },
  { id: 'overclock', name: 'Overclock', cost: 130, tag: 'ground', paths: 0, ms: 'M4' },
];
/* Factions.cs — cooldown / radius / duration / magnitude / passive verbatim. */
const FACTIONS = [
  { id: 'forge', name: 'Forge', ability: 'Overdrive', cd: 30, r: 10, dur: 4, mag: '×1.5 rate', passive: '−10% build cost' },
  { id: 'ember', name: 'Ember', ability: 'Ignition Wave', cd: 22, r: 6, dur: 0, mag: 'burn', passive: '+30% burn duration' },
  { id: 'tempest', name: 'Tempest', ability: 'Chain Surge', cd: 26, r: 7, dur: 0, mag: '6 dmg / hit', passive: '+12% fire rate' },
  { id: 'glacier', name: 'Glacier', ability: 'Cryo Field', cd: 24, r: 8, dur: 0, mag: 'chill', passive: '+25% vs slowed' },
  { id: 'specter', name: 'Specter', ability: 'Reveal Pulse', cd: 34, r: 0, dur: 0, mag: 'map-wide', passive: 'weak points shown' },
];

/* ── the Augur treatment ───────────────────────────────────────────────
   A projected panel is not a card. It has no fill worth speaking of, its
   edges fade rather than stop, it carries the projector's line raster, and
   it is registered by corner ticks — the marks the optics calibrate against. */
const RASTER = 'repeating-linear-gradient(180deg,rgba(101,220,228,.055) 0 1px,transparent 1px 3px)';
const ticks = (c) => ['left:0;top:0;border-left:1px solid;border-top:1px solid', 'right:0;top:0;border-right:1px solid;border-top:1px solid',
  'left:0;bottom:0;border-left:1px solid;border-bottom:1px solid', 'right:0;bottom:0;border-right:1px solid;border-bottom:1px solid']
  .map((s) => A(`${s};width:9px;height:9px;border-color:${c};pointer-events:none`)).join('');

const T = {
  id: 'augur', name: 'Augur',
  accent: 'var(--arcane-300)', accent2: 'var(--arcane-500)',
  hairline: 'rgba(101,220,228,.30)', ink: 'var(--steel-050)', ink2: 'var(--arcane-300)',
  track: 'rgba(101,220,228,.12)',
  card: (body, extra = '') => `<div style="position:relative;background:linear-gradient(180deg,rgba(101,220,228,.055),rgba(101,220,228,.012)),${RASTER};clip-path:var(--clip-chamfer);box-shadow:inset 0 1px 0 rgba(101,220,228,.42),inset 0 -1px 0 rgba(101,220,228,.18),0 0 34px rgba(47,180,190,.13);${extra}">${body}${ticks('var(--arcane-300)')}</div>`,
  slot: (body, on, extra = '') => `<div style="background:${on ? 'rgba(101,220,228,.10)' : 'transparent'};clip-path:var(--clip-chamfer-sm);box-shadow:inset 0 0 0 1px ${on ? 'var(--arcane-300)' : 'rgba(101,220,228,.26)'}${on ? ',0 0 18px rgba(101,220,228,.35)' : ''};display:grid;place-items:center;position:relative;${extra}">${body}</div>`,
};

/* ── helmet ────────────────────────────────────────────────────────────── */

/** Layer 2 — the glass sits a hand's width from the eye. */
function depth() {
  return `${A('inset:0;pointer-events:none;background:radial-gradient(120% 78% at 50% 46%,transparent 42%,rgba(8,11,17,.46) 74%,rgba(8,11,17,.94) 100%)')}
  ${A('inset:0;pointer-events:none;background:var(--tex-scanline);opacity:.5')}
  ${A('left:96px;right:96px;top:78px;bottom:76px;pointer-events:none;box-shadow:inset 0 0 0 1px rgba(101,220,228,.16),inset 0 0 60px rgba(47,180,190,.07)')}`;
}

/** Layer 4 — the visor's own surface, between the projection and the eye. */
function optics() {
  const dust = [[318, 214, 2], [512, 640, 1], [744, 168, 1], [901, 848, 2], [1140, 300, 1], [1322, 690, 2],
  [1487, 205, 1], [1640, 520, 1], [420, 900, 1], [1780, 764, 2], [660, 420, 1], [1044, 966, 1]]
    .map(([x, y, r]) => A(`left:${x}px;top:${y}px;width:${r}px;height:${r}px;background:rgba(236,240,247,.16);border-radius:50%`)).join('');
  const scratch = [[210, 300, 190, -24], [1420, 250, 260, 12], [700, 830, 150, -8]]
    .map(([x, y, w, d]) => A(`left:${x}px;top:${y}px;width:${w}px;height:1px;transform:rotate(${d}deg);background:linear-gradient(90deg,transparent,rgba(236,240,247,.13),transparent)`)).join('');
  return `${dust}${scratch}
  ${A('inset:0;pointer-events:none;background:linear-gradient(108deg,transparent 33%,rgba(236,240,247,.05) 43%,transparent 49%),linear-gradient(255deg,transparent 60%,rgba(236,240,247,.032) 70%,transparent 76%)')}
  ${A('left:50%;bottom:0;transform:translateX(-50%);width:1400px;height:420px;pointer-events:none;background:radial-gradient(60% 100% at 50% 100%,rgba(47,180,190,.15),transparent 72%)')}
  ${A('left:96px;right:96px;top:78px;bottom:76px;pointer-events:none;box-shadow:inset 2px 0 0 rgba(101,220,228,.10),inset -2px 0 0 rgba(224,84,120,.07)')}
  ${A('left:96px;right:96px;top:78px;bottom:76px;pointer-events:none', ticks('rgba(101,220,228,.55)').replace(/width:9px;height:9px/g, 'width:22px;height:22px'))}`;
}

/** Layer 5 — the shell. Opaque, over everything: brow, cheeks, chin, nose. */
function shell() {
  const plate = 'linear-gradient(180deg,#0b0e15,#05070b)';
  const vent = (n) => `<div class="row" style="gap:5px">${Array.from({ length: n }, () => `<i style="width:3px;height:11px;background:#04060a;box-shadow:0 -1px 0 rgba(236,240,247,.06)"></i>`).join('')}</div>`;
  const s = M.suit;
  const tele = [['O₂', s.o2 + '%', s.o2 > 90 ? T.accent : 'var(--state-warning)'], ['SEAL', s.seal, 'var(--venom-400)'],
  ['TEMP', s.temp + '°C', 'var(--text-muted)'], ['PWR', s.pwr + '%', 'var(--text-muted)']]
    .map(([k, v, c]) => `<span class="row" style="gap:5px;align-items:baseline">${lab(k, 'font-size:9px;color:var(--text-disabled)')}<span class="mono" style="font-size:var(--size-micro);color:${c}">${v}</span></span>`).join(vrule(T, 10));
  return `
  ${A(`left:0;right:0;top:0;height:78px;background:${plate};clip-path:polygon(0 0,100% 0,100% 58%,93% 100%,7% 100%,0 58%);box-shadow:inset 0 -1px 0 rgba(101,220,228,.20),0 6px 22px rgba(4,6,10,.8)`,
    `${A('left:120px;top:26px', `<span class="mono" style="font-size:9px;color:var(--text-disabled);letter-spacing:var(--tracking-micro)">${s.visor}</span>`)}
     ${A('left:50%;top:24px;transform:translateX(-50%)', `<div class="row" style="gap:14px;align-items:center">${tele}</div>`)}
     ${A('right:120px;top:24px', vent(11))}`)}
  ${A(`left:0;top:0;bottom:0;width:98px;background:${plate};clip-path:polygon(0 0,100% 0,100% 7%,64% 22%,64% 78%,100% 93%,100% 100%,0 100%);box-shadow:inset -1px 0 0 rgba(101,220,228,.16)`,
    A('left:22px;top:50%;transform:translateY(-50%) rotate(90deg)', `<span class="mono" style="font-size:9px;color:var(--text-disabled);letter-spacing:.18em">PORT</span>`))}
  ${A(`right:0;top:0;bottom:0;width:98px;background:${plate};clip-path:polygon(0 0,100% 0,100% 100%,0 100%,0 93%,36% 78%,36% 22%,0 7%);box-shadow:inset 1px 0 0 rgba(101,220,228,.16)`,
    A('right:22px;top:50%;transform:translateY(-50%) rotate(-90deg)', `<span class="mono" style="font-size:9px;color:var(--text-disabled);letter-spacing:.18em">STBD</span>`))}
  ${A(`left:0;right:0;bottom:0;height:76px;background:${plate};clip-path:polygon(0 100%,0 0,8% 0,15% 42%,85% 42%,92% 0,100% 0,100% 100%);box-shadow:inset 0 1px 0 rgba(101,220,228,.18)`)}
  ${A(`left:50%;bottom:0;transform:translateX(-50%);width:230px;height:96px;background:radial-gradient(74% 100% at 50% 100%,#0c1017,#06080d 62%,transparent 78%);clip-path:polygon(20% 100%,6% 46%,26% 6%,74% 6%,94% 46%,80% 100%)`,
    `${A('left:50%;bottom:14px;transform:translateX(-50%)', vent(7))}
     ${A('left:0;right:0;top:6px;height:1px;background:rgba(101,220,228,.12)')}`)}`;
}

/** Layer 6 — a body inside the suit. Condensation where the shell runs cold. */
function breath() {
  return `${A('left:96px;bottom:76px;width:300px;height:150px;pointer-events:none;background:radial-gradient(70% 100% at 0% 100%,rgba(236,240,247,.055),transparent 70%)')}
  ${A('right:96px;bottom:76px;width:300px;height:150px;pointer-events:none;background:radial-gradient(70% 100% at 100% 100%,rgba(236,240,247,.05),transparent 70%)')}
  ${A('left:50%;bottom:76px;transform:translateX(-50%);width:420px;height:96px;pointer-events:none;background:radial-gradient(60% 100% at 50% 100%,rgba(236,240,247,.045),transparent 72%)')}`;
}
const visorBack = () => depth();
const visorFront = () => `${optics()}${shell()}${breath()}`;

/** Stand-in for the Spire atrium: vertical structure, seen through glass. */
function spireWorld() {
  return `<div class="world">
    ${A('left:0;right:0;top:38%;height:1px;background:linear-gradient(90deg,transparent,rgba(101,220,228,.16),transparent)')}
    ${[[6, 200, 620], [17, 90, 780], [29, 150, 520], [62, 120, 700], [76, 240, 840], [90, 80, 560]].map(([l, w, h]) =>
    A(`left:${l}%;bottom:26%;width:${w}px;height:${h}px;background:linear-gradient(180deg,#0b0e15,#080b11);opacity:.94;box-shadow:inset 1px 0 0 rgba(101,220,228,.05)`)).join('')}
    ${A('left:44%;bottom:26%;width:260px;height:900px;background:linear-gradient(180deg,#0d1119,#080b11);box-shadow:inset 0 0 0 1px rgba(101,220,228,.06)',
    Array.from({ length: 9 }, (_, i) => A(`left:0;right:0;bottom:${60 + i * 96}px;height:1px;background:rgba(101,220,228,.07)`)).join(''))}
    <div class="horizon"></div></div>`;
}

/* ── components ────────────────────────────────────────────────────────── */

/** Core integrity — the loudest thing on screen. On a map whose fight ends
 *  forty metres up, a leak you did not see is what you most need to know. */
function coreBlock({ big = true } = {}) {
  const frac = M.lives / M.maxLives, low = frac <= 0.4;
  const col = low ? 'var(--state-danger)' : T.accent;
  return `<div class="col" style="gap:6px;align-items:center">
    ${lab('Core integrity', `color:${T.ink2}`)}
    <div class="row" style="gap:8px;align-items:baseline">
      <span class="mono" style="font:var(--weight-black) ${big ? '58px' : '42px'}/1 var(--font-display);color:${col};text-shadow:0 0 30px rgba(101,220,228,.5)">${M.lives}</span>
      <span class="mono" style="font-size:var(--size-stat-sm);color:var(--text-muted)">/ ${M.maxLives}</span>
    </div>
    <div class="row" style="gap:2px">${Array.from({ length: M.maxLives }, (_, i) =>
    `<i style="width:6px;height:${big ? 14 : 11}px;background:${i < M.lives ? col : 'transparent'};box-shadow:${i < M.lives ? `0 0 7px rgba(101,220,228,.5)` : 'inset 0 0 0 1px rgba(101,220,228,.2)'};clip-path:polygon(50% 0,100% 22%,100% 100%,0 100%,0 22%)"></i>`).join('')}</div>
    <span class="mono" style="font-size:var(--size-micro);color:var(--text-muted)">ROOF · 40 m · ${M.map.toUpperCase()}</span>
  </div>`;
}

/** Wave dossier: where you are in twelve, and what weather is coming. */
function waveBlock() {
  return `<div class="col" style="gap:8px;min-width:250px">
    <div class="row between">
      <span class="row" style="gap:8px;align-items:baseline">${lab('Wave')}
        <span class="mono" style="font:var(--weight-black) 30px/1 var(--font-display);color:${T.ink}">${String(M.wave).padStart(2, '0')}</span>
        <span class="mono" style="font-size:var(--size-caption);color:var(--text-muted)">/ ${M.total}</span></span>
      <span class="col" style="gap:2px;align-items:flex-end">${lab(M.phase, 'color:var(--wave-active)')}
        <span class="mono" style="font-size:var(--size-stat-sm);color:${T.ink}">${M.timer}</span></span>
    </div>
    <div class="row" style="gap:3px">${Array.from({ length: M.total }, (_, i) => {
    const sched = M.schedule.find(([w]) => w === i + 1);
    const bg = i < M.wave - 1 ? 'var(--venom-500)' : i === M.wave - 1 ? 'var(--wave-active)' : sched ? 'var(--soul-500)' : 'transparent';
    return `<i title="wave ${i + 1}" style="flex:1;height:5px;background:${bg};box-shadow:${bg === 'transparent' ? 'inset 0 0 0 1px rgba(101,220,228,.22)' : i === M.wave - 1 ? '0 0 8px var(--ember-500)' : 'none'}"></i>`;
  }).join('')}</div>
    <div class="row between">
      <span class="row" style="gap:6px">${lab('Alive')}<span class="mono" style="font-size:var(--size-caption);color:${T.ink}">${M.alive}</span></span>
      <span class="row" style="gap:6px">${lab('Next')}<span class="mono" style="font-size:var(--size-caption);color:var(--soul-400)">W8 NIGHT</span></span>
    </div>
  </div>`;
}

/** Spire's signature: a forty-metre building read as a vertical gauge. */
function floorGauge({ h = 300, w = 214 } = {}) {
  const rows = M.floors.map(([name, m]) => {
    const here = M.lanes.filter((l) => l.tier === m);
    const you = M.station === 'midFloor' && m === 20;
    return `<div class="row" style="gap:8px;align-items:center;height:${h / 5}px">
      <span class="mono" style="width:26px;text-align:right;font-size:var(--size-micro);color:${m === 40 ? T.accent : 'var(--text-muted)'}">${m}</span>
      <i style="width:7px;height:7px;transform:rotate(45deg);background:${m === 40 ? T.accent : here.length ? 'var(--state-danger)' : 'transparent'};box-shadow:${m === 40 ? `0 0 8px ${T.accent}` : here.length ? 'none' : 'inset 0 0 0 1px rgba(101,220,228,.3)'}"></i>
      <span class="col" style="gap:1px;flex:1;min-width:0">
        <span style="font:var(--weight-semibold) var(--size-micro)/1 var(--font-display);color:${m === 40 ? T.ink : 'var(--text-secondary)'};text-transform:uppercase;letter-spacing:var(--tracking-label)">${name}${m === 40 ? ' · core' : ''}</span>
        ${here.length ? `<span class="row" style="gap:4px">${here.map((l) => `${ic('enemy_' + l.lead, '', 11)}<span class="mono" style="font-size:9px;color:var(--threat-400)">×${l.count}</span>`).join('')}</span>` : ''}
      </span>
      ${you ? `<span class="mono" style="font-size:9px;color:${T.accent}">YOU</span>` : ''}
    </div>`;
  }).join('');
  return `<div style="width:${w}px;position:relative">
    ${A(`left:36px;top:8px;bottom:8px;width:1px;background:linear-gradient(180deg,${T.accent},${T.hairline})`)}
    <div class="col" style="gap:0">${rows}</div></div>`;
}

/** Three lanes, three answers. Layer, leader, count, ETA — Waves["spire"][5]. */
function laneBoard({ w = 340 } = {}) {
  return `<div class="col" style="gap:7px;width:${w}px">
    ${M.lanes.map((l) => `<div class="row" style="gap:9px;align-items:center">
      ${T.slot(ic('enemy_' + l.lead, '', 18), l.id === 'stair', 'width:30px;height:30px;flex:0 0 auto')}
      <span class="col" style="gap:2px;flex:1;min-width:0">
        <span class="row between"><span style="font:var(--weight-semibold) var(--size-caption)/1 var(--font-display);color:${T.ink}">${l.label}</span>
          <span class="mono" style="font-size:var(--size-micro);color:${l.eta === 'contact' ? 'var(--state-danger)' : 'var(--text-muted)'}">${l.eta}</span></span>
        <span class="row between"><span class="mono" style="font-size:var(--size-micro);color:var(--text-muted)">${l.layer} · ${l.tier} m · ${l.note}</span>
          <span class="mono" style="font-size:var(--size-micro);color:var(--threat-400)">×${l.count}</span></span>
      </span></div>`).join('')}
  </div>`;
}

/** The wallet. Credits loud, scrap quiet, income explained. */
function econBlock({ inline = false } = {}) {
  const chips = Object.entries(M.scrap).map(([k, v]) => `<span class="row" style="gap:4px">${ic('scrap_' + k, v ? '' : 'dim', 15)}<span class="mono" style="font-size:var(--size-caption);color:${v ? T.ink : 'var(--text-disabled)'}">${v}</span></span>`).join('');
  if (inline) return `<div class="row" style="gap:12px;align-items:center"><i style="width:9px;height:9px;transform:rotate(45deg);background:var(--res-gold);box-shadow:0 0 8px var(--res-gold)"></i><span class="mono" style="font:var(--weight-bold) 20px/1 var(--font-display);color:${T.accent}">${M.credits}</span>${vrule(T, 18)}${chips}</div>`;
  return `<div class="col" style="gap:8px;min-width:222px">
    <div class="row" style="gap:10px;align-items:baseline">
      <i style="width:11px;height:11px;transform:rotate(45deg);background:var(--res-gold);box-shadow:0 0 10px var(--res-gold)"></i>
      <span class="mono" style="font:var(--weight-black) 32px/1 var(--font-display);color:${T.accent}">${M.credits}</span>
      ${lab('credits')}
      <span class="mono" style="margin-left:auto;font-size:var(--size-micro);color:var(--venom-400)">+${M.income}</span>
    </div>
    ${rule(T)}
    <div class="row between">${chips}</div>
    <span class="mono" style="font-size:9px;color:var(--text-muted)">${M.teamShare}% of drops → team pool</span>
  </div>`;
}

/** Vitals + the faction ability. */
function vitalsBlock({ w = 400 } = {}) {
  const y = M.you, f = FACTIONS.find((x) => x.id === y.faction), pct = Math.round(y.cd / y.cdMax * 100);
  return `<div class="row" style="gap:13px;align-items:stretch;width:${w}px">
    ${T.slot(`${ic('ability_' + f.ability.toLowerCase().replace(/ /g, ''), 'arc')}
      ${A('left:5px;top:4px;font:var(--weight-bold) 10px/1 var(--font-mono);color:var(--text-muted)', 'Q')}
      ${A(`right:5px;bottom:4px;font:var(--weight-bold) var(--size-micro)/1 var(--font-mono);color:${T.accent}`, 'RDY')}`, true, 'width:74px;height:74px;flex:0 0 auto')}
    <span class="col grow" style="gap:5px;justify-content:center;min-width:0">
      <span class="row between">
        <span class="row" style="gap:6px"><span class="faction ${f.id}"></span>
          <span style="font:var(--weight-semibold) var(--size-caption)/1 var(--font-display);color:${T.ink}">${f.name} · ${f.ability}</span>
          <span class="mono" style="font-size:9px;color:${T.accent}">L${y.lvl}</span></span>
        <span class="mono" style="font-size:9px;color:var(--text-muted)">${f.passive}</span>
      </span>
      <span class="row between">${lab('HP')}<span class="mono" style="font-size:var(--size-caption);color:${T.ink}">${y.hp}/100</span></span>
      <div style="height:7px;background:${T.track};box-shadow:inset 0 0 0 1px ${T.hairline}"><i style="display:block;height:100%;width:${y.hp}%;background:var(--bar-hp)"></i></div>
      <span class="row between">${lab('Armour plate')}<span class="mono" style="font-size:9px;color:var(--bar-armor)">${y.armor}/25</span></span>
      <div style="height:4px;background:${T.track}"><i style="display:block;height:100%;width:${y.armor / 25 * 100}%;background:var(--bar-armor)"></i></div>
      <span class="row" style="gap:5px">${lab(`${f.r} m · ${f.dur ? f.dur + ' s · ' : ''}${f.mag}`, 'font-size:9px')}<span style="flex:1"></span>${lab(pct >= 100 ? 'ready' : y.cdMax - y.cd + ' s', `font-size:9px;color:${T.accent}`)}</span>
    </span></div>`;
}

/** Loadout: platform, magazine, ammo, and the seven Gunsmith slots. */
function loadoutBlock({ w = 400 } = {}) {
  const wp = M.weapon;
  return `<div class="col" style="gap:8px;width:${w}px;align-items:flex-end">
    <div class="row" style="gap:13px;align-items:stretch;width:100%;justify-content:flex-end">
      <span class="col grow" style="gap:5px;align-items:flex-end;min-width:0">
        <span class="row" style="gap:8px">${lab('Rifle · MARKSMAN B', `color:${T.ink2}`)}${ic('weapon_' + wp.id)}</span>
        <span class="row" style="gap:9px;align-items:baseline">
          <span class="mono" style="font:var(--weight-black) 46px/1 var(--font-display);color:${T.ink}">${wp.mag}</span>
          <span class="mono" style="font-size:var(--size-stat-sm);color:var(--text-muted)">/ ${wp.reserve}</span></span>
        <div class="row" style="gap:2px;width:100%;justify-content:flex-end">${Array.from({ length: wp.magMax }, (_, i) =>
    `<i style="width:6px;height:9px;background:${i < wp.mag ? T.accent : 'transparent'};box-shadow:${i < wp.mag ? '0 0 6px rgba(101,220,228,.45)' : `inset 0 0 0 1px ${T.hairline}`}"></i>`).join('')}</div>
        <span class="row" style="gap:8px;align-items:center">${ic('ammo_' + wp.ammo, '', 15)}
          <span class="mono" style="font-size:var(--size-caption);color:${T.ink}">AP</span>
          <span class="mono" style="font-size:9px;color:var(--text-muted)">×0.85 · ignores flat armour</span></span>
      </span>
      <span class="col" style="gap:6px">
        ${[['sidearm', 1], ['rifle', 2], [wp.melee, 3]].map(([s, k]) => T.slot(`${ic('weapon_' + s, s === wp.id ? 'arc' : 'dim', 22)}${A('left:4px;top:3px;font:var(--weight-bold) 9px/1 var(--font-mono);color:var(--text-muted)', k)}`, s === wp.id, 'width:52px;height:52px')).join('')}
      </span>
    </div>
    ${rule(T, 'width:100%')}
    <div class="row" style="gap:4px;flex-wrap:wrap;justify-content:flex-end">${M.attach.map(([a, slot]) =>
    `<span class="row" style="gap:4px;padding:3px 6px;box-shadow:inset 0 0 0 1px ${T.hairline};clip-path:var(--clip-chamfer-sm)">${ic('attach_' + a, '', 13)}<span class="mono" style="font-size:9px;color:var(--text-muted)">${slot}</span></span>`).join('')}</div>
  </div>`;
}

/** Teammates: faction, health, and which floor they are holding. */
function teamBlock({ w = 232 } = {}) {
  return `<div class="col" style="gap:6px;width:${w}px">
    ${M.team.map((p) => `<div class="row" style="gap:9px;align-items:center;padding:6px 8px;box-shadow:inset 0 0 0 1px ${p.hp <= 45 ? 'var(--threat-500)' : T.hairline};clip-path:var(--clip-chamfer-sm);background:${p.you ? 'rgba(101,220,228,.06)' : 'transparent'}">
      <span class="faction ${p.faction}"></span>
      <span class="col" style="gap:3px;flex:1;min-width:0">
        <span class="row between"><span style="font:var(--weight-semibold) var(--size-caption)/1 var(--font-display);color:${T.ink}">${p.name}</span>
          <span class="mono" style="font-size:9px;color:${p.hp <= 45 ? 'var(--state-danger)' : 'var(--text-muted)'}">${p.hp}%</span></span>
        <div style="height:3px;background:${T.track}"><i style="display:block;height:100%;width:${p.hp}%;background:${p.hp <= 45 ? 'var(--bar-hp-low)' : 'var(--bar-hp)'}"></i></div>
        <span class="mono" style="font-size:9px;color:var(--text-muted)">${p.you ? 'you · mid 20 m' : p.station === 'roof' ? 'roof 40 m' : 'mezzanine 10 m'}</span>
      </span></div>`).join('')}
  </div>`;
}

/** Built towers: what is standing, at what level, and what is hurt. */
function structuresBlock({ w = 250 } = {}) {
  return `<div class="col" style="gap:5px;width:${w}px">
    ${M.built.map((b) => { const d = PLACE.find((p) => p.id === b.id); return `<div class="row" style="gap:8px;align-items:center">
      ${ic('tower_' + b.id, '', 16)}
      <span style="font:var(--weight-semibold) var(--size-micro)/1 var(--font-display);color:${T.ink};flex:1">${d.name}</span>
      <span class="mono" style="font-size:9px;color:var(--text-muted)">${b.socket}</span>
      <span class="row" style="gap:2px">${Array.from({ length: 5 }, (_, i) => `<i style="width:4px;height:8px;background:${i < b.lvl ? T.accent : 'transparent'};box-shadow:${i < b.lvl ? 'none' : 'inset 0 0 0 1px rgba(101,220,228,.22)'}"></i>`).join('')}</span>
      <span class="mono" style="font-size:9px;width:30px;text-align:right;color:${b.hp < 100 ? 'var(--state-warning)' : 'var(--text-disabled)'}">${b.hp}%</span>
    </div>`; }).join('')}
  </div>`;
}

function feedBlock({ align = 'flex-end' } = {}) {
  const col = { arc: 'var(--arcane-400)', brass: 'var(--brass-400)', dan: 'var(--threat-400)', '': 'var(--text-secondary)' };
  return `<div class="col" style="gap:4px;align-items:${align}">${M.feed.map(([t, c]) =>
    `<span style="font:var(--weight-medium) var(--size-caption)/1.3 var(--font-ui);color:${col[c]};padding:3px 8px 3px 10px;box-shadow:inset 1px 0 0 ${col[c]}">${t}</span>`).join('')}</div>`;
}

function crosshairMark() {
  const c = 'var(--arcane-300)';
  return `${A('left:50%;top:50%;transform:translate(-50%,-50%);width:56px;height:56px',
    `${A(`left:50%;top:0;width:2px;height:11px;margin-left:-1px;background:${c}`)}${A(`left:50%;bottom:0;width:2px;height:11px;margin-left:-1px;background:${c}`)}
     ${A(`top:50%;left:0;height:2px;width:11px;margin-top:-1px;background:${c}`)}${A(`top:50%;right:0;height:2px;width:11px;margin-top:-1px;background:${c}`)}
     ${A(`left:50%;top:50%;width:2px;height:2px;margin:-1px 0 0 -1px;background:${c}`)}
     ${A(`left:50%;top:50%;width:30px;height:30px;transform:translate(-50%,-50%);box-shadow:inset 0 0 0 1px ${c};clip-path:polygon(50% 0,100% 25%,100% 75%,50% 100%,0 75%,0 25%);opacity:.65`)}`)}
  ${A('left:50%;top:calc(50% + 40px);transform:translateX(-50%)', `<span class="mono" style="font-size:var(--size-micro);color:var(--bar-armor)">ARMOURED · BRING AP</span>`)}`;
}

function overheads() {
  return [
    { x: 760, y: 500, hp: 62, name: 'Monolith', status: ['chill', 'shred'], w: 128 },
    { x: 1290, y: 452, hp: 100, sh: 100, name: 'Warden', status: ['mark'], w: 112 },
    { x: 1055, y: 566, hp: 38, name: 'Drifter', status: ['burn'], w: 96 },
  ].map((e) => A(`left:${e.x}px;top:${e.y}px;transform:translate(-50%,-100%)`, `
    <div class="col" style="gap:3px;align-items:center;width:${e.w}px">
      <span class="row" style="gap:6px;align-items:center">${lab(e.name, `color:${T.ink2};font-size:9px`)}
        ${e.status.map((s) => ic('status_' + s, '', 12)).join('')}</span>
      <div style="width:100%;height:4px;background:${T.track};box-shadow:inset 0 0 0 1px ${T.hairline}"><i style="display:block;height:100%;width:${e.hp}%;background:${e.hp <= 30 ? 'var(--bar-hp-low)' : 'var(--bar-hp)'}"></i></div>
      ${e.sh ? `<div style="width:100%;height:3px;background:${T.track}"><i style="display:block;height:100%;width:${e.sh}%;background:var(--bar-shield)"></i></div>` : ''}
    </div>`)).join('');
}

/** The strip along the visor's inner lip — mission identity, canted back so it
 *  reads as lying against the top of the glass rather than floating. */
function lipStrip() {
  return A('left:50%;top:96px;transform:translateX(-50%)', cant(0, 'center top', `<div class="row" style="gap:16px;align-items:center;padding:6px 20px;box-shadow:inset 0 -1px 0 rgba(101,220,228,.24)">
    <span class="mono" style="font-size:var(--size-micro);color:${T.accent};letter-spacing:var(--tracking-micro)">${M.map.toUpperCase()} · ${M.sector.toUpperCase()}</span>
    ${vrule(T, 12)}<span class="mono" style="font-size:var(--size-micro);color:var(--text-muted)">MID FLOOR · 20 m</span>
    ${vrule(T, 12)}<span class="mono" style="font-size:var(--size-micro);color:var(--text-muted)">CORE +20 m ABOVE</span>
    ${vrule(T, 12)}<span class="mono" style="font-size:var(--size-micro);color:var(--venom-400)">LINK OK</span>
  </div>`, ' rotateX(24deg)'));
}

/* ── screen: HUD ───────────────────────────────────────────────────────── */
function hudScreen() {
  return `${spireWorld()}${visorBack()}${overheads()}${crosshairMark()}
    ${lipStrip()}
    ${A('left:110px;top:150px', cant(16, 'left center', T.card(econBlock({ inline: true }), 'padding:9px 14px')))}
    ${A('left:110px;top:216px', cant(16, 'left center', T.card(teamBlock({ w: 218 }), 'padding:8px 9px')))}
    ${A('left:110px;top:436px', cant(14, 'left center', floorGauge({ h: 280, w: 206 })))}
    ${A('right:110px;top:150px', cant(-16, 'right center', T.card(`${lab('Inbound', `color:${T.ink2};display:block;margin-bottom:8px`)}${laneBoard({ w: 300 })}`, 'padding:11px 13px')))}
    ${A('right:110px;top:374px', cant(-14, 'right center', structuresBlock({ w: 232 })))}
    ${A('right:110px;top:566px', cant(-14, 'right center', feedBlock()))}
    ${A('left:50%;bottom:0;transform:translateX(-50%);width:1560px;height:360px',
    `${A('inset:auto 0 0 0;height:290px;background:linear-gradient(180deg,rgba(101,220,228,.05),rgba(8,11,17,.5));clip-path:polygon(0 100%,11% 0,89% 0,100% 100%);box-shadow:inset 0 1px 0 rgba(101,220,228,.3)')}
     ${A('left:50%;bottom:244px;transform:translateX(-50%)', `<div class="row" style="gap:22px;align-items:center">${coreBlock({ big: false })}${vrule(T, 74)}${waveBlock()}</div>`)}
     ${A('left:116px;bottom:150px', cant(20, 'left bottom', vitalsBlock({ w: 428 })))}
     ${A('right:116px;bottom:150px', cant(-20, 'right bottom', loadoutBlock({ w: 428 })))}
     ${A('left:50%;bottom:104px;transform:translateX(-50%)', `<div class="row" style="gap:6px;align-items:center">
       ${PLACE.slice(0, 8).map((p) => T.slot(`${ic('tower_' + p.id, M.credits >= p.cost ? '' : 'dim', 19)}${A(`top:2px;right:4px;font:var(--weight-bold) var(--size-micro)/1 var(--font-mono);color:${M.credits >= p.cost ? T.accent : 'var(--text-disabled)'}`, p.cost)}`, false, 'width:46px;height:46px')).join('')}</div>`)}`)}
    ${A('left:50%;bottom:326px;transform:translateX(-50%)', `<div class="toast prompt brass"><span class="rail"></span><kbd class="key">E</kbd><span class="body"><b>Build</b> · socket g5 · ground</span></div>`)}
    ${visorFront()}`;
}

/* ── screen: build ─────────────────────────────────────────────────────── */
function buildScreen() {
  const n = PLACE.length;
  const wheel = `<div style="position:relative;width:620px;height:620px">
    ${A(`inset:0;box-shadow:inset 0 0 0 1px ${T.hairline};border-radius:50%;background:radial-gradient(circle,rgba(101,220,228,.05),transparent 70%)`)}
    ${A(`left:50%;top:50%;width:210px;height:210px;transform:translate(-50%,-50%);border-radius:50%;box-shadow:inset 0 0 0 1px ${T.hairline}`)}
    ${PLACE.map((p, i) => {
    const a = (i / n) * Math.PI * 2 - Math.PI / 2, x = 310 + Math.cos(a) * 232, y = 310 + Math.sin(a) * 232;
    const afford = M.credits >= p.cost, sel = p.id === 'nova';
    return A(`left:${x}px;top:${y}px;transform:translate(-50%,-50%)`, T.slot(`
        <span class="col" style="gap:3px;align-items:center;padding:8px 4px">
          ${ic('tower_' + p.id, afford ? '' : 'dim', 26)}
          <span style="font:var(--weight-bold) 10px/1 var(--font-display);color:${afford ? T.ink : 'var(--text-disabled)'};text-transform:uppercase;letter-spacing:.04em">${p.name}</span>
          <span class="mono" style="font-size:10px;color:${afford ? T.accent : 'var(--state-danger)'}">${p.cost}</span>
          ${p.ms ? `<span class="mono" style="font-size:8px;color:var(--soul-400)">${p.ms}</span>` : ''}
        </span>`, sel, 'width:104px;height:104px'));
  }).join('')}
    ${A('left:50%;top:50%;transform:translate(-50%,-50%)', `<div class="col" style="gap:4px;align-items:center">
      ${lab('socket', `color:${T.ink2}`)}<span class="mono" style="font:var(--weight-black) 34px/1 var(--font-display);color:${T.ink}">g5</span>
      <span class="mono" style="font-size:var(--size-micro);color:var(--text-muted)">ground · −8, 0, −4.9</span>
      <span class="mono" style="font-size:9px;color:var(--venom-400)">6 m off the stair</span></div>`)}
  </div>`;
  const detail = T.card(`
    <div class="row between" style="margin-bottom:10px"><span class="row" style="gap:8px">${ic('tower_nova')}<span style="font:var(--weight-bold) var(--size-body)/1 var(--font-display);color:${T.ink}">Nova</span></span>
      <span class="mono" style="font-size:var(--size-caption);color:${T.accent}">115</span></div>
    ${rule(T)}
    <div class="col" style="gap:7px;margin:10px 0">
      ${[['Damage', '22'], ['Rate', '0.5 /s'], ['Range', '16 m · min 5 m'], ['Splash', '3.2 m · 0.35 falloff'], ['Targets', 'ground only'], ['Structure', '140 hp']].map(([k, v]) =>
    `<span class="row between">${lab(k)}<span class="mono" style="font-size:var(--size-caption);color:${T.ink}">${v}</span></span>`).join('')}
    </div>
    ${rule(T)}
    <div class="row" style="gap:6px;margin-top:10px">${['damage', 'range', 'rate'].map((p) =>
    `<span class="row" style="gap:5px;padding:4px 7px;box-shadow:inset 0 0 0 1px ${T.hairline}">${ic('path_' + p, '', 13)}<span class="mono" style="font-size:9px;color:var(--text-muted)">${p}</span></span>`).join('')}</div>
    <p style="margin:10px 0 0;font:var(--weight-regular) var(--size-caption)/1.45 var(--font-ui);color:var(--text-secondary);text-wrap:pretty">The clump answer. A Monolith corking the stair is one target — this is not for that. It is for the six drifters climbing the escape behind it.</p>`,
    'padding:14px 16px;width:340px');
  return `${spireWorld()}${visorBack()}${lipStrip()}
    ${A('left:50%;top:53%;transform:translate(-50%,-50%);width:660px;height:660px;border-radius:50%;background:radial-gradient(circle,rgba(101,220,228,.10),transparent 68%)')}
    ${A('left:50%;top:53%;transform:translate(-50%,-50%)', wheel)}
    ${A('right:112px;top:50%;transform:translateY(-50%)', cant(-16, 'right center', detail))}
    ${A('left:112px;top:152px', cant(16, 'left center', T.card(econBlock({ inline: true }), 'padding:9px 14px')))}
    ${A('left:112px;bottom:120px', cant(14, 'left center', T.card(`${lab('Spire · vertical', `color:${T.ink2};display:block;margin-bottom:8px`)}${floorGauge({ h: 250, w: 206 })}`, 'padding:12px 14px')))}
    ${A('left:50%;bottom:104px;transform:translateX(-50%)', `<div class="row" style="gap:10px"><span class="btn">Place <kbd class="key">LMB</kbd></span><span class="btn sec">Cancel <kbd class="key">Esc</kbd></span></div>`)}
    ${visorFront()}`;
}

/* ── screen: upgrade ───────────────────────────────────────────────────── */
function upgradeScreen() {
  /* Filament: named paths (ramp / peak / optics), StdCosts 40·54·73·98·132,
     L4 is the shipped breakpoint, L6–10 authored in data but gated. */
  const paths = [
    { id: 'ramp', name: 'Ramp', per: '×1.12', lvl: 3, desc: 'how fast it climbs' },
    { id: 'peak', name: 'Peak', per: '×1.15', lvl: 2, desc: 'how high it climbs' },
    { id: 'optics', name: 'Optics', per: '×1.10', lvl: 1, desc: 'range' },
  ];
  const costs = [40, 54, 73, 98, 132];
  const grid = paths.map((p) => `<div class="col" style="gap:7px">
    <div class="row between">
      <span class="row" style="gap:8px"><i style="width:8px;height:8px;transform:rotate(45deg);background:${T.accent}"></i>
        <span style="font:var(--weight-bold) var(--size-caption)/1 var(--font-display);color:${T.ink};text-transform:uppercase;letter-spacing:.05em">${p.name}</span>
        <span class="mono" style="font-size:9px;color:var(--text-muted)">${p.desc} · ${p.per}/lvl</span></span>
      <span class="mono" style="font-size:var(--size-caption);color:${T.accent}">L${p.lvl}</span>
    </div>
    <div class="row" style="gap:3px">${Array.from({ length: 10 }, (_, i) => {
    const on = i < p.lvl, bp = i + 1 === 4, gated = i + 1 > 5;
    return `<i style="flex:1;height:16px;background:${on ? (bp ? T.accent : T.accent2) : 'transparent'};box-shadow:inset 0 0 0 1px ${on ? 'transparent' : gated ? T.hairline : 'rgba(101,220,228,.16)'};clip-path:${bp ? 'polygon(50% 0,100% 50%,50% 100%,0 50%)' : 'none'}"></i>`;
  }).join('')}</div>
    <div class="row between">
      <span class="mono" style="font-size:9px;color:var(--text-muted)">next L${p.lvl + 1} · ${costs[p.lvl]} credits</span>
      ${p.lvl + 1 === 4 ? `<span class="row" style="gap:5px">${lab('breakpoint', 'font-size:9px;color:var(--brass-400)')}${ic('scrap_flux', '', 12)}<span class="mono" style="font-size:9px;color:${T.ink}">4</span></span>` : `<span class="mono" style="font-size:9px;color:var(--text-disabled)">L6–10 gated</span>`}
    </div>
  </div>`).join('');
  return `${spireWorld()}${visorBack()}${lipStrip()}
    ${A('left:34%;top:55%;transform:translate(-50%,-50%);width:520px;height:520px;border-radius:50%;box-shadow:inset 0 0 0 1px rgba(201,59,40,.28);background:radial-gradient(circle,rgba(201,59,40,.08),transparent 70%)')}
    ${A('left:112px;top:152px', cant(16, 'left center', T.card(econBlock({ inline: true }), 'padding:9px 14px')))}
    ${A('right:112px;top:186px;width:560px', cant(-14, 'right center', T.card(`
      <div class="row between" style="margin-bottom:12px">
        <span class="row" style="gap:10px">${ic('tower_filament')}
          <span class="col" style="gap:2px"><span style="font:var(--weight-bold) var(--size-body-lg)/1 var(--font-display);color:${T.ink}">Filament</span>
            <span class="mono" style="font-size:9px;color:var(--text-muted)">socket w15 · wall · 23.5 m · beam</span></span></span>
        <span class="col" style="gap:2px;align-items:flex-end">${lab('Structure')}
          <span class="mono" style="font-size:var(--size-caption);color:var(--state-warning)">120 / 130</span></span>
      </div>
      ${rule(T)}
      <div class="row" style="gap:16px;margin:12px 0">
        ${[['Damage', '7 → 21'], ['Ramp', '0.6 /s'], ['Cap', '×3.0'], ['Range', '12 m']].map(([k, v]) =>
    `<span class="col" style="gap:2px">${lab(k)}<span class="mono" style="font-size:var(--size-stat-sm);color:${T.ink}">${v}</span></span>`).join('')}
      </div>
      ${rule(T)}
      <div class="col" style="gap:16px;margin-top:14px">${grid}</div>
      ${rule(T, 'margin:14px 0')}
      <p style="margin:0;font:var(--weight-regular) var(--size-caption)/1.45 var(--font-ui);color:var(--text-secondary);text-wrap:pretty">Damage climbs while it holds one target and resets the moment it switches — the answer to a Monolith in a stairwell, and actively bad against the six drifters behind it.</p>`,
    'padding:16px 18px')))}
    ${A('right:112px;bottom:112px', `<div class="row" style="gap:10px"><span class="btn">Upgrade Ramp · 98</span><span class="btn sec">Sell · +70%</span></div>`)}
    ${A('left:112px;bottom:112px', cant(18, 'left bottom', T.card(vitalsBlock({ w: 380 }), 'padding:13px 15px')))}
    ${visorFront()}`;
}

/* ── screen: lobby ─────────────────────────────────────────────────────── */
function lobbyScreen() {
  const cards = FACTIONS.map((f, i) => {
    const taken = i === 0 ? 'Kestrel · you' : i === 2 ? 'Vane' : i === 4 ? 'Aldis' : '';
    const sel = i === 0;
    return `<div class="col" style="gap:0;flex:1;min-width:0;background:${sel ? 'rgba(101,220,228,.06)' : 'transparent'};box-shadow:inset 0 0 0 1px ${sel ? T.accent : T.hairline};clip-path:var(--clip-chamfer)">
      <div class="row" style="gap:8px;align-items:center;padding:10px 12px;border-bottom:1px solid ${T.hairline}">
        <span class="faction ${f.id}"></span>
        <span style="font:var(--weight-bold) var(--size-caption)/1 var(--font-display);color:${T.ink};text-transform:uppercase;letter-spacing:.05em;flex:1">${f.name}</span>
        ${taken ? `<span class="mono" style="font-size:9px;color:${sel ? T.accent : 'var(--text-muted)'}">${taken}</span>` : `<span class="mono" style="font-size:9px;color:var(--text-disabled)">open</span>`}
      </div>
      <div class="col" style="gap:9px;padding:12px">
        <div style="height:112px;display:grid;place-items:center;box-shadow:inset 0 0 0 1px ${T.hairline};background:${RASTER}">${ic('faction_' + f.id, '', 56)}</div>
        <span class="col" style="gap:2px">${lab('Signature')}<span style="font:var(--weight-semibold) var(--size-caption)/1.2 var(--font-display);color:${T.ink}">${f.ability}</span></span>
        <div class="col" style="gap:4px">
          ${[['Cooldown', f.cd + ' s'], ['Radius', f.r ? f.r + ' m' : 'map'], ['Effect', f.mag]].map(([k, v]) =>
      `<span class="row between">${lab(k, 'font-size:9px')}<span class="mono" style="font-size:9px;color:${T.ink}">${v}</span></span>`).join('')}
        </div>
        ${rule(T)}
        <span class="col" style="gap:2px">${lab('Passive', 'font-size:9px')}<span class="mono" style="font-size:var(--size-micro);color:${T.accent}">${f.passive}</span></span>
        <div class="row" style="gap:2px;margin-top:2px">${Array.from({ length: 5 }, (_, k) => `<i style="flex:1;height:4px;background:${k < (sel ? 3 : 1) ? T.accent : 'transparent'};box-shadow:${k < (sel ? 3 : 1) ? 'none' : 'inset 0 0 0 1px rgba(101,220,228,.2)'}"></i>`).join('')}</div>
        <span class="mono" style="font-size:9px;color:var(--text-muted)">L${sel ? 3 : 1} · ${sel ? 240 : 60}/500 xp</span>
      </div></div>`;
  }).join('');
  return `${A('inset:0;background:var(--obsidian-900)')}${A('inset:0;background-image:var(--tex-grid);opacity:.5')}
    ${A('inset:0;background:radial-gradient(ellipse at 50% 0%,rgba(47,180,190,.12),transparent 60%)')}
    ${A(`left:0;right:0;top:0;height:64px;display:flex;align-items:center;gap:20px;padding:0 32px;background:var(--surface-panel);border-bottom:2px solid ${T.accent2}`,
    `<h1 style="margin:0;font:var(--weight-black) var(--size-title)/1 var(--font-display);letter-spacing:var(--tracking-display);color:${T.ink}">DEEP FIELD</h1>
     <span class="mono" style="font-size:var(--size-caption);color:var(--text-muted)">AUGUR · LOBBY</span>
     <span style="flex:1"></span>${econBlock({ inline: true })}
     <span class="mono" style="font-size:var(--size-caption);color:var(--venom-400)">3 / 4 READY</span>`)}
    ${A('left:32px;right:32px;top:96px', `<div class="row between" style="align-items:flex-end;margin-bottom:16px">
      <span class="col" style="gap:4px">${lab('Choose your faction', `color:${T.ink2}`)}
        <span style="font:var(--weight-black) var(--size-display-sm)/1 var(--font-display);color:${T.ink}">ONE EACH. NO DUPLICATES.</span></span>
      <span class="mono" style="font-size:var(--size-caption);color:var(--text-muted)">Factions.cs · level 1–5 · ×0.94 cooldown, ×1.06 radius, ×1.05 magnitude per level</span>
    </div>`)}
    ${A('left:32px;right:32px;top:196px', `<div class="row" style="gap:14px;align-items:stretch">${cards}</div>`)}
    ${A('left:32px;right:32px;bottom:32px', T.card(`<div class="row between" style="align-items:center;padding:14px 20px">
      <span class="row" style="gap:18px;align-items:center">
        <span class="col" style="gap:3px">${lab('Deploying to')}<span style="font:var(--weight-bold) var(--size-body-lg)/1 var(--font-display);color:${T.ink}">Spire · Sector 03</span></span>
        ${vrule(T, 34)}
        <span class="col" style="gap:3px">${lab('Waves')}<span class="mono" style="font-size:var(--size-stat-sm);color:${T.ink}">12</span></span>
        <span class="col" style="gap:3px">${lab('Weather')}<span class="mono" style="font-size:var(--size-caption);color:var(--soul-400)">NIGHT W8 · FOG W12</span></span>
        <span class="col" style="gap:3px">${lab('Routes')}<span class="mono" style="font-size:var(--size-caption);color:${T.ink}">stair · escape · air</span></span>
      </span>
      <span class="row" style="gap:10px"><span class="btn sec">Armory</span><span class="btn lg">Deploy <kbd class="key">F</kbd></span></span>
    </div>`))}`;
}

/* ── screen: sector select ─────────────────────────────────────────────── */
function sectorScreen() {
  const sectors = [
    { id: 'foundry', name: 'Foundry', n: 1, waves: 10, cond: 'FOG · W9', best: '10 / 10', tiers: '2 · 0 m · 6 m', state: 'cleared', routes: 'ground · air' },
    { id: 'switchyard', name: 'Switchyard', n: 2, waves: 10, cond: 'NIGHT · W9', best: '10 / 10', tiers: '3 · 0 m · 5 m · 10 m', state: 'cleared', routes: 'ground · short · air' },
    { id: 'spire', name: 'Spire', n: 3, waves: 12, cond: 'NIGHT · W8 · FOG · W12', best: '7 / 12', tiers: '5 · 0 m → 40 m', state: 'active', routes: 'stair · escape · air' },
  ];
  return `${A('inset:0;background:var(--obsidian-900)')}${A('inset:0;background-image:var(--tex-grid);opacity:.5')}
    ${A(`left:0;right:0;top:0;height:64px;display:flex;align-items:center;gap:20px;padding:0 32px;background:var(--surface-panel);border-bottom:2px solid ${T.accent2}`,
    `<h1 style="margin:0;font:var(--weight-black) var(--size-title)/1 var(--font-display);letter-spacing:var(--tracking-display);color:${T.ink}">DEEP FIELD</h1>
     <span class="mono" style="font-size:var(--size-caption);color:var(--text-muted)">AUGUR · SECTOR SELECT</span>
     <span style="flex:1"></span>${econBlock({ inline: true })}`)}
    ${A('left:32px;top:96px', `<span class="col" style="gap:4px">${lab('Campaign', `color:${T.ink2}`)}
      <span style="font:var(--weight-black) var(--size-display-sm)/1 var(--font-display);color:${T.ink}">THREE SECTORS. EACH ONE TEACHES.</span></span>`)}
    ${A('left:32px;right:32px;top:186px', `<div class="row" style="gap:18px;align-items:stretch">${sectors.map((s) => {
    const on = s.state === 'active';
    return `<div class="col" style="gap:0;flex:1;background:${on ? 'rgba(101,220,228,.05)' : 'transparent'};box-shadow:inset 0 0 0 1px ${on ? T.accent : T.hairline};clip-path:var(--clip-chamfer)">
      <div class="row" style="gap:10px;align-items:center;padding:12px 14px;border-bottom:1px solid ${T.hairline}">
        <span class="mono" style="font:var(--weight-black) 22px/1 var(--font-display);color:${on ? T.accent : 'var(--text-muted)'}">0${s.n}</span>
        <span style="font:var(--weight-bold) var(--size-body-lg)/1 var(--font-display);color:${T.ink};flex:1">${s.name}</span>
        <span class="mono" style="font-size:9px;color:${on ? 'var(--wave-active)' : 'var(--venom-400)'}">${s.state.toUpperCase()}</span>
      </div>
      <div style="height:250px;position:relative;box-shadow:inset 0 0 0 1px ${T.hairline};overflow:hidden;background:${RASTER}">
        ${A('inset:0;background-image:var(--tex-grid);opacity:.3')}
        ${s.id === 'spire'
        ? `${A(`left:50%;bottom:18px;transform:translateX(-50%);width:96px;height:206px;box-shadow:inset 0 0 0 1px ${T.accent}`, Array.from({ length: 5 }, (_, i) => A(`left:0;right:0;bottom:${8 + i * 40}px;height:1px;background:${T.hairline}`)).join(''))}
           ${A(`left:50%;bottom:214px;width:11px;height:11px;background:${T.accent};transform:translateX(-50%) rotate(45deg);box-shadow:0 0 12px ${T.accent}`)}
           ${A('left:22%;bottom:18px;width:1px;height:190px;background:linear-gradient(0deg,var(--threat-500),transparent)')}
           ${A('right:22%;bottom:18px;width:1px;height:190px;background:linear-gradient(0deg,var(--el-frost),transparent)')}`
        : `${A(`left:14%;right:14%;top:46%;height:1px;background:${T.hairline}`)}
           ${A('left:14%;top:calc(46% - 3px);width:7px;height:7px;transform:rotate(45deg);background:var(--threat-500)')}
           ${A(`right:14%;top:calc(46% - 4px);width:9px;height:9px;transform:rotate(45deg);background:${T.accent};box-shadow:0 0 10px ${T.accent}`)}
           ${A(`left:30%;top:30%;width:1px;height:34%;background:${T.hairline}`)}${A(`left:56%;top:46%;width:1px;height:22%;background:${T.hairline}`)}`}
      </div>
      <div class="col" style="gap:7px;padding:12px 14px">
        ${[['Waves', s.waves], ['Tiers', s.tiers], ['Routes', s.routes], ['Best clear', s.best]].map(([k, v]) =>
        `<span class="row between">${lab(k)}<span class="mono" style="font-size:var(--size-caption);color:${T.ink}">${v}</span></span>`).join('')}
        ${rule(T)}
        <span class="row between">${lab('Weather')}<span class="mono" style="font-size:9px;color:var(--soul-400)">${s.cond}</span></span>
      </div>
      <div style="padding:0 14px 14px">${on ? '<span class="btn" style="width:100%">Deploy</span>' : '<span class="btn sec" style="width:100%">Replay</span>'}</div>
    </div>`;
  }).join('')}</div>`)}
    ${A('left:32px;right:32px;bottom:32px', `<div class="row between" style="align-items:center">
      <span class="mono" style="font-size:var(--size-caption);color:var(--text-muted)">Campaign.Sectors · foundry → switchyard → spire · a sector unlocks when the one before it is cleared</span>
      <span class="row" style="gap:10px"><span class="btn sec">Back</span><span class="btn">Confirm <kbd class="key">F</kbd></span></span>
    </div>`)}`;
}

/* ── registry ──────────────────────────────────────────────────────────── */
export const SCREENS_V2 = [];
const S = (o) => SCREENS_V2.push(o);
const HELMET = 'Inside the helmet: the shell (brow, cheek plates, chin guard, nose bridge) is drawn OVER the HUD, so panels tuck under the brow instead of floating on a border. Suit telemetry — O₂, seal, temperature, power — is etched into the brow itself, where a visor would carry it. Glass artefacts sit between the projection and the eye: dust, hairline scratches, a cyan/magenta fringe at the aperture, calibration ticks at the corners, the HUD\'s own glow bouncing back off the inside of the visor, and breath condensation at the cold corners. Every panel is canted on the visor\'s curve.';
S({ id: 'v2-augur-hud', group: 'Augur — in-helmet', label: 'HUD — full', tag: 'spire w6',
  note: `${HELMET} Core integrity is the loudest element, sat in the chevron's notch — on a map whose fight ends forty metres up, a leak you did not see is what you most need to know. The vertical gauge reads the building itself: five floors from Lobby 0 m to Roof 40 m, each lane's leader at its floor, the core at the top. Inbound is the real wave — Waves["spire"][5]: monolith ×1 corking the stair, drifter ×6 up the escape at +40 s, skiff ×3 spiralling the air lane at +90 s. The lip strip carries mission identity along the top of the glass.`,
  render: () => hudScreen() });
S({ id: 'v2-augur-build', group: 'Augur — in-helmet', label: 'Build wheel', tag: '9 + socket',
  note: `All nine placeables at their real Towers.cs cost — Lance 75, Nova 115, Singularity 110, Skywatch 90, Arc 90, Detector 70, Filament 125, Barricade 60, Overclock 130 (M4, still absent from Towers.cs). The shipped screens carry stale numbers. Socket g5 is the live one: ground, (−8, 0, −4.9), six metres off the stair. The wheel is projected at the focal centre of the visor, with the detail panel canted off the starboard cheek.`,
  render: () => buildScreen() });
S({ id: 'v2-augur-upgrade', group: 'Augur — in-helmet', label: 'Upgrade panel', tag: 'named paths',
  note: `Filament, because its paths are named rather than the generic three: ramp (how fast it climbs) ×1.12, peak (how high) ×1.15, optics (range) ×1.10. Ten pips per path with L4 as the shipped breakpoint (diamond, Flux ×4 recipe) and L6–10 drawn as authored-but-gated outlines. Level costs are StdCosts: 40 · 54 · 73 · 98 · 132.`,
  render: () => upgradeScreen() });
S({ id: 'v2-augur-lobby', group: 'Augur — menus', label: 'Lobby + factions', tag: '5 factions',
  note: `Helmet off — menus are read on a terminal, not through a visor, so the shell is absent here by design. Five faction columns with the real Factions.cs rows: cooldown, radius, effect and passive verbatim, plus the level curve (×0.94 cooldown, ×1.06 radius, ×1.05 magnitude per level, cap 5). One each, no duplicates. The deploy bar carries the map's weather schedule so the run is chosen with its weather known.`,
  render: () => lobbyScreen() });
S({ id: 'v2-augur-sector', group: 'Augur — menus', label: 'Sector select', tag: '3 maps',
  note: `Campaign.Sectors in order: Foundry (10 waves, two tiers, Fog W9), Switchyard (10 waves, three tiers, Night W9), Spire (12 waves, five tiers 0→40 m, Night W8 and Fog W12). Spire's sketch is drawn as an elevation rather than a plan, because it is the only map whose shape is vertical.`,
  render: () => sectorScreen() });
