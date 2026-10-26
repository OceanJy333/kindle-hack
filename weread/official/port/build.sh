#!/bin/sh
# input: repository sources and prepared ARMhf Qt; output: ARMv7 binaries; pos: container cross build.
set -eu
build=/lab/native-runtime
source=/src/weread/official/port
headers=$build/qt-headers/usr/include/arm-linux-gnueabihf/qt6
libs=$build/debian-root/usr/lib/arm-linux-gnueabihf
mkdir -p "$build/port"
cp "$source/"*.cpp "$source/"*.h "$build/port/"
cd "$build/port"
if [ "${1:-}" != --checks-only ]; then
  for name in auth library notes reader voyage eink desk power-client wifi; do
    /usr/lib/qt6/libexec/moc -I"$headers" -I"$headers/QtCore" "$name.h" -o "moc_$name.cpp"
  done
  for name in display display-geometry input; do
    arm-linux-gnueabihf-gcc -O2 -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard \
      -I/src/native -c "/src/native/$name.c" -o "$name-native.o"
  done
  arm-linux-gnueabihf-g++ -std=c++17 -O2 -fPIC -Wno-psabi -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard \
    -I"$headers" -I"$headers/QtCore" -I"$headers/QtNetwork" -I"$headers/QtGui" \
    -I"$headers/QtQml" -I"$headers/QtQuick" -I/src/native -I"$build/debian-root/usr/include" \
    -I"$build/debian-root/usr/include/arm-linux-gnueabihf" \
    progress.cpp progress-network.cpp progress-protocol.cpp reader-progress.cpp offline-store.cpp offline.cpp \
    main.cpp auth.cpp network.cpp library.cpp notes.cpp notes-reviews.cpp chapter.cpp chapter-key.cpp archive.cpp \
    display-native.o display-geometry-native.o input-native.o reader.cpp reader-images.cpp reader-marks.cpp moc_*.cpp \
    -L"$libs" -Wl,--sysroot="$build/debian-root",--allow-shlib-undefined -l:libQt6Quick.so.6 -l:libQt6Qml.so.6 \
    -l:libQt6Gui.so.6 -l:libQt6Network.so.6 -l:libQt6Core.so.6 -lqrencode -l:libz.a -l:libcrypto.so.3 \
    -o "$build/weread-login"
fi
arm-linux-gnueabihf-g++ -std=c++17 -O2 -fPIC -Wno-psabi -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard \
  -I"$headers" -I"$headers/QtCore" -I"$headers/QtNetwork" wifi-data-test.cpp \
  -L"$libs" -Wl,--sysroot="$build/debian-root",--allow-shlib-undefined -l:libQt6Network.so.6 -l:libQt6Core.so.6 -o "$build/wifi-data-check"
arm-linux-gnueabihf-g++ -std=c++17 -O2 -fPIC -Wno-psabi -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard \
  -I"$headers" -I"$headers/QtCore" -I"$headers/QtGui" -I"$headers/QtQml" -I"$headers/QtQuick" settings-preview.cpp \
  -L"$libs" -Wl,--sysroot="$build/debian-root",--allow-shlib-undefined -l:libQt6Quick.so.6 -l:libQt6Qml.so.6 -l:libQt6Gui.so.6 -l:libQt6Core.so.6 \
  -o "$build/settings-preview"
arm-linux-gnueabihf-g++ -std=c++17 -O2 -fPIC -Wno-psabi -march=armv7-a -mfpu=vfpv3-d16 -mfloat-abi=hard \
  -I"$headers" -I"$headers/QtCore" -I"$headers/QtNetwork" /src/weread/official/runtime-check.cpp \
  -L"$libs" -Wl,--sysroot="$build/debian-root",--allow-shlib-undefined -l:libQt6Network.so.6 -l:libQt6Core.so.6 -o "$build/runtime-check"
