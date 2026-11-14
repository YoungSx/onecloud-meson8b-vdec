import importlib.util
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
spec = importlib.util.spec_from_file_location('runtime_device', ROOT / 'scripts/runtime-device.py')
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)


class RuntimeDeviceTests(unittest.TestCase):
    def test_restore_replaced_and_new_firmware_after_interrupted_apply(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            package, state, firmware = [root / name for name in ('package', 'state', 'firmware')]
            state.mkdir()
            firmware.mkdir()
            entries = {}
            for name in ('meson/vdec/old.bin', 'meson/venc/new.bin'):
                path = package / 'firmware' / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'new firmware')
                entries['firmware/' + name] = runtime.sha(path)
            original = firmware / 'meson/vdec/old.bin'
            original.parent.mkdir(parents=True)
            original.write_bytes(b'original firmware')
            runtime.stage_firmware(package, state, entries, firmware)
            self.assertEqual(original.read_bytes(), b'new firmware')
            runtime.restore_firmware(state, firmware)
            self.assertEqual(original.read_bytes(), b'original firmware')
            self.assertFalse((firmware / 'meson/venc/new.bin').exists())
            runtime.restore_firmware(state, firmware)  # rollback is repeatable
            self.assertEqual(original.read_bytes(), b'original firmware')

    def test_refuse_later_change_or_symlink_without_overwriting_original(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            package, state, firmware = [root / name for name in ('package', 'state', 'firmware')]
            state.mkdir()
            firmware.mkdir()
            relative = 'meson/vdec/old.bin'
            payload = package / 'firmware' / relative
            payload.parent.mkdir(parents=True)
            payload.write_bytes(b'candidate')
            target = firmware / relative
            target.parent.mkdir(parents=True)
            target.write_bytes(b'original')
            runtime.stage_firmware(package, state, {'firmware/' + relative: runtime.sha(payload)}, firmware)
            target.write_bytes(b'legitimate later update')
            with self.assertRaises(ValueError):
                runtime.restore_firmware(state, firmware)
            self.assertEqual(target.read_bytes(), b'legitimate later update')
            target.unlink()
            target.symlink_to(payload)
            with self.assertRaises(ValueError):
                runtime.restore_firmware(state, firmware)
            self.assertEqual(payload.read_bytes(), b'candidate')

    def test_reject_manifest_path_escape_and_corrupt_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            manifest = root / 'SHA256SUMS'
            manifest.write_text('00  ../escape\n')
            with self.assertRaises(ValueError):
                runtime.checked_package(root)
            payload = root / 'modules/example.ko'
            payload.parent.mkdir()
            payload.write_bytes(b'broken module')
            manifest.write_text('00  modules/example.ko\n')
            with self.assertRaises(ValueError):
                runtime.checked_package(root)
