#!/bin/sh
# input: installed runtime; output: native UI and own power service; pos: stock-to-native test launcher.
set -eu
for job in framework pillow webreader x; do stop "$job" 2>/dev/null || true; done
for pidfile in /tmp/agent-notify.pid /tmp/agent-board.pid /tmp/weread-display.pid /tmp/weread-login.pid /tmp/ocean-power.pid /tmp/desktop-power.pid; do
 [ -r "$pidfile" ] || continue
 pid=$(cat "$pidfile")
 [ -r /proc/"$pid"/cmdline ] || continue
 command=$(tr '\000' ' ' </proc/"$pid"/cmdline)
 case "$command" in
  */agent-notify*|*/agent-board*|*/weread-display*|*/weread-login*|*/ocean-powerd*|*desktop-power.lua*)
   kill -CONT "$pid" 2>/dev/null || true
   kill -TERM "$pid" 2>/dev/null || true;;
 esac
 done
sleep 1
if ! mount | grep -q 'on /var/tmp/weread-native '; then
 mkdir -p /var/tmp/weread-native
 mount -o loop /mnt/us/native-runtime.ext2 /var/tmp/weread-native
fi
ln -sfn /var/tmp/weread-native /tmp/weread-native
root=/tmp/weread-native
saved_light=$(cat /sys/class/backlight/max77696-bl/brightness)
stop powerd 2>/dev/null || true
printf '%s\n' "$saved_light" >/sys/class/backlight/max77696-bl/brightness
sh "$root/power-policy.sh" apply
"$root/ocean-powerd" >/tmp/ocean-power.log 2>&1 &
echo $! >/tmp/ocean-power.pid
sh "$root/launch.sh" >/tmp/weread-login.log 2>&1 </dev/null &
sleep 2
kill -0 "$(cat /tmp/ocean-power.pid)"
kill -0 "$(cat /tmp/weread-login.pid)"
