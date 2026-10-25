#!/bin/sh
# input: pinned toolchain image and glibc archive; output: local ARMhf build inputs; pos: container preparation.
set -eu
mkdir -p /lab/native-runtime
cd /lab/native-runtime
mkdir -p debian-root
cp -a /opt/armhf/. debian-root/
mkdir -p qt-headers/usr/include
cp -a /opt/armhf/usr/include/arm-linux-gnueabihf qt-headers/usr/include/
cp /opt/armhf-packages.sha256 packages.sha256
cp /etc/ssl/certs/ca-certificates.crt .
curl -fL --retry 3 https://ftp.gnu.org/gnu/glibc/glibc-2.39.tar.xz -o glibc-2.39.tar.xz
printf '%s  %s\n' f77bd47cf8170c57365ae7bf86696c118adb3b120d3259c64c502d3dc1e2d926 glibc-2.39.tar.xz | sha256sum -c -
