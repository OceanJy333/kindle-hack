#!/bin/sh
# input: Kindle proc/sysfs/idme; output: safe JSON or shell assignments; pos: model detection.
set -eu
sys=${OCEAN_SYS_ROOT:-}
safe() { printf '%s' "$1" | LC_ALL=C tr -cd 'A-Za-z0-9._-'; }
kernel=$(safe "$(uname -r)")
arch=$(safe "$(uname -m)")
firmware=unknown
if [ -r "$sys/etc/prettyversion.txt" ]; then
 firmware=$(sed -n 's/^Kindle \([0-9.]*\).*/\1/p' "$sys/etc/prettyversion.txt" | head -n 1)
 firmware=$(safe "$firmware")
fi
prefix=unknown
if [ -r "$sys/proc/usid" ]; then
 prefix=$(LC_ALL=C cut -c1-6 "$sys/proc/usid" | head -n 1)
elif command -v idme >/dev/null 2>&1; then
 prefix=$(idme -d --serial 2>/dev/null | LC_ALL=C cut -c1-6 | head -n 1)
fi
prefix=$(safe "$prefix" | tr 'a-z' 'A-Z')
profile=unknown
case "$prefix" in
 B024*|B01B*|B01C*|B01D*|B01F*|B020*) profile=pw1;;
 B0D4*|B05A*|B0D5*|B0D6*|B0D7*|B0D8*|B0F2*|B017*|B060*|B0F4*|B0F9*|B062*|B061*|B05F*) profile=pw2;;
 B013*|B054*|B02A*|B04F*|B052*|B053*) profile=voyage;;
 G090G1|G090G2|G090G4|G090G5|G090G6|G090G7|G090KB|G090KC|G090KD|G090KE|G090KF|G090L0|G090LK|G090LL) profile=pw3;;
esac
case "$prefix" in B*) prefix=$(printf '%s' "$prefix" | cut -c1-4);; esac
# Device-tree model supplements serial detection; board names alone do not distinguish Wario models.
model=
for file in "$sys/sys/firmware/devicetree/base/model" "$sys/proc/device-tree/model"; do
 if [ -r "$file" ]; then model=$(tr -d '\000' < "$file"); break; fi
done
if [ "$profile" = unknown ]; then
 case "$model" in
  *Voyage*|*voyage*) profile=voyage;;
  *Paperwhite\ 1*|*PaperWhite\ 1*) profile=pw1;;
  *Paperwhite\ 2*|*PaperWhite\ 2*) profile=pw2;;
  *Paperwhite\ 3*|*PaperWhite\ 3*) profile=pw3;;
 esac
fi
qt_candidate=0
kernel_supported=0
major=${kernel%%.*}
case "$major" in ''|*[!0-9]*) major=0;; esac
case "$profile" in pw2|pw3|voyage) [ "$major" -lt 3 ] || qt_candidate=1;; esac
case "$profile:$kernel" in
 pw1:2.6.31|pw1:2.6.31-*) kernel_supported=1;;
 pw2:3.0.35|pw2:3.0.35-*|pw3:3.0.35|pw3:3.0.35-*|voyage:3.0.35|voyage:3.0.35-*) kernel_supported=1;;
esac
case "${1:---json}" in
 --json) printf '{"profile":"%s","modelPrefix":"%s","firmware":"%s","kernel":"%s","arch":"%s","qtCandidate":%s,"kernelSupported":%s}\n' "$profile" "$prefix" "$firmware" "$kernel" "$arch" "$qt_candidate" "$kernel_supported";;
 --shell) printf 'OCEAN_DEVICE_PROFILE=%s\nOCEAN_MODEL_PREFIX=%s\nOCEAN_FIRMWARE=%s\nOCEAN_KERNEL=%s\nOCEAN_ARCH=%s\nOCEAN_QT_CANDIDATE=%s\nOCEAN_KERNEL_SUPPORTED=%s\n' "$profile" "$prefix" "${firmware:-unknown}" "$kernel" "$arch" "$qt_candidate" "$kernel_supported";;
 *) exit 2;;
esac
