#!/bin/sh
# input: staged candidate in /mnt/us/ocean-os on a stock-booted Voyage; output: RAM boot of the independent system, host acknowledgement and evidence; pos: physical trial driver (Mac side).
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo=$(CDPATH= cd -- "$here/.." && pwd)
access=${KINDLE_ACCESS_DIR:-$repo/build/device-install/physical-access}
stock_host=${KINDLE_HOST:-192.168.77.2}
ramboot_host=${KINDLE_RAMBOOT_HOST:-192.168.77.2}
out=$repo/build/reports/independent-os/physical-trial-$(date +%Y%m%dT%H%M%S)
mkdir -p "$out"
dev() { "$here/device.sh" "$@"; }
at() { # host port command...
 h=$1 p=$2; shift 2
 ssh -i "${KINDLE_KEY:-$access/id_ed25519}" -o "UserKnownHostsFile=$access/known_hosts" \
  -o IdentitiesOnly=yes -o BatchMode=yes \
  -o ConnectTimeout=4 -p "$p" "root@$h" "$@"
}
sha() { dev "sed -n '/\"$1\"/,/sha256/p' /mnt/us/ocean-os/candidate.json | sed -n 's/.*\"sha256\": *\"\\([0-9a-f]*\\)\".*/\\1/p'"; }
k=$(sha zImage); i=$(sha initramfs.cpio); r=$(sha voyage-rootfs.ext3)
[ -n "$k" ] && [ -n "$i" ] && [ -n "$r" ]
echo "candidate kernel=$k initramfs=$i rootfs=$r" | tee "$out/candidate.txt"

# RAM-only SSH on 2223: its binaries, key and log live in /tmp, so /var/local can go read-only.
dev 'set -e; d=/tmp/ram-ssh; mkdir -p $d; cp /var/local/ocean/connect/dropbear $d/; cp /var/local/ocean/connect/host_key $d/
 cp /mnt/us/ocean-os/kexec-ctl /mnt/us/ocean-os/physical-handoff /tmp/; chmod 700 /tmp/kexec-ctl /tmp/physical-handoff
 lsmod | grep -q ocean_kexec || insmod /mnt/us/ocean-os/ocean-kexec.ko
 mdev -s 2>/dev/null || true; [ -c /dev/kexec ]
 peer=${SSH_CONNECTION%% *}
 iptables -C INPUT -p tcp -s "$peer" --dport 2223 -j ACCEPT 2>/dev/null || iptables -I INPUT 1 -p tcp -s "$peer" --dport 2223 -j ACCEPT
 [ -f $d/pid ] && kill -0 $(cat $d/pid) 2>/dev/null || $d/dropbear -E -s -j -k -p 2223 -r $d/host_key -D /tmp/root/.ssh -P $d/pid 2>$d/log
 sleep 1; echo ram-ssh-ready' | tee "$out/prepare.txt"
at "$stock_host" 2223 "/tmp/physical-handoff load $k $i $r" | tee "$out/load.txt"
echo "executing handoff"
at "$stock_host" 2223 "initctl stop ocean-connect >/dev/null 2>&1 || true
 for f in /var/local/ocean/connect/server.pid; do [ -f \$f ] && kill \$(cat \$f) 2>/dev/null || true; done
 /tmp/physical-handoff execute $k $i $r" >"$out/transition.txt" 2>&1 || true
start=$(date +%s)
while [ $(( $(date +%s) - start )) -lt 175 ]; do
 if at "$ramboot_host" 2222 'uname -r' >"$out/independent-uname.txt" 2>/dev/null; then
  at "$ramboot_host" 2222 'echo 0 >/sys/module/kernel/parameters/ocean_boot_timeout; cat /sys/module/kernel/parameters/ocean_boot_timeout'
  echo "independent system acknowledged after $(( $(date +%s) - start ))s" | tee "$out/result.txt"
  at "$ramboot_host" 2222 'dmesg; echo ==ps; ps; echo ==net; ifconfig -a; echo ==events; cat /mnt/us/ocean-os/blackbox/current/events.log' >"$out/independent-state.txt" 2>&1 || true
  exit 0
 fi
 sleep 3
done
echo "no independent system on USB within 175s; waiting for stock return" | tee "$out/result.txt"
for _ in $(seq 1 40); do dev true </dev/null 2>/dev/null && break; sleep 5; done
dev 'cd /mnt/us/ocean-os/blackbox && tar cf - .' >"$out/blackbox.tar" 2>/dev/null || true
cp /dev/null "$out/.done"
exit 1
