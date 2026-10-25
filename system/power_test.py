#!/usr/bin/env python3
# input: disposable guest and real power socket; output: rejected-suspend behavior; pos: native lock integration test.
import json
import subprocess
import time
from settings_test import Screen


def verify_power(qmp_path, evidence, prefix, ssh, serial):
    def run(command):
        result = subprocess.run(ssh + [command], capture_output=True, text=True, timeout=20)
        if result.returncode:raise RuntimeError(result.stdout + result.stderr)
        return result.stdout
    def status():
        return json.loads(run('/usr/sbin/ocean-powerctl status').strip().splitlines()[-1])
    screen = Screen(qmp_path)
    try:
        before = run('pidof ocean-powerd').strip()
        run("set -e; /usr/sbin/ocean-powerctl 'light 100'; echo mem >/run/suspend-rejected; "
            'mount --bind /run/suspend-rejected /sys/power/state; '
            'mount -o remount,bind,ro /sys/power/state; /usr/sbin/ocean-powerctl sleep')
        # The supervisor sends the power service's output to the durable boot record.
        message = 'Suspend deferred: Read-only file system'
        def failure_logged():
            found = subprocess.run(ssh + ['grep -q "%s" /mnt/us/ocean-os/blackbox/current/power.log' % message],
                                   capture_output=True, timeout=20)
            return found.returncode == 0 or message in serial.read_text(errors='replace')
        failure = False
        for _ in range(20):
            failure = failure_logged()
            if failure:break
            time.sleep(.5)
        locked = status()
        off = run('cat /sys/class/backlight/max77696-bl/brightness').strip() == '0'
        screen.capture(evidence / (prefix + '-locked.png'))
        run('/usr/sbin/ocean-powerctl wake')
        time.sleep(1)
        awake = status()
        restored = run('cat /sys/class/backlight/max77696-bl/brightness').strip() == '100'
        same = run('pidof ocean-powerd').strip() == before
        shared = run('readlink /opt/ocean/ocean-powerd').strip() == '../../usr/sbin/ocean-powerd'
        report = {'suspendFailureObserved': failure, 'lockStatePreserved': locked['locked'] is True,
                  'lightStayedOff': off, 'explicitWakeRestoredLight': awake['locked'] is False and restored,
                  'samePowerProcess': same, 'singlePowerBinary': shared}
        (evidence / (prefix + '-power.json')).write_text(json.dumps(report, indent=2) + '\n')
        return report
    finally:
        subprocess.run(ssh + ['/usr/sbin/ocean-powerctl wake; umount /sys/power/state'],
                       capture_output=True, timeout=20)
        screen.close()
