"""Verify the sole expected difference from a preserved calibration capture."""
from pathlib import Path
import argparse
import hashlib
import json
import math
import struct


def sha256(file):
    return hashlib.sha256(file.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('original', 'corrected', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    original = json.loads(args.original.read_text(encoding='utf-8'))
    corrected = json.loads(args.corrected.read_text(encoding='utf-8'))
    assert not original['provenance'].get('native_shader_fixes'), 'Baseline already has native corrections'
    assert corrected['provenance']['native_shader_fixes'] == ['calibration-final-square-index']
    assert original['adapter'] == corrected['adapter'], 'Expected the same GPU adapter'
    assert original['frames'] == corrected['frames'], 'Reference inputs or scene definitions changed'
    assert original['provenance']['reference_hashes'] == corrected['provenance']['reference_hashes'], 'Preserved source changed'
    changed_pixels = changed_channels = compared = unchanged = 0
    seen = set()
    for frame in original['frames']:
        for key in ('input_file', 'palette_file', 'packed_file'):
            name = frame[key]
            assert Path(name).name == name
            assert (args.original.parent/name).read_bytes() == (args.corrected.parent/name).read_bytes(), name
        name = frame['file']
        assert Path(name).name == name and name not in seen
        seen.add(name)
        before = (args.original.parent/name).read_bytes()
        after = (args.corrected.parent/name).read_bytes()
        assert len(before) == len(after)
        compared += len(before)
        if before == after:
            unchanged += 1
            continue
        assert frame['format'] == 'rgba16float' and frame['calibration']['calibrating_hdr_headroom'], name
        width, height = frame['width'], frame['height']
        fraction = frame['calibration']['width_frac']
        f32 = lambda value: struct.unpack('<f', struct.pack('<f', value))[0]
        size = math.floor(f32(f32(width*f32(fraction))*f32(1/14)))
        left, right = width*.5+size*6, width*.5+size*7
        top, bottom = height*.5-size*.5, height*.5+size*.5
        assert len(before) == width*height*8
        required_changes = 0
        for offset in range(0, len(before), 8):
            pixel = offset//8
            x, y = pixel % width + .5, pixel//width + .5
            radius = f32(size*f32(.12))
            # The strip has rounded outer corners. Only its last square's
            # covered pixels may change, and every such pixel must change.
            px, py = abs(x-width*.5), abs(y-height*.5)
            corner_x = max(0, px-(size*7-radius))
            corner_y = max(0, py-(size*.5-radius))
            required = (left <= x < right and top <= y < bottom
                        and corner_x**2+corner_y**2 <= radius**2)
            differs = before[offset:offset+8] != after[offset:offset+8]
            assert differs == required, (name, x, y, 'unexpected/missing correction')
            required_changes += required
            if not differs:
                continue
            old = struct.unpack_from('<4e', before, offset)
            new = struct.unpack_from('<4e', after, offset)
            circle = ((x-left)/size-.5)**2 + ((y-top)/size-.5)**2 < .35**2
            expected_before, expected_after = (2.55, 2.75) if circle else (2.8, 3.0)
            assert all(abs(value-expected_before) <= .001953125 for value in old), (x, y, old)
            assert all(value == expected_after for value in new), (x, y, new)
            changed_pixels += 1
            changed_channels += 4
        assert required_changes > 0
    assert changed_pixels > 0 and unchanged == len(original['frames'])-1
    title = original['title_file']
    assert title == corrected['title_file'] and (args.original.parent/title).read_bytes() == (args.corrected.parent/title).read_bytes()
    result = {'result': 'pass', 'frames': len(original['frames']), 'unchanged_outputs_exact': unchanged,
              'output_bytes_compared': compared, 'corrected_pixels': changed_pixels,
              'corrected_channels': changed_channels, 'native_fix': 'calibration-final-square-index',
              'original_fixtures_sha256': sha256(args.original),
              'corrected_fixtures_sha256': sha256(args.corrected),
              'scope': 'Only the fourteenth calibration square changes from 2.8/2.55 to 3.0/2.75; all other outputs and inputs exact'}
    args.output.mkdir(parents=True, exist_ok=False)
    (args.output/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
