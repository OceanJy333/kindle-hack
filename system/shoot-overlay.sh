#!/bin/sh
# input: apply | revert; output: the device desk running with the filming list (or back to stock); pos: temporary /tmp overlay, gone after reboot.
# The read-only image stays untouched: /tmp/weread-native points at a tmpfs tree of links to /opt/ocean plus the changed files.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
lab=$here/..
out=$lab/build
dev() { sh "$here/device.sh" "$@"; }
restart='p=$(cat /tmp/weread-login.pid 2>/dev/null||:);[ -z "$p" ]||kill "$p"'
case "${1:-}" in
 apply)
  stage=$(mktemp -d)
  cp "$out/weread/native-runtime/weread-login" "$lab/weread/official/port/NativeDesk.qml" \
     "$lab/weread/official/port/NativeSettings.qml" "$lab/weread/official/port/ShootList.qml" \
     "$lab/games/native-launch.sh" "$stage/"
  cp "$out/arcade/arcade-voyage-shoot" "$stage/arcade-voyage"
  tar -C "$stage" -cf - . | dev 'set -eu
   f=/tmp/ocean-shoot-files o=/tmp/ocean-shoot
   rm -rf "$f" "$o";mkdir -p "$f" "$o/games";tar -xf - -C "$f"
   for e in /opt/ocean/*;do [ "$e" = /opt/ocean/games ]||ln -s "$e" "$o/";done
   for e in /opt/ocean/games/*;do ln -s "$e" "$o/games/";done
   rm "$o/games/arcade";mkdir "$o/games/arcade"
   for e in /opt/ocean/games/arcade/*;do ln -s "$e" "$o/games/arcade/";done
   # A real loader file makes Qt resolve its application directory inside the overlay.
   for n in ld-linux-armhf.so.3 weread-login NativeDesk.qml NativeSettings.qml;do rm -f "$o/$n";done
   cp /opt/ocean/ld-linux-armhf.so.3 "$o/"
   cp "$f/weread-login" "$f/NativeDesk.qml" "$f/NativeSettings.qml" "$f/ShootList.qml" "$o/"
   rm -f "$o/games/native-launch.sh" "$o/games/arcade/arcade-voyage"
   cp "$f/native-launch.sh" "$o/games/";cp "$f/arcade-voyage" "$o/games/arcade/"
   chmod 755 "$o/weread-login" "$o/games/arcade/arcade-voyage" "$o/ld-linux-armhf.so.3"
   rm -rf "$f";rm /tmp/weread-native;ln -s "$o" /tmp/weread-native;'"$restart"
  rm -rf "$stage";;
 revert) dev 'rm /tmp/weread-native;ln -s /opt/ocean /tmp/weread-native;rm -rf /tmp/ocean-shoot;'"$restart";;
 *) echo 'usage: shoot-overlay.sh apply|revert' >&2; exit 2;;
esac
