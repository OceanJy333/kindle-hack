#!/bin/sh
# input: existing KMC boot hook; output: starts the internal-partition SSH guard; pos: secondary original-system recovery entry.
# Primary entry is /etc/upstart/ocean-connect.conf; this covers boots where that job was removed by an update.
initctl status ocean-connect 2>/dev/null | grep -q running && exit 0
[ -x /var/local/ocean/connect/ocean-connectd.sh ] || exit 0
/bin/sh /var/local/ocean/connect/ocean-connectd.sh </dev/null >/dev/null 2>&1 &
exit 0
