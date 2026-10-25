#!/usr/bin/env python3
# input: verified original rootfs backup; output: private driver firmware archive; pos: offline hardware provisioning.
from pathlib import Path
from host_tools import E2FS
import gzip
import hashlib
import json
import posixpath
import shutil
import subprocess
import tarfile
import tempfile

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'
BACKUP = WORK / 'private-backup'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def extract():
    original = BACKUP / 'rootfs.ext3.gz'
    expected = json.loads((BACKUP / 'manifest.json').read_text())['rootfs.ext3.gz']['sha256']
    assert digest(original) == expected
    destination = BACKUP / 'hardware'
    destination.mkdir(mode=0o700, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='firmware-extract-', dir=WORK) as folder:
        temporary = Path(folder)
        image = temporary / 'rootfs.ext3'
        with gzip.open(original, 'rb') as source, image.open('wb') as target:
            shutil.copyfileobj(source, target)
        tree = temporary / 'tree'
        paths = ['lib/firmware', 'opt/ar6k/target/AR6003/hw2.1.1/bin']
        for name in paths:
            target = tree / name
            target.parent.mkdir(parents=True)
            result = subprocess.run([str(E2FS / 'debugfs'), '-R',
                                     f'rdump /{name} {target.parent}', str(image)],
                                    text=True, capture_output=True, check=True)
            assert any(target.iterdir()), result.stderr
        files = {str(p.relative_to(tree)): digest(p) for p in tree.rglob('*') if p.is_file()}
        assert 'opt/ar6k/target/AR6003/hw2.1.1/bin/fw-4.bin' in files
        partial = destination / 'firmware.tar.gz.partial'
        def ownership(info):
            info.uid = info.gid = 0
            info.uname = info.gname = 'root'
            if info.issym() and info.linkname.startswith('/'):
                info.linkname=posixpath.relpath(info.linkname.lstrip('/'),posixpath.dirname(info.name))
            return info
        with tarfile.open(partial, 'w:gz') as archive:
            for name in paths:
                archive.add(tree / name, arcname=name, filter=ownership)
        partial.chmod(0o600)
        partial.replace(destination / 'firmware.tar.gz')
        receipt = {'sourceRootfsSha256': expected,
                   'archiveSha256': digest(destination / 'firmware.tar.gz'), 'files': files}
        (destination / 'firmware-backup.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps({'firmwareFiles': len(files), 'archiveBytes': (destination / 'firmware.tar.gz').stat().st_size}))


if __name__ == '__main__':
    extract()
