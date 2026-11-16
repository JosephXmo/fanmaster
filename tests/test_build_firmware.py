import importlib.util
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('build_firmware', ROOT / 'scripts/build_firmware.py')
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class StagingTests(unittest.TestCase):
    def test_actual_sources_shared_config_and_stale_files(self):
        with tempfile.TemporaryDirectory() as directory:
            main = builder.stage(ROOT, Path(directory) / 'main', 'main')
            uploader = builder.stage(ROOT, Path(directory) / 'uploader', 'uploader')
            self.assertTrue((main / 'fanmaster.ino').is_file())
            self.assertTrue((main / 'FanController.cpp').is_file())
            self.assertFalse((main / 'ResourceUploader.ino').exists())
            self.assertTrue((uploader / 'ResourceUploader.ino').is_file())
            self.assertTrue((uploader / 'ExitButton.h').is_file())
            self.assertTrue((uploader / 'BootSlots.h').is_file())
            self.assertFalse((uploader / 'FanController.cpp').exists())
            self.assertEqual((main / 'Config.h').read_bytes(), (uploader / 'Config.h').read_bytes())
            source = ROOT / 'src/fanmaster'
            for path in main.iterdir():
                self.assertEqual(path.read_bytes(), (source / path.name).read_bytes())
            (main / 'RemovedController.cpp').write_text('stale translation unit')
            (main / 'FanController.cpp').write_text('stale source')
            builder.stage(ROOT, Path(directory) / 'main', 'main')
            self.assertFalse((main / 'RemovedController.cpp').exists())
            self.assertEqual((main / 'FanController.cpp').read_bytes(), (source / 'FanController.cpp').read_bytes())

    def test_missing_sketch_does_not_accept_old_staged_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src/fanmaster').mkdir(parents=True)
            (root / 'src/fanmaster/Config.h').write_text('config only')
            old = root / 'build/fanmaster'
            old.mkdir(parents=True)
            (old / 'fanmaster.ino').write_text('stale sketch')
            with self.assertRaisesRegex(FileNotFoundError, 'Missing firmware source'):
                builder.stage(root, root / 'build', 'main')
            (root / 'src/ResourceUploader').mkdir()
            (root / 'src/ResourceUploader/ResourceUploader.ino').write_text('sketch only')
            with self.assertRaisesRegex(FileNotFoundError, 'Missing firmware source'):
                builder.stage(root, root / 'build', 'uploader')

    def test_original_sources_cannot_be_used_as_staging_output(self):
        with self.assertRaisesRegex(ValueError, 'outside src/'):
            builder.stage(ROOT, ROOT / 'src', 'main')
        with self.assertRaisesRegex(ValueError, 'outside src/'):
            builder.stage(ROOT, ROOT / 'src', 'uploader')


if __name__ == '__main__':
    unittest.main()
