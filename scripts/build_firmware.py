#!/usr/bin/env python3
"""Stage an Arduino sketch with the shared Config/storage files, then compile."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def stage(root, work, target):
    root, work = Path(root).resolve(), Path(work).resolve()
    main = root / 'src' / 'fanmaster'
    uploader = root / 'src' / 'ResourceUploader'
    name = 'fanmaster' if target == 'main' else 'ResourceUploader'
    if target == 'main':
        sources = sorted(p for p in main.iterdir() if p.suffix in ('.cpp', '.h', '.ino'))
        required = main / 'fanmaster.ino'
    else:
        sources = [uploader / filename
                   for filename in ('ResourceUploader.ino', 'UploadDisplay.h', 'ExitButton.h')]
        sources += [main / filename for filename in
                    ('Config.h', 'W25q64.h', 'W25q64.cpp', 'AssetFormat.h',
                     'FanPowerOutput.h', 'BootSlots.h', 'FirmwareIdentity.h')]
        required = uploader / 'ResourceUploader.ino'
    for source in [required, *sources]:
        if not source.is_file():
            raise FileNotFoundError(f'Missing firmware source: {source}')
    sketch = work / name
    if sketch.is_relative_to(root / 'src'):
        raise ValueError('Stage firmware outside src/; edit the original sources there')
    sketch.mkdir(parents=True, exist_ok=True)
    # Reused staging directories must not retain removed/renamed translation units.
    expected = {source.name for source in sources}
    for previous in sketch.iterdir():
        if previous.is_file() and previous.suffix in ('.cpp', '.h', '.ino') and previous.name not in expected:
            previous.unlink()
    for source in sources:
        shutil.copy2(source, sketch / source.name)
    return sketch


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', choices=('main', 'uploader'))
    parser.add_argument('--arduino-cli', default='arduino-cli')
    parser.add_argument('--libraries', type=Path, action='append', default=[])
    parser.add_argument('--fqbn', default='esp32:esp32:esp32s3:CDCOnBoot=cdc')
    parser.add_argument('--work-dir', type=Path)
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--build-property', action='append', default=[])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    work = (args.work_dir or Path(tempfile.mkdtemp(prefix='fanmaster-build-'))).resolve()
    sketch = stage(root, work, args.target)
    print(sketch, flush=True)
    if not args.prepare_only:
        command = [args.arduino_cli, 'compile', '--fqbn', args.fqbn,
                   '--build-path', str(work / 'build'), str(sketch)]
        for path in args.libraries:
            command.extend(['--libraries', str(path.resolve())])
        for value in args.build_property:
            command.extend(['--build-property', value])
        subprocess.run(command, check=True)


if __name__ == '__main__':
    main()
