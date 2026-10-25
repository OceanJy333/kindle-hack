#!/bin/sh
# input: kernel CPUFREQ policy; output: demand scaling and reversible original policy; pos: Voyage desktop power control.
set -eu
cpu=/sys/devices/system/cpu/cpu0/cpufreq
saved=/tmp/agent-board-cpufreq.original
[ -r "$cpu/scaling_governor" ] || exit 0
case "${1:-status}" in
  apply)
    current=$(cat "$cpu/scaling_governor")
    # Keep an existing dynamic policy. Only replace the fixed-high-frequency default.
    [ "$current" = performance ] || exit 0
    grep -qw ondemand "$cpu/scaling_available_governors" || exit 0
    [ -f "$saved" ] || printf '%s\n' "$current" > "$saved"
    printf '%s\n' ondemand > "$cpu/scaling_governor"
    ;;
  restore)
    if [ -f "$saved" ]; then
      original=$(cat "$saved")
      if [ "$(cat "$cpu/scaling_governor")" = ondemand ] && [ "$original" = performance ]; then
        printf '%s\n' "$original" > "$cpu/scaling_governor"
      fi
      unlink "$saved"
    fi
    ;;
  status)
    for name in scaling_governor scaling_cur_freq scaling_min_freq scaling_max_freq; do
      printf '%s=' "$name";cat "$cpu/$name"
    done
    ;;
  *) exit 2;;
esac
