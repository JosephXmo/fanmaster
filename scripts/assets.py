#!/usr/bin/env python3
"""Pack/verify/upload a read-only W25Q64 resource image (no third-party pack deps)."""
import argparse
import mimetypes
from pathlib import Path
import struct
import time
import zlib

MAGIC = b'FMASSET1'
HEADER = struct.Struct('<8s6I')
ENTRY = struct.Struct('<96s48s4I')
CAPACITY = 8 * 1024 * 1024
MAX_ENTRIES = 64


def crc(data):
    return zlib.crc32(data) & 0xffffffff


def cstring(text, capacity):
    value = text.encode('utf-8')
    if not value or len(value) >= capacity or b'\0' in value:
        raise ValueError(f'Invalid or too long field: {text!r}')
    return value + bytes(capacity - len(value))


def pack(root):
    root = Path(root)
    files = sorted((p for p in root.rglob('*') if p.is_file()),
                   key=lambda p: p.relative_to(root).as_posix())
    if not files or len(files) > MAX_ENTRIES:
        raise ValueError('Resource count must be 1..64')
    entries, payload = bytearray(), bytearray()
    offset = HEADER.size + ENTRY.size * len(files)
    for path in files:
        if path.is_symlink() or root.resolve() not in path.resolve().parents:
            raise ValueError('Resources must be regular files within the resource directory')
        url = '/' + path.relative_to(root).as_posix()
        if any(c in url for c in '\r\n?#%\\') or any(x.startswith('.') for x in url.split('/')[1:]):
            raise ValueError(f'Unsafe resource path: {url}')
        data = path.read_bytes()
        mime = {'html': 'text/html; charset=utf-8', 'css': 'text/css; charset=utf-8',
                'js': 'text/javascript; charset=utf-8', 'svg': 'image/svg+xml'}.get(
                    path.suffix[1:].lower(), mimetypes.guess_type(path.name)[0] or 'application/octet-stream')
        entries += ENTRY.pack(cstring(url, 96), cstring(mime, 48), offset, len(data), crc(data), 0)
        payload += data
        offset += len(data)
        if offset > CAPACITY:
            raise ValueError('Resource image exceeds W25Q64 capacity')
    prefix = struct.pack('<8s5I', MAGIC, 1, len(files), offset, crc(entries), crc(payload))
    image = prefix + struct.pack('<I', crc(prefix)) + entries + payload
    verify(image)
    return image


def verify(image):
    if len(image) < HEADER.size:
        raise ValueError('Truncated header')
    magic, version, count, size, index_crc, payload_crc, header_crc = HEADER.unpack_from(image)
    if magic != MAGIC or version != 1 or not 1 <= count <= MAX_ENTRIES:
        raise ValueError('Invalid header')
    start = HEADER.size + count * ENTRY.size
    if size != len(image) or size > CAPACITY or start > size or crc(image[:28]) != header_crc:
        raise ValueError('Invalid image length or header CRC')
    if crc(image[HEADER.size:start]) != index_crc or crc(image[start:]) != payload_crc:
        raise ValueError('Index or payload CRC mismatch')
    previous = ''
    names = []
    for i in range(count):
        path, mime, offset, length, checksum, reserved = ENTRY.unpack_from(image, HEADER.size + i * ENTRY.size)
        if b'\0' not in path or b'\0' not in mime:
            raise ValueError('Unterminated index string')
        path = path.split(b'\0')[0].decode('utf-8')
        mime = mime.split(b'\0')[0].decode('utf-8')
        if (not path.startswith('/') or path <= previous or not mime or '\r' in mime or '\n' in mime or
                reserved or offset != start or offset + length > size):
            raise ValueError('Invalid resource index')
        if crc(image[offset:offset + length]) != checksum:
            raise ValueError('Resource CRC mismatch')
        names.append(path)
        previous, start = path, offset + length
    if start != size:
        raise ValueError('Unexpected trailing data')
    return names


def upload(image, port, baud):
    verify(image)
    try:
        import serial
    except ImportError as exc:
        raise SystemExit('Install pyserial in a venv: python3 -m pip install pyserial') from exc
    # Never auto-select a port or flash the ESP32 application.
    with serial.Serial(port, baud, timeout=1, write_timeout=10) as connection:
        def wait_for(expected, timeout):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                line = connection.readline().decode('ascii', errors='replace').strip()
                if line.startswith('ERROR'):
                    raise RuntimeError(line)
                if line == expected:
                    return line
                if line.startswith('ACK ') or line.startswith('DONE'):
                    raise RuntimeError(f'Unexpected uploader response: {line!r}')
            raise TimeoutError(f'Timed out waiting for {expected!r}')
        # Synchronize with the sketch after USB reset/enumeration, without a fixed sleep.
        deadline = time.monotonic() + 15
        while True:
            connection.write(b'PING\n')
            try:
                wait_for('PONG', 1)
                break
            except TimeoutError:
                if time.monotonic() >= deadline:
                    raise TimeoutError('ResourceUploader did not respond; flash the uploader first')
        connection.write(f'UPLOAD {len(image)} {crc(image):08x}\n'.encode('ascii'))
        wait_for('READY', 600)
        for offset in range(0, len(image), 256):
            chunk = image[offset:offset + 256]
            connection.write(chunk)
            wait_for(f'ACK {offset + len(chunk)}', 10)
        print(wait_for('DONE verified', 15))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    build = commands.add_parser('pack')
    build.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[1] / 'web')
    build.add_argument('--output', type=Path, required=True)
    check = commands.add_parser('verify')
    check.add_argument('image', type=Path)
    send = commands.add_parser('upload')
    send.add_argument('image', type=Path)
    send.add_argument('--port', required=True)
    send.add_argument('--baud', type=int, default=115200)
    args = parser.parse_args()
    try:
        if args.command == 'pack':
            image = pack(args.source)
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(image)
            print(f'{len(image)} bytes; {len(verify(image))} resources -> {args.output}')
        elif args.command == 'verify':
            print('\n'.join(verify(args.image.read_bytes())))
        else:
            upload(args.image.read_bytes(), args.port, args.baud)
    except (ValueError, OSError, TimeoutError, RuntimeError) as exc:
        raise SystemExit(str(exc)) from exc


if __name__ == '__main__':
    main()
