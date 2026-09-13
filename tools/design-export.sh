#!/usr/bin/env bash
# Rebuild game/assets from Claude Design's sources.
#
# Design's models are three.js code (docs/design/models/*.js), and the page that
# turns them into GLBs needs a real browser: the skyboxes are shaders baked to a
# texture, which no headless exporter can do. So this starts a small host that
# serves the design project, opens the export page in your browser, and writes
# every file the page produces straight into the repo — game/assets/<folder>/
# per AssetLibrary.Routes, docs/palette.json, docs/forward-manifest.json,
# docs/ASSET-DELIVERY.md, docs/design-system/, game/assets/structures/manifest.json
# — then prepares the icons and re-imports for Godot.
#
#   tools/design-export.sh                  # full run
#   tools/design-export.sh --only lance     # one tower: chassis + its 30 stages
#   tools/design-export.sh --only drifter   # one model, by item id or file stem
#   tools/design-export.sh --only vfx       # a category: everything one module builds
#   tools/design-export.sh --serve          # just host the page (debugging the export)
#
# A tweak to one model is a one-line edit in docs/design/models/*.js, and --only
# is what makes checking it cost one file instead of the whole drop. A run merges
# its rows into the existing manifest rather than replacing it, and skips the
# palette/icon/design-system bundle, which it never rewrites anyway.
#
# The page lists every file it writes beside the viewer; click a row to load that
# .glb back off disk and look at it.
#
# Afterwards: `make assets && make usage`, and read docs/ASSET-DELIVERY.md for
# what design says changed.
set -euo pipefail
cd "$(dirname "$0")/.."
HOST=tools/design-export
PORT="${PORT:-8765}"
GODOT="${GODOT:-$HOME/Applications/Godot_mono.app/Contents/MacOS/Godot}"
ONLY=""
SERVE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --only)   ONLY="${2:-}"; [ -n "$ONLY" ] || { echo "--only needs a selector"; exit 2; }; shift 2 ;;
    --only=*) ONLY="${1#--only=}"; shift ;;
    --serve)  SERVE=1; shift ;;
    *) echo "unknown argument: $1 (usage: $0 [--only <sel>[,<sel>...]] [--serve])"; exit 2 ;;
  esac
done

command -v node >/dev/null || { echo "node is required (see docs/INSTALL.md)"; exit 1; }
[ -d docs/design/models ] || { echo "docs/design/models/ is missing — vendor the design project first"; exit 1; }

# Every kit the export page imports must actually be vendored.
#
# export.html imports its kits statically, which is correct — it is a faithful
# port of design's own page and should stay one. But a static import of a file a
# drop did not carry throws at module *resolution*, before a single line of the
# export runs, so one absent kit produces zero assets for every other kit on the
# page. That is not a hypothetical: `toaster.js` and `vehicles.js` were named
# there and not vendored, and the export had been silently producing nothing —
# which is why the Spire, whose kit was written and correct, was forty metres of
# untinted boxes for three milestones.
#
# The page cannot guard against it without diverging from design's copy. This
# can, and it costs one grep.
missing=""
while read -r kit; do
  [ -f "docs/design/models/$kit.js" ] || missing="$missing $kit.js"
done <<EOF
$(grep -oE "from '\./models/[a-z-]+\.js'" tools/design-export/export.html \
    | sed -E "s#from '\./models/##; s#\.js'##" | sort -u)
EOF
if [ -n "$missing" ]; then
  echo "export.html imports kits this checkout does not have:$missing"
  echo "vendor the drop that carries them, or the export will produce nothing at all."
  exit 1
fi
if [ ! -d "$HOST/node_modules/three" ]; then
  echo "installing three.js (pinned in $HOST/package.json)…"
  (cd "$HOST" && npm install --no-audit --no-fund --silent)
fi

PORT="$PORT" node "$HOST/server.mjs" &
SERVER=$!
trap 'kill $SERVER 2>/dev/null || true' EXIT
for _ in $(seq 1 50); do curl -sf "http://127.0.0.1:$PORT/status" >/dev/null && break; sleep 0.2; done

URL="http://127.0.0.1:$PORT/export.html"
if [ -n "$ONLY" ]; then URL="$URL?only=$ONLY"; fi
if [ -n "$SERVE" ]; then
  echo "serving $URL — Ctrl-C to stop"
  wait $SERVER
  exit 0
fi

echo "opening $URL — the export runs on load and writes into the repo"
if command -v open >/dev/null; then open "$URL"; else echo "open $URL in a browser"; fi

# The page reports progress to the host; wait for its verdict.
while :; do
  sleep 2
  tail=$(curl -sf "http://127.0.0.1:$PORT/status" | python3 -c 'import sys,json; print(json.load(sys.stdin)["tail"])' 2>/dev/null || true)
  printf '\r%s' "$(echo "$tail" | tail -1 | cut -c1-110)"
  case "$tail" in
    *"DONE"*) echo; break ;;
    *"ERROR"*) echo; echo "export failed — see $HOST/export.log"; exit 1 ;;
  esac
done

# Only a full run rewrites the icons, so preparing them again buys nothing.
if [ -z "$ONLY" ]; then ./tools/prepare-icons.sh; fi
if [ -x "$GODOT" ]; then
  echo "importing for Godot…"
  "$GODOT" --headless --path game --import >/dev/null 2>&1 || echo "godot import reported errors — run: $GODOT --headless --path game --import"
else
  echo "Godot not at $GODOT — run the import by hand: godot --headless --path game --import"
fi
echo "done: $(find game/assets -name '*.glb' | wc -l | tr -d ' ') GLB files. Next: make assets && make usage"
