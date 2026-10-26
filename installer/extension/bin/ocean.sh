#!/bin/sh
# input: KUAL action and release payload; output: selected components/status; pos: device entry.
set -eu
usb=${OCEAN_USB_ROOT:-/mnt/us}
ext=${OCEAN_EXTENSION_ROOT:-$(CDPATH='' cd "$(dirname "$0")/.." && pwd)}
state="$usb/ocean/state"
runtime="$ext/payload/runtime"
mkdir -p "$state"
export OCEAN_USB_ROOT="$usb" OCEAN_EXTENSION_ROOT="$ext"
notify() {
 printf '%s\n' "$1"
 if command -v eips >/dev/null 2>&1; then eips 0 2 "$1" 2>/dev/null || :; fi
}
bounded() {
 "$@" & work=$!
 (
  sleep 25 & nap=$!
  trap 'kill "$nap" 2>/dev/null || :; exit 0' HUP INT TERM
  wait "$nap"
  kill -TERM "$work" 2>/dev/null || :
 ) & timer=$!
 result=0
 wait "$work" || result=$?
 kill "$timer" 2>/dev/null || :
 wait "$timer" 2>/dev/null || :
 return "$result"
}
check() {
 "$ext/bin/detect.sh" --shell > "$state/device.env"
 "$ext/bin/detect.sh" --json > "$state/device.json"
 . "$state/device.env"
 export OCEAN_DEVICE_PROFILE
 hardware=hardware_probe_failed
 probe_ok=0
 OCEAN_HARDWARE_READY=0
 if [ -x "$ext/bin/device-probe" ]; then
  # Both outputs are produced by our fixed-value binary; never evaluate serial or firmware text.
  if bounded "$ext/bin/device-probe" --shell > "$state/probe.env" 2> "$state/probe.log"; then hardware=ready; probe_ok=1; fi
  if ! grep -Ev '^OCEAN_(STATIC_RUNTIME|KERNEL_READY|DISPLAY_READY|INPUT_READY|HARDWARE_READY|FB_WIDTH|FB_HEIGHT|FB_BPP)=[0-9]+$' "$state/probe.env" | grep -q .; then
   . "$state/probe.env"
  fi
  bounded "$ext/bin/device-probe" --json > "$state/probe.json" 2>> "$state/probe.log" || :
 fi
 [ "$probe_ok" = 1 ] || OCEAN_HARDWARE_READY=0
 reader=reader_runtime_unavailable
 if [ "$OCEAN_QT_CANDIDATE" = 1 ] && [ "$OCEAN_KERNEL_SUPPORTED" = 1 ]; then
  export QT_PLUGIN_PATH="$runtime/plugins" SSL_CERT_FILE="$runtime/ca-certificates.crt"
  export OPENSSL_CONF="$runtime/config/openssl.cnf"
  reader=reader_payload_missing
  if [ -x "$runtime/ld-linux-armhf.so.3" ] && [ -x "$runtime/runtime-check" ] && [ -x "$runtime/weread-login" ] && [ -f "$runtime/ReaderApp.qml" ]; then
   reader=reader_runtime_failed
   if bounded "$runtime/ld-linux-armhf.so.3" --library-path "$runtime" "$runtime/runtime-check" --local > "$state/reader-probe.json" 2> "$state/reader-probe.log"; then
    if bounded "$runtime/ld-linux-armhf.so.3" --library-path "$runtime" --list "$runtime/weread-login" > "$state/reader-dependencies.log" 2>> "$state/reader-probe.log"; then reader=ready; fi
   fi
  fi
 fi
 doom=game_payload_missing
 neogeo=game_payload_missing
 if [ -x "$ext/payload/games/doom/doom-voyage" ]; then
  doom=game_runtime_failed
  if bounded "$ext/payload/games/doom/doom-voyage" --probe > "$state/doom-probe.log" 2>&1; then doom=missing_doom1_wad; [ ! -f "$usb/games/doom1.wad" ] || doom=ready; fi
 fi
 if [ -x "$ext/payload/games/arcade/arcade-voyage" ]; then
  neogeo=game_runtime_failed
  if bounded "$ext/payload/games/arcade/arcade-voyage" --probe > "$state/neogeo-probe.log" 2>&1; then
   neogeo=missing_mslug_or_neogeo_zip
   if [ -f "$usb/games/roms/mslug.zip" ] && [ -f "$usb/games/roms/neogeo.zip" ]; then neogeo=ready; fi
  fi
 fi
 if [ "$OCEAN_HARDWARE_READY" != 1 ]; then
  hardware=hardware_probe_failed
  [ "$OCEAN_DEVICE_PROFILE" != unknown ] || hardware=unsupported_device_profile
  [ "$reader" != ready ] || reader=hardware_probe_failed
  [ "$doom" != ready ] || doom=hardware_probe_failed
  [ "$neogeo" != ready ] || neogeo=hardware_probe_failed
 fi
 if [ "$OCEAN_KERNEL_SUPPORTED" != 1 ]; then
  hardware=unsupported_kernel
  [ "$OCEAN_DEVICE_PROFILE" != unknown ] || hardware=unsupported_device_profile
  [ "$reader" != ready ] || reader=unsupported_kernel
  [ "$doom" != ready ] || doom=unsupported_kernel
  [ "$neogeo" != ready ] || neogeo=unsupported_kernel
 fi
 next=read_probe_logs
 case "$hardware" in unsupported_*) next=report_device_json;; ready)
  case "$reader:$doom:$neogeo" in
   *ready*) next=open_kual_app;;
   *missing_doom*|*missing_mslug*) next=copy_game_data_then_check;;
   *payload_missing*) next=reinstall_release;;
  esac;;
 esac
 printf 'OCEAN_READER_STATUS=%s\nOCEAN_DOOM_STATUS=%s\nOCEAN_NEOGEO_STATUS=%s\n' "$reader" "$doom" "$neogeo" > "$state/components.env"
 printf '{"phase":"checked","profile":"%s","hardware":"%s","reader":"%s","doom":"%s","neogeo":"%s","next":"%s","sleepResume":"unverified"}\n' "$OCEAN_DEVICE_PROFILE" "$hardware" "$reader" "$doom" "$neogeo" "$next" > "$state/status.json"
}
show_status() {
 if [ -f "$state/status.json" ]; then cat "$state/status.json"; else printf '{"phase":"awaiting_device_check"}\n'; fi
 if command -v eips >/dev/null 2>&1 && [ -r "$state/components.env" ]; then
  . "$state/components.env"
  eips 0 4 "Reader: $OCEAN_READER_STATUS" 2>/dev/null || :
  eips 0 5 "DOOM: $OCEAN_DOOM_STATUS" 2>/dev/null || :
  eips 0 6 "NeoGeo: $OCEAN_NEOGEO_STATUS" 2>/dev/null || :
 fi
}
case "${1:-install}" in
 install|verify) check; show_status; notify "Ocean checked. See ocean/state/status.json";;
 status) show_status; notify "Ocean status: ocean/state/status.json";;
 weread|doom|neogeo) exec "$ext/bin/launch.sh" "$1";;
 stock) exec "$ext/bin/launch.sh" stock;;
 uninstall)
  "$ext/bin/launch.sh" stock
  printf '{"phase":"uninstalled","dataPreserved":true}\n' > "$state/status.json"
  rm -rf "$ext"
  notify "Ocean removed. Accounts and saves kept.";;
 *) printf 'Usage: ocean.sh install|verify|status|weread|doom|neogeo|stock|uninstall\n'; exit 2;;
esac
