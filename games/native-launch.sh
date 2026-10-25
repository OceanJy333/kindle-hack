#!/bin/sh
# input: local game choice; output: silent native game process; pos: application-owned game lifecycle.
set -eu
root=/tmp/weread-native/games
mkdir -p /mnt/us/games
echo $$ >/tmp/game.pid
case "${1:-}" in
 doom) cd "$root/doom"; exec ./doom-voyage -iwad /mnt/us/games/doom1.wad -nosound -nomusic -config /mnt/us/games/doom.cfg -savedir /mnt/us/games -warp 1 1;;
 mslug) cd "$root/arcade";[ "${2:-}" != old ] || export ARCADE_WAVEFORM=old;exec ./arcade-voyage /mnt/us/games/roms/mslug.zip;;
 *) exit 2;;
esac
