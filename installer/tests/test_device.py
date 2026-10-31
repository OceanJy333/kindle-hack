# input: fake Kindle paths and tools; output: device ordering and cleanup tests; pos: installer tests.
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import unittest

EXTENSION = Path(__file__).resolve().parents[1] / 'extension'


class DeviceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.usb = self.root / 'usb'
        self.ext = self.usb / 'extensions/ocean'
        shutil.copytree(EXTENSION, self.ext)
        self.fake = self.root / 'bin'
        self.fake.mkdir()
        self.sys = self.root / 'sys'
        (self.sys / 'proc').mkdir(parents=True)
        (self.sys / 'etc').mkdir()
        (self.sys / 'proc/usid').write_text('B013SECRET1234567')
        (self.sys / 'etc/prettyversion.txt').write_text('Kindle 5.13.6 (build)\n')
        self.trace = self.root / 'trace'
        self.env = dict(os.environ, PATH=f'{self.fake}:{os.environ["PATH"]}',
                        OCEAN_USB_ROOT=str(self.usb), OCEAN_EXTENSION_ROOT=str(self.ext),
                        OCEAN_SYS_ROOT=str(self.sys), OCEAN_TMP_ROOT=str(self.root / 'tmp'),
                        TRACE=str(self.trace))
        self.script(self.fake / 'uname', 'case "$1" in -r) echo 3.0.35-lab126;; -m) echo armv7l;; esac')
        self.script(self.fake / 'lipc-set-prop', 'echo "LIPC $*" >> "$TRACE"')
        self.script(self.fake / 'lipc-get-prop', 'echo 0')
        self.script(self.fake / 'pidof', 'echo 1234')
        self.script(self.fake / 'killall', 'echo "KILLALL $*" >> "$TRACE"')
        self.script(self.fake / 'lipc-wait-event', 'exec sleep 300')
        self.script(self.ext / 'bin/device-probe', '''echo "HARDWARE $1" >> "$TRACE"
if [ "$1" = --json ]; then echo '{"hardwareReady":true,"screenUpdated":false}'; else
 echo OCEAN_STATIC_RUNTIME=1; echo OCEAN_DISPLAY_READY=1; echo OCEAN_INPUT_READY=1
 echo OCEAN_HARDWARE_READY=1; echo OCEAN_FB_WIDTH=1072; echo OCEAN_FB_HEIGHT=1448; echo OCEAN_FB_BPP=8; fi''')
        runtime = self.ext / 'payload/runtime'
        runtime.mkdir(parents=True)
        self.script(runtime / 'ld-linux-armhf.so.3', 'echo "LOADER $*" >> "$TRACE"; shift 2; [ "$1" != --list ] || exit 0; exec "$@"')
        self.script(runtime / 'runtime-check', 'echo "RUNTIME $*" >> "$TRACE"; [ "$1" = --local ]')
        for name in ('ReaderApp.qml', 'fonts.conf', 'ca-certificates.crt', 'fonts/NotoSansCJKsc-Regular.otf'):
            path = runtime / name
            path.parent.mkdir(exist_ok=True)
            path.write_text('fixture')
        self.app(runtime / 'weread-login')
        for directory, program in (('doom', 'doom-voyage'), ('arcade', 'arcade-voyage')):
            folder = self.ext / 'payload/games' / directory
            folder.mkdir(parents=True)
            self.app(folder / program)
            (folder / 'overlay.gray').write_text('fixture')
            (folder / 'missing.gray').write_text('fixture')
        for name in ('doom1.wad', 'roms/mslug.zip', 'roms/neogeo.zip'):
            path = self.usb / 'games' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('user')

    def script(self, path, text):
        path.write_text('#!/bin/sh\nset -eu\n' + text + '\n')
        path.chmod(0o755)

    def app(self, path):
        path.write_text(f'#!{sys.executable}\n' + '''import os, signal, sys, time
def log(text):
 with open(os.environ['TRACE'], 'a') as out: out.write(text + '\\n')
if '--probe' in sys.argv:
 log('GAME_PROBE ' + os.path.basename(sys.argv[0])); sys.exit(0)
log('APP_STARTED ' + os.environ.get('OCEAN_DEVICE_PROFILE', '') + ' ' + os.environ.get('OCEAN_STOCK_APP', ''))
def stop(signum, frame):
 log('APP_SIGNAL ' + str(signum)); sys.exit(0)
signal.signal(signal.SIGUSR2, stop)
signal.signal(signal.SIGTERM, stop)
if os.environ.get('APP_EXIT') == '1': sys.exit(7)
while True: time.sleep(.02)
''')
        path.chmod(0o755)

    def run_ocean(self, action, **kwargs):
        return subprocess.run([str(self.ext / 'bin/ocean.sh'), action], env=self.env,
                              capture_output=True, text=True, timeout=15, **kwargs)

    def status(self):
        return json.loads((self.usb / 'ocean/state/status.json').read_text())

    def wait_started(self, process):
        until = time.monotonic() + 8
        while time.monotonic() < until:
            if self.trace.exists() and 'APP_STARTED' in self.trace.read_text(): return
            if process.poll() is not None: self.fail(process.communicate())
            time.sleep(.02)
        self.fail('app did not start')

    def test_detection_safe_prefix_and_local_runtime_order(self):
        result = self.run_ocean('install')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.status()['reader'], 'ready')
        inventory = (self.usb / 'ocean/state/device.json').read_text()
        self.assertNotIn('1234567', inventory)
        self.assertEqual(json.loads(inventory)['profile'], 'voyage')
        self.assertEqual(json.loads(inventory)['modelPrefix'], 'B013')
        trace = self.trace.read_text()
        self.assertLess(trace.index('HARDWARE'), trace.index('RUNTIME --local'))
        self.assertNotIn('KILLALL', trace)

    def test_pw1_does_not_execute_qt_loader(self):
        (self.sys / 'proc/usid').write_text('B024SECRET12345')
        self.script(self.fake / 'uname', 'case "$1" in -r) echo 2.6.31-lab126;; -m) echo armv7l;; esac')
        self.assertEqual(self.run_ocean('install').returncode, 0)
        self.assertEqual(self.status()['reader'], 'reader_runtime_unavailable')
        self.assertNotIn('LOADER', self.trace.read_text())
        self.assertIn('GAME_PROBE', self.trace.read_text())

    def test_unknown_profile_and_failed_hardware_block_handoff(self):
        (self.sys / 'proc/usid').write_text('UNKNOWN123456789')
        self.script(self.ext / 'bin/device-probe', 'echo OCEAN_HARDWARE_READY=0; exit 1')
        self.assertEqual(self.run_ocean('doom').returncode, 1)
        self.assertEqual(self.status()['hardware'], 'unsupported_device_profile')
        self.assertNotIn('KILLALL', self.trace.read_text())

    def test_unlisted_serial_prefix_uses_wario_board_and_758_wide_epdc(self):
        (self.sys / 'proc/usid').write_text('901722SECRET1234')
        (self.sys / 'proc/cpuinfo').write_text('Hardware\t: Freescale i.MX 6SoloLite based Wario Board\n')
        graphics = self.sys / 'sys/class/graphics/fb0'
        graphics.mkdir(parents=True)
        (graphics / 'virtual_size').write_text('768,4096\n')
        self.assertEqual(self.run_ocean('install').returncode, 0)
        inventory = json.loads((self.usb / 'ocean/state/device.json').read_text())
        self.assertEqual(inventory['profile'], 'pw2')
        self.assertEqual(inventory['kernelSupported'], 1)

    def test_unlisted_serial_prefix_with_other_screen_stays_unknown(self):
        (self.sys / 'proc/usid').write_text('901722SECRET1234')
        (self.sys / 'proc/cpuinfo').write_text('Hardware\t: Freescale i.MX 6SoloLite based Wario Board\n')
        graphics = self.sys / 'sys/class/graphics/fb0'
        graphics.mkdir(parents=True)
        (graphics / 'virtual_size').write_text('1088,5792\n')
        self.run_ocean('install')
        self.assertEqual(json.loads((self.usb / 'ocean/state/device.json').read_text())['profile'], 'unknown')

    def test_missing_game_data_blocks_handoff(self):
        (self.usb / 'games/doom1.wad').unlink()
        self.assertEqual(self.run_ocean('doom').returncode, 1)
        self.assertEqual(self.status()['doom'], 'missing_doom1_wad')
        self.assertNotIn('KILLALL', self.trace.read_text())

    def test_new_kernel_is_not_assumed_to_have_old_epdc_abi(self):
        self.script(self.fake / 'uname', 'case "$1" in -r) echo 4.1.15-lab126;; -m) echo armv7l;; esac')
        self.assertEqual(self.run_ocean('weread').returncode, 1)
        self.assertEqual(self.status()['hardware'], 'unsupported_kernel')
        self.assertEqual(self.status()['next'], 'report_device_json')
        self.assertNotIn('LOADER', self.trace.read_text())
        self.assertNotIn('KILLALL', self.trace.read_text())

    def test_local_runtime_failure_is_reported_before_handoff(self):
        self.script(self.ext / 'payload/runtime/runtime-check', 'exit 1')
        self.assertEqual(self.run_ocean('weread').returncode, 1)
        self.assertEqual(self.status()['reader'], 'reader_runtime_failed')
        self.assertNotIn('KILLALL', self.trace.read_text())

    def test_existing_runtime_link_is_preserved_before_handoff(self):
        temp = self.root / 'tmp'
        temp.mkdir()
        (temp / 'weread-native').symlink_to('/existing/runtime')
        self.assertEqual(self.run_ocean('weread').returncode, 1)
        self.assertEqual(os.readlink(temp / 'weread-native'), '/existing/runtime')
        self.assertNotIn('KILLALL', self.trace.read_text())

    def test_lipc_listener_failure_exits_and_restores_stock(self):
        self.script(self.fake / 'lipc-wait-event', 'sleep .3; exit 1')
        self.assertEqual(self.run_ocean('weread').returncode, 143)
        self.assert_restored()

    def test_pw1_old_firmware_uses_soft_pillow_and_restores(self):
        (self.sys / 'proc/usid').write_text('B024SECRET12345')
        (self.sys / 'etc/prettyversion.txt').write_text('Kindle 5.6.1.1 (build)\n')
        self.script(self.fake / 'uname', 'case "$1" in -r) echo 2.6.31-lab126;; -m) echo armv7l;; esac')
        self.env['APP_EXIT'] = '1'
        self.assertEqual(self.run_ocean('doom').returncode, 7)
        trace = self.trace.read_text()
        self.assertIn('nativeBridge.hideMe()', trace)
        self.assertIn('nativeBridge.showMe()', trace)
        self.assertNotIn('disableEnablePillow', trace)
        self.assertIn('KILLALL -CONT awesome', trace)

    def test_app_failure_restores_stock_and_removes_runtime_link(self):
        self.env['APP_EXIT'] = '1'
        result = self.run_ocean('weread')
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assert_restored()

    def test_stock_menu_requests_graceful_reader_exit(self):
        process = subprocess.Popen([str(self.ext / 'bin/ocean.sh'), 'weread'], env=self.env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            self.wait_started(process)
            self.assertEqual(self.run_ocean('stock').returncode, 0)
            process.communicate(timeout=8)
            self.assertIn(f'APP_SIGNAL {signal.SIGUSR2}', self.trace.read_text())
            self.assert_restored()
        finally:
            if process.poll() is None: process.kill(); process.communicate()

    def test_sleep_event_exits_reader_and_restores_stock(self):
        self.script(self.fake / 'lipc-wait-event', 'sleep .3; echo goingToScreenSaver')
        result = self.run_ocean('weread')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(f'APP_SIGNAL {signal.SIGUSR2}', self.trace.read_text())
        self.assert_restored()

    def test_device_uninstall_keeps_user_resources(self):
        account = self.usb / 'ocean/state/weread/account'
        account.parent.mkdir(parents=True, exist_ok=True)
        account.write_text('private')
        self.assertEqual(self.run_ocean('uninstall').returncode, 0)
        self.assertFalse(self.ext.exists())
        self.assertEqual(account.read_text(), 'private')
        self.assertTrue((self.usb / 'games/doom1.wad').exists())

    def assert_restored(self):
        trace = self.trace.read_text()
        self.assertIn('KILLALL -STOP awesome', trace)
        self.assertIn('KILLALL -CONT awesome', trace)
        self.assertIn('disableEnablePillow enable', trace)
        self.assertIn('preventScreenSaver 0', trace)
        self.assertIn('app://com.lab126.booklet.home', trace)
        self.assertNotIn('powerd stop', trace)
        self.assertFalse((self.root / 'tmp/weread-native').is_symlink())
        self.assertFalse((self.root / 'tmp/ocean-session').exists())
        session = json.loads((self.usb / 'ocean/state/session.json').read_text())
        self.assertEqual(session['phase'], 'returned_to_stock')
        self.assertEqual(session['sleepResume'], 'unverified')


if __name__ == '__main__':
    unittest.main()
