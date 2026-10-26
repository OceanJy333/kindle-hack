#!/bin/sh
# input: pinned toolchain image and glibc archive; output: local ARMhf build inputs; pos: container preparation.
set -eu
mkdir -p /lab/native-runtime
cd /lab/native-runtime
mkdir -p debian-root/usr/lib debian-root/lib debian-root/usr/include/arm-linux-gnueabihf
# Copy only build/runtime inputs. Linux man pages and headers contain case-only
# names and dangling links that cannot be materialized on a default macOS volume.
for prefix in usr/lib lib; do
  if [ -d "/opt/armhf/$prefix/arm-linux-gnueabihf" ]; then
    mkdir -p "debian-root/$prefix/arm-linux-gnueabihf"
    cp -a "/opt/armhf/$prefix/arm-linux-gnueabihf/." "debian-root/$prefix/arm-linux-gnueabihf/"
  fi
done
cp debian-root/lib/arm-linux-gnueabihf/ld-linux-armhf.so.3 debian-root/lib/ld-linux-armhf.so.3
cp -aL /opt/armhf/usr/include/openssl debian-root/usr/include/
cp -aL /opt/armhf/usr/include/qrencode.h /opt/armhf/usr/include/zlib.h /opt/armhf/usr/include/zconf.h debian-root/usr/include/
cp -aL /opt/armhf/usr/include/arm-linux-gnueabihf/openssl debian-root/usr/include/arm-linux-gnueabihf/
mkdir -p qt-headers/usr/include
mkdir -p qt-headers/usr/include/arm-linux-gnueabihf
cp -aL /opt/armhf/usr/include/arm-linux-gnueabihf/qt6 qt-headers/usr/include/arm-linux-gnueabihf/
python3 - <<'PY'
from pathlib import Path
import shutil
for source in Path('/opt/armhf/usr/share/doc').glob('*/copyright'):
    if source.is_file():
        target = Path('debian-root/usr/share/doc') / source.parent.name / 'copyright'
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
PY
cp /opt/armhf-packages.sha256 packages.sha256
cp /opt/debian-package-index.json /opt/debian-package-index.controls.txt .
cp /etc/ssl/certs/ca-certificates.crt .
curl -fL --retry 3 https://ftp.gnu.org/gnu/glibc/glibc-2.39.tar.xz -o glibc-2.39.tar.xz
printf '%s  %s\n' f77bd47cf8170c57365ae7bf86696c118adb3b120d3259c64c502d3dc1e2d926 glibc-2.39.tar.xz | sha256sum -c -
