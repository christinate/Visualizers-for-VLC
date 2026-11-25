"""Check native dialogs and real recording reselection; no audio is saved."""
from pathlib import Path
import argparse
import json
import os
import platform
import re
import subprocess
from check_dawn_window import control_directory

ROOT = Path(__file__).resolve().parents[1]


def owned_records(folder):
    """Use only nonce records belonging to this checker case's unique root."""
    directory = control_directory()
    if not directory.is_dir():
        return []
    matches = []
    for record in list(directory.glob('*.json'))[:2048]:
        try:
            if record.stat().st_size > 4096:
                continue
            value = json.loads(record.read_text(encoding='utf-8'))
            if value['protocol'] != 1 or value['owner_pid'] != 0 or value['id'] != 'terrain':
                continue
            if Path(value['suite']).resolve() != folder.resolve() or not re.fullmatch('[0-9a-f]{32}', value['token']):
                continue
            matches.append((record, value['token']))
        except (OSError, ValueError, KeyError, TypeError):
            continue
    return matches


def request_close(folder):
    for record, token in owned_records(folder):
        record.with_name(record.name + '.close').write_text(token, encoding='ascii')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--picker-probe', type=Path, required=True)
    parser.add_argument('--shader', type=Path, default=ROOT.parent/'Terrain64/assets/terrain/terrain.wgsl')
    parser.add_argument('--plugin', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, default=ROOT/'build/dawn-input-check')
    parser.add_argument('--software', action='store_true')
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    probe = subprocess.run([str(args.picker_probe.resolve())], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=15)
    (output/'picker.stdout.log').write_text(probe.stdout, encoding='utf-8')
    (output/'picker.stderr.log').write_text(probe.stderr, encoding='utf-8')
    if probe.returncode or 'owner-close pass' not in probe.stdout:
        raise RuntimeError('Native picker fixture failed: ' + probe.stderr[-4096:])
    cases = []
    for fullscreen in (False, True):
        name = 'fullscreen-reselection' if fullscreen else 'windowed-reselection'
        folder = output/name
        folder.mkdir(parents=True, exist_ok=True)
        # Never accept a previous run's report as evidence for this process.
        result = folder/'result.json'
        if result.exists():
            result.unlink()
        arguments = ['--terrain-shader', str(args.shader.resolve()), '--plugin', str(args.plugin.resolve()),
                     '--self-test', '--reselection-test', '--select-input', '--time-scale', '180',
                     '--seconds', '25', '--output-dir', str(folder), '--render-width', '320', '--render-height', '180']
        if fullscreen:
            arguments.append('--startup-fullscreen')
        environment = os.environ.copy()
        if args.software:
            arguments.extend(['--render-width', '192', '--render-height', '128'])
            if platform.system() == 'Linux':
                driver = Path('/usr/share/vulkan/icd.d/lvp_icd.json')
                if not driver.is_file():
                    raise RuntimeError('Installed lavapipe ICD is required for this software test')
                environment['VK_DRIVER_FILES'] = str(driver)
            else:
                arguments.append('--software')
        stdout_path, stderr_path = folder/'stdout.log', folder/'stderr.log'
        if platform.system() == 'Darwin':
            bundle = args.exe.resolve().parents[2]
            if bundle.suffix != '.app':
                raise RuntimeError('On macOS --exe must name the native app bundle executable')
            command = ['open', '-W', '-n', '--stdout', str(stdout_path), '--stderr', str(stderr_path), str(bundle), '--args', *arguments]
        else:
            command = [str(args.exe.resolve()), *arguments]
        with stdout_path.open('w', encoding='utf-8') as stdout, stderr_path.open('w', encoding='utf-8') as stderr:
            child = subprocess.Popen(command, stdout=stdout, stderr=stderr, env=environment)
            try:
                code = child.wait(timeout=35)
            except subprocess.TimeoutExpired:
                request_close(folder)
                try:
                    child.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=5)
                raise RuntimeError('Native input check timed out; inspect its permission/error log: ' + name)
        if not result.is_file():
            raise RuntimeError('Native input check did not write a fresh report: ' + name)
        report = json.loads(result.read_text(encoding='utf-8'))
        assert code == 0 and report['result'] == 'pass', report
        assert report['recording_started'] and report['recording_input_changes'] == 4, report
        assert report['input_picker_opens'] == 4 and report['recording_frames'] > 0, report
        assert report['closed_by'] == 'escape' and report['final_fullscreen'] == fullscreen, report
        assert report['native_plugin_api'] == 4 and report['maximum_frames_in_flight'] <= 2, report
        assert not owned_records(folder), 'Window control record was not released'
        cases.append({'case': name, 'result': 'pass', 'observed': report})
        print(name + ': pass', flush=True)
    (output/'summary.json').write_text(json.dumps({'result': 'pass', 'audio_saved': False, 'cases': cases}, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
