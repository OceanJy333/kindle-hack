#!/usr/bin/env python3
# input: firmware event handler; output: isolated delivery checks; pos: hardware-loader acceptance before flashing.
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parent
WORK = SOURCE.parent / 'build/independent-os'
ENV = os.environ.copy()


def docker(*args, check=True):
    return subprocess.run(['docker', 'exec', 'voyage-os-build', *map(str, args)],
                          env=ENV, check=check, capture_output=True, text=True)


def verify():
    with tempfile.TemporaryDirectory(prefix='hotplug-test-', dir=WORK) as folder:
        root = Path(folder)
        for name in ['bin', 'sbin', 'usr/bin', 'sys/devices/fixture', 'var/local/ocean/hardware',
                     'opt/ar6k', 'lib/firmware/cyttsp4', 'run']:
            (root / name).mkdir(parents=True, exist_ok=True)
        device_root = Path('/work') / root.relative_to(WORK)
        docker('cp', '/bin/busybox', device_root / 'bin/busybox')
        for name in ['sh', 'cat']:
            (root / 'bin' / name).symlink_to('busybox')
        handler = root / 'usr/bin/ocean-hotplug'
        shutil.copyfile(SOURCE / 'rootfs/usr/bin/ocean-hotplug', handler)
        (root / 'sbin/mdev').write_text('#!/bin/sh\necho dispatched >/run/mdev-called\n')
        (root / 'sbin/mdev').chmod(0o755)
        cases = [
            ('panel', 'imx/epdc_panel.fw', 'var/local/ocean/hardware/panel.wrf.gz', b'panel-blob'),
            ('wireless', '/opt/ar6k/fw.bin', 'opt/ar6k/fw.bin', b'wireless-blob'),
            ('touch', 'cyttsp4/fw.bin', 'lib/firmware/cyttsp4/fw.bin', b'touch-blob'),
        ]
        result = {}
        request = root / 'sys/devices/fixture'
        def event(firmware):
            return docker('env', 'PATH=/bin:/sbin', 'SUBSYSTEM=firmware', 'ACTION=add',
                          'DEVPATH=/devices/fixture', 'FIRMWARE=' + firmware,
                          'chroot', device_root, '/bin/sh', '/usr/bin/ocean-hotplug', check=False)
        for name, firmware, path, content in cases:
            (root / path).write_bytes(content)
            (request / 'loading').write_text('')
            (request / 'data').write_bytes(b'')
            call = event(firmware)
            assert call.returncode == 0, call.stderr
            assert (request / 'data').read_bytes() == content
            assert (request / 'loading').read_text() == '0\n'
            result[name] = 'delivered'
        call = event('missing.bin')
        assert call.returncode != 0
        assert (request / 'loading').read_text() == '-1\n'
        result['missing'] = 'rejected'
        docker('env', 'PATH=/bin:/sbin', 'SUBSYSTEM=input', 'ACTION=add', 'chroot', device_root,
               '/bin/sh', '/usr/bin/ocean-hotplug')
        assert (root / 'run/mdev-called').read_text() == 'dispatched\n'
        result['deviceEvent'] = 'delegated-to-mdev'
        (WORK / 'hotplug-verification.json').write_text(json.dumps(result, indent=2) + '\n')
        print(json.dumps(result))


if __name__ == '__main__':
    verify()
