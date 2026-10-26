#!/bin/sh
# input: Node 24+, Python 3.12+ and Docker; output: USB-ready release and sources; pos: repeatable developer build.
set -eu
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
python3 tools/fetch-deps.py
npm ci --prefix native
node --test native/test.ts native/notifications.test.ts
python3 doom/build.py
python3 arcade/build.py
mkdir -p build/installer
build/native-deps/zig/zig cc -target arm-linux.2.6.31-musleabi -mcpu=cortex_a8 \
  -Os -static -s -Wall -Wextra -Werror installer/device-probe.c -o build/installer/device-probe
docker build -t kindle-hack-weread \
  -f weread/official/native-build.Dockerfile weread/official
for task in prepare-runtime.sh build-native-libc.sh port/build.sh; do
  docker run --rm -e "JOBS=${JOBS:-2}" \
    -v "$repo:/src:ro" -v "$repo/build/weread:/lab" \
    kindle-hack-weread sh "/src/weread/official/$task"
done
python3 weread/official/package-public-runtime.py
python3 weread/official/source-public-runtime.py --fetch
python3 -m unittest discover -s installer/tests -v
python3 tools/package-release.py "$@"
