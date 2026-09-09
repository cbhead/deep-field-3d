#!/usr/bin/env bash
# Every map, against the rules in docs/MAP-AUTHORING.md §4.
#
# This is a ratchet, not a gate. All three maps fail the rules today — that is
# the finding, not a bug in the probe — so each map carries a baseline in
# docs/map-validation-baseline.tsv and CI fails only when a map gets *worse*.
# Lower a baseline in the same commit that improves a map and the gain is
# locked in; the alternative is a red build nobody can turn green, which gets
# switched off within a week.
#
# The rules that matter most here are the coverage ones (§4.4–§4.7). Nothing
# else in this repo checks them, and a map can pass every gate, every smoke run
# and every screenshot review while having an enemy lane no tower can reach.
set -euo pipefail
cd "$(dirname "$0")/.."

BASELINE=docs/map-validation-baseline.tsv
MAPS=${MAPS:-"foundry switchyard spire"}
OUT=$(mktemp -d)
status=0

for MAP in $MAPS; do
  # The probe writes its verdict to a file and may still abort in Godot's
  # teardown, so the exit code is not the signal — the file is.
  ./play --headless -- --shot "$MAP" "$OUT/$MAP.txt" validate > "$OUT/$MAP.log" 2>&1 || true

  if [ ! -s "$OUT/$MAP.txt" ]; then
    echo "$MAP: PROBE PRODUCED NOTHING — see $OUT/$MAP.log"
    status=1
    continue
  fi

  count=$(grep -m1 '^violations=' "$OUT/$MAP.txt" | cut -d= -f2)
  max=$(awk -F'\t' -v m="$MAP" '$1 == m { print $2; exit }' "$BASELINE")
  : "${max:=0}"

  # Everything above the violations= line is the per-rule summary; everything
  # below it is the individual violations.
  sed -n "2,/^violations=/p" "$OUT/$MAP.txt" | sed '/^violations=/d' | sed 's/^/  /'

  if [ "$count" -gt "$max" ]; then
    echo "$MAP: FAIL — $count violation(s), baseline allows $max"
    sed -n "/^violations=/,\$p" "$OUT/$MAP.txt" | tail -n +2 | head -10 | sed 's/^/    /'
    status=1
  elif [ "$count" -lt "$max" ]; then
    echo "$MAP: IMPROVED — $count violation(s), baseline allows $max. Lower it to $count in $BASELINE."
  else
    echo "$MAP: holding at $count violation(s)"
  fi
  echo
done

exit $status
