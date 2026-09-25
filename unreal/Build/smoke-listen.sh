#!/bin/zsh
# WS-00 smoke (PROGRAMME.md §5.1 DoD, §7 DF.Net.ListenHostPlusClient stand-in until Gauntlet): a
# listen host on L_Dev_Empty and one or more clients that join it, all headless (-nullrhi). Prints
# the join lines from every log and exits 0 only if every client was admitted ('Welcomed by server',
# sent only after ADFGameMode::PreLogin's join validators accept), the host saw each of them join,
# seated (seat > 0), and the host refused no one ('join refused'). deepfield.ps1's `smoke` is the
# Windows twin and keeps the same condition.
#   unreal/Build/smoke-listen.sh                                  # host + 1 client on /Game/DF/Dev/L_Dev_Empty
#   unreal/Build/smoke-listen.sh -client-count 2                  # host + 2 clients
#   unreal/Build/smoke-listen.sh /Game/DF/Maps/Foundry/L_Foundry  # another map (or -map <path>)
#   unreal/Build/smoke-listen.sh -port 7790                       # another port
# Every process is an editor process: on the shared Mac wrap the whole smoke in editor-lock.sh.
# Logs: Saved/Logs/smoke-host.log, smoke-client-1.log, smoke-client-2.log, ...
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"; REPO="$(cd "$HERE/../.." && pwd)"
UE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
ED="$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
PROJECT="$REPO/unreal/DeepField/DeepField.uproject"
MAP="/Game/DF/Dev/L_Dev_Empty"
CLIENTS=1
PORT=7788
JOIN_TIMEOUT="${DF_SMOKE_TIMEOUT:-180}"
while [ $# -gt 0 ]; do
  case "$1" in
    -client-count|--client-count) CLIENTS="$2"; shift 2 ;;
    -map|--map) MAP="$2"; shift 2 ;;
    -port|--port) PORT="$2"; shift 2 ;;
    -h|--help) sed -n '2,13p' "$0"; exit 0 ;;
    -*) echo "smoke: unknown option $1" >&2; exit 2 ;;
    *) MAP="$1"; shift ;;
  esac
done
case "$CLIENTS" in ''|*[!0-9]*|0) echo "smoke: -client-count must be a positive integer" >&2; exit 2 ;; esac
if [ ! -x "$ED" ]; then echo "smoke: no editor at $ED (set UE_ROOT)"; exit 2; fi
LOGS="$REPO/unreal/DeepField/Saved/Logs"; mkdir -p "$LOGS"
rm -f "$LOGS/smoke-host.log" "$LOGS"/smoke-client-*.log
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
COMMON=(-game -nullrhi -unattended -nop4 -nosplash -NoSound -log)
PIDS=()
cleanup() { kill "${PIDS[@]}" 2>/dev/null; sleep 3; kill -9 "${PIDS[@]}" 2>/dev/null; }
trap cleanup EXIT INT TERM

"$ED" "$PROJECT" "$MAP?listen" -port=$PORT "${COMMON[@]}" -abslog="$LOGS/smoke-host.log" >/dev/null 2>&1 &
PIDS+=($!)
# wait for the host to bring its map up (max 120 s)
for i in {1..120}; do
  grep -q "LogNet: .*listening\|LogWorld: Bringing up level\|Game class is" "$LOGS/smoke-host.log" 2>/dev/null && break
  sleep 1
done
sleep 5
# Clients start staggered: each is a full editor process and the Mac has 8 GB.
for c in $(seq 1 "$CLIENTS"); do
  "$ED" "$PROJECT" "127.0.0.1:$PORT" "${COMMON[@]}" -abslog="$LOGS/smoke-client-$c.log" >/dev/null 2>&1 &
  PIDS+=($!)
  [ "$c" -lt "$CLIENTS" ] && sleep 4
done

# The listen host's own player is the first "player joined"; the clients are the rest.
NEED=$((CLIENTS + 1))
joined() { [ "$(grep -c "player joined" "$LOGS/smoke-host.log" 2>/dev/null)" -ge "$NEED" ]; }
# 'Bringing up level for play took' is not proof: a client that fails to connect loads its default map.
welcomed() { grep -q "Welcomed by server" "$LOGS/smoke-client-$1.log" 2>/dev/null; }
all_welcomed() { for c in $(seq 1 "$CLIENTS"); do welcomed "$c" || return 1; done; return 0; }
for i in $(seq 1 "$JOIN_TIMEOUT"); do
  joined && all_welcomed && break
  grep -q "join refused" "$LOGS/smoke-host.log" 2>/dev/null && break
  sleep 1
done
sleep 5
cleanup; trap - EXIT INT TERM

echo "--- host"; grep -E "LogNet: |player joined|LogDFMatch|LogDFCore|LogDFContent|Deep Field 3D|Error" "$LOGS/smoke-host.log" | grep -v "LogNet: UNetConnection::Close\|LogNet: UChannel" | head -25
for c in $(seq 1 "$CLIENTS"); do
  echo "--- client $c"; grep -E "LogNet: |Welcomed|LogWorld: Bringing up|Error|Warning: Failed" "$LOGS/smoke-client-$c.log" | head -25
done
HOST_JOINS="$(grep -c "player joined" "$LOGS/smoke-host.log" 2>/dev/null || echo 0)"
FAILED=0
if grep -q "join refused" "$LOGS/smoke-host.log" 2>/dev/null; then
  echo "smoke: the host refused a client: $(grep -o 'join refused ([^)]*)' "$LOGS/smoke-host.log" | head -3 | tr '\n' ' ')"; FAILED=1
fi
if grep -qE "player joined: .* \(seat 0\)" "$LOGS/smoke-host.log" 2>/dev/null; then
  echo "smoke: a player joined without a seat (seat 0)"; FAILED=1
fi
[ "$HOST_JOINS" -ge "$NEED" ] || { echo "smoke: host saw $HOST_JOINS player(s) join, needed $NEED (host + $CLIENTS client(s))"; FAILED=1; }
for c in $(seq 1 "$CLIENTS"); do
  welcomed "$c" || { echo "smoke: client $c never reached the host's map"; FAILED=1; }
done
if [ "$FAILED" -eq 0 ]; then
  echo "smoke: OK (host + $CLIENTS client(s) on $MAP)"; exit 0
fi
echo "smoke: FAILED (see $LOGS/smoke-host.log, smoke-client-*.log)"; exit 1
