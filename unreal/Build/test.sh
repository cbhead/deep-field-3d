#!/bin/zsh
# Run Unreal automation tests headless (PROGRAMME.md §7).
#   unreal/Build/test.sh                    # DF.Unit + DF.Content + DF.Net (the pre-PR set)
#   unreal/Build/test.sh DF.Func.Tower      # any filter prefix; '+' joins several (DF.Unit+DF.Content)
#   UE_ROOT=... unreal/Build/test.sh        # other engine install
# On the shared Mac wrap it in the editor lock: unreal/Build/editor-lock.sh unreal/Build/test.sh <filter>
#
# The verdict comes from the automation controller's JSON report (-ReportExportPath), not from the
# editor's exit code (which is 0 whatever the tests did) and not from grepping log lines alone.
# A filter that matches no test is a failure too: a typo must not pass silently
# (DF_TEST_ALLOW_EMPTY=1 to permit it, e.g. for a suite a workstream has not written yet).
# Exit: 0 every matched test passed · 1 a test failed or none matched · 2 the editor did not finish.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJECT="$REPO/unreal/DeepField/DeepField.uproject"
FILTER="${1:-DF.Unit+DF.Content+DF.Net}"
SAFE="$(echo "$FILTER" | tr '+.' '__')"
SAVED="$REPO/unreal/DeepField/Saved"
LOG="$SAVED/Logs/test-$SAFE.log"
REPORT="$SAVED/Automation/Reports/test-$SAFE"
TIMEOUT="${DF_TEST_TIMEOUT:-1800}"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"

ED="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
if [ ! -x "$ED" ]; then echo "tests: no editor at $ED (set UE_ROOT)"; exit 2; fi

# A green run on a stale binary is the worst verdict this harness can produce: it is
# indistinguishable from a real pass, and the falsification rule cannot catch it either — a stale
# binary passes that too, for the same reason. WS-05 nearly shipped on one (a stray `*/` failed the
# build in 7 seconds; `--no-build` then tested the previous binary and reported "34 passed").
# So: whatever the caller did or skipped, the project's modules must be newer than the code they
# were built from. DF_TEST_ALLOW_STALE=1 for the rare deliberate case (testing a known-good binary
# after a doc-only edit); it prints what it is ignoring, because a silent override is the same bug.
BIN_DIR="$REPO/unreal/DeepField/Binaries/Mac"
NEWEST_BIN=$(ls -t "$BIN_DIR"/*.dylib "$BIN_DIR"/*.target 2>/dev/null | head -1)
if [ -z "$NEWEST_BIN" ]; then
  echo "tests: nothing built at $BIN_DIR — build before testing"; exit 2
fi
NEWEST_SRC=$(find "$REPO/unreal/DeepField/Source" "$REPO/unreal/DeepField/Config" "$REPO/unreal/DeepField/DeepField.uproject" \
  -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.inl' -o -name '*.cs' -o -name '*.ini' -o -name '*.uproject' \) \
  -newer "$NEWEST_BIN" -print 2>/dev/null | head -1)
if [ -n "$NEWEST_SRC" ]; then
  REL="${NEWEST_SRC#$REPO/}"
  if [ "${DF_TEST_ALLOW_STALE:-0}" = "1" ]; then
    echo "tests: WARNING — $REL is newer than the built modules; testing anyway (DF_TEST_ALLOW_STALE=1)"
  else
    echo "tests: REFUSING — $REL is newer than $(basename "$NEWEST_BIN"), so the editor would test code that is not in it."
    echo "       Build first (this is what a green run on a stale binary looks like), or set DF_TEST_ALLOW_STALE=1 if you mean it."
    exit 2
  fi
fi
mkdir -p "$SAVED/Logs"
rm -rf "$REPORT"; mkdir -p "$REPORT"      # a stale report must never supply the verdict

"$ED" "$PROJECT" \
  -nullrhi -unattended -nop4 -nosplash -NoSound \
  -ExecCmds="Automation RunTests $FILTER; Quit" \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="$REPORT" \
  -log -abslog="$LOG" > /dev/null 2>&1 &
PID=$!
waited=0
while kill -0 $PID 2>/dev/null; do
  if [ "$waited" -ge "$TIMEOUT" ]; then
    echo "tests: TIMEOUT after ${TIMEOUT}s, killing the editor ($LOG)"
    kill $PID 2>/dev/null; sleep 5; kill -9 $PID 2>/dev/null
    exit 2
  fi
  sleep 2; waited=$((waited+2))
done
wait $PID; EDITOR_EXIT=$?

# Primary verdict: the JSON report. Fallback: the controller's per-test log lines
# (5.8 prints "LogAutomationController: Display|Error: Test Completed. Result={Success|Fail} Name={...} Path={...}").
if [ -f "$REPORT/index.json" ]; then
  python3 - "$REPORT/index.json" "$LOG" "${DF_TEST_ALLOW_EMPTY:-0}" <<'PY'
import json, sys
report, log, allow_empty = sys.argv[1], sys.argv[2], sys.argv[3] == "1"
data = json.load(open(report, encoding="utf-8-sig"))   # the controller writes a BOM
tests = data.get("tests", [])
failed = 0
for t in sorted(tests, key=lambda t: t.get("fullTestPath", "")):
    state = t.get("state", "?")
    ok = state == "Success"
    failed += 0 if ok else 1
    print(f"{'PASS' if ok else 'FAIL'} {t.get('fullTestPath', '?')}  [{state}]"
          + (f"  ({t.get('errors', 0)} error(s), {t.get('warnings', 0)} warning(s))" if not ok else ""))
    if not ok:
        for e in t.get("entries", []):
            ev = e.get("event", {})
            if ev.get("type") in ("Error", "Warning"):
                print(f"     {ev.get('type')}: {ev.get('message', '')}")
if not tests and not allow_empty:
    print("no test matched the filter (a typo? set DF_TEST_ALLOW_EMPTY=1 if an empty suite is expected)")
    print(f"tests: FAILED ({log})"); sys.exit(1)
if failed:
    print(f"tests: FAILED — {failed} of {len(tests)} ({log})"); sys.exit(1)
print(f"tests: OK — {len(tests)} passed ({log})"); sys.exit(0)
PY
  exit $?
fi

echo "tests: no report at $REPORT/index.json (editor exit $EDITOR_EXIT); falling back to the log"
# 5.8 writes no report at all when the filter matches nothing: the command line logs
# "No automation tests matched '<filter>'" and the editor exits 3. That is the "nothing matched"
# verdict (exit 1, or 0 under DF_TEST_ALLOW_EMPTY=1), not a missing result (exit 2).
if grep -q "No automation tests matched" "$LOG" 2>/dev/null; then
  grep "No automation tests matched" "$LOG" | head -1
  if [ "${DF_TEST_ALLOW_EMPTY:-0}" = "1" ]; then echo "tests: OK — no test matched '$FILTER' and DF_TEST_ALLOW_EMPTY=1 ($LOG)"; exit 0; fi
  echo "no test matched the filter (a typo? set DF_TEST_ALLOW_EMPTY=1 if an empty suite is expected)"
  echo "tests: FAILED ($LOG)"; exit 1
fi
if ! grep -qE "Test Completed\. Result=\{(Success|Fail)\}" "$LOG" 2>/dev/null; then
  grep -E "Error:|Fatal|Assertion" "$LOG" 2>/dev/null | head -10
  echo "tests: NO RESULT — the editor did not run the tests ($LOG)"; exit 2
fi
grep -E "Test Completed\. Result=\{(Success|Fail)\}" "$LOG" | sed -E 's/^.*Result=\{([A-Za-z]+)\}.* Path=\{([^}]*)\}.*$/\1 \2/' | sort | uniq
if grep -qE "Test Completed\. Result=\{Fail\}" "$LOG"; then
  echo "tests: FAILED ($LOG)"; exit 1
fi
echo "tests: OK ($LOG)"
