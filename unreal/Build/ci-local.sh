#!/bin/zsh
# The INT pre-merge set (PROGRAMME.md §6.8) in one script — what the nightly self-hosted lane
# runs, and what INT runs on a rebased branch before merging it to unreal/main.
#
#   unreal/Build/ci-local.sh                         # everything, in order, stop at the first failure
#   unreal/Build/ci-local.sh --skip smoke            # skip a step (repeatable): layering ownership schema plan-status build tests smoke
#   unreal/Build/ci-local.sh --filter DF.Unit        # test filter (default DF.Unit+DF.Content)
#   unreal/Build/ci-local.sh --client-count 2        # smoke with two clients
#   unreal/Build/ci-local.sh --ws 04 --base main     # ownership as a workstream against another base (default INT vs origin/unreal/main)
#
# Steps: layering-check.py · ownership-check.py · validate-content-json.py · plan-status.py --check ·
# Build.sh DeepFieldEditor Mac Development (-WaitMutex: UBT serialises builds machine-wide) ·
# editor-lock.sh test.sh <filter> · editor-lock.sh smoke-listen.sh (the lock is held for the whole smoke).
# Prints a summary table on exit, whatever happened. Exit 1 on the first failing step.
# Takes ~15 min on the 8 GB Mac (a first build ~6 min, tests ~2 min, smoke ~3 min).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJECT="$REPO/unreal/DeepField/DeepField.uproject"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
FILTER="DF.Unit+DF.Content"
WS="INT"
BASE="origin/unreal/main"
CLIENTS=1
SKIP=()
while [ $# -gt 0 ]; do
  case "$1" in
    --skip) SKIP+=("$2"); shift 2 ;;
    --filter) FILTER="$2"; shift 2 ;;
    --ws) WS="$2"; shift 2 ;;
    --base) BASE="$2"; shift 2 ;;
    --client-count) CLIENTS="$2"; shift 2 ;;
    -h|--help) sed -n '2,16p' "$0"; exit 0 ;;
    *) echo "ci-local: unknown option $1" >&2; exit 2 ;;
  esac
done

STEPS=(layering ownership schema plan-status build tests smoke)
typeset -A RESULT SECONDS_OF
for s in "${STEPS[@]}"; do RESULT[$s]="not run"; SECONDS_OF[$s]=""; done
START=$(date +%s)
FAILED_STEP=""

summary() {
  echo
  echo "ci-local summary ($(( $(date +%s) - START ))s total)"
  printf '  %-12s %-9s %s\n' step result seconds
  for s in "${STEPS[@]}"; do printf '  %-12s %-9s %s\n' "$s" "${RESULT[$s]}" "${SECONDS_OF[$s]}"; done
  if [ -n "$FAILED_STEP" ]; then echo "ci-local: FAILED at $FAILED_STEP"; else echo "ci-local: OK"; fi
}
trap summary EXIT

skipped() { for s in "${SKIP[@]:-}"; do [ "$s" = "$1" ] && return 0; done; return 1; }

# run <step> <command...>: records the result, prints a header, stops the script on failure.
run() {
  local step="$1"; shift
  if skipped "$step"; then RESULT[$step]="skipped"; echo "== $step: skipped"; return 0; fi
  echo; echo "== $step: $*"
  local t0=$(date +%s)
  "$@"; local rc=$?
  SECONDS_OF[$step]=$(( $(date +%s) - t0 ))
  if [ $rc -ne 0 ]; then RESULT[$step]="FAIL"; FAILED_STEP="$step"; exit 1; fi
  RESULT[$step]="ok"
}

build() {
  local log="$REPO/unreal/DeepField/Saved/Logs/ci-build.log"; mkdir -p "$(dirname "$log")"
  "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DeepFieldEditor Mac Development \
    -Project="$PROJECT" -WaitMutex -NoHotReload > "$log" 2>&1
  local rc=$?
  grep -E " error |Result:|Total execution time" "$log" | tail -20   # never the whole log
  [ $rc -eq 0 ] || echo "build: FAILED (rc $rc, $log)"
  return $rc
}

run layering    python3 "$HERE/layering-check.py"
run ownership   python3 "$HERE/ownership-check.py" --ws "$WS" --base "$BASE"
run schema      python3 "$HERE/validate-content-json.py"
run plan-status python3 "$HERE/plan-status.py" --check
run build       build
run tests       "$HERE/editor-lock.sh" "$HERE/test.sh" "$FILTER"
run smoke       "$HERE/editor-lock.sh" "$HERE/smoke-listen.sh" -client-count "$CLIENTS"
