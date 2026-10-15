import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock
import types

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('assets', ROOT / 'scripts/assets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)


class AssetTests(unittest.TestCase):
    def test_roundtrip_and_corruption(self):
        image = assets.pack(ROOT / 'web')
        names = assets.verify(image)
        self.assertIn('/setup.html', names)
        self.assertIn('/assets/panel.js', names)
        self.assertEqual(image, assets.pack(ROOT / 'web'))
        for offset in (0, 28, 40, len(image) - 1):
            corrupt = bytearray(image)
            corrupt[offset] ^= 1
            with self.assertRaises(ValueError):
                assets.verify(corrupt)
        for truncated in (b'', image[:31], image[:-1], image + b'bad'):
            with self.assertRaises(ValueError):
                assets.verify(truncated)

    def test_invalid_resources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(ValueError):
                assets.pack(root)
            (root / ('x' * 96)).write_text('too long path')
            with self.assertRaises(ValueError):
                assets.pack(root)
            (root / ('x' * 96)).unlink()
            (root / 'outside').symlink_to(ROOT / 'Config.h')
            with self.assertRaises(ValueError):
                assets.pack(root)

    def test_upload_handshake_and_exact_acknowledgements(self):
        image = assets.pack(ROOT / 'web')
        instances = []
        class Serial:
            bad_ack = False
            def __init__(self, *args, **kwargs):
                self.lines = []
                self.received = bytearray()
                self.uploading = False
                instances.append(self)
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def readline(self): return self.lines.pop(0) if self.lines else b''
            def write(self, value):
                if not self.uploading and value == b'PING\n':
                    self.lines.append(b'PONG\n')
                elif not self.uploading and value.startswith(b'UPLOAD '):
                    self.uploading = True
                    self.lines.append(b'READY\n')
                else:
                    self.received.extend(value)
                    suffix = '0' if self.bad_ack else ''
                    self.lines.append(f'ACK {len(self.received)}{suffix}\n'.encode())
                    if len(self.received) == len(image): self.lines.append(b'DONE verified\n')
                return len(value)
        with mock.patch.dict('sys.modules', {'serial': types.SimpleNamespace(Serial=Serial)}):
            with mock.patch('builtins.print'):
                assets.upload(image, '/dev/fake', 115200)
            self.assertEqual(instances[-1].received, image)
            Serial.bad_ack = True
            with self.assertRaisesRegex(RuntimeError, 'Unexpected uploader response'):
                assets.upload(image, '/dev/fake', 115200)

    def test_browser_assets_reference_existing_resources(self):
        import re
        files = set(assets.verify(assets.pack(ROOT / 'web')))
        for html in (ROOT / 'web').glob('*.html'):
            for reference in re.findall(r'(?:src|href)="(/[^\"]+)"', html.read_text()):
                self.assertIn(reference, files)


if __name__ == '__main__':
    unittest.main()
