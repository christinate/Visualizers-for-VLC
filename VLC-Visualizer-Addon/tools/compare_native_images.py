#!/usr/bin/env python3
"""Compare owner-local native and unchanged-Canvas image/palette diagnostics."""
import argparse
import json
import pathlib


def read_pixels(folder, name, width, height):
    if pathlib.Path(name).name != name or not 0 < width <= 512 or not 0 < height <= 512:
        raise ValueError('Invalid image fixture layout')
    data = (folder / name).read_bytes()
    if len(data) != width * height * 4:
        raise ValueError('Invalid image fixture byte count')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native', type=pathlib.Path, required=True)
    parser.add_argument('--reference', type=pathlib.Path, required=True)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    args = parser.parse_args()
    actual = json.loads((args.native / 'result.json').read_text(encoding='utf-8'))
    reference = json.loads((args.reference / 'fixtures.json').read_text(encoding='utf-8'))
    if actual['result'] != 'pass' or reference['result'] != 'pass' or len(actual['cases']) != len(reference['frames']):
        raise ValueError('Image probes failed or corpus lengths differ')
    report = dict(result='pass', cases=[], scope='Lossless/orientation/color-management checks; native resampling differences are recorded separately from original-pixel parity')
    for native, original in zip(actual['cases'], reference['frames']):
        name = native['file']
        if name != original['input']:
            raise ValueError('Image corpus identity differs')
        if native.get('expected_error'):
            if not original.get('skipped_native_bound_or_corrupt'):
                raise ValueError('Expected invalid/native-bounded input was not marked')
            report['cases'].append(dict(file=name, native_error_expected=True))
            continue
        if (native['width'], native['height']) != (original['width'], original['height']):
            raise ValueError('Native/original oriented dimensions differ: ' + name)
        a = read_pixels(args.native, native['raw_file'], native['width'], native['height'])
        b = read_pixels(args.reference, original['file'], original['width'], original['height'])
        differences = [abs(x - y) for x, y in zip(a, b)]
        maximum = max(differences)
        palette_error = None
        if 'palette' in native and 'palette' in original:
            palette_error = max(abs(x - y) for ca, cb in zip(native['palette'], original['palette']) for x, y in zip(ca, cb))
        limit = None
        if name.startswith('downsample'):
            category = 'native resampling; pixel/palette differences recorded, parity not asserted'
        elif name == 'alpha.png':
            category, limit = 'premultiplication rounding', 1
        elif name == 'tagged-wide.png':
            category, limit = 'embedded ICC conversion', 3
        elif name.endswith('.jpg'):
            category, limit = 'JPEG decode and EXIF orientation', 3
        else:
            category, limit = 'lossless pixels', 0
        item = dict(file=name, category=category, max_byte_error=maximum,
                    mean_byte_error=sum(differences) / len(differences), differing_bytes=sum(bool(v) for v in differences),
                    max_alpha_error=max(differences[3::4]), max_palette_error=palette_error, byte_error_limit=limit)
        report['cases'].append(item)
        if limit is not None and maximum > limit:
            report['result'] = 'fail'
        if maximum == 0 and palette_error is not None and palette_error > 2e-7:
            report['result'] = 'fail'
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(result=report['result'], cases=len(report['cases']), output=str(args.output))))
    if report['result'] != 'pass':
        raise SystemExit(1)


if __name__ == '__main__':
    main()
