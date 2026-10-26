# input: temporary USB root and release; output: host contract tests; pos: installer tests.
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace

ENTRY = Path(__file__).resolve().parents[1] / 'install.py'
spec = importlib.util.spec_from_file_location('ocean_install', ENTRY)
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class HostTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.usb = self.root / 'Kindle'
        self.source = self.root / 'source'
        self.usb.mkdir()
        for name in installer.REQUIRED:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('release\n')

    def test_install_reinstall_preserve_other_extensions_and_user_data(self):
        other = self.usb / 'extensions/koreader/keep'
        account = self.usb / 'ocean/state/weread/session.json'
        save = self.usb / 'games/save.dsg'
        for path in (other, account, save):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('keep')
        self.assertEqual(installer.run('install', self.usb, self.source)['phase'], 'copied')
        obsolete = self.usb / 'extensions/ocean/old-version'
        obsolete.write_text('old')
        installer.run('install', self.usb, self.source)
        self.assertFalse(obsolete.exists())
        self.assertTrue(installer.run('verify', self.usb, self.source)['ok'])
        self.assertEqual(installer.read_status(self.usb)['phase'], 'awaiting_device_check')
        installer.run('uninstall', self.usb, self.source)
        self.assertFalse((self.usb / 'extensions/ocean').exists())
        for path in (other, account, save):
            self.assertEqual(path.read_text(), 'keep')
        self.assertEqual(installer.read_status(self.usb)['phase'], 'uninstalled')

    def test_incomplete_release_does_not_replace_existing_install(self):
        installer.run('install', self.usb, self.source)
        (self.source / 'bin/device-probe').unlink()
        with self.assertRaisesRegex(ValueError, '发行包不完整'):
            installer.run('install', self.usb, self.source)
        self.assertTrue((self.usb / 'extensions/ocean/bin/device-probe').exists())

    def test_verify_reports_missing_payload(self):
        result = installer.run('verify', self.usb, self.source)
        self.assertFalse(result['ok'])
        self.assertIn('bin/device-probe', result['missing'])

    def test_low_space_is_reported_before_mutation(self):
        with patch.object(installer.shutil, 'disk_usage', return_value=SimpleNamespace(free=0)):
            with self.assertRaisesRegex(ValueError, '空间不足'):
                installer.run('install', self.usb, self.source)
        self.assertFalse((self.usb / 'extensions/ocean').exists())

    def test_installer_can_check_release_already_on_usb(self):
        installer.run('install', self.usb, self.source)
        target = self.usb / 'extensions/ocean'
        installer.run('install', self.usb, target)
        self.assertTrue(installer.run('verify', self.usb, target)['ok'])

    def test_explicit_usb_path_and_json_status(self):
        self.assertEqual(installer.find_kindle(str(self.usb)), self.usb.resolve())
        result = installer.run('status', self.usb, self.source)
        self.assertEqual(json.loads(json.dumps(result))['device']['phase'], 'awaiting_device_check')
        with self.assertRaises(ValueError):
            installer.find_kindle(str(self.root / 'missing'))


if __name__ == '__main__':
    unittest.main()
