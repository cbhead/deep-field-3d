#!/usr/bin/env python3
"""Every delivered .glb against the contract the client actually reads.

`make assets` answers "did the file land", and `--asset-audit` answers "does the
name resolve". Neither opens a model. Everything in ART-INTEGRATION.md under
"Things that are load-bearing" — the rig chain a turret is driven by, the empties
a stage module is merged onto, the mount nodes a gun is assembled from, the pivot
a hitbox is placed against — was unchecked, and each one fails silently and
plausibly: a missing `_yaw` turns the whole tower like a graybox, a bad pivot
looks like the model floating.

The bug that prompted this: the export dropped every per-arm increment the
Singularity builds, so six of its twenty upgrade stages shipped identical to the
stage below them. It survived a full drop, `make check`, `make assets`,
`--asset-audit`, eight CI probes and a screenshot review, because every one of
those asks a different question. The check that catches it is four lines — a
stage whose manifest says it *adds* something must add triangles.

Reads the glTF JSON chunk directly: no Blender, no Godot, no browser, no
dependencies, a second or two for the whole drop, so it runs in CI's fast lane.

    tools/model-validate.py           # the report
    tools/model-validate.py --list    # every violation, not just the counts
    tools/model-validate.py --refresh # rewrite the ratchet baseline

Everything is ratcheted against docs/model-validation-baseline.tsv, in the
spirit of map-validate.sh: the delivered art has legitimate exceptions (a Leaper
mid-jump does not stand on the ground, a broken Barricade sags below it) and a
red build nobody can turn green gets switched off within a week. A violation not
in the baseline fails the build; one that disappears asks to be removed from it.
A baselined *contract* check is a real defect somebody chose to carry rather
than a convention, so those are restated on every run instead of passing
quietly.
"""
import json, struct, sys, os, math, glob, collections

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(REPO, 'game', 'assets')
MANIFEST = os.path.join(ASSETS, 'structures', 'manifest.json')
BASELINE = os.path.join(REPO, 'docs', 'model-validation-baseline.tsv')
BOUNDS_TOL = 0.006          # design rounds bounds to 3dp on the way out
PIVOT_TOL = 0.02            # ground-centre / spine-base, in metres
CENTRE_TOL = 0.35           # how far a placeable's footprint may sit off-axis


# ── glTF reading ──────────────────────────────────────────────────────────
def read_glb(path):
    with open(path, 'rb') as fh:
        data = fh.read()
    magic, _ver, length = struct.unpack_from('<III', data, 0)
    if magic != 0x46546C67:
        raise ValueError('not a glb')
    off, js = 12, None
    while off < length:
        clen, ctype = struct.unpack_from('<II', data, off)
        off += 8
        if ctype == 0x4E4F534A:
            js = json.loads(data[off:off + clen])
        off += clen
    if js is None:
        raise ValueError('no JSON chunk')
    return js


def quat_mat(q):
    x, y, z, w = q
    return [[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w), 0],
            [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w), 0],
            [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y), 0],
            [0, 0, 0, 1]]


def mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def local_matrix(n):
    if 'matrix' in n:
        m = n['matrix']
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]   # column-major
    t = n.get('translation', [0, 0, 0])
    r = n.get('rotation', [0, 0, 0, 1])
    s = n.get('scale', [1, 1, 1])
    T = [[1, 0, 0, t[0]], [0, 1, 0, t[1]], [0, 0, 1, t[2]], [0, 0, 0, 1]]
    S = [[s[0], 0, 0, 0], [0, s[1], 0, 0], [0, 0, s[2], 0], [0, 0, 0, 1]]
    return mul(mul(T, quat_mat(r)), S)


def xform(m, p):
    v = p + [1]
    return [sum(m[i][k] * v[k] for k in range(4)) for i in range(3)]


def survey(path):
    g = read_glb(path)
    nodes, meshes, acc = g.get('nodes', []), g.get('meshes', []), g.get('accessors', [])
    scene = g['scenes'][g.get('scene', 0)]
    roots = scene.get('nodes', [])
    out = {
        'file': os.path.basename(path), 'bytes': os.path.getsize(path),
        'roots': [nodes[i].get('name', '') for i in roots], 'node': {},
        'min': [1e9] * 3, 'max': [-1e9] * 3, 'tris': 0, 'parts': 0,
        'neg_scale': [], 'anims': len(g.get('animations', [])), 'skins': len(g.get('skins', [])),
        'extras': {}, 'root_rot': None, 'unreachable': 0,
    }
    seen = set()

    def walk(i, world, parent):
        n = nodes[i]
        seen.add(i)
        name = n.get('name', f'#{i}')
        w = mul(world, local_matrix(n))
        out['node'][name] = {
            'parent': parent, 't': [round(v, 4) for v in n.get('translation', [0, 0, 0])],
            'rot': n.get('rotation'), 'world': [round(v, 4) for v in xform(w, [0, 0, 0])],
            'mesh': 'mesh' in n,
        }
        if 'extras' in n:
            out['extras'][name] = n['extras']
        if any(v < 0 for v in n.get('scale', [1, 1, 1])):
            out['neg_scale'].append(name)
        if 'mesh' in n:
            out['parts'] += 1
            for prim in meshes[n['mesh']].get('primitives', []):
                a = acc[prim['attributes']['POSITION']]
                count = acc[prim['indices']]['count'] if 'indices' in prim else a['count']
                if prim.get('mode', 4) == 4:
                    out['tris'] += count // 3
                for cx in (a['min'][0], a['max'][0]):
                    for cy in (a['min'][1], a['max'][1]):
                        for cz in (a['min'][2], a['max'][2]):
                            q = xform(w, [cx, cy, cz])
                            for k in range(3):
                                out['min'][k] = min(out['min'][k], q[k])
                                out['max'][k] = max(out['max'][k], q[k])
        for c in n.get('children', []):
            walk(c, w, name)

    identity = [[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1]]
    for r in roots:
        walk(r, identity, None)
    out['unreachable'] = len(nodes) - len(seen)
    if roots and nodes[roots[0]].get('rotation'):
        q = nodes[roots[0]]['rotation']
        out['root_rot'] = round(2 * math.degrees(math.acos(max(-1.0, min(1.0, q[3])))), 1)
    if out['parts'] == 0:
        out['min'] = out['max'] = None
    else:
        out['min'] = [round(v, 3) for v in out['min']]
        out['max'] = [round(v, 3) for v in out['max']]
    return out


# ── checks ────────────────────────────────────────────────────────────────
class Report:
    """Every violation is ratcheted against the baseline; the severity only
    changes how loudly a *known* one is reported. A contract violation somebody
    accepted is worth restating on every run — it is a bug with a note on it,
    not a convention — while a pivot exception that is correct as it stands is
    just noise after the first read."""

    def __init__(self):
        self.items = []     # (check, file, detail, severity)

    def fail(self, check, file, detail):
        self.items.append((check, file, detail, 'contract'))

    def soft_fail(self, check, file, detail):
        self.items.append((check, file, detail, 'pivot'))


def stem(name):
    return name[:-4] if name.endswith('.glb') else name


def check_manifest(models, rows, rep):
    """Design's receipt must describe what shipped: TowerRig reads its rig rows
    and DesignManifest reads its bounds, so a stale row is a silent wrong answer."""
    for f in rows:
        if f not in models:
            rep.fail('manifest/orphan-row', f, 'row has no file on disk')
    for f in models:
        if f not in rows:
            rep.fail('manifest/missing-row', f, 'file on disk has no manifest row')
    for f, row in rows.items():
        m = models.get(f)
        if not m:
            continue
        for key in ('tris', 'parts'):
            if key in row and row[key] != m[key]:
                rep.fail(f'manifest/{key}', f, f'manifest {row[key]}, file {m[key]}')
        # `bytes` is deliberately NOT checked. Nothing reads it — the game takes
        # bounds and the rig rows and leaves the rest — and it is the one field
        # that moves on its own: re-exporting an unchanged model can shift a file
        # by a few bytes (glTF float formatting, and the canvas-drawn floor
        # textures re-encode differently between runs) with identical geometry.
        # Checking it turned an ordinary `make design-export` into twenty-four
        # failures that no edit could clear, which is the red build this script's
        # docstring promises not to be. Geometry is what has to agree.
        want, got = row.get('bounds'), (m['min'], m['max'])
        if bool(want) != bool(m['min']):
            rep.fail('manifest/bounds', f, f'manifest {"has" if want else "no"} bounds, file differs')
        elif want and m['min']:
            drift = max(abs(a - b) for a, b in zip(want['min'] + want['max'], got[0] + got[1]))
            if drift > BOUNDS_TOL:
                rep.fail('manifest/bounds', f, f'{drift:.3f} m from the manifest')


def check_hygiene(models, rep):
    """Things the import pipeline assumes: one root named as the file, motion
    added in code rather than baked, and no geometry the scene cannot reach."""
    for f, m in models.items():
        if len(m['roots']) != 1:
            rep.fail('root/count', f, f'{len(m["roots"])} roots, expected 1')
        elif m['roots'][0] != stem(f):
            rep.fail('root/name', f, f'root "{m["roots"][0]}" != "{stem(f)}" (GameRoot.Unwrap looks it up by name)')
        if m['anims']:
            rep.fail('hygiene/animation', f, f'{m["anims"]} animation(s); motion is added in code')
        if m['skins']:
            rep.fail('hygiene/skin', f, f'{m["skins"]} skin(s); the brief ships static meshes')
        if m['unreachable']:
            rep.fail('hygiene/unreachable', f, f'{m["unreachable"]} node(s) not in the scene graph')
        if m['neg_scale']:
            rep.fail('hygiene/negative-scale', f, f'negative scale on {m["neg_scale"][0]} (flips normals)')
        # The export turns every tower to face -Z; nothing else is flipped.
        flipped = m['root_rot'] is not None and abs(m['root_rot'] - 180.0) < 0.5
        if f.startswith('tower_') and not flipped:
            rep.fail('root/flip', f, 'tower root is not turned 180° (export.html chassis/staged)')
        if not f.startswith('tower_') and flipped:
            rep.fail('root/flip', f, 'non-tower root carries the tower 180° flip')


def check_rigs(models, rows, rep):
    """TowerRig.Resolve finds <id>_yaw / _pitch / _muzzle by name and drives them;
    an aura tower must have no yaw at all, because one that pointed at things
    would lie about how it works."""
    for f, row in rows.items():
        if row.get('kind') != 'chassis':
            continue
        m = models.get(f)
        if not m:
            continue
        tower = row.get('tower', '')
        if 'rig' in row:
            rig = row['rig']
            chain = [rig.get('yaw'), rig.get('pitch'), rig.get('muzzle')]
            for node in [f'{tower}_foot'] + chain:
                if node not in m['node']:
                    rep.fail('rig/missing-node', f, f'{node} is named in the manifest but not in the file')
            if all(n in m['node'] for n in chain):
                if m['node'][chain[1]]['parent'] != chain[0]:
                    rep.fail('rig/nesting', f, f'{chain[1]} hangs off {m["node"][chain[1]]["parent"]}, not {chain[0]}')
                if m['node'][chain[2]]['parent'] != chain[1]:
                    rep.fail('rig/nesting', f, f'{chain[2]} hangs off {m["node"][chain[2]]["parent"]}, not {chain[1]}')
                # barrel-up is -X about pitch, and the bore leaves down -Z
                if m['node'][chain[2]]['world'][2] >= 0:
                    rep.fail('rig/muzzle-forward', f, f'muzzle sits at z={m["node"][chain[2]]["world"][2]}, not down -Z')
            role = m['extras'].get(rig.get('muzzle'), {}).get('role')
            if role != 'projectileSpawn':
                rep.fail('rig/muzzle-role', f, f'muzzle extras.role is {role!r}, expected projectileSpawn')
        else:
            spin = row.get('spin')
            if spin and spin not in m['node']:
                rep.fail('rig/missing-spin', f, f'{spin} is named in the manifest but not in the file')
            if f'{tower}_yaw' in m['node']:
                rep.fail('rig/aura-points', f, f'aura tower carries {tower}_yaw; it would track a target it cannot shoot')
            if spin and m['extras'].get(spin, {}).get('role') != 'cosmeticSpin':
                rep.fail('rig/spin-role', f, f'{spin} is not tagged extras.role = cosmeticSpin')


def check_modules(models, rows, rep):
    """GameRoot.MergeRig moves a module's parts onto the chassis node of the same
    name. That only lands them in the right place if the shared empties agree
    exactly — and a stage the manifest says adds something must actually add it."""
    chassis_of = {}
    for f, row in rows.items():
        if row.get('kind') == 'chassis':
            chassis_of[row.get('tower')] = f

    ladders = collections.defaultdict(dict)
    for f, row in rows.items():
        if row.get('kind') != 'module':
            continue
        ladders[(row['tower'], row['path'])][row['stage']] = f

    for (tower, path), stages in sorted(ladders.items()):
        cf = chassis_of.get(tower)
        chassis = models.get(cf) if cf else None
        for stage in sorted(stages):
            f = stages[stage]
            m = models.get(f)
            if not m:
                continue
            row = rows[f]
            if stage == 1 and m['parts']:
                rep.fail('module/s1-not-empty', f, f'_s1 must be an empty root; it has {m["parts"]} mesh part(s)')
            if stage > 1:
                prev = models.get(stages.get(stage - 1, ''), {})
                if prev and m['tris'] < prev['tris']:
                    rep.fail('module/not-cumulative', f, f'{m["tris"]} tris, fewer than s{stage - 1}\'s {prev["tris"]}')
                # The Singularity catch.
                if prev and row.get('adds') and m['tris'] == prev['tris']:
                    rep.fail('module/invisible-upgrade', f,
                             f'adds nothing over s{stage - 1} ({m["tris"]} tris) but the manifest '
                             f'says it adds "{row["adds"]}"')
            if not chassis:
                continue
            for node, nd in m['node'].items():
                if node not in chassis['node'] or node == m['roots'][0]:
                    continue
                cd = chassis['node'][node]
                mp = None if nd['parent'] == m['roots'][0] else nd['parent']
                cp = None if cd['parent'] == chassis['roots'][0] else cd['parent']
                if nd['t'] != cd['t'] or mp != cp or (nd['rot'] or [0, 0, 0, 1]) != (cd['rot'] or [0, 0, 0, 1]):
                    rep.fail('module/rig-mirror', f,
                             f'{node} does not match the chassis ({nd["t"]} under {mp} '
                             f'vs {cd["t"]} under {cp}) — MergeRig would shift its parts')


# Gunsmith slots (sim/Sim.Core/Content/Gunsmith.cs) that WeaponAssembly mounts.
SLOTS = ('barrel', 'muzzle', 'optic', 'magazine', 'stock', 'underbarrel', 'infusion')


def check_weapons(models, rep):
    """WeaponAssembly.Build hangs each fitted module off <weapon>_mount_<slot>,
    and a muzzle device off the fitted barrel's own attach_mount_muzzle."""
    for f, m in models.items():
        if f.startswith('weapon_') and (f.endswith('_vm.glb') or f.endswith('_world.glb')):
            wid = stem(f).replace('weapon_', '').replace('_vm', '').replace('_world', '')
            if not any(f'{wid}_mount_{s}' in m['node'] for s in SLOTS):
                continue          # a melee platform mounts nothing (the wrench)
            for slot in SLOTS:
                if f'{wid}_mount_{slot}' not in m['node']:
                    rep.fail('weapon/mount', f, f'no {wid}_mount_{slot} for WeaponAssembly to hang a module on')
        if f.startswith('attach_') and 'barrel' in f and 'attach_mount_muzzle' not in m['node']:
            rep.fail('weapon/barrel-muzzle', f, 'a fitted barrel must carry attach_mount_muzzle')


def check_vfx(models, rows, rep):
    """Vfx.cs drives named sub-groups without a lookup table: _spin turns, _pulse
    breathes, _rise lifts. A file with none of them can only fade."""
    for f, m in models.items():
        if not f.startswith('vfx_'):
            continue
        if rows.get(f, {}).get('kind') != 'vfx':
            continue
        if any(n.endswith(('_spin', '_pulse', '_rise')) for n in m['node']):
            continue
        # muzzle flashes, impacts and tracers are scaled and faded whole
        if any(f.startswith(p) for p in ('vfx_muzzle_', 'vfx_impact_', 'vfx_tracer_')):
            continue
        rep.soft_fail('vfx/no-driven-group', f, 'no _spin / _pulse / _rise group for Vfx.cs to drive')


# Pivot conventions, DESIGN-BRIEF §2: ground-centre for placeables, spine base
# for anything that walks. Stage modules are partial geometry and map kits are
# laid out in world space, so neither is held to it.
GROUNDED = ('tower_barricade', 'trap_', 'socket_', 'enemy_', 'hero_', 'boss_', 'pickup_')


def check_pivots(models, rows, rep):
    for f, m in models.items():
        if not m['min']:
            continue
        is_chassis = rows.get(f, {}).get('kind') == 'chassis'
        if not (is_chassis or f.startswith(GROUNDED)):
            continue
        if abs(m['min'][1]) > PIVOT_TOL:
            rep.soft_fail('pivot/off-ground', f, f'sits {m["min"][1]:+.3f} m off its own origin')
        cx = (m['min'][0] + m['max'][0]) / 2
        cz = (m['min'][2] + m['max'][2]) / 2
        if max(abs(cx), abs(cz)) > CENTRE_TOL:
            rep.soft_fail('pivot/off-centre', f, f'footprint centred at ({cx:+.2f}, {cz:+.2f})')


# ── baseline ──────────────────────────────────────────────────────────────
def load_baseline():
    allowed = set()
    if not os.path.exists(BASELINE):
        return allowed
    with open(BASELINE) as fh:
        for line in fh:
            line = line.split('#', 1)[0].strip()
            if not line:
                continue
            parts = line.split('\t')
            if len(parts) >= 2:
                allowed.add((parts[0], parts[1]))
    return allowed


def write_baseline(items):
    with open(BASELINE, 'w') as fh:
        fh.write('# What the delivered art does not satisfy today.\n'
                 '# tools/model-validate.py fails on a violation that is NOT listed here, so\n'
                 '# this is a ratchet: fix a model and drop its row in the same commit, and the\n'
                 '# gain is locked in. Regenerate with: tools/model-validate.py --refresh\n'
                 '#\n'
                 '# Most rows are pivot conventions, and several are correct as they stand — a\n'
                 '# Leaper mid-jump is airborne, a broken Barricade sags, a wall socket hangs\n'
                 '# off a wall. A row for a *contract* check is different: that is a real defect\n'
                 '# someone chose to carry, so give it a reason and a way out. Those are\n'
                 '# restated on every run rather than passing quietly.\n'
                 '#\n# check\tfile\tnote\n')
        for check, file, detail, _sev in sorted(items):
            fh.write(f'{check}\t{file}\t{detail}\n')


# ── main ──────────────────────────────────────────────────────────────────
def main(argv):
    listing = '--list' in argv
    refresh = '--refresh' in argv

    files = sorted(glob.glob(os.path.join(ASSETS, '**', '*.glb'), recursive=True))
    if not files:
        print(f'no .glb under {ASSETS} — nothing to validate')
        return 0

    models, broken = {}, []
    for path in files:
        try:
            models[os.path.basename(path)] = survey(path)
        except Exception as err:
            broken.append((os.path.basename(path), repr(err)))

    rows = {}
    if os.path.exists(MANIFEST):
        with open(MANIFEST) as fh:
            rows = {r['file']: r for r in json.load(fh).get('files', [])}

    rep = Report()
    for name, err in broken:
        rep.fail('file/unreadable', name, err)
    if rows:
        check_manifest(models, rows, rep)
        check_rigs(models, rows, rep)
        check_modules(models, rows, rep)
        check_vfx(models, rows, rep)
    else:
        print(f'! {MANIFEST} is missing — manifest, rig and module checks skipped')
    check_hygiene(models, rep)
    check_weapons(models, rep)
    check_pivots(models, rows, rep)

    if refresh:
        write_baseline(rep.items)
        print(f'baseline refreshed: {len(rep.items)} exception(s) recorded in {os.path.relpath(BASELINE, REPO)}')
        return 0

    allowed = load_baseline()
    new = [i for i in rep.items if (i[0], i[1]) not in allowed]
    known = [i for i in rep.items if (i[0], i[1]) in allowed]
    seen = {(c, f) for c, f, _d, _s in rep.items}
    healed = sorted(a for a in allowed if a not in seen)

    print(f'{len(models)} model(s), {len(rows)} manifest row(s)')

    if new:
        contract = [i for i in new if i[3] == 'contract']
        pivot = [i for i in new if i[3] == 'pivot']
        print(f'\nFAILED — {len(new)} violation(s) not in the baseline:')
        for label, group in (('the delivery contract', contract), ('pivot conventions', pivot)):
            if not group:
                continue
            print(f'  {label}:')
            for check in sorted({c for c, _f, _d, _s in group}):
                hits = [(f, d) for c, f, d, _s in group if c == check]
                print(f'    {check} ({len(hits)})')
                for f, d in (hits if listing else hits[:4]):
                    print(f'        {f}: {d}')
                if not listing and len(hits) > 4:
                    print(f'        … {len(hits) - 4} more (--list)')
        if pivot:
            print(f'  Fix the model, or add the row to {os.path.relpath(BASELINE, REPO)} with a reason.')
    else:
        print('\nall rules pass: manifest, rigs, module merge, weapon mounts, hygiene, pivots')

    known_contract = [i for i in known if i[3] == 'contract']
    if known_contract:
        print(f'\ncarried in the baseline — real defects with a note on them, not conventions:')
        for check, f, d, _s in sorted(known_contract):
            print(f'  {check}  {f}: {d}')
    known_pivot = len(known) - len(known_contract)
    if known_pivot:
        print(f'pivot ratchet: {known_pivot} known exception(s), none new')

    if healed:
        print(f'\nimproved — {len(healed)} baseline row(s) no longer apply, drop them:')
        for check, f in healed:
            print(f'  {check}\t{f}')

    return 1 if new else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
