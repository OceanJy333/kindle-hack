#!/usr/bin/env python3
# input: pinned upstream archives and OpenSSL headers; output: native Wi-Fi binaries; pos: reproducible network build.
from pathlib import Path
from sources import SOURCES as UPSTREAM
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import tempfile

SOURCE = Path(__file__).resolve().parent
REPO = SOURCE.parent
LAB = REPO / 'build'
WORK = LAB / 'independent-os'
DEBIAN = LAB / 'weread/native-runtime/debian-root/usr'
ENV = os.environ.copy()
SOURCES = {UPSTREAM[name]['archive']: UPSTREAM[name]['sha256']
           for name in ['wpa_supplicant', 'libnl']}


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def build():
    with tempfile.TemporaryDirectory(prefix='wifi-build-', dir=WORK) as folder:
        temporary = Path(folder)
        for name, expected in SOURCES.items():
            archive = WORK / name
            assert digest(archive) == expected, archive
            with tarfile.open(archive) as source:
                source.extractall(temporary, filter='data')
        headers = temporary / 'ssl/include/openssl'
        shutil.copytree(DEBIAN / 'include/openssl', headers)
        shutil.copytree(DEBIAN / 'include/arm-linux-gnueabihf/openssl', headers, dirs_exist_ok=True)
        libraries = temporary / 'ssl/lib'
        libraries.mkdir()
        for name in ['ssl', 'crypto']:
            shutil.copyfile(DEBIAN / f'lib/arm-linux-gnueabihf/lib{name}.so.3', libraries / f'lib{name}.so.3')
            (libraries / f'lib{name}.so').symlink_to(f'lib{name}.so.3')
        device_path = Path('/work') / temporary.relative_to(WORK)
        for name in ['build-inner.sh', 'wpa.config']:
            shutil.copyfile(SOURCE / 'network' / name, temporary / name)
        subprocess.run(['docker', 'exec', 'voyage-os-build', 'sh', str(device_path / 'build-inner.sh'), str(device_path)],
                       env=ENV, check=True)
        receipt = {'sources': SOURCES, 'configurationSha256': digest(SOURCE / 'network/wpa.config'),
                   'cpuTranslation': False, 'physicalAssociationValidated': False,
                   'binaries': {name: digest(WORK / 'wifi' / name)
                                for name in ['wpa_supplicant', 'wpa_cli', 'wpa_passphrase']}}
        (WORK / 'wifi/manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    build()
