#!/bin/zsh
# WS-00 smoke (PROGRAMME.md §5.1 DoD): a listen host on L_Dev_Empty and a client that joins it,
# both headless (-nullrhi). Prints the join lines from both logs and exits 0 only if the client
# reached the host's map and the host saw the player join.
#   unreal/Build/smoke-listen.sh [map]      (default /Game/DF/Dev/L_Dev_Empty)
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ED="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
PROJECT="$REPO/unreal/DeepField/DeepField.uproject"
MAP="${1:-/Game/DF/Dev/L_Dev_Empty}"
LOGS="$REPO/unreal/DeepField/Saved/Logs"; mkdir -p "$LOGS"
PORT=7788
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
COMMON=(-game -nullrhi -unattended -nop4 -nosplash -NoSound -log)

"$ED" "$PROJECT" "$MAP?listen" -port=$PORT "${COMMON[@]}" -abslog="$LOGS/smoke-host.log" >/dev/null 2>&1 &
HOST=$!
# wait for the host to open its port (max 120 s)
for i in {1..120}; do
  grep -q "LogNet: .*listening\|LogWorld: Bringing up level\|Game class is" "$LOGS/smoke-host.log" 2>/dev/null && break
  sleep 1
done
sleep 5
"$ED" "$PROJECT" "127.0.0.1:$PORT" "${COMMON[@]}" -abslog="$LOGS/smoke-client.log" >/dev/null 2>&1 &
CLIENT=$!
# the listen host's own player is the first "player joined"; the client is the second
for i in {1..150}; do
  [ "$(grep -c "player joined" "$LOGS/smoke-host.log" 2>/dev/null)" -ge 2 ] && break
  grep -q "Welcomed by server" "$LOGS/smoke-client.log" 2>/dev/null && break
  sleep 1
done
sleep 5
kill $CLIENT $HOST 2>/dev/null; sleep 3; kill -9 $CLIENT $HOST 2>/dev/null

echo "--- host"; grep -E "LogNet: |player joined|LogDFMatch|LogDFCore|LogDFContent|Deep Field 3D|Error" "$LOGS/smoke-host.log" | grep -v "LogNet: UNetConnection::Close\|LogNet: UChannel" | head -25
echo "--- client"; grep -E "LogNet: |Welcomed|LogWorld: Bringing up|Error|Warning: Failed" "$LOGS/smoke-client.log" | head -25
if [ "$(grep -c "player joined" "$LOGS/smoke-host.log")" -ge 2 ] && grep -qE "Welcomed by server|Bringing up level for play took" "$LOGS/smoke-client.log"; then
  echo "smoke: OK"; exit 0
fi
echo "smoke: FAILED (see $LOGS/smoke-host.log, smoke-client.log)"; exit 1
