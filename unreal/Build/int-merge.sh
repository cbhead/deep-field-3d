#!/bin/zsh
# INT: land one workstream branch on unreal/main (PROGRAMME.md §6.8).
#   unreal/Build/int-merge.sh <branch> [--ws NN] [--no-smoke] [--dry-run] [--resume]
# Steps: fetch → rebase <branch> onto origin/unreal/main (in a temp worktree so the clone stays
# clean) → layering + ownership + schema + STATUS regeneration check → editor build → DF.Unit+
# DF.Content tests → listen smoke → land (re-fetch; if main moved by ledger/doc-only commits,
# rebase again and push without re-verifying; if code moved, verify again) → push.
# --resume reuses the temp worktree a previous run left behind and skips straight to landing
# when nothing but ledger/doc files moved on main since it was verified.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
BRANCH="${1:?branch}"; shift
WS="INT"; SMOKE=1; DRY=0; RESUME=0
while [ $# -gt 0 ]; do case "$1" in --ws) WS="$2"; shift 2;; --no-smoke) SMOKE=0; shift;; --dry-run) DRY=1; shift;; --resume) RESUME=1; shift;; *) echo "unknown arg $1"; exit 2;; esac; done
TMP="/Volumes/Toshiba/Deepfield-Unreal/int-merge-$(echo "$BRANCH" | tr '/' '-')"
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
fail() { echo "int-merge: FAILED at: $*  (worktree left at $TMP)"; exit 1; }
# Paths whose change on main never invalidates a verification (no code, config, content or tooling).
DOC_ONLY_RE='^(unreal/PLAN/|docs/|README\.md$|.*\.md$)'
main_moved_code_only_docs() {   # 0 = only doc/ledger files differ between $1 and $2
  local files; files=$(git diff --name-only "$1" "$2")
  [ -z "$files" ] && return 0
  echo "$files" | grep -vE "$DOC_ONLY_RE" >/dev/null && return 1 || return 0
}

verify() {
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
}

step "fetch"
git -C "$REPO" fetch -q origin || fail fetch
if [ "$RESUME" = 1 ] && [ -d "$TMP" ]; then
  cd "$TMP" || fail cd
  VERIFIED_BASE=$(git merge-base HEAD origin/unreal/main)
  echo "resuming from $TMP (verified against $(git rev-parse --short "$VERIFIED_BASE"))"
else
  [ -d "$TMP" ] && git -C "$REPO" worktree remove --force "$TMP" 2>/dev/null
  git -C "$REPO" worktree add -q "$TMP" "origin/$BRANCH" || fail "worktree add (does origin/$BRANCH exist?)"
  cd "$TMP" || fail cd
  git checkout -q -B "int/$BRANCH" "origin/$BRANCH"
  step "rebase onto origin/unreal/main"
  git rebase -q origin/unreal/main || fail "rebase (resolve in $TMP, then rerun with --resume)"
  git log --oneline origin/unreal/main..HEAD
  verify
  VERIFIED_BASE=$(git merge-base HEAD origin/unreal/main)
fi

if [ "$DRY" = 1 ]; then echo "dry run: not pushing. Worktree at $TMP"; exit 0; fi

step "land"
for attempt in 1 2 3; do
  git -C "$REPO" fetch -q origin || fail fetch
  if [ "$(git rev-parse origin/unreal/main)" != "$VERIFIED_BASE" ]; then
    if main_moved_code_only_docs "$VERIFIED_BASE" origin/unreal/main; then
      echo "main moved by ledger/doc-only commits since verification; rebasing without re-verifying"
      git rebase -q origin/unreal/main || fail "rebase onto moved main (resolve in $TMP, rerun with --resume)"
    else
      echo "main moved with code/content changes since verification; verifying again"
      git rebase -q origin/unreal/main || fail "rebase onto moved main (resolve in $TMP, rerun with --resume)"
      verify
    fi
    VERIFIED_BASE=$(git rev-parse origin/unreal/main)
  fi
  git push -q origin "HEAD:$BRANCH" --force-with-lease || fail "push branch"
  if git push -q origin "HEAD:unreal/main" 2>/dev/null; then
    cd "$REPO" && git pull -q --rebase origin unreal/main 2>/dev/null
    git -C "$REPO" worktree remove --force "$TMP"
    echo "int-merge: landed $BRANCH on unreal/main"
    exit 0
  fi
  echo "push to unreal/main rejected (attempt $attempt); refetching"
done
fail "push unreal/main after 3 attempts (rerun with --resume)"
