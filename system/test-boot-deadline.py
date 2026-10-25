#!/usr/bin/env python3
# input: native kernel and missing-root fault; output: real reset evidence; pos: unattended boot recovery validation.
from pathlib import Path
import hashlib
import json
import select
import socket
import subprocess
import tempfile
import time
from boot_console import connect

SOURCE = Path(__file__).resolve().parent
LAB = SOURCE.parent / 'build'
WORK = LAB / 'independent-os'
EVIDENCE = SOURCE.parent / 'build/reports/independent-os'


def test():
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    kernel = WORK / 'artifacts/uImage'
    serial = EVIDENCE / 'emulated-boot-deadline.log'
    events = []
    forced = False
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix='boot-deadline-', dir=WORK) as folder:
        qmp = Path(folder) / 'qmp.sock'
        command = [LAB / 'tools/eink-emulator/qemu/build/qemu-system-arm',
                   '-machine', 'imx6sl-wario,idme-serial=B013000000000001', '-m', '512M',
                   '-kernel', kernel, '-append',
                   'console=ttymxc0,115200 root=/dev/ocean-missing rootwait loglevel=7 ocean_boot_timeout=15',
                   '-display', 'none', '-monitor', 'none', '-serial', 'file:' + str(serial),
                   '-no-reboot', '-qmp', f'unix:{qmp},server=on,wait=off']
        with (EVIDENCE / 'emulated-boot-deadline-qemu.log').open('w') as output:
            process = subprocess.Popen(list(map(str, command)), stdout=output, stderr=subprocess.STDOUT)
            client = None
            try:
                client = connect(qmp)
                client.sendall(b'{"execute":"qmp_capabilities"}\n')
                pending = b''
                deadline = time.monotonic() + 60
                while time.monotonic() < deadline:
                    if select.select([client], [], [], .5)[0]:
                        chunk = client.recv(65536)
                        if not chunk:
                            break
                        pending += chunk
                        while b'\n' in pending:
                            line, pending = pending.split(b'\n', 1)
                            if line.strip():events.append(json.loads(line))
                if process.poll() is None:
                    try:process.wait(timeout=3)
                    except subprocess.TimeoutExpired:pass
            finally:
                if client:client.close()
                if process.poll() is None:
                    forced = True
                    process.terminate()
                    process.wait(timeout=5)
        log = serial.read_text(errors='replace')
        reset = any(e.get('event') == 'SHUTDOWN' and e.get('data', {}).get('reason') in
                    ['guest-reset', 'watchdog'] for e in events)
        report = {'kernelSha256': hashlib.file_digest(kernel.open('rb'), 'sha256').hexdigest(),
                  'missingRootWaitReached': 'Waiting for root device /dev/ocean-missing' in log,
                  'guardArmed': 'OCEAN_BOOT_GUARD: 15 seconds' in log,
                  'deadlineExpired': 'OCEAN_BOOT_TIMEOUT:' in log,
                  'resetObserved': reset and not forced, 'qemuExitCode': process.returncode,
                  'forcedTermination': forced, 'wallSeconds': round(time.monotonic()-started, 2),
                  'events': events, 'physicalBootValidated': False}
        (EVIDENCE / 'emulated-boot-deadline.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
        if not all(report[k] for k in ['missingRootWaitReached', 'guardArmed', 'deadlineExpired', 'resetObserved']):
            raise SystemExit(1)


if __name__ == '__main__':
    test()
