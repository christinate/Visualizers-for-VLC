"""Stage/render/close/remove the real third-party module on a local test suite.

Uses generated audio only, never an installed player. Does not run an installer.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
from validate import manifest, native_schema


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--software', action='store_true')
    args = parser.parse_args()
    suite, package, output = args.suite.resolve(), args.package.resolve(), args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    value = json.loads((package/'manifest.json').read_text(encoding='utf-8'))
    manifest(value, native_schema)
    assert value['id'] == 'audio_color' and package.name == 'audio_color', 'This checker owns only the example ID'
    destination = suite/'plugins/audio_color'
    assert not destination.exists(), 'Do not overwrite an existing package'
    extension = '.exe' if platform.system() == 'Windows' else ''
    ctl, host = suite/('VisualizersCtl'+extension), suite/('Visualizers'+extension)
    environment = os.environ.copy()
    if args.software:
        assert platform.system() == 'Linux'
        environment['VK_DRIVER_FILES'] = '/usr/share/vulkan/icd.d/lvp_icd.json'
    installed = {}
    try:
        shutil.copytree(package, destination)
        if platform.system() == 'Darwin':
            module = destination/value['entrypoint']
            for line in subprocess.check_output(['otool', '-L', str(module)], text=True).splitlines()[1:]:
                dependency = line.strip().split(' (compatibility')[0]
                if 'libwebgpu_dawn' in Path(dependency).name:
                    subprocess.run(['install_name_tool', '-change', dependency, '@loader_path/../../native/'+Path(dependency).name, str(module)], check=True)
            subprocess.run(['codesign', '--force', '--sign', '-', str(module)], check=True, capture_output=True)
        installed = {p.relative_to(destination): hashlib.sha256(p.read_bytes()).hexdigest() for p in destination.rglob('*') if p.is_file()}
        scan = subprocess.run([str(ctl), 'scan', '--suite', str(suite)], check=True, capture_output=True, text=True)
        registry = json.loads(scan.stdout)
        assert any(e['id'] == 'audio_color' and e['api_version'] == 4 for e in registry['visualizers']), registry
        subprocess.run([str(ctl), 'check', '--suite', str(suite), '--package', str(destination)], check=True, capture_output=True)
        report = output/'result.json'
        if report.exists():
            report.unlink()
        command = [host, '--visualizer', 'audio_color', '--demo', '--self-test', '--exercise', '--close-test', 'menu', '--quit-after', '20000',
                   '--render-width', '192', '--render-height', '128', '--output-dir', output, '--profile', output/'profile']
        result = subprocess.run([str(p) for p in command], capture_output=True, text=True, encoding='utf-8', errors='replace', env=environment, timeout=35)
        (output/'frontend.stdout.log').write_text(result.stdout, encoding='utf-8')
        (output/'frontend.stderr.log').write_text(result.stderr, encoding='utf-8')
        observed = json.loads(report.read_text(encoding='utf-8'))
        assert result.returncode == 0 and observed['result'] == 'pass' and observed['visualizer'] == 'audio_color', observed
        assert observed['native_plugin_api'] == 4 and observed['frames'] >= 90 and observed['closed_by'] == 'menu' and observed['restored_bounds'], observed
        assert not observed['recording_started'] and observed['input_picker_opens'] == 0, observed
    finally:
        # Delete only files this checker copied, after exact byte verification.
        # Leave anything changed/new in place for diagnosis instead of rmtree.
        for relative, digest in installed.items():
            target = destination/relative
            assert target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == digest, 'Modified example file preserved: '+str(target)
        if installed:
            for relative in installed:
                (destination/relative).unlink()
            for folder in sorted((p for p in destination.rglob('*') if p.is_dir()), key=lambda p: len(p.parts), reverse=True):
                folder.rmdir()
            destination.rmdir()
        subprocess.run([str(ctl), 'scan', '--suite', str(suite)], check=True, capture_output=True)
    registry = json.loads((suite/'installed-visualizers.json').read_text(encoding='utf-8'))
    assert all(e['id'] != 'audio_color' for e in registry['visualizers'])
    (output/'summary.json').write_text(json.dumps(dict(result='pass', observed=observed, removed_and_rescanned=True,
                                                     real_player_validation=False, installer_executed=False), indent=2)+'\n', encoding='utf-8')
    print('Third-party native example: scan/load/render/resize/fullscreen/menu-close/remove/rescan pass', flush=True)


if __name__ == '__main__':
    main()
