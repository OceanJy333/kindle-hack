#!/usr/bin/env python3
# input: pinned kernel artifacts and native runtime; output: independent ext3 rootfs; pos: Voyage image assembly.
from pathlib import Path
from host_tools import E2FS
import gzip
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile

SOURCE = Path(__file__).resolve().parent
REPO = SOURCE.parent
LAB = REPO / 'build'
WORK = LAB / 'independent-os'
NATIVE = LAB / 'weread/native-runtime'
VERSION = '3.0.35-ocean-voyage'
DOCKER_ENV = os.environ.copy()


def run(*args):
    try:
        return subprocess.check_output([str(x) for x in args], stderr=subprocess.STDOUT, env=DOCKER_ENV)
    except subprocess.CalledProcessError as error:
        print(error.output.decode(errors='replace'), flush=True)
        raise


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def arm(root, *args):
    device_path = Path('/work') / root.relative_to(WORK)
    return run('docker', 'exec', 'voyage-os-build', 'qemu-arm', '-L', device_path,
               device_path / 'bin/busybox', *args)


def copy(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    destination.chmod(source.stat().st_mode & 0o777)


def unpack_runtime(root, temporary):
    image = temporary / 'runtime.ext2'
    with gzip.open(NATIVE / 'native-runtime.ext2.gz', 'rb') as source, image.open('wb') as target:
        shutil.copyfileobj(source, target)
    target = root / 'opt/ocean'
    target.mkdir(parents=True)
    run(E2FS / 'debugfs', '-R', f'rdump / {target}', image)
    image.unlink()
    for legacy in ['desk-launch.sh', 'desktop-power.lua', 'lost+found']:
        path = target / legacy
        if path.is_dir():
            shutil.rmtree(path)
        elif path.exists():
            path.unlink()


def install_base(root):
    for name in ['dev', 'proc', 'sys', 'tmp', 'run', 'mnt/us', 'var/local', 'lib', 'sbin', 'bin']:
        (root / name).mkdir(parents=True, exist_ok=True)
    shutil.copytree(SOURCE / 'rootfs', root, dirs_exist_ok=True)
    for path in root.rglob('*'):
        if path.is_file() and path.read_bytes()[:2] == b'#!':
            path.chmod(0o755)
    copy(WORK / 'artifacts/busybox', root / 'bin/busybox')
    for name in ['ld-linux-armhf.so.3', 'libc.so.6', 'libm.so.6', 'libresolv.so.2']:
        copy(NATIVE / 'glibc-install/lib' / name, root / 'lib' / name)
    apps = arm(root, '--list').decode().splitlines()
    # All applets share one executable; init and /bin/sh resolve before services start.
    for app in apps:
        (root / 'bin' / app).symlink_to('busybox')
    for app in ['init', 'mdev', 'modprobe', 'depmod', 'reboot', 'poweroff', 'losetup']:
        (root / 'sbin' / app).symlink_to('../bin/busybox')
    for name in ['ocean-powerd', 'ocean-powerctl']:
        copy(WORK / name, root / 'usr/sbin' / name)
        old = root / 'opt/ocean' / name
        old.unlink(missing_ok=True)
        old.symlink_to('../../usr/sbin/' + name)
    for name in ['wpa_supplicant', 'wpa_cli', 'wpa_passphrase']:
        copy(WORK / 'wifi' / name, root / 'usr/sbin' / name)
    for name in ['libssl.so.3', 'libcrypto.so.3']:
        (root / 'lib' / name).symlink_to('../opt/ocean/' + name)
    ssh = WORK / 'ssh'
    copy(ssh / 'dropbear', root / 'usr/sbin/dropbear')
    copy(ssh / 'dropbearkey', root / 'usr/bin/dropbearkey')
    (root / 'root').symlink_to('var/local/ocean/root')
    (root / 'lib/firmware').symlink_to('../var/local/ocean/hardware/lib/firmware')
    (root / 'opt/ar6k').symlink_to('../var/local/ocean/hardware/opt/ar6k')
    (root / 'etc/resolv.conf').symlink_to('../run/resolv.conf')
    (root / 'var/run').symlink_to('../run')
    (root / 'etc/passwd').write_text('root:x:0:0:Ocean:/root:/bin/sh\n')
    (root / 'etc/group').write_text('root:x:0:\n')
    (root / 'etc/shadow').write_text('root:!:0:0:99999:7:::\n')
    (root / 'etc/shadow').chmod(0o600)
    (root / 'etc/hostname').write_text('ocean-voyage\n')
    (root / 'etc/hosts').write_text('127.0.0.1 localhost ocean-voyage\n')
    (root / 'etc/os-release').write_text('NAME="Ocean Voyage"\nID=ocean-voyage\nVERSION_ID="0.1-dev"\n')
    return apps


def install_modules(root):
    destination = root / 'lib/modules' / VERSION / 'kernel'
    destination.mkdir(parents=True)
    with tarfile.open(WORK / 'artifacts/modules.tar.gz') as archive:
        archive.extractall(destination, filter='data')
    for module in destination.rglob('*.ko'):
        assert ('vermagic=' + VERSION + ' ').encode() in module.read_bytes(), module
    device_root = Path('/work') / root.relative_to(WORK)
    # BusyBox's small depmod has no -b switch. Its fixed search path is scoped
    # to this dedicated build container and removed even when generation fails.
    run('docker', 'exec', 'voyage-os-build', 'ln', '-s', device_root / 'lib/modules', '/lib/modules')
    try:
        arm(root, 'depmod', VERSION)
    finally:
        run('docker', 'exec', 'voyage-os-build', 'rm', '/lib/modules')
    assert (root / 'lib/modules' / VERSION / 'modules.dep.bb').is_file()


def verify_sources(root, apps):
    for app in ['init', 'mdev', 'mount', 'modprobe', 'udhcpd', 'udhcpc']:
        assert app in apps, app
    arm(root, 'true')
    assert b'Ocean' in arm(root, 'echo', 'Ocean userspace ARM execution OK')
    scripts = []
    for path in root.rglob('*'):
        if path.is_file() and not path.is_symlink() and path.read_bytes()[:2] == b'#!':
            run('/bin/sh', '-n', path)
            scripts.append(str(path.relative_to(root)))
    assert not (root / 'etc/ld.so.preload').exists()
    device_path = Path('/work') / root.relative_to(WORK)
    version = run('docker', 'exec', 'voyage-os-build', 'qemu-arm', '-L', device_path,
                  device_path / 'usr/sbin/wpa_supplicant', '-v').decode().strip()
    assert 'v2.12' in version, version
    ssh_version = run('docker', 'exec', 'voyage-os-build', 'qemu-arm', '-L', device_path,
                      device_path / 'usr/sbin/dropbear', '-V').decode().strip()
    assert '2026.94' in ssh_version, ssh_version
    phrase = run('docker', 'exec', 'voyage-os-build', 'qemu-arm', '-L', device_path,
                 device_path / 'usr/sbin/wpa_passphrase', 'IEEE', 'password').decode()
    expected = hashlib.pbkdf2_hmac('sha1', b'password', b'IEEE', 4096, 32).hex()
    assert 'psk=' + expected in phrase
    check=root/'tmp/wifi-data-check'
    copy(NATIVE/'wifi-data-check',check)
    try:
        wifi_data=run('docker','exec','voyage-os-build','qemu-arm','-L',device_path,
                     device_path/'lib/ld-linux-armhf.so.3','--library-path',
                     str(device_path/'lib')+':'+str(device_path/'opt/ocean'),
                     device_path/'tmp/wifi-data-check').decode().strip()
        assert 'Wi-Fi data checks passed' in wifi_data,wifi_data
    finally:check.unlink()
    return {'armBusyboxExecuted': True, 'shellSyntaxChecked': scripts,
            'wifiToolVersion': version, 'wifiPskDerivationValidated': True, 'wifiUiDataChecks':wifi_data,'sshToolVersion': ssh_version,
            'amazonInitIncluded': False, 'amazonPowerServiceIncluded': False,
            'physicalBootValidated': False, 'wifiProvisioningComplete': False,
            'panelFirmwareProvisioningComplete': False}


def build():
    subprocess.run([sys.executable, SOURCE / 'build-power.py'], check=True)
    with tempfile.TemporaryDirectory(prefix='rootfs-', dir=WORK) as folder:
        temporary = Path(folder)
        root = temporary / 'root'
        root.mkdir()
        unpack_runtime(root, temporary)
        apps = install_base(root)
        install_modules(root)
        validation = verify_sources(root, apps)
        image = temporary / 'rootfs.ext3'
        with image.open('wb') as stream:
            stream.truncate(450 * 1024 * 1024)
        run(E2FS / 'mke2fs', '-q', '-F', '-t', 'ext3', '-O', '^64bit,^metadata_csum',
            '-L', 'OCEANOS', '-d', root, image)
        commands = temporary / 'nodes.txt'
        ownership = []
        for entry in [root, *root.rglob('*')]:
            name = '/' + str(entry.relative_to(root)).replace('"', '\\"')
            ownership += [f'set_inode_field "{name}" uid 0', f'set_inode_field "{name}" gid 0']
        commands.write_text('\n'.join(ownership) + '\ncd /dev\nmknod console c 5 1\nmknod null c 1 3\n')
        run(E2FS / 'debugfs', '-w', '-f', commands, image)
        run(E2FS / 'e2fsck', '-f', '-n', image)
        output = WORK / 'voyage-rootfs.ext3.gz'
        with image.open('rb') as source, gzip.open(str(output) + '.partial', 'wb', compresslevel=1) as target:
            shutil.copyfileobj(source, target)
        expected = digest(image)
        with gzip.open(str(output) + '.partial', 'rb') as source:
            assert hashlib.file_digest(source, 'sha256').hexdigest() == expected
        Path(str(output) + '.partial').replace(output)
        receipt = {'kernelVersion': VERSION, 'rootfsBytes': image.stat().st_size,
                   'imageSha256': expected, 'archiveSha256': digest(output),
                   'kernelSha256': digest(WORK / 'artifacts/uImage'),
                   'modulesSha256': digest(WORK / 'artifacts/modules.tar.gz'),
                   'powerDaemonSha256': digest(WORK / 'ocean-powerd'),
                   'powerBuildManifestSha256': digest(WORK / 'power-build-manifest.json'),
                   'nativeRuntimeSha256': digest(NATIVE / 'native-runtime.ext2.gz'),
                   'validation': validation}
        (WORK / 'rootfs-manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    build()
