#!/bin/zsh
# Run a command while holding the machine-wide Unreal editor lock (PROGRAMME.md §6.7 on an
# 8 GB Mac: one UnrealEditor-Cmd at a time; UBT already serialises itself with -WaitMutex).
#   unreal/Build/editor-lock.sh <command...>
# Waits up to EDITOR_LOCK_TIMEOUT seconds (default 1800). A lock older than 45 min whose
# owning pid is gone is treated as stale and reclaimed.
set -u
LOCK="${EDITOR_LOCK_DIR:-/Volumes/Toshiba/Deepfield-Unreal/.editor-lock}"
TIMEOUT="${EDITOR_LOCK_TIMEOUT:-1800}"
waited=0
while ! mkdir "$LOCK" 2>/dev/null; do
  if [ -f "$LOCK/pid" ]; then
    pid=$(cat "$LOCK/pid" 2>/dev/null || echo 0)
    age=$(( $(date +%s) - $(stat -f %m "$LOCK" 2>/dev/null || date +%s) ))
    if ! kill -0 "$pid" 2>/dev/null && [ "$age" -gt 2700 ]; then
      echo "editor-lock: reclaiming stale lock (pid $pid gone, ${age}s old)" >&2
      rm -rf "$LOCK"; continue
    fi
  fi
  if [ "$waited" -ge "$TIMEOUT" ]; then echo "editor-lock: timed out after ${TIMEOUT}s" >&2; exit 75; fi
  sleep 5; waited=$((waited+5))
done
echo $$ > "$LOCK/pid"
trap 'rm -rf "$LOCK"' EXIT INT TERM
# The DDC override variable has a dash in its name, which a shell cannot export; env can.
exec env "UE-LocalDataCachePath=${UE_LOCAL_DDC:-/Volumes/Toshiba/Deepfield-Unreal/DDC}" \
         "DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}" "$@"
