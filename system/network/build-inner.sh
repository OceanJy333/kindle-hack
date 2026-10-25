#!/bin/sh
# input: extracted upstream sources and SSL sysroot; output: ARMv7 Wi-Fi tools; pos: isolated cross compilation.
set -eu
work=$1
export CC=arm-linux-gnueabihf-gcc AR=arm-linux-gnueabihf-ar
export CFLAGS='-O2 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -fPIC'
cd "$work/libnl-3.12.0"
./configure --host=arm-linux-gnueabihf --prefix="$work/prefix" --disable-cli --disable-pthreads --enable-static --disable-shared
make -j2
make install
cd "$work/wpa_supplicant-2.12/wpa_supplicant"
cp "$work/wpa.config" .config
cat >>.config <<CONFIG
CC=$CC
CFLAGS += -I$work/ssl/include -I$work/prefix/include/libnl3
LDFLAGS += -L$work/ssl/lib -L$work/prefix/lib
CONFIG
export PKG_CONFIG_PATH="$work/prefix/lib/pkgconfig"
make -j2 wpa_supplicant wpa_cli wpa_passphrase
mkdir -p /work/wifi
for name in wpa_supplicant wpa_cli wpa_passphrase; do
 arm-linux-gnueabihf-strip "$name"
 cp "$name" /work/wifi/
done
