#!/usr/bin/env bash
# Design's icons are stroke line-art using `currentColor` — they are meant to
# take the colour of whatever draws them. Godot's SVG rasteriser has no CSS
# context, so `currentColor` resolves to black and every icon lands as a black
# silhouette on a black panel.
#
# Rasterise them white instead and let UiTheme tint at draw time, which is what
# `currentColor` was asking for in the first place. Idempotent: run it again
# after any re-delivery.
set -euo pipefail
cd "$(dirname "$0")/.."

count=0
for f in game/assets/ui/icon_*.svg; do
  [ -e "$f" ] || continue
  grep -q 'currentColor' "$f" || continue
  # BSD and GNU sed both accept this form.
  sed -i.bak 's/currentColor/#ffffff/g' "$f" && rm -f "$f.bak"
  count=$((count + 1))
done

echo "prepared $count icon(s) for runtime tinting"
