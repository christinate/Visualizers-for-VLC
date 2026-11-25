#!/usr/bin/env python3
"""Local native/Canvas font inspection. Uses only the Python standard library.

Font engines may differ in hinting, fallback and antialiasing. This records those
differences; it does not impose a false bit-exact equivalence between engines.
The executable probe separately checks native clipping, bounds and lifetimes.
"""
import argparse
import json
import pathlib
import struct
import zlib


def png(path, width, height, rgb):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    rows = b''.join(b'\0' + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b''))


def pixels(folder, case):
    width, height = case['width'], case['height']
    if not width and not height:
        return b''
    name = case['file']
    if pathlib.Path(name).name != name or width > 8192 or height > 8192 or width * height > 16777216:
        raise ValueError('Invalid font fixture layout')
    data = (folder / name).read_bytes()
    if len(data) != width * height * 4:
        raise ValueError('Incorrect font fixture size')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=pathlib.Path, required=True)
    parser.add_argument('--reference', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    native = json.loads((args.native / 'result.json').read_text(encoding='utf-8'))
    original = json.loads((args.reference / 'fixtures.json').read_text(encoding='utf-8'))
    if native['result'] != 'pass' or original['result'] != 'pass':
        raise ValueError('A font probe failed')
    if len(native['cases']) != len(original['frames']):
        raise ValueError('Font corpus lengths differ')
    pairs = list(zip(original['frames'], native['cases']))
    column = max(case['width'] for pair in pairs for case in pair) + 16
    row_height = max(case['height'] for pair in pairs for case in pair) + 16
    width, height = column * 2, row_height * len(pairs)
    rgb = bytearray(b'\xff' * (width * height * 3))
    report = {'result': 'pass', 'layout': 'Original Canvas left, native right; rows follow cases',
              'scope': 'Native font inspection; pixel or metric parity is not asserted', 'cases': []}
    for n, (reference, actual) in enumerate(pairs):
        if actual['text'] != reference['text'] or actual.get('clipped_ink_pixels', 0):
            raise ValueError('Native corpus or clipping regression')
        report['cases'].append({'text': actual['text'], 'reference_size': [reference['width'], reference['height']],
                                'native_size': [actual['width'], actual['height']],
                                'reference_clipped_ink_pixels': reference.get('clipped_ink_pixels', 0),
                                'native_clipped_ink_pixels': actual.get('clipped_ink_pixels', 0)})
        for side, (folder, case) in enumerate(((args.reference, reference), (args.native, actual))):
            rgba = pixels(folder, case)
            for y in range(case['height']):
                for x in range(case['width']):
                    shade = 255 - rgba[(y * case['width'] + x) * 4 + 3]
                    target = ((n * row_height + 8 + y) * width + side * column + 8 + x) * 3
                    rgb[target:target + 3] = bytes((shade, shade, shade))
    args.output.mkdir(parents=True, exist_ok=True)
    png(args.output / 'fonts.png', width, height, rgb)
    (args.output / 'comparison.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'result': report['result'], 'cases': len(pairs), 'image': str(args.output / 'fonts.png')}))


if __name__ == '__main__':
    main()
