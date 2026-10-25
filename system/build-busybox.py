#!/usr/bin/env python3
# input: pinned BusyBox source and checked-in config; output: ARMhf runtime utility; pos: base userspace build.
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
from sources import SOURCES

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'
PIN = SOURCES['busybox']


def build():
    archive = WORK / PIN['archive']
    assert hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest() == PIN['sha256']
    with tempfile.TemporaryDirectory(prefix='busybox-build-', dir=WORK) as folder:
        temporary = Path(folder)
        with tarfile.open(archive) as packed:
            packed.extractall(temporary, filter='data')
        source = temporary / 'busybox-1.36.1'
        shutil.copyfile(SOURCE / 'configs/busybox.config', source / '.config')
        device = Path('/work') / source.relative_to(WORK)
        command = ['docker', 'exec', '-w', str(device), 'voyage-os-build', 'make',
                   'CROSS_COMPILE=arm-linux-gnueabihf-']
        subprocess.run(command + ['oldconfig'], input=b'\n' * 2048, env=os.environ.copy(), check=True)
        subprocess.run(command + ['-j2'], env=os.environ.copy(), check=True)
        destination = WORK / 'artifacts'
        destination.mkdir(exist_ok=True)
        shutil.copyfile(source / 'busybox', destination / 'busybox')
        (destination / 'busybox').chmod(0o755)
        shutil.copyfile(source / '.config', destination / 'busybox.config')
        report = {'source': PIN, 'busyboxSha256': hashlib.file_digest(
            (destination / 'busybox').open('rb'), 'sha256').hexdigest()}
        (WORK / 'busybox-build-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    build()
