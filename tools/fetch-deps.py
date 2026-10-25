#!/usr/bin/env python3
# input: checked-in dependency lock; output: verified local build dependencies; pos: source bootstrap.
from pathlib import Path
import hashlib
import json
import platform
import shutil
import tarfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / 'build'
LOCK = json.loads((Path(__file__).with_name('dependencies.json')).read_text())


def download(url, checksum, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest() != checksum:
        print('Downloading', url, flush=True)
        with urllib.request.urlopen(url) as source, target.open('wb') as out:
            shutil.copyfileobj(source, out)
    if hashlib.sha256(target.read_bytes()).hexdigest() != checksum:
        raise RuntimeError('Checksum mismatch: ' + str(target))


def unpack(archive, destination):
    if destination.is_dir():
        return
    temporary = destination.with_name(destination.name + '.unpack')
    temporary.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive) as source:
        source.extractall(temporary, filter='data')
    roots = list(temporary.iterdir())
    if len(roots) != 1 or not roots[0].is_dir():
        raise RuntimeError('Expected one source root in ' + str(archive))
    roots[0].rename(destination)
    temporary.rmdir()


def main():
    for name, destination in [('doomgeneric', 'doom/doomgeneric-master'),
                              ('fbalpha2012_neogeo', 'arcade/fbalpha2012_neogeo-master'),
                              ('zlib', 'native-deps/zlib-1.3.1')]:
        item = LOCK[name]
        archive = BUILD / item['archive']
        download(item['url'], item['sha256'], archive)
        unpack(archive, BUILD / destination)
    for name, item in LOCK.items():
        if name.startswith('font-'):
            download(item['url'], item['sha256'], BUILD / item['file'])
    cpu = {'arm64': 'aarch64', 'aarch64': 'aarch64', 'x86_64': 'x86_64', 'AMD64': 'x86_64'}[platform.machine()]
    system = {'Darwin': 'macos', 'Linux': 'linux'}[platform.system()]
    item = LOCK['zig'][cpu + '-' + system]
    archive = BUILD / 'native-deps' / Path(item['tarball']).name
    download(item['tarball'], item['shasum'], archive)
    unpack(archive, BUILD / 'native-deps/zig')
    print('Dependencies ready in', BUILD)


if __name__ == '__main__':
    main()
