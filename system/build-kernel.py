#!/usr/bin/env python3
# input: pinned GPL source, board config and patches; output: Voyage kernel/modules; pos: reproducible firmware build.
from pathlib import Path
from sources import KERNEL_COMMIT, SOURCES
import hashlib
import json
import os
import subprocess
import tempfile
import uuid

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'
ENV = os.environ.copy()
COMMIT = KERNEL_COMMIT


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def build():
    assert digest(WORK / SOURCES['kernel']['archive']) == SOURCES['kernel']['sha256']
    (WORK / 'artifacts').mkdir(parents=True, exist_ok=True)
    scratch = '/tmp/ocean-kernel-' + uuid.uuid4().hex
    log = WORK / 'kernel-final-build.log'
    def run(*args):
        with log.open('ab') as output:
            subprocess.run(['docker', 'exec', 'voyage-os-build', *map(str, args)],
                           env=ENV, stdout=output, stderr=subprocess.STDOUT, check=True)
    def copy(source, target):
        subprocess.run(['docker', 'cp', str(source), 'voyage-os-build:' + str(target)], env=ENV, check=True)
    log.write_text('Ocean Voyage kernel build\n')
    run('mkdir', scratch)
    try:
        run('tar', 'xf', '/work/linux-source.tar.gz', '-C', scratch)
        kernel = Path(scratch) / ('linux-3.0.35-lab126-' + COMMIT)
        output = Path(scratch) / 'build'
        install = Path(scratch) / 'install'
        run('mkdir', output, install)
        copy(SOURCE / 'configs/kernel.config', output / '.config')
        patches = [SOURCE / 'patches' / name for name in
                   ['0001-modern-gcc-attributes.patch', '0004-arm-initcall-persistent-log.patch', '0005-kernel-ramboot-deadline.patch',
                    '0006-arm-emergency-restart.patch', '0007-epdc-waveform-bounds.patch']]
        for patch in patches:
            destination = Path(scratch) / patch.name
            copy(patch, destination)
            run('patch', '-d', kernel, '-p1', '-i', destination)
        make = ['make', '-C', kernel, 'O=' + str(output), 'ARCH=arm',
                'CROSS_COMPILE=arm-linux-gnueabi-', 'KCFLAGS=-fno-pie -fno-delete-null-pointer-checks',
                'KBUILD_BUILD_TIMESTAMP=2026-10-18 00:00:00 UTC', 'KBUILD_BUILD_VERSION=1',
                'KBUILD_BUILD_USER=ocean', 'KBUILD_BUILD_HOST=voyage', 'LOADADDR=0x80008000']
        run(*make, 'oldnoconfig')
        run(*make, '-j2', 'uImage', 'modules')
        run(*make, 'modules_install', 'INSTALL_MOD_PATH=' + str(install), 'INSTALL_MOD_STRIP=1')
        module_root = install / 'lib/modules/3.0.35-ocean-voyage/kernel'
        run('tar', '--sort=name', '--mtime=@0', '--owner=0', '--group=0', '-czf',
            Path(scratch) / 'modules.tar.gz', '-C', module_root, '.')
        targets = {'zImage': output / 'arch/arm/boot/zImage', 'uImage': output / 'arch/arm/boot/uImage',
                   'kernel.config': output / '.config', 'Module.symvers': output / 'Module.symvers',
                   'System.map': output / 'System.map', 'modules.tar.gz': Path(scratch) / 'modules.tar.gz'}
        with tempfile.TemporaryDirectory(prefix='kernel-artifacts-', dir=WORK) as folder:
            temporary = Path(folder)
            for name, path in targets.items():
                subprocess.run(['docker', 'cp', 'voyage-os-build:' + str(path), temporary / name],
                               env=ENV, check=True)
            if (temporary / 'uImage').read_bytes()[:4] != bytes.fromhex('27051956'):
                raise ValueError('Invalid uImage header')
            for name in targets:
                (temporary / name).replace(WORK / 'artifacts' / name)
        manifest = {str(p.relative_to(WORK / 'artifacts')): digest(p)
                    for p in (WORK / 'artifacts').rglob('*') if p.is_file() and p.name != 'manifest.json'}
        (WORK / 'artifacts/manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        report = {'sourceCommit': COMMIT, 'sourceArchiveSha256': digest(WORK / 'linux-source.tar.gz'),
                  'patches': {patch.name: digest(patch) for patch in patches}, 'kernelSha256': manifest['uImage'],
                  'physicalBootValidated': False}
        (WORK / 'kernel-build-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    finally:
        run('python3', '-c', 'import shutil,sys;shutil.rmtree(sys.argv[1])', scratch)


if __name__ == '__main__':
    build()
