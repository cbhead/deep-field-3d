#!/bin/zsh
# INT: land one workstream branch on unreal/main (PROGRAMME.md §6.8).
#   unreal/Build/int-merge.sh <branch> [--ws NN] [--no-smoke] [--dry-run] [--resume]
# Steps: fetch → rebase <branch> onto origin/unreal/main (in a temp worktree so the clone stays
# clean) → layering + ownership + schema checks → editor build → DF.Unit+DF.Content tests → listen
# smoke → land (re-fetch; if main moved by ledger/doc-only commits, rebase again and push without
# re-verifying; if code moved, verify again) → STATUS.md regenerated as its own commit → push.
# --resume reuses the temp worktree a previous run left behind. The base it was verified against
# is kept in <worktree>.verified-base, so a hand-rebased worktree is verified again, never trusted.
# STATUS.md is generated: a rebase conflict on it alone is resolved by regenerating it; any other
# conflict is left for a human (resolve in the worktree, rerun with --resume).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
BRANCH="${1:?branch}"; shift
WS="INT"; SMOKE=1; DRY=0; RESUME=0
while [ $# -gt 0 ]; do case "$1" in --ws) WS="$2"; shift 2;; --no-smoke) SMOKE=0; shift;; --dry-run) DRY=1; shift;; --resume) RESUME=1; shift;; *) echo "unknown arg $1"; exit 2;; esac; done
TMP="/Volumes/Toshiba/Deepfield-Unreal/int-merge-$(echo "$BRANCH" | tr '/' '-')"
VB_FILE="$TMP.verified-base"
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
fail() { echo "int-merge: FAILED at: $*  (worktree left at $TMP)"; exit 1; }
# Paths whose change on main never invalidates a verification: nothing the build/test verdict
# depends on (ledger, docs, the ledger scripts and this script).
DOC_ONLY_RE='^(unreal/PLAN/|docs/|README\.md$|.*\.md$|unreal/Build/(int-merge\.sh|plan-[a-z]+\.py|plan_lib\.py)$)'
main_moved_code_only_docs() {   # 0 = only doc/ledger files differ between $1 and $2
  local files; files=$(git diff --name-only "$1" "$2")
  [ -z "$files" ] && return 0
  echo "$files" | grep -vE "$DOC_ONLY_RE" >/dev/null && return 1 || return 0
}
in_rebase() { [ -d "$(git rev-parse --git-path rebase-merge)" ] || [ -d "$(git rev-parse --git-path rebase-apply)" ]; }
# Rebase onto origin/unreal/main (or finish one in progress), regenerating STATUS.md whenever it is
# the only conflicted file.
rebase_onto_main() {
  in_rebase || git rebase -q origin/unreal/main >/dev/null 2>&1 || true
  while in_rebase; do
    local conflicted; conflicted=$(git diff --name-only --diff-filter=U)
    if [ "$conflicted" = "unreal/PLAN/STATUS.md" ]; then
      echo "STATUS.md conflict: regenerated"
      python3 unreal/Build/plan-status.py >/dev/null || return 1
      git add unreal/PLAN/STATUS.md
      if git diff --cached --quiet; then GIT_EDITOR=true git rebase --skip >/dev/null 2>&1 || return 1
      else GIT_EDITOR=true git rebase --continue >/dev/null 2>&1 || return 1; fi
    else
      echo "conflicts left for a human:"; echo "$conflicted"; return 1
    fi
  done
  return 0
}
regen_status_commit() {   # STATUS.md for the state about to land, as its own commit if it moved
  python3 unreal/Build/plan-status.py >/dev/null
  if ! git diff --quiet -- unreal/PLAN/STATUS.md; then
    git add unreal/PLAN/STATUS.md && git commit -qm "PLAN: STATUS regenerated after landing $BRANCH"
    echo "STATUS.md regenerated (own commit)"
  fi
}
record_verified() { VERIFIED_BASE=$(git rev-parse origin/unreal/main); echo "$VERIFIED_BASE" > "$VB_FILE"; }

verify() {
  step "checks"
  python3 unreal/Build/layering-check.py || fail layering
  python3 unreal/Build/ownership-check.py --ws "$WS" --base origin/unreal/main || fail ownership
  python3 unreal/Build/validate-content-json.py > /tmp/int-schema.log || { cat /tmp/int-schema.log; fail schema; }
  step "build"
  "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DeepFieldEditor Mac Development -Project="$TMP/unreal/DeepField/DeepField.uproject" -WaitMutex -NoHotReload 2>&1 | grep -E " error |Result:" | tee /tmp/int-build.log
  grep -q "Result: Succeeded" /tmp/int-build.log || fail build
  step "tests"
  unreal/Build/editor-lock.sh unreal/Build/test.sh DF.Unit+DF.Content || fail tests
  if [ "$SMOKE" = 1 ]; then
    step "smoke"
    unreal/Build/editor-lock.sh unreal/Build/smoke-listen.sh || fail smoke
  fi
  record_verified
}

step "fetch"
git -C "$REPO" fetch -q origin || fail fetch
if [ "$RESUME" = 1 ] && [ -d "$TMP" ]; then
  cd "$TMP" || fail cd
  if in_rebase; then echo "finishing the rebase left in $TMP"; rebase_onto_main || fail "rebase (resolve in $TMP, then rerun with --resume)"; fi
  VERIFIED_BASE=$(cat "$VB_FILE" 2>/dev/null || true)
  if [ -n "$VERIFIED_BASE" ] && git merge-base --is-ancestor "$VERIFIED_BASE" HEAD 2>/dev/null; then
    echo "resuming from $TMP (verified against $(git rev-parse --short "$VERIFIED_BASE"))"
  else
    echo "resuming from $TMP with no verification record; verifying"
    rebase_onto_main || fail "rebase (resolve in $TMP, then rerun with --resume)"
    git log --oneline origin/unreal/main..HEAD
    verify
  fi
else
  [ -d "$TMP" ] && git -C "$REPO" worktree remove --force "$TMP" 2>/dev/null
  rm -f "$VB_FILE"
  git -C "$REPO" worktree add -q "$TMP" "origin/$BRANCH" || fail "worktree add (does origin/$BRANCH exist?)"
  cd "$TMP" || fail cd
  git checkout -q -B "int/$BRANCH" "origin/$BRANCH"
  step "rebase onto origin/unreal/main"
  rebase_onto_main || fail "rebase (resolve in $TMP, then rerun with --resume)"
  git log --oneline origin/unreal/main..HEAD
  verify
fi

if [ "$DRY" = 1 ]; then echo "dry run: not pushing. Worktree at $TMP"; exit 0; fi

step "land"
for attempt in 1 2 3; do
  git -C "$REPO" fetch -q origin || fail fetch
  if [ "$(git rev-parse origin/unreal/main)" != "$VERIFIED_BASE" ]; then
    if main_moved_code_only_docs "$VERIFIED_BASE" origin/unreal/main; then
      echo "main moved by ledger/doc-only commits since verification; rebasing without re-verifying"
      rebase_onto_main || fail "rebase onto moved main (resolve in $TMP, rerun with --resume)"
      record_verified
    else
      echo "main moved with code/content changes since verification; verifying again"
      rebase_onto_main || fail "rebase onto moved main (resolve in $TMP, rerun with --resume)"
      verify
    fi
  fi
  regen_status_commit
  git push -q origin "HEAD:$BRANCH" --force-with-lease || fail "push branch (did its author push meanwhile?)"
  if git push -q origin "HEAD:unreal/main" 2>/dev/null; then
    cd "$REPO"
    if git diff --quiet && git diff --cached --quiet && ! in_rebase; then git pull -q --rebase origin unreal/main 2>/dev/null || true; else echo "clone has local changes; not pulling it"; fi
    git worktree remove --force "$TMP"; rm -f "$VB_FILE"
    echo "int-merge: landed $BRANCH on unreal/main"
    exit 0
  fi
  echo "push to unreal/main rejected (attempt $attempt); refetching"
done
fail "push unreal/main after 3 attempts (rerun with --resume)"
