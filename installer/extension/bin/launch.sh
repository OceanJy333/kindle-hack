#!/bin/sh
# input: checked component; output: app session returning to stock; pos: stock Kindle lifecycle.
set -eu
usb=${OCEAN_USB_ROOT:-/mnt/us}
ext=${OCEAN_EXTENSION_ROOT:-$(CDPATH='' cd "$(dirname "$0")/.." && pwd)}
temp=${OCEAN_TMP_ROOT:-/tmp}
state="$usb/ocean/state"
session="$temp/ocean-session"
link="$temp/weread-native"
runtime="$ext/payload/runtime"
export OCEAN_USB_ROOT="$usb" OCEAN_EXTENSION_ROOT="$ext"
action=${1:-stock}
say() { printf '%s\n' "$1"; command -v eips >/dev/null 2>&1 && eips 0 3 "$1" 2>/dev/null || :; }
home() { lipc-set-prop com.lab126.appmgrd start app://com.lab126.booklet.home >/dev/null 2>&1 || :; }
if [ "$action" = stock ]; then
 if [ -r "$session/pid" ]; then
  pid=$(cat "$session/pid")
  case "$pid" in ''|*[!0-9]*) exit 1;; esac
  kill -TERM "$pid" 2>/dev/null || :
  while [ -r "$session/pid" ] && kill -0 "$pid" 2>/dev/null; do sleep 1; done
 else
  command -v lipc-set-prop >/dev/null 2>&1 && home || :
 fi
 exit 0
fi
case "$action" in weread|doom|neogeo) :;; *) exit 2;; esac
mkdir -p "$state" "$temp"
"$ext/bin/ocean.sh" install > "$state/launch-check.log" 2>&1
. "$state/device.env"
. "$state/components.env"
export OCEAN_DEVICE_PROFILE OCEAN_STOCK_APP=1
case "$action" in
 weread) component=$OCEAN_READER_STATUS;;
 doom) component=$OCEAN_DOOM_STATUS;;
 neogeo) component=$OCEAN_NEOGEO_STATUS;;
esac
if [ "$component" != ready ]; then say "Ocean: $component (ocean/state/status.json)"; exit 1; fi
# Resolve every dependency and data path before pausing stock drawing.
for tool in lipc-set-prop lipc-get-prop lipc-wait-event killall pidof; do
 command -v "$tool" >/dev/null 2>&1 || { say "Ocean: missing $tool"; exit 1; }
done
case "$action" in
 weread)
  for file in weread-login ld-linux-armhf.so.3 ReaderApp.qml fonts.conf ca-certificates.crt fonts/NotoSansCJKsc-Regular.otf; do
   [ -f "$runtime/$file" ] || { say "Ocean: reader_payload_missing"; exit 1; }
  done;;
 doom) [ -f "$usb/games/doom1.wad" ] && [ -f "$ext/payload/games/doom/overlay.gray" ] || exit 1;;
 neogeo) [ -f "$usb/games/roms/mslug.zip" ] && [ -f "$usb/games/roms/neogeo.zip" ] && [ -f "$ext/payload/games/arcade/overlay.gray" ] && [ -f "$ext/payload/games/arcade/missing.gray" ] || exit 1;;
esac
if [ -e "$link" ] || [ -L "$link" ]; then say "Ocean: runtime_path_in_use"; exit 1; fi
if ! mkdir "$session" 2>/dev/null; then say "Ocean: session_already_running"; exit 1; fi
printf '%s\n' "$$" > "$session/pid"
app=
watcher=
paused=0
pillow=0
power=
restore() {
 trap - 0 INT TERM
 [ -z "$watcher" ] || { kill "$watcher" 2>/dev/null || :; wait "$watcher" 2>/dev/null || :; }
 if [ -n "$app" ] && kill -0 "$app" 2>/dev/null; then
  if [ "$action" = weread ]; then kill -USR2 "$app" 2>/dev/null || :; else kill -TERM "$app" 2>/dev/null || :; fi
  wait "$app" 2>/dev/null || :
 fi
 [ "$paused" = 0 ] || killall -CONT awesome >/dev/null 2>&1 || :
 case "$pillow" in
  hard) lipc-set-prop com.lab126.pillow disableEnablePillow enable >/dev/null 2>&1 || :;;
  soft) lipc-set-prop com.lab126.pillow interrogatePillow '{"pillowId":"default_status_bar","function":"nativeBridge.showMe();"}' >/dev/null 2>&1 || :;;
 esac
 [ -z "$power" ] || lipc-set-prop com.lab126.powerd preventScreenSaver "$power" >/dev/null 2>&1 || :
 home
 [ ! -L "$link" ] || rm -f "$link"
 rm -rf "$session"
 printf '{"app":"%s","phase":"returned_to_stock","sleepResume":"unverified"}\n' "$action" > "$state/session.json"
}
trap restore 0
trap '' HUP
trap 'exit 143' INT TERM
power=$(lipc-get-prop com.lab126.powerd preventScreenSaver)
case "$power" in 0|1) :;; *) say "Ocean: cannot_read_powerd"; exit 1;; esac
ln -s "$runtime" "$link"
# Keep powerd active. Value 0 allows the physical power key to produce the sleep event.
lipc-set-prop com.lab126.powerd preventScreenSaver 0
# Match KOReader's stock launcher: old firmware hides Pillow, 5.6.5+ disables it.
# https://github.com/koreader/koreader/blob/master/platform/kindle/koreader.sh
old_pillow=$(awk -v fw="$OCEAN_FIRMWARE" 'BEGIN {split(fw,a,"."); print (a[1]*1000000+a[2]*1000+a[3] < 5006005) ? 1 : 0}')
if [ "$old_pillow" = 1 ]; then
 lipc-set-prop com.lab126.pillow interrogatePillow '{"pillowId":"default_status_bar","function":"nativeBridge.hideMe();"}'
 pillow=soft
else
 lipc-set-prop com.lab126.pillow disableEnablePillow disable
 pillow=hard
fi
if pidof awesome >/dev/null 2>&1; then killall -STOP awesome; paused=1; fi
export WEREAD_STATE_DIR="$state/weread" WEREAD_CACHE_DIR="$usb/weread-offline"
export QT_QPA_PLATFORM=offscreen QSG_RENDER_LOOP=basic QT_QUICK_BACKEND=software
export QT_PLUGIN_PATH="$runtime/plugins" SSL_CERT_FILE="$runtime/ca-certificates.crt"
export OPENSSL_CONF="$runtime/config/openssl.cnf" FONTCONFIG_FILE="$runtime/fonts.conf"
export XDG_RUNTIME_DIR="$session/xdg"
mkdir -p "$WEREAD_STATE_DIR" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
case "$action" in
 weread) "$runtime/ld-linux-armhf.so.3" --library-path "$runtime" "$runtime/weread-login" > "$state/app.log" 2>&1 & app=$!;;
 doom) cd "$ext/payload/games/doom"; ./doom-voyage -iwad "$usb/games/doom1.wad" -nosound -nomusic -config "$usb/games/doom.cfg" -savedir "$usb/games" -warp 1 1 > "$state/app.log" 2>&1 & app=$!;;
 neogeo) cd "$ext/payload/games/arcade"; ./arcade-voyage "$usb/games/roms/mslug.zip" > "$state/app.log" 2>&1 & app=$!;;
esac
printf '{"app":"%s","phase":"running","sleepResume":"unverified"}\n' "$action" > "$state/session.json"
# A sleep event ends this session; wake returns to stock. The reader flushes progress on USR2.
parent=$$
(
 event=
 trap '[ -z "$event" ] || kill "$event" 2>/dev/null || :; exit 0' HUP INT TERM
 lipc-wait-event -s 0 com.lab126.powerd goingToScreenSaver > "$session/sleep-event" 2> "$state/sleep.log" & event=$!
 if wait "$event"; then
  if [ "$action" = weread ]; then kill -USR2 "$app" 2>/dev/null || :; else kill -TERM "$app" 2>/dev/null || :; fi
 else
  kill -TERM "$parent" 2>/dev/null || :
 fi
) & watcher=$!
result=0
wait "$app" || result=$?
app=
exit "$result"
