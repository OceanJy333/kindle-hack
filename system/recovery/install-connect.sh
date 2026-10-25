#!/bin/sh
# input: files staged in /tmp/ocean-connect; output: guard installed on /var/local and upstart; pos: one-time device installer (run as root on Voyage).
set -eu
src=/tmp/ocean-connect
dst=/var/local/ocean/connect
mkdir -p "$dst"
chmod 700 "$dst"
cp "$src/dropbear" "$src/dropbearkey" "$src/authorized_keys" "$src/ocean-connectd.sh" "$dst/"
chmod 700 "$dst/dropbear" "$dst/dropbearkey" "$dst/ocean-connectd.sh"
chmod 600 "$dst/authorized_keys"
[ ! -f "$src/allowed-peers" ] || cp "$src/allowed-peers" "$dst/"
[ -s "$dst/host_key" ] || cp /var/local/ocean-ssh/host_key "$dst/host_key"
mntroot rw
cp "$src/ocean-connect.conf" /etc/upstart/ocean-connect.conf
chmod 644 /etc/upstart/ocean-connect.conf
sync
mntroot ro
cp "$src/emergency.sh" /mnt/us/emergency.sh
sync
echo installed
