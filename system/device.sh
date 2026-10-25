#!/bin/sh
# input: remote command and paired key; output: device SSH session; pos: Voyage development transport.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
access=${KINDLE_ACCESS_DIR:-$repo/build/device-install/physical-access}
host=${KINDLE_HOST:-192.168.77.2}
exec ssh -i "${KINDLE_KEY:-$access/id_ed25519}" \
 -o "UserKnownHostsFile=$access/known_hosts" -o IdentitiesOnly=yes \
 -o ServerAliveInterval=5 -o ServerAliveCountMax=6 \
 -o BatchMode=yes -o ConnectTimeout=4 -p "${KINDLE_PORT:-2222}" "root@$host" "$@"
