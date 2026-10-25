#!/bin/sh
# input: boot hook and installed paired bootstrap; output: persistent failure log and SSH; pos: unattended firmware recovery.
set -eu
export PATH=/sbin:/bin:/usr/sbin:/usr/bin
export OCEAN_RECOVERY_AUTOMATIC=1
[ "$(id -u)" = 0 ] || exit 1
state=/var/local/ocean/recovery
mkdir -p "$state"
chmod 700 "$state"
# The directory lock lives in RAM and disappears after a real reboot.
mkdir /tmp/ocean-recovery.lock 2>/dev/null || exit 0
trap 'rmdir /tmp/ocean-recovery.lock' EXIT
stamp=$(date -u +%Y%m%dT%H%M%SZ)
if [ -r /proc/printkbuf ]; then
 cat /proc/printkbuf >"$state/previous-kernel-$stamp.log"
fi
for attempt in 1 2 3 4 5 6; do
 if /bin/sh /mnt/us/ocean-install/physical-bootstrap.sh >"$state/connect.log" 2>&1; then
  printf 'kernel=%s\nboot=%s\nconnection=started\n' "$(uname -r)" "$stamp" >"$state/status"
  sync
  exit 0
 fi
 sleep 5
done
printf 'kernel=%s\nboot=%s\nconnection=failed\n' "$(uname -r)" "$stamp" >"$state/status"
exit 1
