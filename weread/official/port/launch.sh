#!/bin/sh
# input: mounted native runtime; output: silent Voyage Qt UI; pos: device launcher.
set -eu
root=/tmp/weread-native
export QT_QPA_PLATFORM=offscreen
export QSG_RENDER_LOOP=basic QT_QUICK_BACKEND=software
export QT_PLUGIN_PATH="$root/plugins"
export SSL_CERT_FILE="$root/ca-certificates.crt"
export OPENSSL_CONF="$root/config/openssl.cnf" FONTCONFIG_FILE="$root/fonts.conf"
export XDG_RUNTIME_DIR=/var/local/weread-port/runtime
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
echo $$ > /tmp/weread-login.pid
exec "$root/ld-linux-armhf.so.3" --library-path "$root" "$root/weread-login"
