#!/usr/bin/env python3
# input: native rootfs tools, modules and loader; output: minimal boot archives; pos: standalone memory-boot assembly.
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil
import struct
import tarfile
import tempfile
import zlib
from ramboot_cpio import archive

SOURCE=Path(__file__).resolve().parent
WORK=SOURCE.parent/'build/independent-os'
NATIVE=WORK.parent/'weread/native-runtime'
spec=importlib.util.spec_from_file_location('rootfs_build',SOURCE/'build-rootfs.py')
rootfs_build=importlib.util.module_from_spec(spec);spec.loader.exec_module(rootfs_build)


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def base(root):
    for name in ['bin','sbin','lib','dev','proc','sys','mnt/us','newroot']:(root/name).mkdir(parents=True,exist_ok=True)
    shutil.copyfile(WORK/'artifacts/busybox',root/'bin/busybox');(root/'bin/busybox').chmod(0o755)
    for name in ['sh','mount','umount','mkdir','mdev','losetup','insmod','mountpoint','switch_root','reboot','sync','sleep','cat','echo','seq','test','[','modprobe','depmod','dmesg']:
        (root/'bin'/name).symlink_to('busybox')
    (root/'sbin/mdev').symlink_to('../bin/busybox')
    for name in ['ld-linux-armhf.so.3','libc.so.6','libm.so.6','libresolv.so.2']:
        shutil.copyfile(NATIVE/'glibc-install/lib'/name,root/'lib'/name);(root/'lib'/name).chmod(0o755)


def build():
    destination=WORK/'ramboot';destination.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='ramboot-build-',dir=WORK) as folder:
        root=Path(folder)/'root';base(root)
        rootfs_build.install_modules(root)
        shutil.copyfile(SOURCE/'ramboot/init',root/'init');(root/'init').chmod(0o755)
        archive(root,destination/'initramfs.cpio')
        bootstrap=Path(folder)/'bootstrap';base(bootstrap)
        for name in ['ocean-kexec.ko','kexec-ctl']:shutil.copyfile(WORK/'kexec'/name,bootstrap/name)
        (bootstrap/'kexec-ctl').chmod(0o755)
        shutil.copyfile(SOURCE/'ramboot/bootstrap-init',bootstrap/'init');(bootstrap/'init').chmod(0o755)
        archive(bootstrap,destination/'bootstrap.cpio')
    with (WORK/'private-backup/disk-prefix.bin').open('rb') as stream:
        stream.seek(0x41000);header=stream.read(64);size=struct.unpack_from('>I',header,12)[0]
        assert struct.unpack_from('>I',header)[0]==0x27051956
        payload=stream.read(size);assert len(payload)==size
        assert zlib.crc32(payload)==struct.unpack_from('>I',header,24)[0]
        (destination/'stock-uImage').write_bytes(header+payload)
    paths=['initramfs.cpio','bootstrap.cpio','stock-uImage']
    report={'files':{name:{'bytes':(destination/name).stat().st_size,'sha256':digest(destination/name)} for name in paths},
            'targetKernelSha256':digest(WORK/'artifacts/zImage'),
            'kexecModuleSha256':digest(WORK/'kexec/ocean-kexec.ko'),
            'kexecClientSha256':digest(WORK/'kexec/kexec-ctl'),
            'rootfsArchiveSha256':digest(WORK/'voyage-rootfs.ext3.gz'),
            'physicalBootValidated':False}
    (destination/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':build()
