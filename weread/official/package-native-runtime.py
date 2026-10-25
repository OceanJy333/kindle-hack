#!/usr/bin/env python3
# input: rebuilt ARMhf runtime and user-extracted QML; output: private ext2 image; pos: local package assembly.
from pathlib import Path
import argparse
import gzip
import hashlib
import json
import os
import shutil
import subprocess
from elf_deps import needed
from patch_qml import patch_reader

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / 'build/weread'
BUILD = LAB / 'native-runtime'
PORT = Path(__file__).parent / 'port'


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def package(games=False, mke2fs='mke2fs'):
    original = BUILD / 'debian-root/usr/lib/arm-linux-gnueabihf'
    replacements = {p.name: p for p in (BUILD / 'glibc-install').rglob('*')
                    if p.is_file() and '.so' in p.name}
    assert 'libc.so.6' in replacements and 'ld-linux-armhf.so.3' in replacements
    stage = BUILD / 'stage'
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir()
    queue = ['ld-linux-armhf.so.3', 'libQt6Core.so.6', 'libQt6Network.so.6',
             'libssl.so.3', 'libcrypto.so.3', 'plugins/tls/libqopensslbackend.so',
             'libQt6Quick.so.6', 'libQt6Qml.so.6', 'libQt6Gui.so.6', 'libqrencode.so.4',
             'plugins/platforms/libqoffscreen.so', 'plugins/imageformats/libqsvg.so',
             'plugins/imageformats/libqjpeg.so']
    modules = original / 'qt6/qml'
    shutil.copytree(modules, stage / 'qml-modules', symlinks=False)
    for library in modules.rglob('*.so'):
        queue.extend(needed(library))
    files = {}
    while queue:
        name = queue.pop()
        if name in files:
            continue
        source = replacements.get(name, original / name)
        if name.startswith('plugins/'):
            source = original / 'qt6' / name
        if not source.is_file():
            source = BUILD / 'debian-root/lib/arm-linux-gnueabihf' / name
        if not source.is_file():
            raise FileNotFoundError(source)
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
        target.chmod(0o755)
        files[name] = sha(source)
        queue.extend(needed(source))
    for name in ['runtime-check', 'weread-login', 'ca-certificates.crt']:
        shutil.copyfile(BUILD / name, stage / name)
    for name in ['runtime-check', 'weread-login']:
        (stage / name).chmod(0o755)
    subprocess.run(['python3', ROOT / 'system/build-power.py'], check=True)
    for name in ['ocean-powerd', 'ocean-powerctl']:
        shutil.copyfile(ROOT / 'build/independent-os' / name, stage / name)
        (stage / name).chmod(0o755)
    for source, target in [('device-start.sh', 'desk-launch.sh'), ('power-policy.sh', 'power-policy.sh')]:
        shutil.copyfile(ROOT / 'native' / source, stage / target)
    for source in PORT.iterdir():
        if source.suffix == '.qml' or source.name in ['fonts.conf', 'launch.sh']:
            shutil.copyfile(source, stage / source.name)
    (stage / 'launch.sh').chmod(0o755)
    shutil.copytree(LAB / 'official-recovered-resources', stage / 'original')
    patch_reader(stage)
    shutil.copyfile(PORT / 'NotesPanel.qml', stage / 'original/qml/pages/NotesPanel.qml')
    (stage / 'fonts').mkdir()
    shutil.copyfile(ROOT / 'build/fonts/NotoSansCJKsc-Regular.otf', stage / 'fonts/NotoSansCJKsc-Regular.otf')
    (stage / 'config').mkdir()
    (stage / 'config/openssl.cnf').write_text('# Use the bundled OpenSSL defaults.\n')
    if games:
        game_stage = stage / 'games'
        game_stage.mkdir()
        for name in ['catalog.json', 'native-launch.sh']:
            shutil.copyfile(ROOT / 'games' / name, game_stage / name)
        for folder, names in [('doom', ['doom-voyage', 'overlay.gray']),
                              ('arcade', ['arcade-voyage', 'overlay.gray', 'missing.gray'])]:
            target = game_stage / folder
            target.mkdir()
            for name in names:
                shutil.copyfile(ROOT / 'build' / folder / name, target / name)
                if name.endswith('-voyage'):
                    (target / name).chmod(0o755)
    for script in stage.rglob('*.sh'):
        script.chmod(0o755)
    image = BUILD / 'native-runtime.ext2'
    with image.open('wb') as stream:
        stream.truncate(192 * 1024 * 1024)
    subprocess.run([mke2fs, '-q', '-F', '-t', 'ext2', '-O', 'none', '-L', 'NATIVEQT',
                    '-d', str(stage), str(image)], check=True)
    archive = image.with_suffix('.ext2.gz')
    with image.open('rb') as source, gzip.open(archive, 'wb', compresslevel=1) as target:
        shutil.copyfileobj(source, target)
    receipt = {'libraries': files, 'imageSha256': sha(image), 'archiveSha256': sha(archive),
               'applicationSha256': sha(stage / 'weread-login'), 'runtimeQt': '6.4.2',
               'libc': '2.39 patched for Linux 3.0.0', 'runtimeValidated': False,
               'privateAssetsIncluded': True, 'retainedArtifact': archive.name}
    (BUILD / 'package.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps({'archive': str(archive), 'sha256': receipt['archiveSha256']}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--with-games', action='store_true')
    parser.add_argument('--mke2fs', default=os.environ.get('MKE2FS', 'mke2fs'))
    args = parser.parse_args()
    package(args.with_games, args.mke2fs)
