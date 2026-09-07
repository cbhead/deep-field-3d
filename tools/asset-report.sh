#!/usr/bin/env bash
# Art delivery status: what docs/DESIGN-BRIEF.md names vs what is in the build.
#
# The "wired" column is the one that matters day to day — those assets render
# the moment the file lands in game/assets/. The rest are named ahead for later
# milestones and need that milestone's code before they appear.
set -euo pipefail

cd "$(dirname "$0")/.."
MANIFEST=docs/asset-manifest.tsv
ASSETS=game/assets

[ -f "$MANIFEST" ] || { echo "missing $MANIFEST" >&2; exit 1; }

in_manifest() {  # in_manifest <name> — exact row, or the glob row covering it
  local name="$1" stem
  stem=$(echo "$name" | sed 's/_s[0-9]\{1,2\}$/_s*/')
  awk -F'\t' -v a="$name" -v b="$stem" \
    '$1==a || $1==b { found=1 } END { exit !found }' "$MANIFEST"
}

# --verify <audit.log>: every name the code asks for must be a name the brief
# gives design. Drift here is silent and expensive — design ships a file, it
# never loads, and nobody finds out until someone looks at the game.
if [ "${1:-}" = --verify ]; then
  log="${2:?usage: asset-report.sh --verify <audit.log>}"
  drift=0
  # Two shapes of audit line. The content-table audit reports placeholders; a
  # real match reports every name it asked for, which is the only way map kits,
  # skyboxes and traversal fixtures get checked at all — they were unverified
  # until a third map turned up requesting four of them. Icons are excluded:
  # they live in the brief's icon list, not the model manifest.
  names=$(mktemp)
  trap 'rm -f "$names"' EXIT
  {
    sed -n 's/.*\[asset-audit\] placeholder: \([a-z0-9_]*\) .*/\1/p' "$log"
    sed -n 's/.*\[asset-audit\] requested \([a-z0-9_]*\)$/\1/p' "$log" | grep -v '^icon_' || true
  } | sort -u > "$names"
  while IFS= read -r name; do
    in_manifest "$name" && continue
    echo "code requests '$name' but docs/asset-manifest.tsv does not name it" >&2
    drift=$((drift + 1))
  done < "$names"
  [ $drift -eq 0 ] && echo "asset names: code and brief agree" && exit 0
  echo "$drift name(s) out of sync — update the brief and the manifest together" >&2
  exit 1
fi

present() {  # present <folder> <name-or-glob>
  local dir="$ASSETS/$1" name="$2"
  compgen -G "$dir/$name.glb" > /dev/null && return 0
  compgen -G "$dir/$name.tscn" > /dev/null && return 0
  return 1
}

wired_have=0; wired_want=0; later_have=0; later_want=0
missing_wired=()

while IFS=$'\t' read -r asset folder wired; do
  case "$asset" in ''|'#'*|asset) continue ;; esac
  if [ "$wired" = yes ]; then
    wired_want=$((wired_want + 1))
    if present "$folder" "$asset"; then wired_have=$((wired_have + 1))
    else missing_wired+=("$folder/$asset"); fi
  else
    later_want=$((later_want + 1))
    present "$folder" "$asset" && later_have=$((later_have + 1)) || true
  fi
done < "$MANIFEST"

echo "wired now : $wired_have/$wired_want delivered  (renders as soon as the file lands)"
echo "later     : $later_have/$later_want delivered  (waits on M3-M5 code)"

if [ ${#missing_wired[@]} -gt 0 ] && [ "${1:-}" = --list ]; then
  echo
  echo "still on placeholders:"
  printf '  %s\n' "${missing_wired[@]}"
fi

# Anything on disk the manifest does not name — usually a typo in a filename,
# which otherwise fails silently as "design hasn't shipped it yet".
stray=0
while IFS= read -r file; do
  name=$(basename "$file"); name=${name%.*}
  # Numbered stage modules are covered by their glob row, so check both forms.
  stem=$(echo "$name" | sed 's/_s[0-9]\{1,2\}$/_s*/')
  awk -F'\t' -v a="$name" -v b="$stem" \
    '$1==a || $1==b { found=1 } END { exit !found }' "$MANIFEST" && continue
  [ $stray -eq 0 ] && echo && echo "on disk but not in the manifest (check the filename):"
  echo "  $file"
  stray=$((stray + 1))
done < <(find "$ASSETS" -name '*.glb' -o -name '*.tscn' | sort)

exit 0
