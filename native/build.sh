#!/bin/sh
# input: this native source and running emulator; output: compiled unified device app; pos: repeatable local launch.
set -eu
native_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH='' cd -- "$native_dir/.." && pwd)
lab_dir="$repo_dir/build"
zig="$lab_dir/native-deps/zig/zig"
zlib="$lab_dir/native-deps/zlib-1.3.1"
# Cortex-A8 is the PW1 ARMv7 baseline, also supported by later Cortex-A9 Kindles.
"$zig" cc -target arm-linux.2.6.31-musleabi -mcpu=cortex_a8 -Os -static -I "$zlib" "$native_dir/client.c" "$native_dir/board-link.c" "$native_dir/device-settings.c" "$native_dir/display.c" "$native_dir/display-geometry.c" "$native_dir/input.c" "$native_dir/../weread/official/rfb-client.c" \
  "$zlib/adler32.c" "$zlib/crc32.c" "$zlib/inflate.c" "$zlib/inftrees.c" "$zlib/inffast.c" "$zlib/zutil.c" "$zlib/uncompr.c" -o "$lab_dir/agent-board"

"$zig" cc -target arm-linux.2.6.31-musleabi -mcpu=cortex_a8 -Os -static -I "$zlib" "$native_dir/notify-client.c" "$native_dir/board-link.c" "$native_dir/device-settings.c" "$native_dir/display.c" "$native_dir/display-geometry.c" "$native_dir/input.c" \
  "$zlib/adler32.c" "$zlib/crc32.c" "$zlib/inflate.c" "$zlib/inftrees.c" "$zlib/inffast.c" "$zlib/zutil.c" "$zlib/uncompr.c" -o "$lab_dir/agent-notify"
