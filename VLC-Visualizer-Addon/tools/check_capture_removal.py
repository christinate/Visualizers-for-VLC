"""Linux-only local capture removal/reconnection test using a temporary silent monitor."""
from pathlib import Path
import argparse
import json
import os
import re
import signal
import subprocess
import time
import uuid
from check_dawn_input import owned_records


def pactl(*arguments):
    return subprocess.run(['pactl', *arguments], check=True, capture_output=True, text=True, timeout=5).stdout.strip()


def objects(kind):
    if kind == 'modules':
        # PipeWire 1.0's JSON module list omits indices. Its short listing
        # supplies them; module arguments for this test are a single line.
        rows = [line.split('\t', 3) for line in pactl('list', 'short', 'modules').splitlines()]
        return [{'index': row[0], 'name': row[1], 'argument': row[2]} for row in rows if len(row) >= 3 and row[0].isdigit()]
    return json.loads(pactl('--format=json', 'list', kind))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', required=True, type=Path)
    parser.add_argument('--output-dir', required=True, type=Path)
    parser.add_argument('--debug-hang', action='store_true', help='Capture all thread stacks under local GDB if removal hangs')
    parser.add_argument('--visualizer', type=Path)
    parser.add_argument('--plugin', type=Path)
    parser.add_argument('--shader', type=Path)
    parser.add_argument('--software', action='store_true')
    args = parser.parse_args()
    if args.visualizer and (not args.plugin or not args.shader):
        parser.error('--visualizer requires --plugin and --shader')
    folder = args.output_dir.resolve()
    folder.mkdir(parents=True, exist_ok=True)
    probe = args.probe.resolve()
    name = 'vlcvis_capture_test_' + uuid.uuid4().hex
    source_name = name + '.monitor'
    originals = {kind: pactl('get-default-' + kind) for kind in ('sink', 'source')}
    module = None
    child = None
    observations = []

    def restore_defaults():
        for kind, original in originals.items():
            if pactl('get-default-' + kind) in (name, source_name):
                pactl('set-default-' + kind, original)

    def unload():
        nonlocal module
        if module is None:
            return
        # Resolve the current index by the unique owned name. Never unload
        # a stale index that a restarted server may have reassigned.
        found = next((item for item in objects('modules') if item['name'] == 'module-null-sink' and
                      re.search(r'(?:^|\s)sink_name=' + re.escape(name) + r'(?:\s|$)', item.get('argument', ''))), None)
        if found:
            restore_defaults()
            pactl('unload-module', str(found['index']))
        module = None

    def load():
        nonlocal module
        module = pactl('load-module', 'module-null-sink', 'sink_name=' + name, 'format=float32le', 'rate=48000', 'channels=2',
                       'sink_properties=device.description=VLC_Visualizer_Local_Test')
        if not re.fullmatch('[0-9]+', module):
            raise RuntimeError('Audio server did not return a module index')
        restore_defaults()
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            source = next((item for item in objects('sources') if item['name'] == source_name), None)
            if source:
                return source
            time.sleep(.02)
        raise RuntimeError('Temporary silent monitor did not appear')

    def capture(token, label):
        result = folder/(label + '.json')
        if result.exists():
            result.unlink()
        run = subprocess.run([str(probe), '--input', token, '--seconds', '.3', '--output', str(result)], capture_output=True, text=True, timeout=8)
        (folder/(label + '.stderr.log')).write_text(run.stderr, encoding='utf-8')
        if run.returncode or not result.is_file():
            raise RuntimeError('Capture case failed: ' + label + ': ' + run.stderr[-4096:])
        report = json.loads(result.read_text(encoding='utf-8'))
        assert report['result'] == 'pass' and report['capture']['frames'] > 0, report
        observations.append({'case': label, 'capture': report['capture']})

    try:
        source = load()
        identifier = 'PulseAudio:' + source_name
        capture(identifier, 'explicit-id')
        capture(source['description'], 'exact-label')
        command = [str(probe), '--input', identifier, '--seconds', '3']
        environment = os.environ.copy()
        if args.debug_hang:
            environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'  # LSan cannot run under ptrace.
            command = ['gdb', '-batch', '-ex', 'set pagination off', '-ex', 'set confirm off', '-ex', 'run',
                       '-ex', 'thread apply all bt', '-ex', 'kill', '--args', *command]
        child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=environment)
        deadline = time.monotonic() + 3
        attached = False
        while time.monotonic() < deadline and child.poll() is None:
            streams = objects('source-outputs')
            attached = any((args.debug_hang or str(stream.get('properties', {}).get('application.process.id')) == str(child.pid)) and
                           stream.get('source') == source['index'] and not stream.get('corked', False) for stream in streams)
            if attached:
                break
            time.sleep(.02)
        if not attached:
            raise RuntimeError('Capture did not attach to the explicit test input')
        time.sleep(.2)
        unload()
        try:
            stdout, stderr = child.communicate(timeout=8)
        except subprocess.TimeoutExpired:
            if args.debug_hang:
                child.send_signal(signal.SIGINT)
                stdout, stderr = child.communicate(timeout=5)
                (folder/'hang-backtrace.log').write_text(stdout+'\n'+stderr, encoding='utf-8')
                raise RuntimeError('Removal hang reproduced; local thread stacks saved in hang-backtrace.log')
            raise
        (folder/'removed.stdout.log').write_text(stdout, encoding='utf-8')
        (folder/'removed.stderr.log').write_text(stderr, encoding='utf-8')
        if child.returncode not in (0, 1):
            raise RuntimeError('Device removal crashed the capture process: ' + str(child.returncode))
        assert not any(str(item.get('properties', {}).get('application.process.id')) == str(child.pid) for item in objects('source-outputs'))
        observations.append({'case': 'remove-during-capture', 'exit_code': child.returncode, 'bounded_exit': True})
        absent = subprocess.run([str(probe), '--input', identifier, '--seconds', '.3'], capture_output=True, text=True, timeout=8)
        assert absent.returncode == 1 and 'missing' in absent.stderr.lower(), absent.stderr
        observations.append({'case': 'missing-id-after-removal', 'rejected': True})
        load()
        capture(identifier, 'reconnected-same-id')
        if args.visualizer:
            environment = os.environ.copy()
            if args.software:
                driver = Path('/usr/share/vulkan/icd.d/lvp_icd.json')
                if not driver.is_file():
                    raise RuntimeError('Installed lavapipe ICD is required for this software test')
                environment['VK_DRIVER_FILES'] = str(driver)
            def window_command(case, reselect):
                result_folder = folder/case
                result_folder.mkdir(parents=True, exist_ok=True)
                result_file = result_folder/'result.json'
                if result_file.exists():
                    result_file.unlink()
                command = [str(args.visualizer.resolve()), '--line-in', identifier, '--terrain-shader', str(args.shader.resolve()),
                           '--plugin', str(args.plugin.resolve()), '--self-test', '--seconds', '20', '--time-scale', '180',
                           '--render-width', '192', '--render-height', '128', '--output-dir', str(result_folder)]
                if reselect:
                    command.append('--reselection-test')
                return result_folder, result_file, command
            window_folder, result_file, command = window_command('window-device-change', True)
            changed = subprocess.run(command, capture_output=True, text=True, timeout=30, env=environment)
            (window_folder/'stderr.log').write_text(changed.stderr, encoding='utf-8')
            report = json.loads(result_file.read_text(encoding='utf-8'))
            assert changed.returncode == 0 and report['result'] == 'pass', report
            assert report['recording_input_id'] == 'default' and report['recording_input_changes'] == 5 and report['input_picker_opens'] == 4, report
            assert report['recording_frames'] > 0 and report['closed_by'] == 'escape', report
            assert not owned_records(window_folder), 'Window control record remains'
            observations.append({'case': 'window-change-explicit-monitor-to-default', 'result': 'pass', 'input_activations': 5})
            window_folder, result_file, command = window_command('window-device-removal', False)
            child = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=environment)
            deadline = time.monotonic() + 5
            attached = False
            while time.monotonic() < deadline and child.poll() is None:
                attached = any(str(item.get('properties', {}).get('application.process.id')) == str(child.pid) for item in objects('source-outputs'))
                if attached:
                    break
                time.sleep(.02)
            if not attached:
                raise RuntimeError('Native visualizer did not open the explicit monitor')
            unload()
            stdout, stderr = child.communicate(timeout=30)
            (window_folder/'stderr.log').write_text(stderr, encoding='utf-8')
            report = json.loads(result_file.read_text(encoding='utf-8'))
            assert child.returncode == 0 and report['result'] == 'pass' and report['closed_by'] == 'escape', report
            assert not owned_records(window_folder), 'Removed-device window control record remains'
            observations.append({'case': 'window-removal-and-escape-shutdown', 'result': 'pass'})
    finally:
        try:
            if child is not None and child.poll() is None:
                child.kill()
                child.communicate(timeout=5)
        finally:
            try:
                unload()
            finally:
                restore_defaults()
    final_defaults = {kind: pactl('get-default-' + kind) for kind in originals}
    assert final_defaults == originals, 'Defaults changed during the test; inspect local report'
    assert not any(name in item.get('argument', '') for item in objects('modules')), 'Test module remains loaded'
    report = {'result': 'pass', 'audio_saved': False, 'defaults_restored': True, 'test_module_removed': True, 'cases': observations}
    (folder/'summary.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('Capture device ID/label, live removal, missing ID and reconnection: pass; temporary module removed; defaults unchanged')


if __name__ == '__main__':
    main()
