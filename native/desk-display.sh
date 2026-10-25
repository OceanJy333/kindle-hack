#!/bin/sh
# input: local Qt renderer; output: local display process; pos: resume from native game.
set -eu
if [ -r /tmp/agent-board.pid ]; then
 p=$(cat /tmp/agent-board.pid)
 if [ -r /proc/"$p"/cmdline ] && tr '\000' ' ' </proc/"$p"/cmdline | grep -q /tmp/agent-board; then exit 0; fi
fi
cp /tmp/weread-native/agent-board /tmp/agent-board
chmod 700 /tmp/agent-board
/tmp/agent-board >/tmp/agent-board.log 2>&1 &
echo $! >/tmp/agent-board.pid
