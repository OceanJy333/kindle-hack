#!/usr/bin/env python3
# input: pinned Dropbear and Zig 0.15.2; output: static ARM soft-float SSH tools; pos: stock Kindle access bootstrap.
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import struct
import tarfile
import tempfile
from sources import SOURCES

SOURCE = Path(__file__).resolve().parent
BUILD = SOURCE.parent / 'build'
PIN = SOURCES['dropbear']
ZIG = os.environ.get('ZIG', shutil.which('zig') or str(BUILD / 'native-deps/zig/zig'))


def build():
    version = subprocess.check_output([ZIG, 'version'], text=True).strip()
    if version != '0.15.2':
        raise ValueError('Expected Zig 0.15.2')
    archive = BUILD / 'independent-os' / PIN['archive']
    assert hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest() == PIN['sha256']
    destination = BUILD / 'stock-ssh'
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='stock-ssh-', dir=destination) as folder:
        temporary = Path(folder)
        with tarfile.open(archive) as packed:
            packed.extractall(temporary, filter='data')
        source = temporary / 'dropbear-2026.94'
        (source / 'localoptions.h').write_text('''/* Key-only stock-system bootstrap. */
#define DROPBEAR_SVR_PASSWORD_AUTH 0
#define DROPBEAR_SVR_PAM_AUTH 0
#define DROPBEAR_DSS 0
#define DROPBEAR_SVR_AGENTFWD 0
''')
        # configure invokes CC as a command; a wrapper keeps paths with spaces intact.
        compiler = temporary / 'cc'
        compiler.write_text('#!/bin/sh\nexec "$ZIG" cc -target arm-linux-musleabi -mcpu=cortex_a9 "$@"\n')
        compiler.chmod(0o755)
        wrappers = {}
        for tool in ['ar', 'ranlib']:
            path = temporary / tool
            path.write_text('#!/bin/sh\nexec "$ZIG" ' + tool + ' "$@"\n')
            path.chmod(0o755)
            wrappers[tool] = str(path)
        environment = dict(os.environ, ZIG=str(Path(ZIG).resolve()), CC=str(compiler),
                           CFLAGS='-Os', LDFLAGS='-static', AR=wrappers['ar'], RANLIB=wrappers['ranlib'])
        subprocess.run([source / 'configure', '--host=arm-linux-musleabi', '--disable-zlib',
                        '--disable-shadow', '--disable-syslog', '--disable-lastlog', '--disable-harden',
                        '--disable-utmp', '--disable-utmpx', '--disable-wtmp', '--disable-wtmpx'],
                       cwd=source, env=environment, check=True)
        subprocess.run(['make', '-j2', 'PROGRAMS=dropbear dropbearkey'],
                       cwd=source, env=environment, check=True)
        binaries = {}
        for name in ['dropbear', 'dropbearkey']:
            shutil.copyfile(source / name, destination / name)
            (destination / name).chmod(0o755)
            data = (destination / name).read_bytes()
            assert data[:6] == b'\x7fELF\x01\x01' and struct.unpack_from('<H', data, 18)[0] == 40
            offset = struct.unpack_from('<I', data, 28)[0]
            size, count = struct.unpack_from('<HH', data, 42)
            segments = [struct.unpack_from('<I', data, offset + n * size)[0] for n in range(count)]
            assert 2 not in segments and 3 not in segments, 'Expected static ELF without a loader'
            assert struct.unpack_from('<I', data, 36)[0] & 0x400 == 0, 'Expected soft-float ABI'
            binaries[name] = hashlib.sha256(data).hexdigest()
        report = {'source': PIN, 'compiler': 'Zig ' + version,
                  'target': 'arm-linux-musleabi', 'cpu': 'cortex_a9', 'static': True,
                  'passwordAuth': False, 'binaries': binaries}
        (destination / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    build()
