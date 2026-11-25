#!/usr/bin/env python3
"""Generate local synthetic image fixtures without third-party imaging modules."""
import argparse
import base64
import json
import pathlib
import struct
import zlib


def wide_profile():
    # An owned synthetic ICC v2 RGB monitor profile. The three colorant columns
    # sum to PCS D50; the gamut deliberately differs from sRGB. Sampled sRGB
    # transfer curves avoid depending on a platform's profile generator.
    fixed = lambda v: struct.pack('>i', round(v * 65536))
    xyz = lambda values: b'XYZ \0\0\0\0' + b''.join(fixed(v) for v in values)
    name = b'VLC Visualizers synthetic wide RGB\0'
    desc = b'desc\0\0\0\0' + struct.pack('>I', len(name)) + name + bytes(4 + 4 + 2 + 1 + 67)
    curve = b'curv\0\0\0\0' + struct.pack('>I', 4096)
    for n in range(4096):
        v = n / 4095
        linear = v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4
        curve += struct.pack('>H', round(linear * 65535))
    tags = [(b'desc', desc), (b'cprt', b'text\0\0\0\0VLC Visualizers Contributors\0'),
            (b'wtpt', xyz((.9642, 1, .8249))), (b'rXYZ', xyz((.55, .25, 0))),
            (b'gXYZ', xyz((.25, .65, .03))), (b'bXYZ', xyz((.1642, .1, .7949))),
            (b'rTRC', curve), (b'gTRC', curve), (b'bTRC', curve)]
    offset = 128 + 4 + len(tags) * 12
    table, payload = bytearray(), bytearray()
    for signature, data in tags:
        payload += bytes((-len(payload)) % 4)
        table += signature + struct.pack('>II', offset + len(payload), len(data))
        payload += data
    header = bytearray(128)
    header[:24] = struct.pack('>I4sI4s4s4s', offset + len(payload), b'VLC ', 0x02100000, b'mntr', b'RGB ', b'XYZ ')
    header[24:36] = struct.pack('>6H', 2026, 11, 16, 0, 0, 0)
    header[36:44] = b'acspMSFT'
    header[68:80] = b''.join(fixed(v) for v in (.9642, 1, .8249))
    header[80:84] = b'VLC '
    return bytes(header) + struct.pack('>I', len(tags)) + table + payload


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def png(width, height, pixels, profile=None):
    rows = b''.join(b'\0' + pixels[y * width * 4:(y + 1) * width * 4] for y in range(height))
    data = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    if profile:
        data += chunk(b'iCCP', b'fixture\0\0' + zlib.compress(profile))
    return data + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


def gif(width, height, left, top, frame_width, frame_height, transparent=True):
    # Clear before each pixel: fixed 3-bit GIF LZW codes, no dictionary growth.
    codes = []
    for n in range(frame_width * frame_height):
        codes.extend((4, n % 3))
    codes.append(5)
    bits = sum(code << (3 * n) for n, code in enumerate(codes))
    packed = bits.to_bytes((len(codes) * 3 + 7) // 8, 'little')
    blocks = b''.join(bytes((len(packed[n:n + 255]),)) + packed[n:n + 255] for n in range(0, len(packed), 255))
    return (b'GIF89a' + struct.pack('<HHBBB', width, height, 0x81, 3, 0)
            + bytes((255, 0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0))
            + b'!\xf9\x04' + bytes((int(transparent), 0, 0, 3, 0)) + b','
            + struct.pack('<HHHHB', left, top, frame_width, frame_height, 0)
            + b'\x02' + blocks + b'\x00;')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=pathlib.Path)
    parser.add_argument('--profile', type=pathlib.Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cases = []

    def add(name, data, width, height, **options):
        (args.output / name).write_bytes(data)
        cases.append(dict(file=name, width=width, height=height, **options))

    colors = [(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255), (31, 73, 217, 255)]
    pixels = bytes(v for c in colors for v in c)
    expected = [dict(x=n % 2, y=n // 2, rgba=c) for n, c in enumerate(colors)]
    add('opaque.png', png(2, 2, pixels), 2, 2, pixels=expected)
    add('image-\u65e5\u672c\u8a9e-\U0001f3b5.png', png(2, 2, pixels), 2, 2, pixels=expected)
    add('alpha.png', png(2, 1, bytes((31, 73, 217, 13, 255, 255, 0, 0))), 2, 1)
    add('transparent.png', png(1, 1, bytes((255, 0, 0, 0))), 1, 1, transparent=True)
    larger = bytes(v for y in range(64) for x in range(128) for v in (x * 2, y * 4, (x + y) % 256, 255))
    add('downsample.png', png(128, 64, larger), 16, 8, side=16)
    add('full-frame.gif', gif(4, 2, 0, 0, 4, 2), 4, 2)
    add('offset-frame.gif', gif(12, 10, 2, 3, 4, 2), 12, 10)
    add('offset-opaque.gif', gif(12, 10, 2, 3, 4, 2, False), 12, 10)
    add('downsample-offset.gif', gif(256, 192, 123, 97, 4, 2), 64, 48, side=64)
    global_gif = gif(4, 2, 0, 0, 4, 2)
    local_gif = bytearray(global_gif[:13] + global_gif[25:])
    local_gif[10] &= 0x7f
    # Graphic-control extension is eight bytes, then image descriptor is ten.
    local_gif[13 + 8 + 9] = 0x81
    local_gif[13 + 8 + 10:13 + 8 + 10] = global_gif[13:25]
    add('local-table.gif', bytes(local_gif), 4, 2)
    add('truncated-table.gif', global_gif[:15], 0, 0, error=True)
    add('missing-frame.gif', global_gif[:33] + b';', 0, 0, error=True)
    add('corrupt.png', b'not an image', 0, 0, error=True)
    add('oversized.png', png(32769, 1, bytes(32769 * 4)), 0, 0, error=True)
    profile = args.profile.read_bytes() if args.profile else wide_profile()
    (args.output / 'synthetic-wide.icc').write_bytes(profile)
    add('tagged-wide.png', png(2, 2, pixels, profile), 2, 2, profile_required=True)
    # Locally encoded six-color 24x16 test image. The encoder's sRGB ICC packet
    # was removed; the pixel data is owned synthetic data, not an external photo.
    jpeg = base64.b64decode('/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/2wBDAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/wAARCAAQABgDAREAAhEBAxEB/8QAFQABAQAAAAAAAAAAAAAAAAAACQv/xAAUEAEAAAAAAAAAAAAAAAAAAAAA/8QAFwEBAQEBAAAAAAAAAAAAAAAACQoLB//EABQRAQAAAAAAAAAAAAAAAAAAAAD/2gAMAwEAAhEDEQA/ADXDOqIIw6QypzcAfWsJ/wC12AjkASrkwOgRBGe//9k=')
    jpeg_colors = [(204, 51, 0, 255), (0, 204, 51, 255), (204, 204, 0, 255),
                   (0, 51, 204, 255), (85, 85, 85, 255), (0, 204, 204, 255)]
    corner_order = [(0, 2, 3, 5), (2, 0, 5, 3), (5, 3, 2, 0), (3, 5, 0, 2),
                    (0, 3, 2, 5), (3, 0, 5, 2), (5, 2, 3, 0), (2, 5, 0, 3)]
    for orientation in range(1, 9):
        exif = b'Exif\0\0II*\0' + struct.pack('<IH', 8, 1) + struct.pack('<HHIHHI', 0x112, 3, 1, orientation, 0, 0)
        encoded = jpeg[:2] + b'\xff\xe1' + struct.pack('>H', len(exif) + 2) + exif + jpeg[2:]
        width, height = (16, 24) if orientation >= 5 else (24, 16)
        corners = [(0, 0), (width - 1, 0), (0, height - 1), (width - 1, height - 1)]
        checks = [dict(x=x, y=y, rgba=jpeg_colors[color]) for (x, y), color in zip(corners, corner_order[orientation - 1])]
        add('oriented-' + str(orientation) + '.jpg', encoded, width, height, pixels=checks, pixel_tolerance=3)
    (args.output / 'fixtures.json').write_text(json.dumps(dict(cases=cases), ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(cases=len(cases), output=str(args.output))))


if __name__ == '__main__':
    main()
