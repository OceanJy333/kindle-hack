#!/bin/sh
# input: kernel, initramfs and loader on /var/local/ocean/boot; output: stock boot hands over to the independent system; pos: chain boot that replaces the stock userspace without touching boot slots.
export PATH=/sbin:/bin:/usr/sbin:/usr/bin
boot=/var/local/ocean/boot
log=$boot/events.log
note() { echo "$(date -u +%Y%m%dT%H%M%SZ) autoboot: $*" >>"$log"; }
[ -f "$boot/disabled" ] && { note "disabled flag, staying on stock"; exit 0; }
attempts=$(cat "$boot/attempts" 2>/dev/null || echo 0)
# Two boots in a row that never accepted themselves: stay on stock until someone resets the counter.
[ "$attempts" -ge 2 ] && { note "attempts=$attempts, staying on stock"; exit 0; }
echo $((attempts+1)) >"$boot/attempts"
sync
lsmod | grep -q ocean_kexec || insmod "$boot/ocean-kexec.ko" || { note "insmod failed"; exit 0; }
for _ in 1 2 3 4 5; do [ -c /dev/kexec ] && break; sleep 1; done
[ -c /dev/kexec ] || { note "no /dev/kexec"; exit 0; }
"$boot/kexec-ctl" load "$boot/zImage" "$boot/initramfs.cpio" \
 'console=ttymxc0,115200 log_pos_goal=0x8fff0000 root=/dev/loop1 ro rootwait ip=off video=mxcepdcfb:E60,bpp=8,x_mem=3M loglevel=4 ocean_boot_timeout=180 panic=10' \
 >>"$log" 2>&1 || { note "load failed"; exit 0; }
# Let module probes that started on the same event finish before the CPU changes kernels.
for _ in $(seq 1 10); do pidof modprobe >/dev/null || break; sleep 1; done
note "attempt $((attempts+1)) executing"
sync
mount -o remount,ro /var/local 2>/dev/null || sync
exec "$boot/kexec-ctl" execute
