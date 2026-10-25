#!/bin/sh
# input: glibc 2.39 source and ARM32 GCC; output: isolated runtime; pos: old-kernel compatibility build.
set -eu
mkdir -p /work
cd /work
test -d glibc-2.39 || tar -xJf /lab/native-runtime/glibc-2.39.tar.xz
python3 - <<'PY'
from pathlib import Path
for name in ('configure', 'configure.ac'):
    path = Path('glibc-2.39/sysdeps/unix/sysv/linux') / name
    text = path.read_text()
    # Keep modern headers, but compile optional syscall fallbacks for the actual kernel.
    old = 'arch_minimum_kernel=3.2.0'
    new = 'arch_minimum_kernel=3.0.0'
    if old not in text and new not in text:
        raise SystemExit(f'Unexpected upstream configuration: {path}')
    path.write_text(text.replace(old, new))
path = Path('glibc-2.39/stdlib/arc4random.c')
text = path.read_text()
# glibc BZ 31612: INLINE_SYSCALL_CALL returns -1 and sets errno, not -ENOSYS.
# Preserve the upstream /dev/random initialization wait and /dev/urandom fallback.
old = 'else if (l == -ENOSYS)'
new = 'else if (l < 0 && errno == ENOSYS)'
if old not in text and new not in text:
    raise SystemExit('Unexpected arc4random implementation')
path.write_text(text.replace(old, new))
PY
mkdir -p glibc-build
cd glibc-build
../glibc-2.39/configure --host=arm-linux-gnueabihf --build="$(gcc -dumpmachine)" \
    --prefix=/usr --enable-kernel=3.0.0 --disable-werror --disable-nscd \
    CC='arm-linux-gnueabihf-gcc -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard' \
    CXX='arm-linux-gnueabihf-g++ -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard' \
    CFLAGS='-O2 -g0' > /lab/native-runtime/configure.log 2>&1
make -j4 > /lab/native-runtime/build.log 2>&1
make install DESTDIR=/lab/native-runtime/glibc-install > /lab/native-runtime/install.log 2>&1
arm-linux-gnueabihf-readelf -n libc.so | tail -8
