"""Exercise actual scanner, frontend and native-runtime package revalidation.

Windows/Linux/macOS. Reads an expendable native suite; every mutation is made
in a fresh private copy. Generated audio only; no real player or microphone.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import errno
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import struct
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-suite', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--software', action='store_true')
    args = parser.parse_args()
    system = platform.system()
    if system not in ('Windows', 'Linux', 'Darwin'):
        raise RuntimeError('Unsupported native test platform')
    source, output = args.source_suite.resolve(), args.output_dir.resolve()
    if output.is_relative_to(source) or source.is_relative_to(output):
        raise RuntimeError('Keep source and fresh output directories separate')
    output.mkdir()  # A previous run, even an empty one, cannot supply evidence.
    suite = output/'suite'
    suite.mkdir()
    suffix = '.exe' if system == 'Windows' else ''
    for name in ('Visualizers', 'VisualizersCtl'):
        shutil.copy2(source/(name+suffix), suite)
    for name in ('native', 'lib'):
        if (source/name).is_dir():
            shutil.copytree(source/name, suite/name, symlinks=True)
    package = suite/'plugins/terrain'
    shutil.copytree(source/'plugins/terrain', package, symlinks=True)
    manifest_file = package/'manifest.json'
    original = manifest_file.read_bytes()
    manifest = json.loads(original)
    if manifest['adapter'] != 'dawn' or manifest['id'] != 'terrain':
        raise RuntimeError('Provide a native Terrain package')
    module = package/manifest['entrypoint']
    assets = package/manifest['assets']
    if not all(p.resolve().is_relative_to(package.resolve()) for p in (module, assets)):
        raise RuntimeError('Source module/assets must stay inside the copied package')
    module_bytes = module.read_bytes()
    ctl, host = suite/('VisualizersCtl'+suffix), suite/('Visualizers'+suffix)
    runtime = (suite/'native/VisualizerNative.app/Contents/MacOS/VisualizerNative'
               if system == 'Darwin' else suite/'native'/('VisualizerNative'+suffix))
    env = os.environ.copy()
    if args.software:
        if system != 'Linux':
            raise RuntimeError('--software uses installed Linux lavapipe')
        env.pop('VK_ICD_FILENAMES', None)
        env['VK_DRIVER_FILES'] = '/usr/share/vulkan/icd.d/lvp_icd.json'
    rows = []
    report = {'result': 'running', 'system': system, 'cases': rows,
              'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'scope': 'Actual scan/check commands and Open/Settings launch paths; no real VLC QAction, player audio or microphone claim',
              'binary_sha256': {str(p.relative_to(suite)): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in [ctl, host, runtime, module]}}

    def run(command, folder, valid=True):
        folder.mkdir(parents=True)
        p = subprocess.run([str(a) for a in command], env=env, capture_output=True,
                           timeout=30)
        p.stdout = p.stdout.decode('utf-8')  # Invalid scanner output is a failure.
        p.stderr = p.stderr.decode('utf-8', errors='replace')
        (folder/'stdout.log').write_text(p.stdout, encoding='utf-8')
        (folder/'stderr.log').write_text(p.stderr, encoding='utf-8')
        (folder/'exit.json').write_text(json.dumps({'exit': p.returncode})+'\n')
        if valid:
            assert p.returncode == 0, (command, p.returncode, p.stderr[-2000:])
        else:
            # A crash/signal/timeout must never count as correct rejection.
            assert p.returncode in (1, 10), (command, p.returncode, p.stderr[-2000:])
            assert not list(folder.rglob('result.json')), 'Rejected input reached the window/renderer'
        return p

    def scan(folder, count):
        p = run([ctl, 'scan', '--suite', suite], folder)
        value = json.loads(p.stdout)
        assert len(value['visualizers']) == count, value
        assert json.loads((suite/'installed-visualizers.json').read_text(encoding='utf-8')) == value
        return value

    def launches(folder, valid=False):
        for name, binary in [('frontend', host), ('runtime', runtime)]:
            for action in (['--visualizer'] if valid else ['--visualizer', '--settings']):
                target = folder/(name+('-open' if action == '--visualizer' else '-settings'))
                command = [binary, action, 'terrain', '--demo', '--self-test',
                           '--quit-after', '20000', '--render-width', '96', '--render-height', '64',
                           '--profile', target/'profile', '--output-dir', target]
                if name == 'runtime':
                    command += ['--suite', suite]
                run(command, target, valid)
                if valid:
                    result = json.loads((target/'result.json').read_text(encoding='utf-8'))
                    assert result['result'] == 'pass' and result['frames'] >= 90 and result['closed_by'] == 'escape'
                    assert result['visualizer'] == 'terrain' and result['native_plugin_api'] == 4
                    assert not result['recording_started'] and result['input_picker_opens'] == 0

    def rejected_case(name, mutate, restore):
        folder = output/name
        (suite/'installed-visualizers.json').write_bytes(good_cache)
        mutate()
        try:
            run([ctl, 'check', '--suite', suite, '--package', package], folder/'menu-check', False)
            launches(folder)
            scan(folder/'scan', 0)
            rows.append({'case': name, 'result': 'pass', 'launch_rejections': 4})
            print(name+': pass', flush=True)
        finally:
            restore()

    try:
        value = scan(output/'initial-scan', 1)
        assert not value['rejected'] and value['visualizers'][0]['id'] == 'terrain'
        good_cache = (suite/'installed-visualizers.json').read_bytes()
        run([ctl, 'check', '--suite', suite, '--package', package], output/'valid-menu-check')
        launches(output/'valid-launches', True)
        rows.append({'case': 'valid-package', 'result': 'pass', 'rendering_launches': 2})

        changes = [
            ('schema', {'schema_version': 99}), ('api', {'api_version': 99}),
            ('pin', {'dawn_revision': '0'*40}), ('platform', {'platform': 'other'}),
            ('architecture', {'architecture': 'x86'}), ('adapter', {'adapter': 'unknown'}),
            ('id-mismatch', {'id': 'different'}), ('id-type', {'id': 42}),
            ('name-empty', {'name': ''}), ('name-too-long', {'name': 'x'*129}),
            ('version-type', {'version': 42}), ('entrypoint-empty', {'entrypoint': ''}),
            ('entrypoint-type', {'entrypoint': None}),
            ('entrypoint-nul', {'entrypoint': manifest['entrypoint']+'\0invalid'}),
            ('entrypoint-traversal', {'entrypoint': '../'+manifest['entrypoint']}),
            ('entrypoint-absolute', {'entrypoint': str(module.resolve())}),
            ('assets-nul', {'assets': manifest['assets']+'\0invalid'}),
            ('assets-traversal', {'assets': '../assets'}),
            ('assets-absolute', {'assets': str(assets.resolve())}),
            ('assets-missing', {'assets': 'absent-assets'}),
        ]
        for name, fields in changes:
            rejected_case(name, lambda f=fields: manifest_file.write_text(json.dumps(dict(manifest, **f)), encoding='utf-8'),
                          lambda: manifest_file.write_bytes(original))
        for name, content in [('json-malformed', b'{'), ('json-array', b'[]'),
                              ('json-null', b'null'), ('json-oversize', b' '*1048577),
                              ('json-invalid-utf8', b'{"name":"\xff"}')]:
            rejected_case(name, lambda c=content: manifest_file.write_bytes(c),
                          lambda: manifest_file.write_bytes(original))
        wrong = bytearray(module_bytes)
        if system == 'Windows':
            pe = struct.unpack_from('<I', wrong, 0x3c)[0]
            struct.pack_into('<H', wrong, pe+4, 0x14c)
        elif system == 'Darwin':
            struct.pack_into('<I', wrong, 4, 12)  # 32-bit ARM, not arm64
        else:
            wrong[4] = 1
        for name, content in [('module-empty', b''), ('module-wrong-architecture', wrong)]:
            rejected_case(name, lambda c=content: module.write_bytes(c), lambda: module.write_bytes(module_bytes))
        for name, path in [('manifest-removed', manifest_file), ('module-removed', module),
                           ('assets-removed', assets), ('package-removed', package)]:
            saved = path.with_name(path.name+'.saved')
            if path == package:
                saved = suite/'removed-terrain'
            rejected_case(name, lambda p=path, s=saved: p.rename(s), lambda p=path, s=saved: s.rename(p))

        # Resolve real directory links, including Windows junctions, rather
        # than relying only on textual traversal checks.
        for name, link in [('package-link-escape', package), ('assets-link-escape', assets)]:
            external = output/(name+'-outside')/link.name
            external.parent.mkdir()
            def make_link():
                link.rename(external)
                if system == 'Windows':
                    variables = dict(env, VLC_TEST_LINK=str(link), VLC_TEST_TARGET=str(external))
                    subprocess.run(['powershell', '-NoProfile', '-NonInteractive', '-Command',
                                    'New-Item -ItemType Junction -Path $env:VLC_TEST_LINK -Value $env:VLC_TEST_TARGET | Out-Null'],
                                   env=variables, check=True, capture_output=True)
                else:
                    link.symlink_to(external, target_is_directory=True)
            def restore_link():
                if link.is_symlink():
                    link.unlink()
                elif link.exists() and system == 'Windows':
                    assert os.lstat(link).st_reparse_tag == 0xA0000003, 'Expected the test junction'
                    os.rmdir(link)  # Remove this junction itself; never its target.
                external.rename(link)
            try:
                rejected_case(name, make_link, restore_link)
            finally:
                if external.exists():
                    restore_link()

        # One corrupt manifest or non-Unicode filename must not hide a valid
        # package or prevent the persisted catalog from being valid UTF-8 JSON.
        bad_names = [('valid-with-malformed-json', 'broken-json', b'{'),
                     ('valid-with-invalid-utf8', 'broken-utf8', b'{"name":"\xff"}'),
                     ('valid-with-invalid-filename',
                      'bad-'+chr(0xd800) if system == 'Windows' else os.fsdecode(b'bad-\xff'), original)]
        for name, directory, content in bad_names:
            bad = suite/'plugins'/directory
            try:
                bad.mkdir()
            except OSError as error:
                if system == 'Darwin' and name == 'valid-with-invalid-filename' and error.errno == errno.EILSEQ:
                    rows.append({'case': name, 'result': 'unsupported',
                                 'reason': 'The Mac filesystem rejects this invalid UTF-8 filename before the scanner runs',
                                 'errno': error.errno})
                    continue
                raise
            (bad/'manifest.json').write_bytes(content)
            try:
                value = scan(output/name, 1)
                assert len(value['rejected']) == 1 and value['visualizers'][0]['id'] == 'terrain'
                rows.append({'case': name, 'result': 'pass', 'valid_package_retained': True})
            finally:
                (bad/'manifest.json').unlink()
                bad.rmdir()

        # A stale cache cannot redirect a valid package to other renderer data.
        stale = json.loads(good_cache)
        stale['visualizers'][0].update(adapter='webgpu', entrypoint='absent.html', assets='absent', dawn_revision='0'*40)
        (suite/'installed-visualizers.json').write_text(json.dumps(stale), encoding='utf-8')
        launches(output/'fresh-manifest-over-cache', True)
        rows.append({'case': 'fresh-manifest-over-cache', 'result': 'pass', 'rendering_launches': 2})

        for name, content in [('cache-malformed', b'{'), ('cache-array-type', b'{"schema_version":1,"visualizers":42}'),
                              ('cache-null-entry', b'{"schema_version":1,"visualizers":[null]}'),
                              ('cache-oversize', b' '*1048577)]:
            (suite/'installed-visualizers.json').write_bytes(content)
            launches(output/name)
            rows.append({'case': name, 'result': 'pass', 'launch_rejections': 4})
        for name, text in [('cache-package-nul', 'plugins/terrain\0invalid'),
                           ('cache-package-traversal', '../terrain'),
                           ('cache-package-absolute', str(package.resolve()))]:
            bad = json.loads(good_cache); bad['visualizers'][0]['package'] = text
            (suite/'installed-visualizers.json').write_text(json.dumps(bad), encoding='utf-8')
            launches(output/name)
            rows.append({'case': name, 'result': 'pass', 'launch_rejections': 4})

        scan(output/'restored-scan', 1)
        def concurrent(n):
            return scan(output/('concurrent-scan-'+str(n)), 1)
        with ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(concurrent, range(4)))
        assert all(value == results[0] for value in results)
        rows.append({'case': 'four-concurrent-persistent-scans', 'result': 'pass'})
        report['result'] = 'pass'
        report['case_count'] = len(rows)
        report['passed_cases'] = sum(row['result'] == 'pass' for row in rows)
        report['unsupported_cases'] = sum(row['result'] == 'unsupported' for row in rows)
    except Exception as error:
        report['result'] = 'fail'
        report['error'] = repr(error)
        raise
    finally:
        (output/'summary.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'result': report['result'], 'cases': len(rows)}))


if __name__ == '__main__':
    main()
