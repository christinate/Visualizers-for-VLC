"""Check native capture for original engine hosts on owner-local test suites.

No audio is saved. --generated tests transport/IPC independently of hardware;
without it, the real default input is opened through the native backend.
"""
from pathlib import Path
import argparse
import json
import os
import platform
import re
import subprocess
import time
from check_dawn_player import wait_for, records, close_owned, ipc_file


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--generated', action='store_true')
    args = parser.parse_args()
    suite, output = args.suite.resolve(), args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    run = output/('run-'+str(time.time_ns()))
    run.mkdir()
    executable = suite/('Visualizers.exe' if platform.system() == 'Windows' else 'Visualizers')
    assert not (suite/'browser').exists(), 'Use a separate browser-free integration suite'
    cases = []
    for identifier in ('geiss', 'milkdrop'):
        for cancel in (False, True):
            name = identifier+('-cancel' if cancel else '-reselect')
            folder = run/name
            folder.mkdir()
            profile = folder/'profile'
            command = [str(executable), '--visualizer', identifier, '--line-in', 'default', '--self-test', '--capture-service-test',
                       '--quit-after', '8000', '--profile', str(profile)]
            if identifier == 'geiss':
                command.append('--classic')
            if args.generated:
                command.append('--capture-generated-test')
            if cancel:
                command.append('--capture-cancel-test')
            capture_folder = profile/('capture-'+identifier)
            with (folder/'frontend.stdout.log').open('w', encoding='utf-8') as stdout, (folder/'frontend.stderr.log').open('w', encoding='utf-8') as stderr:
                child = subprocess.Popen(command, stdout=stdout, stderr=stderr)
                try:
                    ready = wait_for(capture_folder/'ready.json', [child], timeout=12)
                    assert ready['owner_pid'] == child.pid and ready['published_frames'] >= 1024, ready
                    helper_pid = ready['helper_pid']
                    code = child.wait(timeout=20)
                    result = wait_for(capture_folder/'result.json', [], timeout=10)
                    assert code == 0 and result['result'] == 'pass' and result['closed_by'] == 'owner-exit', result
                    assert result['helper_pid'] == helper_pid and result['owner_pid'] == child.pid, result
                    assert result['input_picker_opens'] == 3 and result['capture_activations'] == (1 if cancel else 4), result
                    assert result['generated_pcm'] == args.generated and not result['audio_saved'], result
                    assert result['capture_frames'] > 20000 and result['published_frames'] > 10000 and not result['permission_pending'], result
                    if not args.generated:
                        assert result['backend'] in ('WASAPI', 'wasapi', 'PulseAudio', 'pulseaudio', 'Core Audio', 'coreaudio', 'ALSA', 'alsa'), result
                    log = (profile/'last-run.log').read_text(encoding='utf-8')
                    assert 'exit=0' in log and 'capture_reselections=3' in log, log
                    peak = re.search(r'audio_peak=([0-9.eE+-]+)', log)
                    assert peak and (float(peak[1]) > .1 if args.generated else float(peak[1]) >= 0), log
                    assert not records(suite, child.pid), 'Capture helper left a control record'
                    mapping = ipc_file(child.pid)
                    assert mapping is None or not mapping.exists(), 'Capture helper left its audio IPC file'
                    (folder/'observed.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
                    cases.append(dict(case=name, result='pass', observed=result))
                    print(name+': pass', flush=True)
                finally:
                    # Only this checker's owned renderer/helper records are used.
                    close_owned(suite, child.pid)
                    if child.poll() is None:
                        try:
                            child.wait(timeout=3)
                        except subprocess.TimeoutExpired:
                            child.kill()
                            child.wait(timeout=5)
                    deadline = time.monotonic()+5
                    while records(suite, child.pid) and time.monotonic() < deadline:
                        time.sleep(.05)
    (output/'summary.json').write_text(json.dumps(dict(result='pass', cases=cases, generated_pcm=args.generated,
                                                     audio_saved=False, real_player_validation=False, run=str(run)), indent=2)+'\n', encoding='utf-8')
    print('Original-engine native capture service: pass', flush=True)


if __name__ == '__main__':
    main()
