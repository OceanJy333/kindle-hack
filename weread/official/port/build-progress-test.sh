#!/bin/sh
# input: native reader sources and ARM Qt; output: standalone regression binary; pos: device test build.
set -eu
/src/weread/official/port/build.sh --checks-only
cd /lab/native-runtime/port
headers=/lab/native-runtime/qt-headers/usr/include/arm-linux-gnueabihf/qt6
libs=/lab/native-runtime/debian-root/usr/lib/arm-linux-gnueabihf
/usr/lib/qt6/libexec/moc -I"$headers" -I"$headers/QtCore" reader.h -o moc_reader.cpp
arm-linux-gnueabihf-g++ -std=c++17 -O2 -fPIC -Wno-psabi -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard \
  -I"$headers" -I"$headers/QtCore" -I"$headers/QtNetwork" -I"$headers/QtGui" -I"$headers/QtQml" -I"$headers/QtQuick" \
  offline-store.cpp progress-test.cpp progress-protocol.cpp reader.cpp reader-progress.cpp reader-images.cpp reader-marks.cpp moc_reader.cpp \
  -L"$libs" -Wl,--allow-shlib-undefined -l:libQt6Quick.so.6 -l:libQt6Qml.so.6 -l:libQt6Gui.so.6 -l:libQt6Network.so.6 -l:libQt6Core.so.6 \
  -o /lab/native-runtime/progress-test
