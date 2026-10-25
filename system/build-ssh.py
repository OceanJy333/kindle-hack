#!/usr/bin/env python3
# input: pinned Dropbear source; output: ARMhf key-only server; pos: independent SSH runtime build.
from pathlib import Path
from sources import SOURCES
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import tempfile

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'
ARCHIVE = SOURCES['dropbear']['archive']
EXPECTED = SOURCES['dropbear']['sha256']
ENV = os.environ.copy()


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def build():
    assert digest(WORK / ARCHIVE) == EXPECTED
    with tempfile.TemporaryDirectory(prefix='ssh-build-', dir=WORK) as folder:
        temporary = Path(folder)
        with tarfile.open(WORK / ARCHIVE) as archive:
            archive.extractall(temporary, filter='data')
        source = temporary / 'dropbear-2026.94'
        (source / 'localoptions.h').write_text('''/* Public keys are the sole remote login method. */
#define DROPBEAR_SVR_PASSWORD_AUTH 0
#define DROPBEAR_SVR_PAM_AUTH 0
#define DROPBEAR_DSS 0
#define DROPBEAR_SVR_AGENTFWD 0
''')
        device = Path('/work') / source.relative_to(WORK)
        prefix = ['docker', 'exec', '-w', str(device), 'voyage-os-build']
        subprocess.run(prefix + ['env', 'CC=arm-linux-gnueabihf-gcc',
                                'CFLAGS=-O2 -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard',
                                'LDFLAGS=-Wl,--as-needed', './configure', '--host=arm-linux-gnueabihf',
                                '--disable-zlib', '--disable-shadow', '--disable-syslog',
                                '--disable-lastlog', '--disable-utmp', '--disable-utmpx',
                                '--disable-wtmp', '--disable-wtmpx'], env=ENV, check=True)
        subprocess.run(prefix + ['make', '-j2', 'PROGRAMS=dropbear dropbearkey'], env=ENV, check=True)
        destination = WORK / 'ssh'
        destination.mkdir(exist_ok=True)
        for name in ['dropbear', 'dropbearkey']:
            subprocess.run(prefix + ['arm-linux-gnueabihf-strip', name], env=ENV, check=True)
            shutil.copyfile(source / name, destination / name)
            (destination / name).chmod(0o755)
        receipt = {'upstream': SOURCES['dropbear']['url'],
                   'sourceSha256': EXPECTED, 'floatAbi': 'hard', 'passwordAuth': False,
                   'binaries': {name: digest(destination / name) for name in ['dropbear', 'dropbearkey']}}
        (destination / 'manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    build()
