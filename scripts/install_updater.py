#!/usr/bin/env python3
"""Install the resident app1 uploader, or both applications for initial setup."""
import argparse
from pathlib import Path
import shlex
import struct
import subprocess
import sys


def application(path, project, capacity):
    data = path.read_bytes()
    # ESP image header (24), first segment header (8), esp_app_desc_t.
    if (len(data) < 328 or len(data) > capacity or data[0] != 0xE9 or
            struct.unpack_from('<I', data, 32)[0] != 0xABCD5432 or
            data[288:296] != b'FMBOOT1\0' or
            data[296:320].split(b'\0')[0] != project.encode('ascii')):
        raise ValueError(f'{path}: invalid/oversized {project} application')
    return path


def installation(main_build, updater_build, both=False):
    main_build, updater_build = Path(main_build), Path(updater_build)
    main_table = main_build / 'fanmaster.ino.partitions.bin'
    updater_table = updater_build / 'ResourceUploader.ino.partitions.bin'
    table = main_table.read_bytes()
    if table != updater_table.read_bytes():
        raise ValueError('Main and updater partition tables differ; build both with the same FQBN')
    slots = {}
    for offset in range(0, len(table) - 31, 32):
        magic, kind, subtype, address, size, label, _ = struct.unpack_from('<HBBII16sI', table, offset)
        if magic != 0x50AA:
            break
        slots[(kind, subtype)] = (address, size, label.split(b'\0')[0])
    # Installer deliberately supports the documented default 4 MiB layout only.
    if (slots.get((0, 0x10)) != (0x10000, 0x140000, b'app0') or
            slots.get((0, 0x11)) != (0x150000, 0x140000, b'app1') or
            slots.get((1, 0)) != (0xE000, 0x2000, b'otadata')):
        raise ValueError('Expected the default app0/app1/otadata partition layout')
    uploader = application(updater_build / 'ResourceUploader.ino.bin', 'ResourceUploader', 0x140000)
    writes = [(0x150000, uploader)]
    if both:
        main = application(main_build / 'fanmaster.ino.bin', 'fanmaster', 0x140000)
        writes = [(0, main_build / 'fanmaster.ino.bootloader.bin'), (0x8000, main_table),
                  (0xE000, main_build / 'boot_app0.bin'), (0x10000, main)] + writes
    for _, path in writes:
        if not path.is_file():
            raise ValueError(f'Missing build artifact: {path}')
    return writes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--main-build', type=Path, default=Path('build/main/build'))
    parser.add_argument('--updater-build', type=Path, default=Path('build/uploader/build'))
    parser.add_argument('--port', required=True)
    parser.add_argument('--esptool', type=Path, help='optional standalone esptool executable, e.g. bundled with Arduino')
    parser.add_argument('--install-both', action='store_true', help='initial installation; resets boot choice to app0')
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    try:
        writes = installation(args.main_build, args.updater_build, args.install_both)
        command = ([str(args.esptool.resolve())] if args.esptool else [sys.executable, '-m', 'esptool'])
        command += ['--chip', 'esp32s3', '--port', args.port, 'write-flash']
        for offset, path in writes:
            command += [hex(offset), str(path)]
        print(shlex.join(command), flush=True)
        if not args.dry_run:
            subprocess.run(command, check=True)
    except (ValueError, OSError, subprocess.CalledProcessError) as exc:
        raise SystemExit(str(exc)) from exc


if __name__ == '__main__':
    main()
