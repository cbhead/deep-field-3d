#!/bin/zsh
# INT: land one workstream branch on unreal/main (PROGRAMME.md §6.8).
#   unreal/Build/int-merge.sh <branch> [--ws NN] [--no-smoke] [--dry-run]
# Steps: fetch → rebase <branch> onto origin/unreal/main (in a temp worktree so the clone stays
# clean) → layering + ownership + schema + STATUS regeneration check → editor build → DF.Unit+
# DF.Content tests → listen smoke → fast-forward unreal/main → push (branch and main).
# Stops at the first failure and leaves the temp worktree for inspection.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
BRANCH="${1:?branch}"; shift
WS="INT"; SMOKE=1; DRY=0
while [ $# -gt 0 ]; do case "$1" in --ws) WS="$2"; shift 2;; --no-smoke) SMOKE=0; shift;; --dry-run) DRY=1; shift;; *) echo "unknown arg $1"; exit 2;; esac; done
TMP="/Volumes/Toshiba/Deepfield-Unreal/int-merge-$(echo "$BRANCH" | tr '/' '-')"
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
fail() { echo "int-merge: FAILED at: $*  (worktree left at $TMP)"; exit 1; }

step "fetch"
git -C "$REPO" fetch -q origin || fail fetch
[ -d "$TMP" ] && git -C "$REPO" worktree remove --force "$TMP" 2>/dev/null
git -C "$REPO" worktree add -q "$TMP" "origin/$BRANCH" || fail "worktree add (does origin/$BRANCH exist?)"
cd "$TMP" || fail cd
git checkout -q -B "int/$BRANCH" "origin/$BRANCH"

step "rebase onto origin/unreal/main"
git rebase -q origin/unreal/main || fail "rebase (resolve in $TMP, then rerun)"
git log --oneline origin/unreal/main..HEAD

step "checks"
python3 unreal/Build/layering-check.py || fail layering
python3 unreal/Build/ownership-check.py --ws "$WS" --base origin/unreal/main || fail ownership
python3 unreal/Build/validate-content-json.py > /tmp/int-schema.log || { cat /tmp/int-schema.log; fail schema; }
cp unreal/PLAN/STATUS.md /tmp/int-status-before.md; python3 unreal/Build/plan-status.py >/dev/null
if ! diff -q <(grep -v '^Generated ' /tmp/int-status-before.md) <(grep -v '^Generated ' unreal/PLAN/STATUS.md) >/dev/null; then
  echo "STATUS.md was stale; regenerated and amended into the last commit"
  git add unreal/PLAN/STATUS.md && git commit -q --amend --no-edit
else
  git checkout -q unreal/PLAN/STATUS.md
fi

step "build"
"$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DeepFieldEditor Mac Development -Project="$TMP/unreal/DeepField/DeepField.uproject" -WaitMutex -NoHotReload 2>&1 | grep -E " error |Result:" | tee /tmp/int-build.log
grep -q "Result: Succeeded" /tmp/int-build.log || fail build

step "tests"
unreal/Build/editor-lock.sh unreal/Build/test.sh DF.Unit+DF.Content || fail tests

if [ "$SMOKE" = 1 ]; then
  step "smoke"
  unreal/Build/editor-lock.sh unreal/Build/smoke-listen.sh || fail smoke
fi

if [ "$DRY" = 1 ]; then echo "dry run: not pushing. Worktree at $TMP"; exit 0; fi

step "land"
git push -q origin "HEAD:$BRANCH" --force-with-lease || fail "push branch"
git push -q origin "HEAD:unreal/main" || fail "push unreal/main (someone landed first — rerun)"
cd "$REPO" && git pull -q --rebase origin unreal/main 2>/dev/null
git -C "$REPO" worktree remove --force "$TMP"
echo "int-merge: landed $BRANCH on unreal/main"
