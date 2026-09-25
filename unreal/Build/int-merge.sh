#!/bin/zsh
# INT: land one workstream branch on unreal/main (PROGRAMME.md §6.8).
#   unreal/Build/int-merge.sh <branch> [--ws NN] [--no-smoke] [--dry-run] [--resume]
# Steps: fetch → rebase <branch> onto origin/unreal/main in the persistent verify worktree → layering +
# ownership + schema checks → editor build (incremental: the worktree keeps its Intermediate) → DF.Unit+
# DF.Content tests → listen smoke → land (re-fetch; ledger/doc-only movement on main = re-run the checks,
# rebase and push without rebuilding; code movement = verify again) → STATUS.md regenerated as its own
# commit → push the branch (lease = the tip this landing took) → push unreal/main.
#
# One verify worktree, /Volumes/Toshiba/Deepfield-Unreal/int-verify, serves every landing so builds are
# incremental instead of 15-minute fresh builds; a mkdir lock serialises landings. Before starting UBT the
# landing yields while any other UnrealBuildTool is running or queued (agent builds are the critical path;
# UBT's -WaitMutex is not FIFO and a landing must never jump them) — INT_YIELD_MAX seconds at most.
# --resume continues the landing the worktree currently holds (same branch, even mid-rebase). What was
# verified is recorded in int-verify.verified-base as "<branch> <base> <head> <smoke> <ws>": --resume trusts
# it only when the worktree HEAD is exactly that head; anything else (a finished rebase, a hand-added
# commit, a wider smoke/ws setting) is verified again. STATUS.md is generated: a rebase conflict on it alone
# is resolved by regenerating it; any other conflict is left for a human (resolve and stage in the
# worktree, then rerun with --resume — the script finishes the rebase and verifies the result).
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
BRANCH="${1:?branch}"; shift
WS="INT"; SMOKE=1; DRY=0; RESUME=0
while [ $# -gt 0 ]; do case "$1" in --ws) WS="$2"; shift 2;; --no-smoke) SMOKE=0; shift;; --dry-run) DRY=1; shift;; --resume) RESUME=1; shift;; *) echo "unknown arg $1"; exit 2;; esac; done
WT="${INT_VERIFY_WT:-/Volumes/Toshiba/Deepfield-Unreal/int-verify}"
LOCK="$WT.lock"; VB_FILE="$WT.verified-base"; TIP_FILE="$WT.branch-tip"
YIELD_MAX="${INT_YIELD_MAX:-5400}"; LOCK_MAX="${INT_LOCK_MAX:-7200}"
INT_BRANCH="int/$BRANCH"
step() { printf '\n\033[1m== %s\033[0m\n' "$*"; }
fail() { echo "int-merge: FAILED at: $*  (worktree left at $WT on $INT_BRANCH)"; exit 1; }

# --- one landing at a time ---------------------------------------------------------------------
HELD=0
release_lock() { [ "$HELD" = 1 ] && rm -rf "$LOCK"; }
trap release_lock EXIT
trap 'exit 143' TERM INT HUP          # zsh runs no EXIT trap on an untrapped signal; via exit it does
waited=0
while ! mkdir "$LOCK" 2>/dev/null; do
  pid=$(cat "$LOCK/pid" 2>/dev/null || echo 0)
  age=$(( $(date +%s) - $(stat -f %m "$LOCK" 2>/dev/null || date +%s) ))
  # A lock with no pid yet is a holder between mkdir and its pid write — unless it is old (crashed there).
  if { [ "$pid" = 0 ] && [ "$age" -gt 60 ]; } || { [ "$pid" != 0 ] && ! kill -0 "$pid" 2>/dev/null; }; then
    echo "stale landing lock (pid $pid, ${age}s old); taking it"; rm -rf "$LOCK"; continue
  fi
  [ "$waited" -ge "$LOCK_MAX" ] && { echo "int-merge: another landing (pid $pid) has held $LOCK for ${LOCK_MAX}s"; exit 1; }
  [ "$waited" = 0 ] && echo "waiting for the landing in progress (pid $pid, $(cat "$LOCK/branch" 2>/dev/null))"
  sleep 30; waited=$((waited+30))
done
HELD=1; echo $$ > "$LOCK/pid"; echo "$BRANCH" > "$LOCK/branch"

# Paths whose change on main never invalidates a BUILD/TEST verdict (ledger, docs, the ledger scripts and
# this script). The cheap checks (layering, ownership, schema) are re-run on any movement: OWNERSHIP.md is
# among these paths and the ownership verdict depends on it.
DOC_ONLY_RE='^(unreal/PLAN/|docs/|README\.md$|.*\.md$|unreal/Build/(int-merge\.sh|plan-[a-z]+\.py|plan_lib\.py)$)'
main_moved_code_only_docs() {   # 0 = only doc/ledger files differ between $1 and $2
  local files; files=$(git diff --name-only "$1" "$2")
  [ -z "$files" ] && return 0
  echo "$files" | grep -vE "$DOC_ONLY_RE" >/dev/null && return 1 || return 0
}
in_rebase() { [ -d "$(git rev-parse --git-path rebase-merge)" ] || [ -d "$(git rev-parse --git-path rebase-apply)" ]; }
current_branch() {   # the branch even mid-rebase, when HEAD is detached and only the rebase state names it
  local hn; hn=$(cat "$(git rev-parse --git-path rebase-merge/head-name)" 2>/dev/null || cat "$(git rev-parse --git-path rebase-apply/head-name)" 2>/dev/null || git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "?")
  echo "${hn#refs/heads/}"
}
# Rebase onto origin/unreal/main, or finish one already in progress, until HEAD is on top of main
# (a fetch may have moved main while a human was resolving, so one pass is not enough). STATUS.md is
# generated: a conflict on it alone is resolved by regenerating. Anything unstaged or untracked means
# we cannot tell what the human intended — stopping there is the only safe move, because `--skip` on a
# merely-unstaged resolution silently drops the author's commit.
rebase_onto_main() {
  local guard=0 before
  while :; do
    if ! in_rebase; then
      git merge-base --is-ancestor origin/unreal/main HEAD && return 0
      git rebase -q origin/unreal/main >/dev/null 2>&1 || true
      if ! in_rebase; then
        git merge-base --is-ancestor origin/unreal/main HEAD && return 0
        echo "git rebase refused to start (or stopped without conflicts):"; git status --short | head -5; return 1
      fi
    fi
    guard=$((guard+1))
    [ "$guard" -gt 200 ] && { echo "rebase: 200 steps without finishing — stopping"; return 1; }

    local conflicted; conflicted=$(git diff --name-only --diff-filter=U)
    if [ -n "$conflicted" ]; then
      # Generated or append-only ledger files resolve themselves; anything else is a human's call.
      if echo "$conflicted" | grep -qvE '^unreal/PLAN/(workstreams/ws-[0-9a-z-]+\.md|STATUS\.md)$'; then
        echo "conflicts left for a human (resolve and STAGE them, then rerun with --resume):"; echo "$conflicted"; return 1
      fi
      echo "$conflicted" | while IFS= read -r F; do
        case "$F" in
          unreal/PLAN/STATUS.md) echo "STATUS.md conflict: regenerated"; python3 unreal/Build/plan-status.py >/dev/null && git add "$F";;
          *) python3 "$HERE/merge-ws-log.py" "$F" || exit 1;;   # $HERE, not the branch: a branch older than the tool does not carry it
        esac
      done || { echo "could not resolve a ledger conflict automatically"; return 1; }
    fi

    if ! git diff --quiet || [ -n "$(git ls-files --others --exclude-standard)" ]; then
      echo "unstaged or untracked changes in $WT — stage what belongs in the commit, discard the rest, then rerun with --resume:"
      git status --short | head -10; return 1
    fi

    # `git rebase --continue` exits non-zero when it commits successfully and then stops at the NEXT
    # conflict, which is the normal case here and not a failure — the loop re-inspects. Only a step
    # that changes nothing at all is stuck.
    before=$(git rev-parse HEAD 2>/dev/null)
    if git diff --cached --quiet; then
      echo "a commit became empty after the rebase; skipping it"
      GIT_EDITOR=true git rebase --skip >/dev/null 2>&1
    else
      GIT_EDITOR=true git rebase --continue >/dev/null 2>&1
    fi
    if in_rebase && [ "$(git rev-parse HEAD 2>/dev/null)" = "$before" ] && [ -z "$(git diff --name-only --diff-filter=U)" ]; then
      echo "rebase is stuck: nothing conflicted, nothing applied. git says:"; git status | head -12; return 1
    fi
  done
}

regen_status_commit() {   # STATUS.md for the state about to land, as its own commit if it moved
  python3 unreal/Build/plan-status.py >/dev/null
  git diff --quiet -- unreal/PLAN/STATUS.md && return 0
  if diff -q <(git show HEAD:unreal/PLAN/STATUS.md | grep -v '^Generated ') <(grep -v '^Generated ' unreal/PLAN/STATUS.md) >/dev/null; then
    git checkout -q -- unreal/PLAN/STATUS.md; return 0     # only the timestamp moved: not a commit
  fi
  git add unreal/PLAN/STATUS.md && git commit -qm "PLAN: STATUS regenerated after landing $BRANCH"
  echo "STATUS.md regenerated (own commit)"
}
record_verified() {
  # The base is the merge-base, never origin/unreal/main itself: refs are shared by every worktree of
  # the clone and any session's fetch moves them mid-build, which would make "main did not move" a lie.
  VERIFIED_BASE=$(git merge-base HEAD origin/unreal/main); VERIFIED_HEAD=$(git rev-parse HEAD)
  printf '%s %s %s %s %s\n' "$BRANCH" "$VERIFIED_BASE" "$VERIFIED_HEAD" "$SMOKE" "$WS" > "$VB_FILE"
}
record_tip() { printf '%s %s\n' "$BRANCH" "$BRANCH_TIP" > "$TIP_FILE"; }
# Real UBT processes only: dotnet running the dll. pgrep -f alone also matches every monitoring shell
# whose command text contains the string (the other sessions watch the queue exactly that way).
other_ubt_count() { ps -axo pid=,comm=,args= 2>/dev/null | awk -v me=$$ '$1 != me && $2 ~ /dotnet$/ && /UnrealBuildTool\.dll/ {n++} END {print n+0}'; }
yield_to_agent_builds() {   # agent builds are the critical path; a landing waits for a quiet mutex
  local w=0 n; n=$(other_ubt_count)
  [ "$n" = 0 ] && return 0
  echo "yielding: $n other UnrealBuildTool process(es) running or queued (up to ${YIELD_MAX}s)"
  while [ "$(other_ubt_count)" != 0 ] && [ "$w" -lt "$YIELD_MAX" ]; do sleep 30; w=$((w+30)); done
  echo "yielded ${w}s; $(other_ubt_count) other build(s) left"
}

checks() {
  step "checks"
  python3 unreal/Build/layering-check.py || fail layering
  python3 unreal/Build/ownership-check.py --ws "$WS" --base origin/unreal/main || fail ownership
  python3 unreal/Build/validate-content-json.py > /tmp/int-schema.log || { cat /tmp/int-schema.log; fail schema; }
  python3 unreal/Build/check-test-coverage.py || fail "test coverage (a registered suite is outside the gate)"
}
verify() {
  checks
  step "build"
  yield_to_agent_builds
  "$UE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh" DeepFieldEditor Mac Development -Project="$WT/unreal/DeepField/DeepField.uproject" -WaitMutex -NoHotReload 2>&1 | grep -E "error:|error generated|Error:|Fatal|Result:|Total time" | tee /tmp/int-build.log
  if ! grep -q "Result: Succeeded" /tmp/int-build.log; then
    echo "--- the diagnostics, in full (a failure that hides its cause costs a whole build to re-read):"
    grep -E "error:|error generated|Fatal" /tmp/int-build.log | head -20
    fail build
  fi
  step "tests"
  unreal/Build/editor-lock.sh unreal/Build/test.sh || fail tests   # no filter: test.sh owns the gate list
  if [ "$SMOKE" = 1 ]; then
    step "smoke"
    unreal/Build/editor-lock.sh unreal/Build/smoke-listen.sh || fail smoke
  fi
  record_verified
}

step "fetch"
git -C "$REPO" fetch -q origin || fail fetch
if [ ! -d "$WT/.git" ] && [ ! -f "$WT/.git" ]; then
  git -C "$REPO" worktree prune
  git -C "$REPO" worktree add -q --detach "$WT" origin/unreal/main || fail "worktree add $WT"
  echo "created the verify worktree at $WT"
fi
cd "$WT" || fail cd
if [ "$RESUME" = 1 ]; then
  CUR=$(current_branch)
  [ "$CUR" = "$INT_BRANCH" ] || fail "resume: the verify worktree holds '$CUR', not $INT_BRANCH (rerun without --resume)"
  read -r TIP_BRANCH BRANCH_TIP < "$TIP_FILE" 2>/dev/null || { TIP_BRANCH=""; BRANCH_TIP=""; }
  [ "$TIP_BRANCH" = "$BRANCH" ] && [ -n "$BRANCH_TIP" ] || fail "resume: no record of which origin/$BRANCH tip this worktree took (rerun without --resume)"
  if in_rebase; then
    echo "finishing the rebase left in $WT"
    rebase_onto_main || fail "rebase (resolve and stage in $WT, then rerun with --resume)"
  else
    # Nothing in flight: what gets verified must be exactly what gets pushed, so the worktree may not
    # carry uncommitted or untracked work (it would pass the build and never reach unreal/main).
    git diff --quiet && git diff --cached --quiet || fail "uncommitted changes in $WT — commit them (they are then verified) or discard them:
$(git status --short | head -10)"
    [ -z "$(git ls-files --others --exclude-standard)" ] || fail "untracked files in $WT: $(git ls-files --others --exclude-standard | head -5 | paste -sd' ' -)"
    rebase_onto_main || fail "rebase (resolve and stage in $WT, then rerun with --resume)"
  fi
  CUR_TIP=$(git rev-parse "origin/$BRANCH")
  if [ "$CUR_TIP" != "$BRANCH_TIP" ]; then
    if git merge-base --is-ancestor "$CUR_TIP" HEAD; then
      echo "origin/$BRANCH moved to ${CUR_TIP:0:7} and this worktree already contains it; lease updated"
      BRANCH_TIP=$CUR_TIP; record_tip
    else
      fail "origin/$BRANCH moved from ${BRANCH_TIP:0:7} to ${CUR_TIP:0:7} and this worktree does not contain the new commits (its author pushed) — rerun without --resume"
    fi
  fi
  read -r VB_BRANCH VERIFIED_BASE VB_HEAD VB_SMOKE VB_WS < "$VB_FILE" 2>/dev/null || { VB_BRANCH=""; VERIFIED_BASE=""; VB_HEAD=""; VB_SMOKE=""; VB_WS=""; }
  if [ "$VB_BRANCH" = "$BRANCH" ] && [ "$VB_HEAD" = "$(git rev-parse HEAD)" ] && [ "$VB_WS" = "$WS" ] && { [ "$VB_SMOKE" = 1 ] || [ "$SMOKE" = 0 ]; }; then
    echo "resuming $INT_BRANCH: HEAD $(git rev-parse --short HEAD) was verified against $(git rev-parse --short "$VERIFIED_BASE")"
  else
    echo "resuming $INT_BRANCH: no verification record for this HEAD/settings; verifying"
    git log --oneline origin/unreal/main..HEAD
    verify
  fi
else
  rm -f "$VB_FILE" "$TIP_FILE"
  in_rebase && git rebase --abort >/dev/null 2>&1
  git reset -q --hard && git clean -qfd     # tracked + untracked reset; ignored Intermediate/Binaries/Saved stay (incremental build)
  git checkout -q -B "$INT_BRANCH" "origin/$BRANCH" || fail "checkout origin/$BRANCH (does it exist?)"
  BRANCH_TIP=$(git rev-parse "origin/$BRANCH"); record_tip      # the lease for the branch push
  step "rebase onto origin/unreal/main"
  rebase_onto_main || fail "rebase (resolve and stage in $WT, then rerun with --resume)"
  git log --oneline origin/unreal/main..HEAD
  verify
fi

if [ "$DRY" = 1 ]; then echo "dry run: not pushing. Worktree at $WT on $INT_BRANCH"; exit 0; fi

step "land"
for attempt in 1 2 3; do
  git -C "$REPO" fetch -q origin || fail fetch
  if [ "$(git rev-parse origin/unreal/main)" != "$VERIFIED_BASE" ]; then
    if main_moved_code_only_docs "$VERIFIED_BASE" origin/unreal/main; then
      echo "main moved by ledger/doc-only commits since verification; rebasing and re-running the checks, no rebuild"
      rebase_onto_main || fail "rebase onto moved main (resolve and stage in $WT, rerun with --resume)"
      checks
      record_verified
    else
      echo "main moved with code/content changes since verification; verifying again"
      rebase_onto_main || fail "rebase onto moved main (resolve and stage in $WT, rerun with --resume)"
      verify
    fi
  fi
  regen_status_commit
  record_verified
  git push -q origin "HEAD:$BRANCH" --force-with-lease="$BRANCH:$BRANCH_TIP" || fail "push branch: origin/$BRANCH moved since this landing took it at ${BRANCH_TIP:0:7} (its author pushed?) — rerun without --resume to take the new tip"
  BRANCH_TIP=$(git rev-parse HEAD); record_tip
  if git push -q origin "HEAD:unreal/main" 2>/dev/null; then
    rm -f "$VB_FILE" "$TIP_FILE"
    cd "$REPO"
    if [ "$(git symbolic-ref -q --short HEAD)" = "unreal/main" ] && git diff --quiet && git diff --cached --quiet && ! in_rebase; then
      git pull -q --ff-only origin unreal/main 2>/dev/null || echo "clone has local commits on unreal/main; not fast-forwarded"
    else echo "clone is not clean on unreal/main; not touching it"; fi
    echo "int-merge: landed $BRANCH on unreal/main"
    exit 0
  fi
  echo "push to unreal/main rejected (attempt $attempt); refetching"
done
fail "push unreal/main after 3 attempts (rerun with --resume)"
