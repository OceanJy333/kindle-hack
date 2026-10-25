#!/bin/sh
# input: installed native game and WAD; output: silent game, then restored Agent Desk; pos: exclusive device lifecycle.
set -eu
root=/mnt/us/doom-experiment
if [ -f /tmp/doom-voyage.pid ]; then
  p=$(cat /tmp/doom-voyage.pid)
  if [ -r /proc/"$p"/cmdline ] && tr '\000' ' ' < /proc/"$p"/cmdline | grep -q /doom-voyage; then
    echo 'DOOM is already running' >&2; exit 1
  fi
fi
for job in framework pillow webreader x; do stop "$job" 2>/dev/null || true; done
if [ -f /tmp/agent-board.pid ]; then
  p=$(cat /tmp/agent-board.pid)
  if [ -r /proc/"$p"/cmdline ] && tr '\000' ' ' < /proc/"$p"/cmdline | grep -q /agent-board; then
    kill -TERM "$p"
    for i in 1 2 3 4 5; do [ -r /proc/"$p"/cmdline ] || break; sleep 1; done
  fi
fi
if [ -f /tmp/weread-login.pid ]; then
  p=$(cat /tmp/weread-login.pid)
  if [ -r /proc/"$p"/cmdline ] && tr '\000' ' ' < /proc/"$p"/cmdline | grep -q /weread-login; then kill -STOP "$p"; fi
fi
previous_power=$(lipc-get-prop com.lab126.powerd preventScreenSaver)
game_pid=''
cleanup(){
  trap - EXIT INT TERM
  if [ -n "$game_pid" ]; then kill -TERM "$game_pid" 2>/dev/null || true; wait "$game_pid" 2>/dev/null || true; fi
  lipc-set-prop com.lab126.powerd preventScreenSaver "$previous_power"
  if ! mount | grep -q weread-native; then
    mkdir -p /tmp/weread-native
    mount -o loop /mnt/us/native-runtime.ext2 /tmp/weread-native
  fi
  sh /tmp/weread-native/launch.sh
}
trap cleanup EXIT INT TERM
lipc-set-prop com.lab126.powerd preventScreenSaver 1
lipc-set-prop com.lab126.deviced fsrkeypadEnable 1
cp "$root/doom-voyage" /tmp/doom-voyage; chmod 700 /tmp/doom-voyage
cd "$root"
/tmp/doom-voyage -iwad /mnt/us/games/doom1.wad -nosound -nomusic -config doom.cfg "$@" > /tmp/doom-voyage.log 2>&1 &
game_pid=$!;echo "$game_pid" > /tmp/doom-voyage.pid
wait "$game_pid"
