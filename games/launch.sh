#!/bin/sh
# input: installed game id; output: silent game then restored desktop; pos: game process and power lifecycle.
set -eu
# A game launched through SSH must return to the same paired desktop as the library launcher.
if [ -z "${BOARD_HOST:-}" ] && [ -r /mnt/us/ocean-install/board-host ]; then
  BOARD_HOST=$(cat /mnt/us/ocean-install/board-host)
  export BOARD_HOST
fi
root=/tmp/weread-native/games
case "${1:-}" in
  doom) directory="$root/doom"; binary=doom-voyage;;
  mslug) directory="$root/arcade"; binary=arcade-voyage;;
  *) echo 'Unknown game' >&2; exit 2;;
esac
if [ -f /tmp/doom-voyage.pid ]; then
  p=$(cat /tmp/doom-voyage.pid)
  if [ -r /proc/"$p"/cmdline ] && tr '\000' ' ' < /proc/"$p"/cmdline | grep -q /doom-voyage; then kill -TERM "$p"; sleep 1; fi
fi
for pidfile in /tmp/agent-board.pid /tmp/weread-login.pid; do
  if [ -f "$pidfile" ]; then
    p=$(cat "$pidfile")
    if [ -r /proc/"$p"/cmdline ]; then
      command=$(tr '\000' ' ' < /proc/"$p"/cmdline)
      case "$command" in
        */agent-board\ *) kill -TERM "$p";sleep 1;;
        */weread-login*) kill -STOP "$p";;
      esac
    fi
  fi
done
previous_power=$(lipc-get-prop com.lab126.powerd preventScreenSaver)
game_pid=''
keepawake_pid=''
cleanup(){
  trap - EXIT INT TERM
  [ -z "$keepawake_pid" ] || kill "$keepawake_pid" 2>/dev/null || true
  if [ -n "$game_pid" ]; then kill -TERM "$game_pid" 2>/dev/null || true;wait "$game_pid" 2>/dev/null || true; fi
  lipc-set-prop com.lab126.powerd preventScreenSaver "$previous_power"
  sh /tmp/weread-native/launch.sh
}
trap cleanup EXIT INT TERM
# Keep an active game awake without disabling the physical power button.
lipc-set-prop com.lab126.powerd preventScreenSaver 0
(
  while sleep 60; do
    if [ "$(lipc-get-prop -s com.lab126.powerd state)" = active ]; then
      lipc-set-prop com.lab126.powerd touchScreenSaverTimeout 1
    fi
  done
) &
keepawake_pid=$!
lipc-set-prop com.lab126.deviced fsrkeypadEnable 1
mkdir -p /mnt/us/games/roms
cp "$directory/$binary" "/tmp/$binary";chmod 700 "/tmp/$binary"
cd "$directory"
if [ "$1" = doom ]; then
  /tmp/doom-voyage -iwad /mnt/us/games/doom1.wad -nosound -nomusic -config /mnt/us/games/doom.cfg -savedir /mnt/us/games -warp 1 1 > /tmp/game.log 2>&1 &
else
  /tmp/arcade-voyage /mnt/us/games/roms/mslug.zip > /tmp/game.log 2>&1 &
fi
game_pid=$!;echo "$game_pid" > /tmp/game.pid
wait "$game_pid"
