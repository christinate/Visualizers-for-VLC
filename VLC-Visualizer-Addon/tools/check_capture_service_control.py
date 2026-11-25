"""Check capture single-writer requests and failure cleanup with generated PCM.

Runs only our native helper and a held test owner. No real audio device/player
is opened. Kept separate from real-backend and physical keyboard checks.
"""
from pathlib import Path
import argparse
import json
import os
import platform
import subprocess
import time
from check_dawn_player import wait_for, records, close_owned, ipc_file
from check_dawn_window import control_directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--headless', action='store_true')
    parser.add_argument('--skip-real-owner-check', action='store_true',
                        help='Run only generated-mode requests; report the real-mode owner check as untested')
    args = parser.parse_args()
    suite, output = args.suite.resolve(), args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    folder = output/('run-'+str(time.time_ns()))
    folder.mkdir()
    if platform.system() == 'Darwin' and not args.skip_real_owner_check:
        raise RuntimeError('Use --skip-real-owner-check for the generated-only Mac fixture; real capture is a separate permission gate')
    executable = (suite/'native/VisualizerNative.app/Contents/MacOS/VisualizerNative'
                  if platform.system() == 'Darwin' else
                  suite/('native/VisualizerNative.exe' if platform.system() == 'Windows' else 'native/VisualizerNative'))
    environment = os.environ.copy()
    if args.headless:
        assert platform.system() == 'Linux'
        environment['QT_QPA_PLATFORM'] = 'offscreen'
    owner, helper = None, None
    children, streams = [], []
    stop = folder/'stop'
    observed = {}

    def stream(name):
        result = (folder/name).open('w', encoding='utf-8')
        streams.append(result)
        return result

    def command(case, extra=()):
        return [str(executable), '--suite', str(suite), '--visualizer', 'geiss', '--capture-for', str(owner.pid),
                '--self-test', '--capture-test', '--capture-test-generated', '--profile', str(folder/'profile'),
                '--output-dir', str(folder/case), '--seconds', '30', *extra]

    def clean():
        assert not records(suite, owner.pid), 'Native capture control record remains'
        lock = control_directory()/('capture-'+str(owner.pid)+'.lock')
        assert not lock.exists(), 'Native capture source lock remains'
        mapping = ipc_file(owner.pid)
        assert mapping is None or not mapping.exists(), 'Native capture audio IPC remains'

    try:
        owner_log = stream('owner.log')
        owner = subprocess.Popen([str(args.probe.resolve()), '--hold', str(folder/'owner.json'), str(stop)], stdout=owner_log, stderr=owner_log, env=environment)
        wait_for(folder/'owner.json', [owner])
        helper_log = stream('helper.log')
        helper = subprocess.Popen(command('service', ['--line-in', 'default']), stdout=helper_log, stderr=helper_log, env=environment)
        children.append(helper)
        ready = wait_for(folder/'service/ready.json', [helper])
        assert ready['helper_pid'] == helper.pid and ready['owner_pid'] == owner.pid, ready
        owned = records(suite, owner.pid)
        assert len(owned) == 1 and owned[0][1]['pid'] == helper.pid and owned[0][1]['input_selection'] is True, owned
        record, identity = owned[0]
        wrong = 'e'*32 if identity['token'] == 'f'*32 else 'f'*32
        record.with_name(record.name+'.input').write_text(wrong, encoding='ascii')
        time.sleep(.35)
        status = wait_for(folder/'service/status.json', [helper])
        assert status['input_picker_opens'] == 0 and status['capture_activations'] == 1, status
        duplicate_log = stream('duplicates.log')
        duplicates = [subprocess.Popen(command('duplicate', ['--line-in', 'default']), stdout=duplicate_log, stderr=duplicate_log, env=environment) for _ in range(10)]
        children.extend(duplicates)
        deadline = time.monotonic()+15
        maximum_controls = 0
        while any(child.poll() is None for child in duplicates):
            current = records(suite, owner.pid)
            maximum_controls = max(maximum_controls, len(current))
            assert len(current) == 1 and current[0][1]['pid'] == helper.pid, 'Concurrent helper created another writer: '+str(current)
            assert time.monotonic() < deadline, 'Duplicate capture requests did not finish'
            time.sleep(.02)
        assert all(child.returncode == 0 for child in duplicates), [child.returncode for child in duplicates]
        deadline = time.monotonic()+5
        while True:
            status = wait_for(folder/'service/status.json', [helper])
            if status['input_picker_opens'] and status['capture_activations'] >= 2:
                break
            assert time.monotonic() < deadline, status
            time.sleep(.05)
        assert status['helper_pid'] == helper.pid and status['capture_activations'] <= 11, status
        close_owned(suite, owner.pid)
        assert helper.wait(timeout=5) == 0
        result = wait_for(folder/'service/result.json', [])
        assert result['result'] == 'pass' and result['closed_by'] == 'menu' and result['published_frames'] > 1024, result
        clean()
        observed['single_writer'] = dict(duplicate_requests=10, maximum_control_records=maximum_controls, original_helper_pid=helper.pid, observed=result)
        print('single writer / bad nonce / 10 concurrent requests / menu close: pass', flush=True)
        failure_log = stream('failures.log')
        for n in range(40):
            case = 'invalid-input-'+str(n)
            attempt = subprocess.run(command(case, ['--line-in', 'missing-native-capture-fixture']), stdout=failure_log, stderr=failure_log, env=environment, timeout=10)
            result = json.loads((folder/case/'result.json').read_text(encoding='utf-8'))
            assert attempt.returncode == 1 and result['result'] == 'fail' and 'missing' in result['error'].lower(), result
            clean()
        observed['initialization_failures'] = 40
        print('40 invalid-input initialization/IPC/lock cleanup cases: pass', flush=True)
        attempt = subprocess.run(command('startup-cancel', ['--capture-test-cancel']), stdout=failure_log, stderr=failure_log, env=environment, timeout=10)
        result = json.loads((folder/'startup-cancel/result.json').read_text(encoding='utf-8'))
        assert attempt.returncode == 0 and result['result'] == 'pass' and result['closed_by'] == 'input-cancelled', result
        assert result['input_picker_opens'] == 1 and result['capture_activations'] == 0 and result['published_frames'] == 0, result
        clean()
        observed['startup_cancel'] = result
        attempt = subprocess.run(command('bare-line-in', ['--line-in', '--seconds', '.1']), stdout=failure_log, stderr=failure_log, env=environment, timeout=10)
        assert attempt.returncode == 0, 'Bare --line-in did not select the default input'
        result = json.loads((folder/'bare-line-in/result.json').read_text(encoding='utf-8'))
        assert result['capture_activations'] == 1 and result['active_input'] == 'default' and result['input_picker_opens'] == 0, result
        clean()
        observed['bare_line_in'] = result
        # A real capture request may not target an arbitrary process even in
        # self-test mode. The generated fixture is the sole explicit exception.
        if args.skip_real_owner_check:
            observed['real_capture_wrong_owner_rejected'] = 'untested; generated-only invocation'
        else:
            actual = [value for value in command('wrong-owner', ['--line-in', 'default']) if value != '--capture-test-generated']
            attempt = subprocess.run(actual, stdout=failure_log, stderr=failure_log, env=environment, timeout=10)
            assert attempt.returncode == 1, 'Actual recording attached to the held test-owner executable'
            clean()
            observed['real_capture_wrong_owner_rejected'] = True
        print('startup cancellation: pass; real-mode owner check '+('untested' if args.skip_real_owner_check else 'pass'), flush=True)
    finally:
        if owner:
            close_owned(suite, owner.pid)
        stop.write_text('stop\n', encoding='ascii')
        for child in children:
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait(timeout=5)
        if owner:
            try:
                owner.wait(timeout=5)
            except subprocess.TimeoutExpired:
                owner.kill()
                owner.wait(timeout=5)
        for item in streams:
            item.close()
    (output/'summary.json').write_text(json.dumps(dict(result='pass', generated_pcm=True, real_device_opened=False,
                                                     observed=observed, run=str(folder)), indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
