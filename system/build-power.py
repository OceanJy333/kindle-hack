#!/usr/bin/env python3
# input: native power sources and Zig; output: tested static ARM programs; pos: single power-service build entry.
from pathlib import Path
import hashlib
import argparse
import json
import os
import shutil
import subprocess
import tempfile

SOURCE = Path(__file__).resolve().parent
LAB = SOURCE.parent / 'build'
WORK = LAB / 'independent-os'
ZIG = os.environ.get('ZIG', shutil.which('zig') or str(LAB / 'native-deps/zig/zig'))
ENV = os.environ.copy()
FLAGS = ['-target', 'arm-linux-musleabihf', '-mcpu=cortex_a9', '-O2', '-static', '-s', '-D_GNU_SOURCE']


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(test_arm=False):
    WORK.mkdir(parents=True, exist_ok=True)
    version = subprocess.check_output([ZIG, 'version'], text=True).strip()
    if version != '0.15.2':raise ValueError('Expected Zig 0.15.2')
    with tempfile.TemporaryDirectory(prefix='power-build-', dir=WORK) as folder:
        temporary = Path(folder)
        outputs = {'ocean-powerd': 'powerd.c', 'ocean-powerctl': 'ctl.c', 'power-cycle': 'cycle.c'}
        for name, source in {**outputs, 'power-check': 'check.c'}.items():
            subprocess.run([ZIG, 'cc', *FLAGS, SOURCE / 'power' / source, '-o', temporary / name], check=True)
        checks = ''
        if test_arm:
            test = subprocess.run(['docker', 'exec', 'voyage-os-build', 'qemu-arm',
                                   str(Path('/work') / temporary.relative_to(WORK) / 'power-check')],
                                  env=ENV, capture_output=True, text=True, check=True)
            if 'POWER_CHECK_PASSED:' not in test.stdout:
                raise ValueError('Missing power checks')
            checks = test.stdout.strip()
        for name in outputs:
            (temporary / name).replace(WORK / name)
        receipt = {'compiler': 'Zig ' + version, 'flags': FLAGS,
                   'sources': {p.name: digest(p) for p in sorted((SOURCE / 'power').iterdir()) if p.is_file()},
                   'binaries': {name: digest(WORK / name) for name in outputs},
                   'armRegressionPassed': test_arm, 'physicalPowerValidated': False,
                   'checks': checks}
        (WORK / 'power-build-manifest.json').write_text(json.dumps(receipt, indent=2) + '\n')
        if test_arm:
            reports = LAB / 'reports/independent-os'
            reports.mkdir(parents=True, exist_ok=True)
            (reports / 'power-lock-race-after.txt').write_text(test.stdout + test.stderr)
        print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--test', action='store_true', help='run ARM power regression in the build container')
    build(parser.parse_args().test)
