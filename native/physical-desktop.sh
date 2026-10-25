#!/bin/sh
# Name: Ocean Desk
# Author: Ocean
# DontUseFBInk
# input: installed runtime and paired computer address; output: native desktop; pos: Kindle library launcher.
set -eu
host=$(cat /mnt/us/ocean-install/board-host)
export BOARD_HOST="$host"
# Leave the library callback before its framework is stopped by the desktop.
nohup /bin/sh -c 'sleep 2; exec sh /mnt/us/ocean-install/device-start.sh' >/mnt/us/ocean-install/desktop-launch.log 2>&1 </dev/null &
