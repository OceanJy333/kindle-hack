#!/bin/sh
# input: host Qt 6 development packages; output: real Reader/QML integration renders; pos: host UI verification.
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output=${1:-/tmp/reader-ui-test}
mkdir -p "$output"
if [ ! -f "$output/moc_reader.cpp" ] || [ "$source_dir/reader.h" -nt "$output/moc_reader.cpp" ]; then
    /usr/lib/qt6/libexec/moc "$source_dir/reader.h" -o "$output/moc_reader.cpp"
fi
for unit in reader-ui-test reader reader-images reader-progress reader-marks offline-store moc_reader; do
    file="$source_dir/$unit.cpp"
    if [ "$unit" = moc_reader ]; then file="$output/moc_reader.cpp"; fi
    if [ ! -f "$output/$unit.o" ] || [ "$file" -nt "$output/$unit.o" ] || [ "$source_dir/reader.h" -nt "$output/$unit.o" ]; then
        c++ -std=c++17 -fPIC $(pkg-config --cflags Qt6Quick Qt6Network) -I"$source_dir" -c "$file" -o "$output/$unit.o"
    fi
done
c++ "$output/"*.o $(pkg-config --libs Qt6Quick Qt6Network) -o "$output/reader-ui-test"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software "$output/reader-ui-test" "$source_dir" "$output"
