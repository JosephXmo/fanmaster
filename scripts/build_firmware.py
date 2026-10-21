#!/usr/bin/env python3
"""Stage an Arduino sketch with the shared Config/storage files, then compile."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


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
    name = 'fanmaster' if args.target == 'main' else 'ResourceUploader'
    sketch = work / name
    sketch.mkdir(parents=True, exist_ok=True)
    # Avoid compiling nested uploader/test sources as part of the main application.
    if args.target == 'main':
        sources = sorted(p for p in root.iterdir() if p.suffix in ('.cpp', '.h', '.ino'))
    else:
        sources = [root / 'tools/ResourceUploader' / name
                   for name in ('ResourceUploader.ino', 'UploadDisplay.h')]
        sources += [root / p for p in ('Config.h', 'W25q64.h', 'W25q64.cpp', 'AssetFormat.h', 'FanPowerOutput.h')]
    for source in sources:
        shutil.copy2(source, sketch / source.name)
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
