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
# (5.8 prints "Test Completed. Result={Passed|Failed} Name={...} Path={...}").
if [ -f "$REPORT/index.json" ]; then
  python3 - "$REPORT/index.json" "$LOG" "${DF_TEST_ALLOW_EMPTY:-0}" <<'PY'
import json, sys
report, log, allow_empty = sys.argv[1], sys.argv[2], sys.argv[3] == "1"
data = json.load(open(report))
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
if ! grep -qE "Test Completed\. Result=\{(Passed|Failed)\}" "$LOG" 2>/dev/null; then
  grep -E "Error:|Fatal|Assertion" "$LOG" 2>/dev/null | head -10
  echo "tests: NO RESULT — the editor did not run the tests ($LOG)"; exit 2
fi
grep -E "Test Completed\. Result=\{(Passed|Failed)\}" "$LOG" | sed -E 's/^.*Result=\{([A-Za-z]+)\} Name=\{([^}]*)\}.*$/\1 \2/' | sort | uniq
if grep -qE "Test Completed\. Result=\{Failed\}" "$LOG"; then
  echo "tests: FAILED ($LOG)"; exit 1
fi
echo "tests: OK ($LOG)"
