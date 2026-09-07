/**
 * Shared viewer for the per-tower model pages.
 *
 *   import { TOWER } from './models/lance.js';
 *   import { mount } from './tower-viewer.js';
 *   mount(TOWER);
 *
 * Owns: the stage, the aim-rig driver + target sim, the upgrade-level UI, and
 * the geometry receipts (tris / parts at the current level vs. maxed). Model
 * files own geometry only.
 */
import { makeKit, applyLevels, triCount, countMeshes } from './models/tower-kit.js';
import { PROJECTILES, ORDNANCE } from './models/projectiles.js';

const ROSTER = [
  { id: 'lance', label: 'Lance', href: 'Lance.html', swatch: '#2b5cff', ms: 'M1' },
  { id: 'nova', label: 'Nova', href: 'Nova.html', swatch: '#f0c83a', ms: 'M1' },
  { id: 'singularity', label: 'Singularity', href: 'Singularity.html', swatch: '#9b5be8', ms: 'M1' },
  { id: 'skywatch', label: 'Skywatch', href: 'Skywatch.html', swatch: '#22d3ee', ms: 'M1' },
  { id: 'arc', label: 'Arc', href: 'Arc.html', swatch: '#f05ae6', ms: 'M2' },
  { id: 'barricade', label: 'Barricade', href: 'Barricade.html', swatch: 'var(--brass-400)', ms: 'M2' },
  { id: 'filament', label: 'Filament', href: 'Filament.html', swatch: '#ff2e4a', ms: 'M3' },
  { id: 'detector', label: 'Detector', href: 'Detector.html', swatch: '#7fe65a', ms: 'M3' },
  { id: 'overclock', label: 'Overclock', href: 'Overclock.html', swatch: '#ff6f1a', ms: 'M4' },
];

const MODES = [
  { id: 'strafe', label: 'Strafe', y: 0.9, air: false },
  { id: 'approach', label: 'Approach', y: 0.9, air: false },
  { id: 'orbit', label: 'Air lane', y: 4.2, air: true },
];

const CSS = `
  *{box-sizing:border-box}
  html,body{margin:0;height:100%}
  body{font:var(--text-body-role);background:var(--surface-base);color:var(--text-primary);
    display:flex;flex-direction:column;overflow:hidden}
  a{color:var(--text-arcane);text-decoration:none}
  a:hover{color:var(--arcane-300)}
  .label{font:var(--text-label);letter-spacing:var(--tracking-label);text-transform:uppercase;color:var(--text-muted)}
  header{flex:0 0 auto;display:flex;align-items:center;gap:var(--space-7);padding:var(--space-4) var(--space-6);
    background:var(--surface-panel);border-bottom:var(--stroke-emphasis) solid var(--border-panel);
    box-shadow:var(--shadow-panel);position:relative;z-index:var(--z-hud)}
  header::after{content:"";position:absolute;inset:auto 0 -3px 0;height:2px;background:var(--tex-hazard);opacity:.7}
  .brand h1{font:var(--text-panel-title);margin:0;letter-spacing:var(--tracking-display)}
  .brand p{margin:2px 0 0;font-size:var(--size-caption);color:var(--text-muted)}
  .tabs{display:flex;gap:var(--space-2);flex-wrap:wrap;align-items:center}
  .tabs .ms{font:var(--text-label);letter-spacing:var(--tracking-label);color:var(--text-muted);padding:0 var(--space-2) 0 var(--space-3)}
  .tabs a{font:var(--weight-semibold) var(--size-caption)/1 var(--font-display);letter-spacing:.03em;
    display:flex;align-items:center;gap:var(--space-2);padding:var(--space-2) var(--space-4);
    background:var(--surface-raised);color:var(--text-secondary);border:var(--stroke-panel) solid var(--border-panel);
    border-radius:var(--radius-2);clip-path:var(--clip-chamfer-sm);
    transition:background var(--dur-fast) var(--ease-snap),color var(--dur-fast) var(--ease-snap)}
  .tabs a:hover{background:var(--obsidian-500);color:var(--text-primary)}
  .tabs a[aria-current="page"]{background:var(--obsidian-500);color:var(--text-accent);
    border-color:var(--border-accent);box-shadow:var(--glow-brass)}
  .tabs button.toggle{font:var(--weight-semibold) var(--size-caption)/1 var(--font-display);letter-spacing:.03em;display:flex;align-items:center;gap:var(--space-2);
    padding:var(--space-2) var(--space-4);cursor:pointer;background:var(--surface-inset);color:var(--text-secondary);border:var(--stroke-panel) solid var(--border-panel);
    border-radius:var(--radius-2);clip-path:var(--clip-chamfer-sm)}
  .tabs button.toggle:hover{color:var(--text-primary);background:var(--obsidian-600)}
  .tabs button.toggle[aria-pressed="true"]{background:var(--arcane-700);color:var(--arcane-300);border-color:var(--arcane-600);box-shadow:var(--glow-arcane)}
  .tabs button.toggle .dot{width:8px;height:8px;border-radius:50%;background:currentColor}
  #ord{right:var(--space-6);bottom:var(--space-6);top:auto;max-height:46%;display:none}
  body[data-ord="1"] #ord{display:block}
  body[data-ord="1"] #ctl{max-height:calc(54% - var(--space-8))}
  .seg.wrap{flex-wrap:wrap}
  .seg.wrap button{flex:1 1 30%;border-left-width:var(--stroke-panel);border-top-width:0}
  .seg.wrap button:nth-child(-n+3){border-top-width:var(--stroke-panel)}
  .swatch{width:8px;height:8px;flex:0 0 auto;transform:rotate(45deg)}
  main{flex:1 1 auto;position:relative;min-height:0}
  three-d-stage{width:100%;height:100%;display:block}
  three-d-stage:not(:defined){visibility:hidden}
  .panel{position:absolute;z-index:var(--z-dock);width:296px;background:var(--surface-glass);
    backdrop-filter:blur(var(--blur-glass));border:var(--stroke-panel) solid var(--border-panel);
    border-top-color:var(--border-bevel-top);box-shadow:var(--shadow-panel);clip-path:var(--clip-chamfer);
    max-height:calc(100% - var(--space-8));overflow:auto}
  .panel>h2{font:var(--text-label);letter-spacing:var(--tracking-label);text-transform:uppercase;
    color:var(--text-accent);margin:0;padding:var(--space-3) var(--space-4);background:var(--surface-inset);
    border-bottom:var(--stroke-panel) solid var(--border-panel);position:sticky;top:0}
  .panel .body{padding:var(--space-4)}
  #rig{left:var(--space-6);top:var(--space-6)}
  #ctl{right:var(--space-6);top:var(--space-6)}
  .stats{display:grid;grid-template-columns:1fr 1fr;gap:var(--space-3) var(--space-4);margin:0}
  .stats dt{font:var(--text-label);letter-spacing:var(--tracking-micro);text-transform:uppercase;color:var(--text-muted);margin:0}
  .stats dd{margin:3px 0 0;font:var(--weight-bold) var(--size-stat-sm)/1 var(--font-mono);
    color:var(--text-primary);font-variant-numeric:tabular-nums}
  .stats dd.accent{color:var(--text-accent)}
  .stats dd.arcane{color:var(--text-arcane)}
  hr{border:0;border-top:var(--stroke-panel) solid var(--border-panel);margin:var(--space-4) 0}
  .gauge{margin-bottom:var(--space-4)}
  .gauge .row,.field .row{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:var(--space-2)}
  .gauge .row b{font:var(--weight-bold) var(--size-caption)/1 var(--font-mono);color:var(--text-secondary)}
  .track{position:relative;height:8px;background:var(--bar-track);box-shadow:var(--shadow-slot);
    border:var(--stroke-hairline) solid var(--border-panel)}
  .track .needle{position:absolute;top:-2px;bottom:-2px;width:2px;background:var(--brass-400);box-shadow:var(--glow-brass)}
  .track .ghost{position:absolute;top:0;bottom:0;width:2px;background:var(--steel-400)}
  .status{font:var(--weight-bold) var(--size-caption)/1.3 var(--font-display);letter-spacing:.06em;
    text-transform:uppercase;padding:var(--space-3);text-align:center;background:var(--surface-inset);
    border:var(--stroke-hairline) solid var(--border-panel);color:var(--text-muted)}
  .status[data-tone="lock"]{color:var(--state-success);border-color:rgba(123,192,67,.5)}
  .status[data-tone="slew"]{color:var(--state-warning);border-color:rgba(232,134,43,.5)}
  .status[data-tone="fail"]{color:var(--state-danger);border-color:rgba(201,59,40,.55);box-shadow:var(--glow-threat)}
  .seg{display:flex;margin-bottom:var(--space-4)}
  .seg button{flex:1 1 0;font:var(--weight-semibold) var(--size-caption)/1 var(--font-display);
    padding:var(--space-3) var(--space-2);cursor:pointer;color:var(--text-secondary);background:var(--surface-inset);
    border:var(--stroke-panel) solid var(--border-panel);border-left-width:0}
  .seg button:first-child{border-left-width:var(--stroke-panel)}
  .seg button:hover{color:var(--text-primary);background:var(--obsidian-600)}
  .seg button[aria-pressed="true"]{background:var(--arcane-700);color:var(--arcane-300);border-color:var(--arcane-600)}
  .field{margin-bottom:var(--space-4)}
  .field output{font:var(--weight-bold) var(--size-caption)/1 var(--font-mono);color:var(--text-accent)}
  input[type="range"]{width:100%;accent-color:var(--brass-500);margin:0;display:block}
  .checks{display:flex;flex-direction:column;gap:var(--space-3)}
  .checks label{display:flex;align-items:center;gap:var(--space-3);font-size:var(--size-caption);
    color:var(--text-secondary);cursor:pointer}
  input[type="checkbox"]{accent-color:var(--arcane-500);width:14px;height:14px;margin:0}
  .path{margin-bottom:var(--space-5)}
  .path .head{display:flex;justify-content:space-between;align-items:baseline;margin-bottom:var(--space-2)}
  .path .head span{font:var(--weight-semibold) var(--size-caption)/1 var(--font-display);color:var(--text-secondary)}
  .path .head b{font:var(--weight-bold) var(--size-caption)/1 var(--font-mono);color:var(--text-accent)}
  .path .cue{font-size:var(--size-micro);color:var(--text-muted);margin:var(--space-2) 0 0}
  .stones{list-style:none;margin:var(--space-2) 0 0;padding:0;display:flex;flex-direction:column;gap:2px}
  .stones li{display:flex;gap:var(--space-3);font-size:var(--size-micro);line-height:1.45;color:var(--text-disabled)}
  .stones li b{font:var(--weight-bold) var(--size-micro)/1.45 var(--font-mono);flex:0 0 22px}
  .stones li[data-on="1"]{color:var(--text-secondary)}
  .stones li[data-on="1"] b{color:var(--state-success)}
  .stones li[data-bp="1"] span{color:var(--text-muted)}
  .stones li[data-bp="1"][data-on="1"] span{color:var(--text-primary)}
  .stones li[data-bp="1"][data-on="1"] b{color:var(--text-accent)}
  .stones li[data-bp="1"] b::after{content:"◆";font-size:7px;margin-left:2px;vertical-align:middle}
  .hier{margin:0;font:var(--weight-regular) var(--size-micro)/1.65 var(--font-mono);color:var(--text-muted);
    white-space:pre;overflow-x:auto}
  .hier b{color:var(--text-arcane);font-weight:var(--weight-bold)}
  .hier i{color:var(--text-accent);font-style:normal}
  .note{font-size:var(--size-caption);line-height:var(--leading-normal);color:var(--text-muted);margin:var(--space-3) 0 0}
`;

export function mount(TOWER) {
  document.title = `Deep Field 3D — ${TOWER.label}`;
  const style = document.createElement('style');
  style.textContent = CSS;
  document.head.appendChild(style);

  const pathIds = TOWER.paths.map((p) => p.id);
  const levels = Object.fromEntries(pathIds.map((id) => [id, 1]));

  document.body.innerHTML = `
    <header>
      <div class="brand">
        <h1>${TOWER.label}</h1>
        <p>Deep Field 3D · ${TOWER.paths.length} upgrade paths × 10 levels, every rung visible · ${TOWER.rig ? 'articulated aim rig' : (TOWER.headline ? TOWER.headline + ', no aim rig' : 'aura tower, no aim rig')}</p>
      </div>
      <nav class="tabs">${ROSTER.map((t, i) => (i === 0 || ROSTER[i - 1].ms !== t.ms ? `<span class="ms">${t.ms}</span>` : '') +
        `<a href="${t.href}"${t.id === TOWER.id ? ' aria-current="page"' : ''}><span class="swatch" style="background:${t.swatch}"></span>${t.label}</a>`).join('')}<span class="ms">View</span><button type="button" class="toggle" id="ord-toggle" aria-pressed="false" title="Projectiles, muzzle and impact meshes — live fire on the target sim"><span class="dot"></span>Projectiles</button><span class="ms">Pack</span><a href="Deep Field 3D - Roster.html"><span class="swatch" style="background:#e9614c"></span>Enemies</a><a href="Deep Field 3D - Gunsmith.html"><span class="swatch" style="background:#c3ccd8"></span>Gunsmith</a><a href="Deep Field 3D - Maps.html"><span class="swatch" style="background:#ff8a1f"></span>Maps</a><a href="Deep Field 3D - UI Screens.html"><span class="swatch" style="background:#c89b3c"></span>UI</a><a href="Deep Field 3D - Asset Export.html"><span class="swatch" style="background:var(--arcane-400)"></span>Export</a></nav>
    </header>
    <main>
      <three-d-stage name="${TOWER.id}" background="#0D1119"></three-d-stage>
      <section class="panel" id="rig">
        <h2>${TOWER.label} — ${TOWER.rig ? 'aim rig' : (TOWER.headline || 'field emitter')}</h2>
        <div class="body">
          <dl class="stats" id="rig-stats"></dl>
          <hr>
          ${TOWER.rig ? `
          <div class="gauge">
            <div class="row"><span class="label">Yaw</span><b id="yaw-read">0.0°</b></div>
            <div class="track"><span class="ghost" id="yaw-ghost"></span><span class="needle" id="yaw-needle"></span></div>
          </div>
          <div class="gauge">
            <div class="row"><span class="label">Pitch</span><b id="pitch-read">0.0°</b></div>
            <div class="track"><span class="ghost" id="pitch-ghost"></span><span class="needle" id="pitch-needle"></span></div>
          </div>` : ''}
          <div class="status" id="status" data-tone="idle">${TOWER.rig ? 'Acquiring…' : (TOWER.headline ? 'Static structure — nothing to aim' : 'Aura tower — nothing to aim')}</div>
          <hr>
          <p class="label" style="margin:0 0 var(--space-3)">Export hierarchy</p>
          <pre class="hier" id="hier"></pre>
          <p class="note">${TOWER.note}</p>
        </div>
      </section>
      <section class="panel" id="ctl">
        <h2>Upgrade paths</h2>
        <div class="body">
          <div id="paths"></div>
          ${TOWER.variants ? `<hr><p class="label" style="margin:0 0 var(--space-3)">State mesh</p><div class="seg" id="variants">${Object.keys(TOWER.variants).map((k, i) => `<button type="button" data-id="${k}" aria-pressed="${i === 0}">${k}</button>`).join('')}</div>` : ''}
          <hr>
          <p class="label" style="margin:0 0 var(--space-3)">Target sim</p>
          <div class="seg" id="modes"></div>
          <div class="field">
            <div class="row"><span class="label">Speed</span><output id="speed-out">4.0 m/s</output></div>
            <input type="range" id="speed" min="1" max="9" step="0.5" value="4">
          </div>
          <div class="field">
            <div class="row"><span class="label">Standoff</span><output id="dist-out">9.0 m</output></div>
            <input type="range" id="dist" min="3" max="18" step="0.5" value="9">
          </div>
          <div class="checks">
            <label><input type="checkbox" id="track"${TOWER.rig || ORDNANCE[TOWER.id] ? ' checked' : ' disabled'}> Track target</label>
            <label><input type="checkbox" id="bore" checked> Bore sight + muzzle marker</label>
            <label><input type="checkbox" id="pivots"> Pivot axes</label>
            <label><input type="checkbox" id="limits" checked> Enforce traverse limits</label>
          </div>
        </div>
      </section>
      <section class="panel" id="ord">
        <h2>Ordnance — ${ORDNANCE[TOWER.id] ? { bolt: 'projectile', lob: 'mortar shell', chain: 'chain beam', ray: 'ramp beam' }[ORDNANCE[TOWER.id].kind] : 'none'}</h2>
        <div class="body" id="ord-body"></div>
      </section>
    </main>`;

  const el = (id) => document.getElementById(id);
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const wrap180 = (d) => ((d + 180) % 360 + 360) % 360 - 180;

  // Upgrade path controls.
  el('paths').innerHTML = TOWER.paths.map((p) => `
    <div class="path" data-path="${p.id}">
      <div class="head"><span>${p.label}</span><b><span data-lvl="${p.id}">1</span> / 10</b></div>
      <input type="range" min="1" max="10" step="1" value="1" data-slider="${p.id}">
      <p class="cue">${p.cue}</p>
      <ul class="stones">${Object.entries(p.steps).map(([lv, text]) =>
        `<li data-stone="${p.id}:${lv}" data-on="0" data-bp="${[4, 7, 10].includes(+lv) ? 1 : 0}"><b>L${lv}</b><span>${text}</span></li>`).join('')}</ul>
    </div>`).join('');

  const stage = document.querySelector('three-d-stage');
  let kit, model, mode = MODES[0], t = 0, demo = null, targetMesh = null, boreLine = null, gizmos = [];
  const aim = { yaw: 0, pitch: TOWER.rig ? Math.max(0, TOWER.rig.pitch[0]) : 0 };

  /* ── Ordnance: projectiles, muzzle flash and impact over the aim rig ───────────── */
  const ORD = ORDNANCE[TOWER.id] || null;
  const ord = { open: localStorage.getItem('df_ord_open') === '1', view: 'live', cache: {}, shots: [], fx: [], cool: 0, ray: null, heat: 0, hitFlash: 0, preview: null, tier: 1 };
  const ordItems = () => { if (!ORD) return []; const it = ORD.proj.map((id, i) => ({ id, view: ORD.proj.length > 1 ? 't' + (i + 1) : 'beam', label: ORD.proj.length > 1 ? 'T' + (i + 1) : 'Beam' })); if (ORD.muzzle) it.push({ id: ORD.muzzle, view: 'muzzle', label: 'Muzzle' }); if (ORD.impact) it.push({ id: ORD.impact, view: 'impact', label: 'Impact' }); return it; };
  const proto = (id) => ord.cache[id] ??= PROJECTILES.find((p) => p.id === id).build(kit);
  const spawn = (id) => { const c = proto(id).clone(); c.traverse((o) => { if (o.isMesh) o.material = o.material.clone(); }); return c; };
  const currentTier = () => { if (!ORD || ORD.proj.length < 2) return 1; if (el('ord-auto')?.checked) { const lv = levels[ORD.tierPath] ?? 1; return lv >= 10 ? 3 : lv >= 7 ? 2 : 1; } return ord.tier; };
  const fadeAll = (obj, a) => obj.traverse((o) => { if (o.isMesh) { o.material.transparent = true; o.material.opacity = a; } });

  function renderOrd() {
    document.body.dataset.ord = ord.open ? '1' : '0';
    el('ord-toggle').setAttribute('aria-pressed', String(ord.open));
    const body = el('ord-body');
    if (!ORD) { body.innerHTML = `<p class="note">${TOWER.headline ? 'A structure — nothing leaves it.' : 'Aura tower — its effect is a field, not a shot. The pulse / link meshes live in the Roster\'s VFX set.'}</p>`; return; }
    if (body.dataset.built) return;
    body.dataset.built = '1';
    const items = ordItems();
    body.innerHTML = `
      <p class="label" style="margin:0 0 var(--space-3)">Show</p>
      <div class="seg wrap" id="ord-view"><button type="button" data-view="live" aria-pressed="true">Live fire</button>${items.map((it) => `<button type="button" data-view="${it.view}" data-id="${it.id}" aria-pressed="false">${it.label}</button>`).join('')}</div>
      <dl class="stats" id="ord-stats"></dl>
      <hr>
      ${ORD.rate ? `<div class="field"><div class="row"><span class="label">Rate</span><output id="rate-out">${ORD.rate.toFixed(1)}/s</output></div><input type="range" id="rate" min="0.2" max="6" step="0.1" value="${ORD.rate}"></div>` : ''}
      ${ORD.speed ? `<div class="field"><div class="row"><span class="label">Muzzle velocity</span><output id="vel-out">${ORD.speed} m/s</output></div><input type="range" id="vel" min="8" max="120" step="1" value="${ORD.speed}"></div>` : ''}
      ${ORD.ramp ? `<div class="gauge"><div class="row"><span class="label">Heat ramp</span><b id="ramp-out">0%</b></div><div class="track"><span class="ghost" id="ramp-ghost"></span><span class="needle" id="ramp-needle"></span></div></div>` : ''}
      ${ORD.proj.length > 1 ? `<div class="checks" style="margin-bottom:var(--space-3)"><label><input type="checkbox" id="ord-auto" checked> Tier follows ${ORD.tierPath} level (L7 → T2 · L10 → T3)</label></div><div class="seg" id="ord-tier" hidden>${[1, 2, 3].map((n) => `<button type="button" data-tier="${n}" aria-pressed="${n === 1}">T${n}</button>`).join('')}</div>` : ''}
      <div class="checks">
        ${ORD.muzzle ? `<label><input type="checkbox" id="ord-flash" checked> Muzzle flash</label>` : ''}
        ${ORD.impact ? `<label><input type="checkbox" id="ord-impact" checked> Impact</label>` : ''}
      </div>
      <p class="note" id="ord-note"></p>`;
    el('ord-view').addEventListener('click', (e) => { const b = e.target.closest('button'); if (b) setOrdView(b.dataset.view, b.dataset.id); });
    el('rate')?.addEventListener('input', (e) => el('rate-out').textContent = (+e.target.value).toFixed(1) + '/s');
    el('vel')?.addEventListener('input', (e) => el('vel-out').textContent = e.target.value + ' m/s');
    el('ord-auto')?.addEventListener('change', (e) => { el('ord-tier').hidden = e.target.checked; ordStats(null); });
    el('ord-tier')?.addEventListener('click', (e) => { const b = e.target.closest('button'); if (!b) return; ord.tier = +b.dataset.tier; for (const o of el('ord-tier').children) o.setAttribute('aria-pressed', String(o === b)); ordStats(null); });
    ordStats(null);
  }

  function ordStats(id) {
    const cur = id ? PROJECTILES.find((p) => p.id === id) : null;
    const rows = cur ? { File: cur.file, Tris: triCount(proto(id)).toLocaleString(), Parts: String(countMeshes(proto(id))), ...cur.stats }
      : { Meshes: String(ordItems().length), Kind: ORD.kind, ...(ORD.proj.length > 1 ? { Tier: 'T' + currentTier(), Fires: ORD.proj[currentTier() - 1] } : { Fires: ORD.proj[0] }), Spawn: ORD.source || TOWER.id + '_muzzle' };
    el('ord-stats').innerHTML = Object.entries(rows).map(([k, v]) => `<div><dt>${k}</dt><dd class="${k === 'Tris' || k === 'Parts' ? 'arcane' : ''}" style="${k === 'File' || k === 'Fires' || k === 'Spawn' ? 'font-size:var(--size-micro);word-break:break-all' : ''}">${v}</dd></div>`).join('');
    el('ord-note').textContent = cur ? cur.note : { bolt: 'Bolts leave the muzzle marker on lock and home to the target; the impact burst plays on arrival. Tier swaps the mesh, nothing else.', lob: 'Shells lob on a parabola to where the target was when the mortar fired — the miss on a fast target is the design. Impact ring shows the 3.2 m splash.', chain: 'No projectile: a beam from the crown to the target, a second hop to a nearby point, impacts at both. Fades over 160 ms.', ray: 'Continuous beam while locked. Heat ramps over 3 s — sheath brightens, helix spins faster, beam thickens — and bleeds off when the lock drops.' }[ORD.kind];
  }

  function clearShots(scene) {
    for (const s of ord.shots) s.obj.parent?.remove(s.obj);
    for (const f of ord.fx) f.obj.parent?.remove(f.obj);
    if (ord.ray) ord.ray.parent?.remove(ord.ray);
    ord.shots = []; ord.fx = []; ord.ray = null; ord.heat = 0;
  }

  function setOrdView(v, id) {
    ord.view = v;
    for (const b of el('ord-view').children) b.setAttribute('aria-pressed', String(b.dataset.view === v));
    clearShots();
    if (v === 'live') { ord.preview = null; stage.setObject(model); frameForTracking(); ordStats(null); }
    else { ord.preview = proto(id).clone(); ord.preview.name = id; ord.preview.rotation.set(0, 0, 0); stage.setObject(ord.preview); if (demo) demo.visible = false; ordStats(id); }
  }

  function fire(scene, from, q, tp) {
    const THREE = kit.THREE;
    const beamTo = (obj, a, b) => { obj.position.copy(a); obj.lookAt(b); obj.rotateY(Math.PI); obj.scale.set(1, 1, a.distanceTo(b)); };
    if (ORD.kind === 'chain') {
      const beam = spawn(ORD.proj[0]); beamTo(beam, from, tp); scene.add(beam); ord.shots.push({ kind: 'chain', obj: beam, t: 0, dur: ORD.life });
      const hopTo = tp.clone().add(new THREE.Vector3(Math.sin(t * 1.7) * ORD.hop, .4, Math.cos(t * 1.7) * ORD.hop));
      const hop = spawn(ORD.proj[0]); beamTo(hop, tp, hopTo); hop.scale.x = hop.scale.y = .7; scene.add(hop); ord.shots.push({ kind: 'chain', obj: hop, t: -.03, dur: ORD.life * 1.1 });
      impact(scene, tp); impact(scene, hopTo, .6);
      if (ORD.muzzle && el('ord-flash').checked) { const f = spawn(ORD.muzzle); f.position.copy(from); f.lookAt(tp); f.rotateY(Math.PI); scene.add(f); ord.fx.push({ obj: f, t: 0, life: .1, base: 1.2 }); }
      return;
    }
    const obj = spawn(ORD.proj[currentTier() - 1]); obj.position.copy(from); scene.add(obj);
    const speed = +el('vel').value, dist = from.distanceTo(tp);
    ord.shots.push({ kind: ORD.kind, obj, from: from.clone(), to: tp.clone(), t: 0, dur: dist / speed, h: ORD.kind === 'lob' ? dist * .28 : 0, track: ORD.kind === 'bolt' });
    if (ORD.muzzle && el('ord-flash').checked) { const f = spawn(ORD.muzzle); f.position.copy(from); f.quaternion.copy(q); f.rotateY(Math.PI); scene.add(f); ord.fx.push({ obj: f, t: 0, life: .09, base: 1 }); }
  }
  function impact(scene, at, base = 1) {
    ord.hitFlash = .12;
    if (!ORD.impact || !el('ord-impact').checked) return;
    const f = spawn(ORD.impact); f.position.copy(at); if (ORD.kind === 'lob') f.position.y = Math.max(.02, at.y - .85); scene.add(f); ord.fx.push({ obj: f, t: 0, life: .28, base });
  }

  function ordTick(dt, scene, tp, locked, muz) {
    const THREE = kit.THREE, from = new THREE.Vector3(), q = new THREE.Quaternion();
    if (muz) { muz.getWorldPosition(from); muz.getWorldQuaternion(q); } else (model.getObjectByName(ORD.source) || model).getWorldPosition(from);
    if (ORD.kind === 'ray') {
      ord.heat = clamp(ord.heat + (locked ? dt / ORD.ramp : -dt * 1.5), 0, 1);
      if (locked) {
        if (!ord.ray) { ord.ray = spawn(ORD.proj[0]); scene.add(ord.ray); }
        const r = ord.ray; r.position.copy(from); r.lookAt(tp); r.rotateY(Math.PI); r.scale.set(1 + ord.heat * .8, 1 + ord.heat * .8, from.distanceTo(tp));
        const ramp = r.getObjectByName('filament_beam_ramp'); ramp.rotation.z += dt * (4 + ord.heat * 16);
        ramp.traverse((o) => { if (o.isMesh) o.material.emissiveIntensity = .3 + ord.heat * 1.7; });
        ord.hitFlash = .1;
      } else if (ord.ray) { scene.remove(ord.ray); ord.ray = null; }
      el('ramp-out').textContent = Math.round(ord.heat * 100) + '%'; el('ramp-needle').style.left = (ord.heat * 100) + '%'; el('ramp-ghost').style.left = (locked ? 100 : 0) + '%';
    } else {
      ord.cool -= dt;
      if (locked && ord.cool <= 0) { ord.cool = 1 / (+el('rate').value); fire(scene, from, q, tp); }
    }
    for (const s of [...ord.shots]) {
      s.t += dt; const u = Math.min(1, s.t / s.dur);
      if (s.kind === 'chain') { if (u >= 1) { scene.remove(s.obj); ord.shots.splice(ord.shots.indexOf(s), 1); } else if (s.t > 0) fadeAll(s.obj, 1 - u * u); continue; }
      const to = s.track ? tp : s.to, at = (k) => { const p = s.from.clone().lerp(to, k); p.y += s.h * 4 * k * (1 - k); return p; };
      const p = at(u); s.obj.position.copy(p); s.obj.lookAt(at(Math.min(1, u + .02))); s.obj.rotateY(Math.PI);
      if (u >= 1) { scene.remove(s.obj); ord.shots.splice(ord.shots.indexOf(s), 1); impact(scene, p); }
    }
    for (const f of [...ord.fx]) {
      f.t += dt; const u = f.t / f.life;
      if (u >= 1) { scene.remove(f.obj); ord.fx.splice(ord.fx.indexOf(f), 1); continue; }
      f.obj.scale.setScalar((.3 + u * 1.0) * f.base); fadeAll(f.obj, 1 - u * u);
    }
    ord.hitFlash = Math.max(0, ord.hitFlash - dt);
    targetMesh.material.emissive.setHex(ord.hitFlash > 0 ? 0xffe2b0 : 0x3a0d08);
  }

  function hierarchy() {
    if (!TOWER.rig) {
      const spin = TOWER.id === 'barricade' ? '' : `\n└─ <i>${TOWER.id}_spin</i>   cosmetic Y spin`;
      return `${TOWER.id}\n├─ <b>${TOWER.id}_foot</b>   static\n├─ …            static body${spin}`;
    }
    const pad = (s) => s + ' '.repeat(Math.max(1, 14 - s.length));
    return `${TOWER.id}\n├─ <b>${pad(TOWER.id + '_foot')}</b>static\n└─ <i>${pad(TOWER.id + '_yaw')}</i>rot.y ← aim\n   └─ <i>${pad(TOWER.id + '_pitch')}</i>rot.x ← −aim\n      └─ <i>${pad(TOWER.id + '_muzzle')}</i>spawn`;
  }

  function maxLevels() {
    return Object.fromEntries(pathIds.map((id) => [id, 10]));
  }

  function refreshStats() {
    const r = TOWER.rig;
    const maxed = kit.__maxModel ??= TOWER.build(kit, maxLevels());
    applyLevels(maxed, TOWER, maxLevels());
    const rows = {
      ...TOWER.stats,
      Layers: TOWER.layers,
      Yaw: r ? `${r.yaw[1] - r.yaw[0]}°` : 'fixed',
      Pitch: r ? `${r.pitch[0]}…${r.pitch[1]}°` : '—',
      Traverse: r ? `${r.traverse}°/s` : '—',
      Elevate: r ? `${r.elevate}°/s` : '—',
      Tris: triCount(model).toLocaleString(),
      Parts: String(countMeshes(model)),
      'Tris @L10': triCount(maxed).toLocaleString(),
      'Parts @L10': String(countMeshes(maxed)),
    };
    el('rig-stats').innerHTML = Object.entries(rows).map(([k, v]) => {
      const cls = k.startsWith('Tris') || k.startsWith('Parts') ? 'arcane' : (k === 'Cost' ? 'accent' : '');
      return `<div><dt>${k}</dt><dd class="${cls}">${v}</dd></div>`;
    }).join('');
  }

  function refreshLevels() {
    applyLevels(model, TOWER, levels);
    for (const p of TOWER.paths) {
      el('paths').querySelector(`[data-lvl="${p.id}"]`).textContent = String(levels[p.id]);
      for (const lv of Object.keys(p.steps)) {
        const li = el('paths').querySelector(`[data-stone="${p.id}:${lv}"]`);
        if (li) li.dataset.on = levels[p.id] >= +lv ? '1' : '0';
      }
    }
    stage.setAttribute('name', `deepfield_${TOWER.id}_` + pathIds.map((id) => id.slice(0, 3) + levels[id]).join('_'));
    refreshStats();
    if (ORD && el('ord-stats') && ord.view === 'live') ordStats(null);
    applyGizmos();
  }

  let variant = TOWER.variants ? Object.keys(TOWER.variants)[0] : null;
  function rebuild() {
    const scene = model?.parent;
    model = TOWER.build(kit, levels);
    if (variant && TOWER.variants[variant]) TOWER.variants[variant](model, kit);
    stage.setObject(ord.view === 'live' || !ord.preview ? model : ord.preview);
    if (scene && demo) { scene.remove(demo); demo = null; }
    clearShots();
    if (window.__deepfieldTower) window.__deepfieldTower.model = model;
    refreshLevels();
  }
  el('variants')?.addEventListener('click', (e) => {
    const id = e.target.dataset?.id;
    if (!id) return;
    variant = id;
    for (const o of el('variants').children) o.setAttribute('aria-pressed', String(o.dataset.id === id));
    rebuild();
  });

  el('paths').addEventListener('input', (e) => {
    const id = e.target.dataset.slider;
    if (!id) return;
    levels[id] = +e.target.value;
    refreshLevels();
  });

  function ensureDemo(scene) {
    if (demo && demo.parent === scene) return;
    const stale = scene.getObjectByName('__demo');
    if (stale) scene.remove(stale);
    const { THREE } = kit;
    demo = kit.grp('__demo');
    targetMesh = new THREE.Mesh(new THREE.CapsuleGeometry(.34, .74, 8, 16),
      new THREE.MeshStandardMaterial({ color: 0xc93b28, roughness: .5, emissive: 0x3a0d08, flatShading: true }));
    targetMesh.name = '__target';
    const ring = new THREE.Mesh(new THREE.TorusGeometry(.5, .022, 6, 32), new THREE.MeshBasicMaterial({ color: 0xe9614c }));
    ring.rotation.x = Math.PI / 2; ring.position.y = -.72; ring.name = '__target_ring';
    targetMesh.add(ring);
    boreLine = new THREE.Line(new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(), new THREE.Vector3()]),
      new THREE.LineBasicMaterial({ color: 0xe3bc66 }));
    boreLine.name = '__bore';
    demo.add(targetMesh, boreLine);
    scene.add(demo);
  }

  function targetPos(dt) {
    const speed = +el('speed').value, r = +el('dist').value;
    t += dt;
    if (mode.id === 'strafe') {
      const span = Math.max(r, 6);
      return new kit.THREE.Vector3(Math.sin(t * speed / span) * span, mode.y, r * .8);
    }
    if (mode.id === 'approach') {
      const z = r + 2 - ((t * speed) % (r * 2));
      return new kit.THREE.Vector3(r * .12, mode.y, z);
    }
    const w = speed / Math.max(r, 4);
    return new kit.THREE.Vector3(Math.sin(t * w) * r, mode.y + Math.sin(t * .6) * .6, Math.cos(t * w) * r);
  }

  function setNeedle(gauge, range, value, desired) {
    const [lo, hi] = range, span = hi - lo || 1;
    const pct = (v) => clamp(((v - lo) / span) * 100, 0, 100);
    el(gauge + '-needle').style.left = pct(value) + '%';
    el(gauge + '-ghost').style.left = pct(desired) + '%';
  }

  /** The stage frames the tower alone; tracking needs the lane in shot too —
   *  but the tower stays the subject, capped at 1.7× its own framing. */
  function frameForTracking() {
    const { THREE } = kit;
    const cam = stage._camera, ctl = stage._controls, key = stage._key;
    const bounds = new THREE.Box3().setFromObject(model);
    const centre = bounds.getCenter(new THREE.Vector3());
    const objR = bounds.getBoundingSphere(new THREE.Sphere()).radius;
    const r = +el('dist').value;
    const laneR = mode.id === 'strafe' ? Math.hypot(Math.max(r, 6), r * .8)
      : mode.id === 'approach' ? r + 2 : Math.hypot(r, mode.y);
    const tracking = el('track').checked && TOWER.rig;
    const objDist = (objR / Math.tan((cam.fov * Math.PI) / 360)) * 1.25;
    const radius = tracking ? Math.max(objR, laneR * .7) : objR;
    const dist = Math.min((radius / Math.tan((cam.fov * Math.PI) / 360)) * 1.1, objDist * 1.7);
    const look = new THREE.Vector3(centre.x, tracking ? Math.max(centre.y, mode.air ? 2.2 : 1.5) : centre.y, centre.z);
    cam.position.copy(look).add(new THREE.Vector3(1.5, .52, .8).normalize().multiplyScalar(dist));
    cam.near = Math.max(dist / 100, .01); cam.far = dist * 100; cam.updateProjectionMatrix();
    ctl.target.copy(look); ctl.update();
    const span = radius * 2.2;
    key.shadow.camera.left = -span; key.shadow.camera.right = span;
    key.shadow.camera.top = span; key.shadow.camera.bottom = -span;
    key.shadow.camera.updateProjectionMatrix();
  }

  function applyGizmos() {
    const { THREE } = kit;
    for (const g of gizmos) g.parent?.remove(g);
    gizmos = [];
    model.traverse((o) => { if (o.userData.gizmo) o.visible = el('bore').checked; });
    if (!el('pivots').checked) return;
    for (const n of [TOWER.id + '_yaw', TOWER.id + '_pitch', TOWER.id + '_spin']) {
      const node = model.getObjectByName(n);
      if (!node) continue;
      const ax = new THREE.AxesHelper(.9);
      ax.name = '__gizmo_' + n; ax.userData.gizmo = true;
      node.add(ax); gizmos.push(ax);
    }
  }

  let last = performance.now(), frameErr = false;
  function frame(now) {
    try { tick(now); } catch (err) {
      if (!frameErr) { frameErr = true; el('status').textContent = 'frame: ' + err.message; }
    }
    requestAnimationFrame(frame);
  }

  function tick(now) {
    const dt = Math.min((now - last) / 1000, .05); last = now;
    if (ord.preview && ord.view !== 'live') { ord.preview.rotation.y += dt * .5; return; }
    const scene = model.parent;
    if (!scene) return;
    ensureDemo(scene);

    const spin = model.getObjectByName(TOWER.id + '_spin') || model.getObjectByName(TOWER.id + '_radar');
    if (spin) spin.rotation.y += dt * (TOWER.rig ? .55 : .9);

    const tracking = el('track').checked && (TOWER.rig || ORD);
    demo.visible = !!tracking;
    const tp = targetPos(dt);
    targetMesh.position.copy(tp);
    if (!tracking) { boreLine.visible = false; if (ORD && (ord.shots.length || ord.fx.length || ord.ray)) clearShots(scene); return; }
    if (!TOWER.rig) { boreLine.visible = false; ordTick(dt, scene, tp, true, null); return; }

    const { THREE } = kit;
    const yawG = model.getObjectByName(TOWER.id + '_yaw');
    const pitchG = model.getObjectByName(TOWER.id + '_pitch');
    const muz = model.getObjectByName(TOWER.id + '_muzzle');
    const pivot = new THREE.Vector3();
    pitchG.getWorldPosition(pivot);
    const d = tp.clone().sub(pivot);
    const desiredYaw = Math.atan2(d.x, d.z) / (Math.PI / 180);
    const desiredPitch = Math.atan2(d.y, Math.hypot(d.x, d.z)) / (Math.PI / 180);
    const lim = el('limits').checked;
    const tgtYaw = lim ? clamp(desiredYaw, TOWER.rig.yaw[0], TOWER.rig.yaw[1]) : desiredYaw;
    const tgtPitch = lim ? clamp(desiredPitch, TOWER.rig.pitch[0], TOWER.rig.pitch[1]) : desiredPitch;
    const step = (from, to, rate) => {
      const delta = wrap180(to - from), max = (lim ? rate : 720) * dt;
      return from + clamp(delta, -max, max);
    };
    aim.yaw = wrap180(step(aim.yaw, tgtYaw, TOWER.rig.traverse));
    aim.pitch = step(aim.pitch, tgtPitch, TOWER.rig.elevate);
    yawG.rotation.y = aim.yaw * (Math.PI / 180);
    pitchG.rotation.x = -aim.pitch * (Math.PI / 180);

    const yawErr = Math.abs(wrap180(desiredYaw - aim.yaw));
    const pitchErr = Math.abs(desiredPitch - aim.pitch);
    const blocked = lim && (desiredPitch < TOWER.rig.pitch[0] - .5 || desiredPitch > TOWER.rig.pitch[1] + .5);
    const s = el('status');
    if (blocked) {
      s.dataset.tone = 'fail';
      s.textContent = desiredPitch < TOWER.rig.pitch[0]
        ? 'Min elevation — target inside dead zone' : 'Max elevation — target overhead';
    } else if (yawErr > 2.5 || pitchErr > 2.5) {
      s.dataset.tone = 'slew';
      s.textContent = `Slewing — ${Math.max(yawErr, pitchErr).toFixed(1)}° behind`;
    } else {
      s.dataset.tone = 'lock';
      s.textContent = 'Locked — on target';
    }
    el('yaw-read').textContent = aim.yaw.toFixed(1) + '°';
    el('pitch-read').textContent = aim.pitch.toFixed(1) + '°';
    setNeedle('yaw', TOWER.rig.yaw, aim.yaw, wrap180(desiredYaw));
    setNeedle('pitch', TOWER.rig.pitch, aim.pitch, desiredPitch);
    if (ORD) ordTick(dt, scene, tp, !blocked && yawErr <= 2.5 && pitchErr <= 2.5, muz);

    boreLine.visible = el('bore').checked;
    if (boreLine.visible && muz) {
      const from = new THREE.Vector3(); muz.getWorldPosition(from);
      const fwd = new THREE.Vector3(0, 0, 1).applyQuaternion(muz.getWorldQuaternion(new THREE.Quaternion()));
      boreLine.geometry.setFromPoints([from, from.clone().addScaledVector(fwd, Math.max(6, tp.distanceTo(from)))]);
      boreLine.geometry.computeBoundingSphere();
    }
  }

  for (const m of MODES) {
    const b = document.createElement('button');
    b.type = 'button'; b.dataset.id = m.id; b.textContent = m.label;
    b.setAttribute('aria-pressed', String(m === mode));
    b.addEventListener('click', () => {
      mode = m; t = 0;
      for (const o of el('modes').children) o.setAttribute('aria-pressed', String(o.dataset.id === m.id));
      frameForTracking();
    });
    el('modes').appendChild(b);
  }
  el('speed').addEventListener('input', (e) => el('speed-out').textContent = (+e.target.value).toFixed(1) + ' m/s');
  el('dist').addEventListener('input', (e) => el('dist-out').textContent = (+e.target.value).toFixed(1) + ' m');
  el('dist').addEventListener('change', frameForTracking);
  el('track').addEventListener('change', frameForTracking);
  el('bore').addEventListener('change', applyGizmos);
  el('pivots').addEventListener('change', applyGizmos);
  el('ord-toggle').addEventListener('click', () => { ord.open = !ord.open; localStorage.setItem('df_ord_open', ord.open ? '1' : '0'); if (!ord.open && ord.view !== 'live' && ORD) setOrdView('live'); renderOrd(); });

  stage.ready.then(async ({ THREE }) => {
    const { applyStudio } = await import('./models/studio.js'); applyStudio(stage, THREE);
    kit = makeKit(THREE);
    model = TOWER.build(kit, levels);
    if (TOWER.rig) {
      const y = model.getObjectByName(TOWER.id + '_yaw');
      const p = model.getObjectByName(TOWER.id + '_pitch');
      y.userData.aim = { axis: 'y', min: TOWER.rig.yaw[0], max: TOWER.rig.yaw[1], degPerSec: TOWER.rig.traverse };
      p.userData.aim = { axis: 'x', min: TOWER.rig.pitch[0], max: TOWER.rig.pitch[1], degPerSec: TOWER.rig.elevate, sign: -1 };
      p.rotation.x = -aim.pitch * (Math.PI / 180);
    }
    stage.setObject(model);
    el('hier').innerHTML = hierarchy();
    renderOrd();
    refreshLevels();
    frameForTracking();
    window.__deepfieldTower = {
      tower: TOWER, model, levels, kit, step: (ms) => tick(ms),
      setLevel(id, v) {
        levels[id] = v;
        const slider = el('paths').querySelector(`[data-slider="${id}"]`);
        if (slider) slider.value = String(v);
        refreshLevels();
      },
    };
    requestAnimationFrame(frame);
  });
}
