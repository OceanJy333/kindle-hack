#!/usr/bin/env python3
# input: pinned barebox release and Voyage board config; output: replacement boot image; pos: modern boot-chain build.
from pathlib import Path
from sources import SOURCES
import hashlib
import json
import os
import subprocess
import uuid

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'
ARCHIVE = SOURCES['barebox']['archive']
EXPECTED = SOURCES['barebox']['sha256']
ENV = os.environ.copy()


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def build():
    assert digest(WORK / ARCHIVE) == EXPECTED
    temporary = Path('/tmp') / ('ocean-barebox-' + uuid.uuid4().hex)
    def execute(*args):
        return subprocess.check_output(['docker', 'exec', 'voyage-os-build', *map(str, args)], env=ENV)
    execute('mkdir', temporary)
    try:
        # barebox contains scripts/Kconfig and scripts/kconfig; extract on Linux.
        execute('tar', 'xf', Path('/work') / ARCHIVE, '-C', temporary)
        device_source = temporary / 'barebox-2026.09.0'
        device_output = temporary / 'build'
        command = ['docker', 'exec', '-w', str(device_source), 'voyage-os-build', 'make',
                   'ARCH=arm', 'CROSS_COMPILE=arm-linux-gnueabi-', 'O=' + str(device_output)]
        subprocess.run(command + ['kindle-gen-6-7_defconfig'], env=ENV, check=True)
        config = device_output / '.config'
        # Field installation never needs irreversible SoC fuse programming.
        execute('python3', '-c', 'from pathlib import Path; import sys; p=Path(sys.argv[1]); p.write_text(p.read_text().replace("CONFIG_IMX_IIM_FUSE_BLOW=y", "# CONFIG_IMX_IIM_FUSE_BLOW is not set"))', config)
        subprocess.run(command + ['olddefconfig'], env=ENV, check=True)
        subprocess.run(command + ['-j2'], env=ENV, check=True)
        destination = WORK / 'bootloader'
        destination.mkdir(exist_ok=True)
        image = device_output / 'images/barebox-kindle-nm460gz.img'
        size = int(execute('stat', '-c', '%s', image))
        assert size <= 384000, size
        for origin, target in [(image, destination / image.name), (config, destination / 'barebox.config')]:
            subprocess.run(['docker', 'cp', 'voyage-os-build:' + str(origin), str(target)], env=ENV, check=True)
        receipt = {'upstream': SOURCES['barebox']['url'],
                   'sourceSha256': EXPECTED, 'board': 'NM460GZ',
                   'imageBytes': size, 'imageSha256': digest(destination / image.name),
                   'configSha256': digest(destination / 'barebox.config'), 'fuseWriteEnabled': False,
                   'physicalBootValidated': False, 'recoveryValidated': False}
        (destination / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps(receipt, indent=2))
    finally:
        execute('python3', '-c', 'import shutil,sys; shutil.rmtree(sys.argv[1])', temporary)


if __name__ == '__main__':
    build()
