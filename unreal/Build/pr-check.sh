#!/bin/zsh
# What a workstream runs before opening a PR (PROGRAMME.md §6.5 session-end checklist, §7):
# layering, ownership for YOUR workstream against origin/main, the content JSON schemas,
# the editor build, then the landing gate (DF_GATE_FILTER in test.sh) under the editor lock — the same
# tests the branch will be landed on, so a PR never learns about a red suite at landing time.
#
#   unreal/Build/pr-check.sh                              # ws from the branch name (ws/NN-slug/topic), the landing gate
#   unreal/Build/pr-check.sh --ws 04 --filter DF.Unit.Towers   # iterate on one area: that filter only, reported PARTIAL
#   unreal/Build/pr-check.sh --no-build                   # the editor is already built for this tree
#   unreal/Build/pr-check.sh --no-tests                   # the python checks only (seconds)
#   unreal/Build/pr-check.sh --smoke                      # also run the listen-host smoke (DF.Net stand-in)
# Exit 1 at the first failure. The same checks run on GitHub for the PR (.github/workflows/unreal-*.yml).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJECT="$REPO/unreal/DeepField/DeepField.uproject"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
WS=""
FILTER=""
BASE="origin/main"
BUILD=1; TESTS=1; SMOKE=0
while [ $# -gt 0 ]; do
  case "$1" in
    --ws) WS="$2"; shift 2 ;;
    --filter) FILTER="$2"; shift 2 ;;
    --base) BASE="$2"; shift 2 ;;
    --no-build) BUILD=0; shift ;;
    --no-tests) TESTS=0; shift ;;
    --smoke) SMOKE=1; shift ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) echo "pr-check: unknown option $1" >&2; exit 2 ;;
  esac
done
if [ -z "$WS" ]; then
  # ws/04-towers/rig -> 04 ; ws/10a-map-foundry/x -> 10a
  BRANCH="$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
  WS="$(echo "$BRANCH" | sed -nE 's#^ws/([0-9]+[a-z]?)-.*#\1#p')"
  if [ -z "$WS" ]; then echo "pr-check: pass --ws NN (branch '$BRANCH' is not ws/NN-slug/topic)" >&2; exit 2; fi
fi
# No filter = the landing gate: test.sh's own default (DF_GATE_FILTER), so pr-check, int-merge and
# ci-local run the same set and there is still exactly one definition of it. A --filter run is for
# iterating and is never reported as OK: an omitted suite has to be visible (CONTRACTS/ci.md).
PARTIAL=""
if [ -n "$FILTER" ]; then PARTIAL="tests: $FILTER only"; fi
if [ "$TESTS" -eq 0 ]; then PARTIAL="no tests"; fi

step() { echo; echo "== $1"; }
fail() { echo "pr-check: FAILED at $1"; exit 1; }

step "layering";  python3 "$HERE/layering-check.py" || fail layering
step "ownership (WS-$WS vs $BASE)"; python3 "$HERE/ownership-check.py" --ws "$WS" --base "$BASE" || fail ownership
step "content json schemas"; python3 "$HERE/validate-content-json.py" || fail schema
if [ "$BUILD" -eq 1 ]; then
  step "build DeepFieldEditor Mac Development (-WaitMutex)"
  LOG="$REPO/unreal/DeepField/Saved/Logs/pr-build.log"; mkdir -p "$(dirname "$LOG")"
  "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DeepFieldEditor Mac Development -Project="$PROJECT" -WaitMutex -NoHotReload > "$LOG" 2>&1
  RC=$?; grep -E " error |Result:|Total execution time" "$LOG" | tail -20
  [ $RC -eq 0 ] || fail "build (see $LOG)"
fi
if [ "$TESTS" -eq 1 ]; then
  if [ -n "$FILTER" ]; then
    step "tests $FILTER (under the editor lock; not the landing gate)"
    "$HERE/editor-lock.sh" "$HERE/test.sh" "$FILTER" || fail tests
  else
    step "tests: the landing gate (DF_GATE_FILTER, under the editor lock)"
    "$HERE/editor-lock.sh" "$HERE/test.sh" || fail tests
  fi
fi
if [ "$SMOKE" -eq 1 ]; then
  step "listen-host smoke (under the editor lock)"
  "$HERE/editor-lock.sh" "$HERE/smoke-listen.sh" || fail smoke
fi
if [ -n "$PARTIAL" ]; then
  echo; echo "pr-check: PARTIAL (WS-$WS, $PARTIAL) — run it without --filter/--no-tests before opening the PR"
else
  echo; echo "pr-check: OK (WS-$WS) — open the PR titled [WS-$WS] ... listing contracts touched and the tests above"
fi
