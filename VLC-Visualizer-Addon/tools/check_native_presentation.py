"""Owner-local native window HDR capability/fallback checks; no audio recording."""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--terrain-module', type=Path, required=True)
    parser.add_argument('--terrain-assets', type=Path, required=True)
    parser.add_argument('--prism-module', type=Path, required=True)
    parser.add_argument('--prism-assets', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expect-sdr', action='store_true')
    parser.add_argument('--suite', type=Path, help='Use the production host with a staged suite')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    observations = []
    for visualizer, module, assets in (
            ('terrain', args.terrain_module, args.terrain_assets),
            ('geiss', args.prism_module, args.prism_assets)):
        for hdr in (False, True):
            folder = output / f'{visualizer}-hdr-{int(hdr)}'
            profile = folder / 'profile'
            profile.mkdir(parents=True)
            (profile / f'{visualizer}.json').write_text(json.dumps({
                'hdr': hdr, 'width': 320, 'height': 240, 'fps': 60,
                'peak': 600, 'seed': 12345}), encoding='utf-8')
            command = [str(args.exe.resolve()), '--visualizer', visualizer,
                       '--profile', str(profile), '--output-dir', str(folder),
                       '--self-test', '--synthetic', '--background', '--seconds', '20',
                       '--render-width', '192', '--render-height', '128']
            if args.suite:
                command.extend(['--suite', str(args.suite.resolve())])
            else:
                command.extend(['--plugin', str(module.resolve()), '--assets', str(assets.resolve())])
            child = subprocess.run(command, capture_output=True, encoding='utf-8',
                                   errors='replace', timeout=40)
            (folder / 'stdout.log').write_text(child.stdout, encoding='utf-8')
            (folder / 'stderr.log').write_text(child.stderr, encoding='utf-8')
            if not (folder / 'result.json').is_file():
                raise RuntimeError(f'{folder.name}: exit {child.returncode} without a report: {child.stderr[-2048:]}')
            report = json.loads((folder / 'result.json').read_text(encoding='utf-8'))
            assert child.returncode == 0 and report['result'] == 'pass', report
            mode = report['presentation']
            assert mode['hdr_requested'] == hdr and mode['hdr_experimental'], mode
            assert report['frames'] >= 90 and report['maximum_frames_in_flight'] <= 2, report
            assert report['closed_by'] == 'escape' and mode['reason'], report
            if not hdr or args.expect_sdr:
                assert not mode['hdr_active'], mode
                if not (hdr and mode['format'] == 'rgba16float'):
                    assert not mode['source_linear'], mode
                    assert mode['format'] in ('bgra8unorm', 'rgba8unorm'), mode
            if mode['hdr_active']:
                assert (mode['display_hdr_enabled'] and mode['native_hdr_transport']
                        and mode['surface_float16'] and mode['source_linear']), mode
                assert mode['format'] == 'rgba16float', mode
            if mode['format'] == 'rgba16float' and mode.get('metal_layer_available'):
                assert (mode['metal_surface_float16'] and mode['metal_edr_requested']
                        and mode['metal_extended_linear_srgb'] and mode['white_scale'] == 1), mode
                assert mode['source_linear'] and mode['output_mode'] == (1 if mode['hdr_active'] else 3), mode
            assert not report['recording_started'], report
            observations.append({'case': folder.name, 'result': 'pass',
                                 'presentation': mode, 'frames': report['frames']})
            print(folder.name + ': pass', flush=True)
    result = {'result': 'pass', 'cases': observations,
              'scope': 'Native capability selection, saved HDR setting, real surface, '
                       'both modern modules and Escape closure. No physical HDR claim.'}
    (output / 'result.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
