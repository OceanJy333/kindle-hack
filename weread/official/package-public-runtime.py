#!/usr/bin/env python3
# input: compiled ARMhf runtime; output: redistributable app directory; pos: public release assembly.
from pathlib import Path
import argparse
import hashlib
import json
import shutil
from elf_deps import needed

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / 'build/weread/native-runtime'
PORT = Path(__file__).parent / 'port'
QML = ['ReaderApp.qml', 'LoginView.qml', 'ReadingView.qml', 'ReaderControls.qml',
       'Shelf.qml', 'OfflinePanel.qml', 'NotesPanel.qml']


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def package(stage):
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    base = BUILD / 'debian-root'
    libs = base / 'usr/lib/arm-linux-gnueabihf'
    replacements = {p.name: p for p in (BUILD / 'glibc-install').rglob('*')
                    if p.is_file() and '.so' in p.name}
    for name in ['libc.so.6', 'ld-linux-armhf.so.3']:
        if name not in replacements:
            raise FileNotFoundError('Build patched glibc first: ' + name)
    queue = ['ld-linux-armhf.so.3', 'libssl.so.3', 'libcrypto.so.3', 'plugins/tls/libqopensslbackend.so',
             'plugins/platforms/libqoffscreen.so', 'plugins/imageformats/libqsvg.so',
             'plugins/imageformats/libqjpeg.so']
    for module in ['QtQuick', 'QtQuick/Window', 'QtQml', 'QtQml/Models', 'QtQml/WorkerScript']:
        source = libs / 'qt6/qml' / module
        target = stage / 'qml-modules' / module
        target.mkdir(parents=True, exist_ok=True)
        for path in source.iterdir():
            if path.is_file():
                shutil.copyfile(path, target / path.name)
                if path.suffix == '.so':
                    queue.extend(needed(path))
    for name in ['weread-login', 'runtime-check']:
        shutil.copyfile(BUILD / name, stage / name)
        (stage / name).chmod(0o755)
        queue.extend(needed(BUILD / name))
    files = {}
    while queue:
        name = queue.pop()
        if name in files:
            continue
        source = replacements.get(name, libs / name)
        if name.startswith('plugins/'):
            source = libs / 'qt6' / name
        if not source.is_file():
            source = base / 'lib/arm-linux-gnueabihf' / name
        if not source.is_file():
            raise FileNotFoundError(source)
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o755)
        files[name] = digest(source)
        queue.extend(needed(source))
    for name in QML:
        shutil.copyfile(PORT / name, stage / name)
    (stage / 'fonts').mkdir()
    shutil.copyfile(ROOT / 'build/fonts/NotoSansCJKsc-Regular.otf', stage / 'fonts/NotoSansCJKsc-Regular.otf')
    shutil.copyfile(BUILD / 'ca-certificates.crt', stage / 'ca-certificates.crt')
    (stage / 'config').mkdir()
    (stage / 'config/openssl.cnf').write_text('# Bundled OpenSSL defaults.\n')
    (stage / 'fonts.conf').write_text('''<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig><dir>/tmp/weread-native/fonts</dir>
<cachedir>/mnt/us/ocean/state/font-cache</cachedir></fontconfig>
''')
    (stage / 'launch.sh').write_text('''#!/bin/sh
# input: public runtime; output: stock-system reader; pos: installed app entry.
set -eu
root=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
export OCEAN_STOCK_APP=1 QT_QPA_PLATFORM=offscreen
export QSG_RENDER_LOOP=basic QT_QUICK_BACKEND=software
export QT_PLUGIN_PATH="$root/plugins" SSL_CERT_FILE="$root/ca-certificates.crt"
export OPENSSL_CONF="$root/config/openssl.cnf" FONTCONFIG_FILE="$root/fonts.conf"
export WEREAD_STATE_DIR="${WEREAD_STATE_DIR:-/mnt/us/ocean/state/weread}"
export XDG_RUNTIME_DIR=/tmp/ocean-reader-runtime
mkdir -p "$XDG_RUNTIME_DIR" "$WEREAD_STATE_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
exec "$root/ld-linux-armhf.so.3" --library-path "$root" "$root/weread-login"
''')
    (stage / 'launch.sh').chmod(0o755)
    notices = stage / 'licenses/debian'
    for source in (base / 'usr/share/doc').glob('*/copyright'):
        target = notices / source.parent.name
        target.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target / 'copyright')
    (stage / 'runtime-manifest.json').write_text(json.dumps({
        'qt': '6.4.2', 'minimumKernel': '3.0.0', 'cpu': 'ARMv7-A/VFPv3-D16 hard-float',
        'privateAssetsIncluded': False, 'libraries': files,
        'qml': QML, 'applicationSha256': digest(stage / 'weread-login')}, indent=2) + '\n')
    print(json.dumps({'runtime': str(stage), 'libraries': len(files), 'privateAssetsIncluded': False}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=BUILD / 'public-runtime')
    package(parser.parse_args().output)
