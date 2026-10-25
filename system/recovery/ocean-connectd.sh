#!/bin/sh
# input: /var/local/ocean/connect binaries and paired key; output: always-on key-only SSH on 2222; pos: original-system self-healing access.
# Lives on the internal ext3 partition so it works while /mnt/us is exported over USB.
export PATH=/sbin:/bin:/usr/sbin:/usr/bin
home=/var/local/ocean/connect
log=/var/local/ocean/recovery/connectd.log
mkdir -p /var/local/ocean/recovery
[ ! -f "$log" ] || [ "$(wc -c <"$log")" -lt 65536 ] || mv "$log" "$log.1"
note() { printf '%s %s\n' "$(date -u +%Y%m%dT%H%M%SZ)" "$*" >>"$log"; }

setup_key() {
 root_home=$(awk -F: '$1 == "root" {print $6}' /etc/passwd)
 [ -n "$root_home" ] && [ "$root_home" != / ] || return 1
 mkdir -p "$root_home/.ssh"
 chown root:root "$root_home" "$root_home/.ssh"
 chmod 700 "$root_home" "$root_home/.ssh"
 touch "$root_home/.ssh/authorized_keys"
 key=$(cat "$home/authorized_keys")
 grep -Fqx "$key" "$root_home/.ssh/authorized_keys" || printf '%s\n' "$key" >>"$root_home/.ssh/authorized_keys"
 chmod 600 "$root_home/.ssh/authorized_keys"
 [ -s "$home/host_key" ] || "$home/dropbearkey" -t ed25519 -f "$home/host_key" >/dev/null 2>&1
}

# Same host key as the manual Ocean Connect, so the Mac known_hosts stays valid.
[ -s "$home/host_key" ] || [ ! -s /var/local/ocean-ssh/host_key ] || cp /var/local/ocean-ssh/host_key "$home/host_key"

server_alive() {
 for pidfile in "$home/server.pid" /var/local/ocean-ssh/server.pid; do
  [ -f "$pidfile" ] || continue
  pid=$(cat "$pidfile")
  [ -r "/proc/$pid/cmdline" ] && tr '\000' ' ' <"/proc/$pid/cmdline" | grep -q dropbear && return 0
 done
 return 1
}

note "start kernel=$(uname -r)"
[ -r /proc/printkbuf ] && cat /proc/printkbuf >/var/local/ocean/recovery/previous-kernel-printk.txt
while :; do
 # Firewall rules are rebuilt by the framework on some network transitions.
 peers=192.168.77.1
 [ ! -f "$home/allowed-peers" ] || peers=$(cat "$home/allowed-peers")
 for peer in $peers; do
  iptables -C INPUT -p tcp -s "$peer" --dport 2222 -j ACCEPT 2>/dev/null ||
   iptables -I INPUT 1 -p tcp -s "$peer" --dport 2222 -j ACCEPT 2>/dev/null
 done
 # root's home is /tmp/root; the framework mounts tmpfs over /tmp after we start,
 # hiding keys written earlier, so the key is re-asserted on every pass.
 setup_key || note "key setup failed"
 if ! server_alive; then
  [ -n "${root_home:-}" ] && "$home/dropbear" -E -s -j -k -p 2222 -r "$home/host_key" \
   -D "$root_home/.ssh" -P "$home/server.pid" 2>>"$home/ssh.log" && note "dropbear started"
 fi
 # Wi-Fi radio off is the common way access disappears; airplane mode stays the user's call only via this file.
 if [ ! -f "$home/allow-wifi-off" ] && command -v lipc-get-prop >/dev/null; then
  if [ "$(lipc-get-prop com.lab126.cmd wirelessEnable 2>/dev/null)" = 0 ]; then
   lipc-set-prop com.lab126.cmd wirelessEnable 1 2>/dev/null && note "wifi re-enabled"
  fi
 fi
 sleep 20
done
