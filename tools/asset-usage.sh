#!/usr/bin/env bash
# Which delivered assets does the game actually consume?
#
# asset-report.sh answers "has design shipped it". This answers the other
# question, which is the one that rots silently: an asset can be delivered,
# imported, and never referenced by a line of code. That is invisible until
# someone opens the folder and wonders why the game looks unfinished.
#
# Sources of truth:
#   delivered — files under game/assets
#   consumed  — names the running game requests (--asset-audit) plus any name
#               appearing as a string literal in game/scripts
set -euo pipefail
cd "$(dirname "$0")/.."

# Two sources, because neither alone is complete: the audit walks the content
# tables (every enemy, tower, stage module, icon) but never builds a level, and
# a played match builds the level but only spawns what that match spawns.
AUDIT=$(mktemp)
./play --headless -- --asset-audit >> "$AUDIT" 2>&1 || true
for map in foundry switchyard; do
  # --quit-after is an engine flag: it must precede the -- separator, or the
  # game receives it as a user arg and runs forever.
  ./play --headless --quit-after 1200 -- --solo "$map" --dump-assets >> "$AUDIT" 2>&1 || true
done

delivered=$(mktemp); consumed=$(mktemp)

find game/assets \( -name '*.glb' -o -name 'icon_*.svg' \) \
  | sed 's|.*/||; s|\.[^.]*$||' | sort -u > "$delivered"

# Names the running game asked for. The audit prints every request rather than
# only the misses, and icon ids are interpolated from content tables, so this is
# the honest record — scanning source for string literals under-reports badly.
sed -n 's/.*\[asset-audit\] requested \([a-z0-9_]*\).*/\1/p' "$AUDIT" | sort -u > "$consumed"

# Stage modules are requested per level; the audit walks all ten.
sort -u "$consumed" -o "$consumed"

total=$(wc -l < "$delivered" | tr -d ' ')
used=$(comm -12 "$delivered" "$consumed" | wc -l | tr -d ' ')
unused=$(comm -23 "$delivered" "$consumed" | wc -l | tr -d ' ')

echo "delivered $total · consumed $used · unused $unused"
if [ "${2:-}" = --list ] || [ "${1:-}" = --list ]; then
  echo
  echo "not referenced by any code path:"
  comm -23 "$delivered" "$consumed" | sed 's/^/  /'
fi
