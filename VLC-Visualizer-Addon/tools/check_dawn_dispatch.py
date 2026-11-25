"""Exercise production host -> native module dispatch on an owner-local suite.

Generated PCM only; this is separate from real VLC/Winamp playback and native
recording. All reports remain in --output-dir. Use an expendable staged suite.
"""
from pathlib import Path
import argparse
import json
import os
import platform
import subprocess
import time
from check_dawn_player import wait_for, records, close_owned, ipc_file


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--software', action='store_true')
    parser.add_argument('--player-probe', type=Path, help='Also check generated player PCM and process ownership through the production frontend')
    args = parser.parse_args()
    suite, output = args.suite.resolve(), args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    windows = platform.system() == 'Windows'
    suffix = '.exe' if windows else ''
    host, ctl = suite/('Visualizers'+suffix), suite/('VisualizersCtl'+suffix)
    environment = os.environ.copy()
    if args.software:
        if platform.system() != 'Linux':
            raise RuntimeError('This integration checker uses installed Linux lavapipe')
        environment['VK_DRIVER_FILES'] = '/usr/share/vulkan/icd.d/lvp_icd.json'
    cases = []

    def run(command, folder, expect=0):
        folder.mkdir(parents=True, exist_ok=True)
        result = subprocess.run([str(p) for p in command], capture_output=True, text=True, encoding='utf-8', errors='replace', env=environment, timeout=40)
        (folder/'stdout.log').write_text(result.stdout, encoding='utf-8')
        (folder/'stderr.log').write_text(result.stderr, encoding='utf-8')
        (folder/'exit.json').write_text(json.dumps(dict(exit=result.returncode)), encoding='utf-8')
        assert (result.returncode == 0 if expect == 0 else result.returncode != 0), (command, result.returncode, result.stderr[-4096:])
        return result

    def native(identifier, name, closure):
        folder = output/name
        report = folder/'result.json'
        if report.exists():
            report.unlink()
        run([host, '--visualizer', identifier, '--demo', '--self-test', '--exercise', '--close-test', closure,
             '--quit-after', '20000', '--render-width', '192', '--render-height', '128',
             '--profile', folder/'profile', '--output-dir', folder], folder)
        value = json.loads(report.read_text(encoding='utf-8'))
        assert value['result'] == 'pass' and value['visualizer'] == identifier and value['native_plugin_api'] == 4, value
        assert value['frames'] >= 90 and value['closed_by'] == closure and value['restored_bounds'], value
        assert value['owner_pid'] == 0 and not value['recording_started'] and value['input_picker_opens'] == 0, value
        cases.append(dict(case=name, result='pass', observed=value))
        print(name+': pass', flush=True)

    run([ctl, 'scan', '--suite', suite], output/'scan')
    native('geiss', 'prism-flow', 'escape')
    native('terrain', 'terrain', 'rightclick')
    # Scan cache metadata must never override fresh package metadata on click.
    cache_file = suite/'installed-visualizers.json'
    original_cache = cache_file.read_bytes()
    try:
        cached = json.loads(original_cache)
        for entry in cached['visualizers']:
            if entry['id'] == 'terrain':
                entry['adapter'], entry['entrypoint'] = 'webgpu', 'missing-reference.html'
        cache_file.write_text(json.dumps(cached), encoding='utf-8')
        native('terrain', 'fresh-manifest-over-cache', 'menu')
    finally:
        cache_file.write_bytes(original_cache)
    for identifier in ('geiss', 'milkdrop', 'terrain'):
        folder = output/('settings-'+identifier)
        run([host, '--settings', identifier, '--self-test', '--profile', folder/'profile'], folder)
        value = json.loads((folder/'profile'/('native-'+identifier)/'result.json').read_text(encoding='utf-8'))
        assert value['result'] == 'pass' and value['native_settings'] and value['settings_saved'], value
        cases.append(dict(case='native-settings-'+identifier, result='pass', observed=value))
    for identifier in ('geiss', 'milkdrop'):
        folder = output/('original-'+identifier)
        command = [host, '--visualizer', identifier, '--demo', '--self-test', '--quit-after', '3500', '--profile', folder/'profile']
        if identifier == 'geiss':
            command.append('--classic')
        run(command, folder)
        log = (folder/'profile/last-run.log').read_text(encoding='utf-8')
        restored = ('double-click restore/borderless=pass' in log or 'double-click restore=pass; borderless=pass' in log)
        assert 'exit=0' in log and restored, log
        cases.append(dict(case='original-engine-'+identifier, result='pass', generated_pcm=True))
        print('original-engine-'+identifier+': pass', flush=True)
    # A removed file and incompatible pin are rejected after a successful scan.
    manifest_path = suite/'plugins/terrain/manifest.json'
    original_manifest = manifest_path.read_bytes()
    try:
        for name, changes in (('missing-module', dict(entrypoint='missing-native-module')), ('incompatible-pin', dict(dawn_revision='0'*40))):
            changed = json.loads(original_manifest)
            changed.update(changes)
            manifest_path.write_text(json.dumps(changed), encoding='utf-8')
            run([host, '--visualizer', 'terrain', '--demo', '--self-test', '--quit-after', '1000', '--profile', output/name/'profile'], output/name, expect=1)
            cases.append(dict(case=name, result='pass', rejected_before_launch=True))
    finally:
        manifest_path.write_bytes(original_manifest)
        run([ctl, 'scan', '--suite', suite], output/'rescan')
    if args.player_probe:
        folder = output/('player-owner-'+str(time.time_ns()))
        folder.mkdir()
        stop = folder/'stop'
        owner = None
        children, streams = [], []
        try:
            stream = (folder/'owner.log').open('w', encoding='utf-8')
            streams.append(stream)
            owner = subprocess.Popen([str(args.player_probe.resolve()), '--writer', str(folder/'owner.json'), str(stop)], stdout=stream, stderr=stream, env=environment)
            wait_for(folder/'owner.json', [owner])
            for identifier in ('geiss', 'terrain'):
                child_folder = folder/identifier
                child_folder.mkdir()
                stream = (child_folder/'frontend.log').open('w', encoding='utf-8')
                streams.append(stream)
                command = [host, '--visualizer', identifier, '--vlc-pid', str(owner.pid), '--self-test', '--player-test',
                           '--close-test', 'player-exit', '--quit-after', '30000', '--render-width', '192', '--render-height', '128',
                           '--profile', child_folder/'profile', '--output-dir', child_folder]
                children.append(subprocess.Popen([str(p) for p in command], stdout=stream, stderr=stream, env=environment))
            for identifier in ('geiss', 'terrain'):
                ready = wait_for(folder/identifier/'ready.json', children)
                assert ready['owner_pid'] == owner.pid and ready['player_pcm_windows'] > 0 and ready['player_pcm_peak'] > .1, ready
                assert ready['input_picker_opens'] == 0, ready
            assert len(records(suite, owner.pid)) == 2, 'Expected two registered player-owned windows'
            stop.write_text('stop\n', encoding='ascii')
            assert owner.wait(timeout=5) == 0
            for identifier, child in zip(('geiss', 'terrain'), children):
                assert child.wait(timeout=10) == 0, identifier
                value = json.loads((folder/identifier/'result.json').read_text(encoding='utf-8'))
                assert value['result'] == 'pass' and value['closed_by'] == 'player-exit', value
                assert value['owner_pid'] == owner.pid and value['player_pcm_windows'] > 0 and value['player_pcm_peak'] > .1, value
                assert not value['recording_started'] and value['input_picker_opens'] == 0 and value['player_input_requests_blocked'] == 1, value
                cases.append(dict(case='production-player-'+identifier, result='pass', generated_pcm=True, observed=value))
            assert not records(suite, owner.pid), 'Player-owned window control records remain'
            mapping = ipc_file(owner.pid)
            assert mapping is None or not mapping.exists(), 'Generated player IPC file remains'
            print('production player PCM/isolation/owner-exit: pass', flush=True)
        finally:
            close_owned(suite, owner.pid if owner else 0)
            for child in children:
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait(timeout=5)
            stop.write_text('stop\n', encoding='ascii')
            if owner:
                try:
                    owner.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    owner.kill()
                    owner.wait(timeout=5)
            for stream in streams:
                stream.close()
    assert not (suite/'browser').exists(), 'This integration suite must not fall back to Electron'
    value = dict(result='pass', cases=cases, audio_saved=False, real_player_validation=False, release_payload=False)
    (output/'summary.json').write_text(json.dumps(value, indent=2)+'\n', encoding='utf-8')
    print('native production dispatch: pass', flush=True)


if __name__ == '__main__':
    main()
