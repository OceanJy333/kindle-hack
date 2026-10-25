#!/bin/sh
# Name: Ocean Connect
# Author: Ocean
# Input: /mnt/us/ocean-install/ssh public key and ARM binaries.
# Output: key-only SSH on port 2222 and physical device inventory.
# Pos: one-tap physical Voyage access, before launching the desktop.
set -eu
umask 077
[ "$(id -u)" = 0 ] || { echo 'Root launcher required'; exit 1; }
src=/mnt/us/ocean-install/ssh
state=/var/local/ocean-ssh
mkdir -p "$state"
chmod 700 "$state"
report=/mnt/us/ocean-install
if [ "${OCEAN_RECOVERY_AUTOMATIC:-0}" = 1 ]; then
    report=/var/local/ocean/recovery
    mkdir -p "$report"
    chmod 700 "$report"
fi
cp "$src/dropbear" "$src/dropbearkey" "$state/"
chmod 700 "$state/dropbear" "$state/dropbearkey"
# Stop permission traversal at root's home; /var/local is group-writable.
root_home=$(awk -F: '$1 == "root" {print $6}' /etc/passwd)
[ -n "$root_home" ] && [ "$root_home" != / ]
mkdir -p "$root_home/.ssh"
chown root:root "$root_home" "$root_home/.ssh"
chmod 700 "$root_home" "$root_home/.ssh"
touch "$root_home/.ssh/authorized_keys"
key=$(cat "$src/authorized_keys")
grep -Fqx "$key" "$root_home/.ssh/authorized_keys" || printf '%s\n' "$key" >> "$root_home/.ssh/authorized_keys"
chown root:root "$root_home/.ssh/authorized_keys"
chmod 600 "$root_home/.ssh/authorized_keys"
[ ! -f "$state/ssh.log" ] || cp "$state/ssh.log" "$report/ssh-previous.log"
if [ ! -s "$state/host_key" ]; then
    "$state/dropbearkey" -t ed25519 -f "$state/host_key" >"$state/host-key.txt" 2>&1
fi
# Allow the paired computer on the local network.
for peer in $(cat /mnt/us/ocean-install/board-host); do
    iptables -C INPUT -p tcp -s "$peer" --dport 2222 -j ACCEPT 2>/dev/null ||
        iptables -I INPUT 1 -p tcp -s "$peer" --dport 2222 -j ACCEPT
done
if [ -f "$state/server.pid" ]; then
    previous=$(cat "$state/server.pid")
    if [ -r "/proc/$previous/cmdline" ] && tr '\000' ' ' < "/proc/$previous/cmdline" | grep -q "$state/dropbear"; then
        kill "$previous"
        sleep 1
    fi
fi
"$state/dropbear" -E -s -j -k -p 2222 -r "$state/host_key" -D "$root_home/.ssh" -P "$state/server.pid" 2>>"$state/ssh.log"
{
    date; id; uname -a; cat /proc/meminfo; cat /proc/bus/input/devices
    ifconfig wlan0; cat /proc/mounts
    for f in /sys/class/power_supply/*/uevent; do [ ! -f "$f" ] || cat "$f"; done
} > "$report/physical-inventory.txt" 2>&1
[ ! -r /proc/printkbuf ] || cat /proc/printkbuf > "$report/previous-kernel-printk.txt"
sync
echo 'Ocean Connect ready - SSH port 2222'
ifconfig wlan0 | sed -n '/inet /p'
echo 'Keep Wi-Fi on. You may return to Home.'
[ "${OCEAN_RECOVERY_AUTOMATIC:-0}" = 1 ] || sleep 5
