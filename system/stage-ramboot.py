#!/usr/bin/env python3
# input: verified build and mounted Kindle storage; output: checked RAM boot candidate; pos: physical image staging.
from pathlib import Path
import gzip
import hashlib
import json
import os
import shutil
import sys
import tempfile

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'
EVIDENCE = SOURCE.parent / 'build/reports/independent-os'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def stage(volume):
    volume = volume.resolve()
    if not (volume / 'ocean-install/ssh/authorized_keys').is_file():
        raise ValueError('Expected the already-paired Kindle user storage')
    root = json.loads((WORK / 'rootfs-manifest.json').read_text())
    ram = json.loads((WORK / 'ramboot/manifest.json').read_text())
    module = json.loads((WORK / 'kexec/manifest.json').read_text())
    if ram.get('kexecModuleSha256') != module['moduleSha256'] or ram.get('kexecClientSha256') != digest(WORK / 'kexec/kexec-ctl'):
        raise ValueError('RAM bootstrap does not match the current loader')
    archive = WORK / 'voyage-rootfs.ext3.gz'
    if digest(archive) != root['archiveSha256'] or ram['rootfsArchiveSha256'] != root['archiveSha256']:
        raise ValueError('Root image does not match current build')
    for name in ['emulated-kexec', 'emulated-kexec-blocked-display', 'emulated-kexec-power-failure']:
        proof = json.loads((EVIDENCE / (name + '-boot.json')).read_text())
        for key in ['kernelReached', 'independentInitReached', 'sshAccepted', 'loopRootMounted',
                    'bootStagesRecoverable', 'bootGuardAcknowledged', 'wifiAssociated', 'wifiDhcpReady', 'wifiGatewayReachable']:
            if proof.get(key) is not True:
                raise ValueError(f'{name}: missing {key}')
        expected = {'rootfsArchiveSha256': root['archiveSha256'],
                    'initramfs.cpioSha256': ram['files']['initramfs.cpio']['sha256'],
                    'bootstrap.cpioSha256': ram['files']['bootstrap.cpio']['sha256'],
                    'kernelSha256': root['kernelSha256']}
        if any(proof.get(key) != value for key, value in expected.items()):
            raise ValueError(f'{name}: evidence belongs to a different build')
        if name.endswith('blocked-display') and not proof.get('displayBlockedDuringBoot'):
            raise ValueError('Display isolation was not exercised')
        if name.endswith('power-failure') and not all(proof.get('power', {}).get(key) is True for key in
                ['suspendFailureObserved', 'lockStatePreserved', 'lightStayedOff',
                 'explicitWakeRestoredLight', 'samePowerProcess', 'singlePowerBinary']):
            raise ValueError('Rejected suspend did not preserve the lock screen')
    guard = json.loads((EVIDENCE / 'emulated-boot-deadline.json').read_text())
    if guard.get('kernelSha256') != root['kernelSha256'] or not all(guard.get(key) is True for key in
            ['missingRootWaitReached', 'guardArmed', 'deadlineExpired', 'resetObserved']):
        raise ValueError('Current kernel has no verified automatic reset on a stalled boot')
    files = {
        'zImage': (WORK / 'artifacts/zImage', ram['targetKernelSha256']),
        'initramfs.cpio': (WORK / 'ramboot/initramfs.cpio', ram['files']['initramfs.cpio']['sha256']),
        'ocean-kexec.ko': (WORK / 'kexec/ocean-kexec.ko', module['moduleSha256']),
        'kexec-ctl': (WORK / 'kexec/kexec-ctl', digest(WORK / 'kexec/kexec-ctl')),
        'physical-handoff': (SOURCE / 'ramboot/physical-handoff', digest(SOURCE / 'ramboot/physical-handoff')),
    }
    for path, expected in files.values():
        if digest(path) != expected:
            raise ValueError(f'Build hash mismatch: {path.name}')
    required = root['rootfsBytes'] + sum(path.stat().st_size for path, _ in files.values()) + 8 * 1024 * 1024
    if shutil.disk_usage(volume).free < required:
        raise ValueError('Insufficient free space for an atomic candidate replacement')
    destination = volume / 'ocean-os'
    destination.mkdir(exist_ok=True)
    manifest = {'purpose': 'RAM boot candidate; does not change boot slots',
                'rootfsArchiveSha256': root['archiveSha256'], 'physicalBootValidated': False, 'files': {}}
    with tempfile.TemporaryDirectory(prefix='.staging-', dir=destination) as folder:
        temporary = Path(folder)
        for name, (path, expected) in files.items():
            shutil.copyfile(path, temporary / name)
            with (temporary / name).open('rb') as staged:os.fsync(staged.fileno())
            manifest['files'][name] = {'sha256': expected, 'bytes': path.stat().st_size}
        image = temporary / 'voyage-rootfs.ext3'
        with gzip.open(archive, 'rb') as source, image.open('wb') as target:
            shutil.copyfileobj(source, target)
            target.flush()
            os.fsync(target.fileno())
        manifest['files'][image.name] = {'sha256': root['imageSha256'], 'bytes': root['rootfsBytes']}
        for name, metadata in manifest['files'].items():
            path = temporary / name
            if path.stat().st_size != metadata['bytes'] or digest(path) != metadata['sha256']:
                raise ValueError(f'USB storage readback failed: {name}')
        # Commit metadata last; an interrupted replacement cannot appear fully verified.
        for name in manifest['files']:
            (temporary / name).replace(destination / name)
        receipt = json.dumps(manifest, indent=2) + '\n'
        (temporary / 'candidate.json').write_text(receipt)
        (temporary / 'candidate.json').replace(destination / 'candidate.json')
        (EVIDENCE / 'physical-staged-candidate.json').write_text(receipt)
    print(json.dumps({'staged': True, 'physicalBootValidated': False,
                      'rootfsSha256': root['imageSha256'], 'files': len(manifest['files'])}))


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: stage-ramboot.py MOUNTED_KINDLE_VOLUME')
    stage(Path(sys.argv[1]))
