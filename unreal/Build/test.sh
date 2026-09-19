#!/bin/zsh
# Run Unreal automation tests headless (PROGRAMME.md §7.3).
#   unreal/Build/test.sh                    # DF.Unit + DF.Content + DF.Net (the pre-PR set)
#   unreal/Build/test.sh DF.Func.Tower      # any filter prefix
#   UE_ROOT=... unreal/Build/test.sh        # other engine install
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
PROJECT="$REPO/unreal/DeepField/DeepField.uproject"
FILTER="${1:-DF.Unit+DF.Content+DF.Net}"
LOG="$REPO/unreal/DeepField/Saved/Logs/test-$(echo "$FILTER" | tr '+.' '__').log"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"

"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" "$PROJECT" \
  -nullrhi -unattended -nop4 -nosplash -NoSound \
  -ExecCmds="Automation RunTests $FILTER; Quit" \
  -TestExit="Automation Test Queue Empty" \
  -log -abslog="$LOG" > /dev/null 2>&1 || true

# The editor exits 0 even when tests fail; the verdict is in the log.
if grep -qE "Test Completed\. Result=\{Failed\}|Automation Test Failed|LogAutomationController: Error" "$LOG"; then
  grep -E "Result=\{(Passed|Failed)\}|Automation Test Failed" "$LOG" | sed 's/^.*LogAutomationController: //' | sort | uniq
  echo "tests: FAILED ($LOG)"; exit 1
fi
grep -E "Result=\{Passed\}" "$LOG" | sed 's/^.*LogAutomationController: //' | sort | uniq
echo "tests: OK ($LOG)"
