#!/bin/zsh
# Run a command while holding the machine-wide Unreal editor lock (PROGRAMME.md §6.7 on an
# 8 GB Mac: one UnrealEditor-Cmd at a time; UBT already serialises itself with -WaitMutex).
#   unreal/Build/editor-lock.sh <command...>
# Waits up to EDITOR_LOCK_TIMEOUT seconds (default 1800). A lock whose owning pid is gone is
# reclaimed once it is older than 60 s (long enough for the pid file to have been written).
#
# The command runs as a child and the lock is released from the EXIT trap. It must not be
# exec'd: zsh skips the EXIT trap on exec, and the first version of this script leaked the lock
# for 45 min after every use — every agent then queued behind a dead pid.
set -u
LOCK="${EDITOR_LOCK_DIR:-/Volumes/Toshiba/Deepfield-Unreal/.editor-lock}"
TIMEOUT="${EDITOR_LOCK_TIMEOUT:-1800}"
waited=0
while ! mkdir "$LOCK" 2>/dev/null; do
  pid=$(cat "$LOCK/pid" 2>/dev/null || echo 0)
  age=$(( $(date +%s) - $(stat -f %m "$LOCK" 2>/dev/null || date +%s) ))
  if [ "$pid" != 0 ] && ! kill -0 "$pid" 2>/dev/null && [ "$age" -gt 60 ]; then
    echo "editor-lock: reclaiming stale lock (pid $pid gone, ${age}s old)" >&2
    rm -rf "$LOCK"; continue
  fi
  if [ "$waited" -ge "$TIMEOUT" ]; then echo "editor-lock: timed out after ${TIMEOUT}s (held by pid $pid, ${age}s)" >&2; exit 75; fi
  if [ "$waited" -eq 0 ]; then echo "editor-lock: waiting for pid $pid ($1 ...)" >&2; fi
  sleep 5; waited=$((waited+5))
done
echo $$ > "$LOCK/pid"
CHILD=""
release() { rm -rf "$LOCK"; }
on_signal() {
  if [ -n "$CHILD" ]; then kill -TERM "$CHILD" 2>/dev/null; wait "$CHILD" 2>/dev/null; fi
  release; exit 143
}
trap release EXIT
trap on_signal INT TERM HUP
# The DDC override variable has a dash in its name, which a shell cannot export; env can.
env "UE-LocalDataCachePath=${UE_LOCAL_DDC:-/Volumes/Toshiba/Deepfield-Unreal/DDC}" \
    "DEVELOPER_DIR=${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}" "$@" &
CHILD=$!
wait "$CHILD"
rc=$?
CHILD=""
exit $rc
