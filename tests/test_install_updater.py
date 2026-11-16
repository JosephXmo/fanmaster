import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('install_updater', Path(__file__).resolve().parents[1] / 'scripts/install_updater.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


class InstallationTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.main = Path(self.tmp.name) / 'main'
        self.updater = Path(self.tmp.name) / 'updater'
        self.main.mkdir(); self.updater.mkdir()
        table = b''.join(struct.pack('<HBBII16sI', 0x50AA, kind, sub, offset, size, name, 0)
                         for kind, sub, offset, size, name in (
                             (1, 2, 0x9000, 0x5000, b'nvs'),
                             (1, 0, 0xE000, 0x2000, b'otadata'),
                             (0, 0x10, 0x10000, 0x140000, b'app0'),
                             (0, 0x11, 0x150000, 0x140000, b'app1')))
        (self.main / 'fanmaster.ino.partitions.bin').write_bytes(table)
        (self.updater / 'ResourceUploader.ino.partitions.bin').write_bytes(table)
        (self.main / 'fanmaster.ino.bin').write_bytes(self.image(b'fanmaster'))
        (self.updater / 'ResourceUploader.ino.bin').write_bytes(self.image(b'ResourceUploader'))
        (self.main / 'fanmaster.ino.bootloader.bin').write_bytes(b'bootloader')
        (self.main / 'boot_app0.bin').write_bytes(b'boot selection')

    @staticmethod
    def image(role):
        data = bytearray(512)
        data[0] = 0xE9
        struct.pack_into('<I', data, 32, 0xABCD5432)
        data[288:296] = b'FMBOOT1\0'
        data[296:296 + len(role)] = role
        return data

    def test_app1_only_and_initial_install(self):
        writes = installer.installation(self.main, self.updater)
        self.assertEqual([x[0] for x in writes], [0x150000])
        writes = installer.installation(self.main, self.updater, both=True)
        self.assertEqual([x[0] for x in writes], [0, 0x8000, 0xE000, 0x10000, 0x150000])
        self.assertNotIn(0x9000, [x[0] for x in writes])

    def test_wrong_image_and_oversize(self):
        target = self.updater / 'ResourceUploader.ino.bin'
        for image in (self.image(b'fanmaster'), b'bad', self.image(b'ResourceUploader') + bytes(0x140000)):
            target.write_bytes(image)
            with self.assertRaises(ValueError):
                installer.installation(self.main, self.updater)

    def test_different_and_unsupported_partition_layouts(self):
        table = self.updater / 'ResourceUploader.ino.partitions.bin'
        table.write_bytes(b'wrong table')
        with self.assertRaisesRegex(ValueError, 'differ'):
            installer.installation(self.main, self.updater)
        (self.main / 'fanmaster.ino.partitions.bin').write_bytes(b'wrong table')
        with self.assertRaisesRegex(ValueError, 'default'):
            installer.installation(self.main, self.updater)


if __name__ == '__main__':
    unittest.main()
