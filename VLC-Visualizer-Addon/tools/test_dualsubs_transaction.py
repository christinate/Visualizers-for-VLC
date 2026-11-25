"""Exercise the staged Windows add-on on disposable copies of stock VLC files."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def run_tests(payload, stock, work):
    control = payload/'bootstrap/VisualizersCtl.exe'
    manifest = json.loads((payload/'payload.json').read_text())
    components = manifest['components']
    dual = 'dualsubs'
    backend = 'visualizers/qt/bin/libqt_backend.dll'
    core = 'libvlccore.dll'
    base = {row['path']: row['sha256'] for row in manifest['files']}
    component_files = {name: {row['path']: row['sha256'] for row in rows} for name, rows in components.items()}
    result = {}
    sample = ('vlc.exe', core, 'libvlc.dll', 'plugins/gui/libqt_plugin.dll', *(
        path for path in component_files[dual] if path.startswith('plugins/codec/')
    ))
    original = {path: digest(stock/path) for path in sample}
    assert original[core] == manifest[dual]['stock_core_sha256']

    def fixture(root):
        root.mkdir()
        for path in sample:
            file = root/path
            file.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(stock/path, file)
        (root/'owner-keep.txt').write_text('not installer owned')

    def command(action, root, *options, expected=0):
        response = subprocess.run([control, action, '--root', root, *options], capture_output=True, text=True, timeout=45)
        if response.returncode != expected:
            raise AssertionError(f'{action} failed with {response.returncode}: {response.stderr}{response.stdout}')
        return response.stderr

    def install(root, selected):
        command('patch', root, '--payload', payload, '--components', ','.join(selected), '--recover', 'yes')
        command('verify', root)
        expected = dict(base)
        for name in selected:
            expected.update(component_files[name])
        assert digest(root/backend) == expected[backend]
        assert digest(root/core) == expected.get(core, original[core])
        for name, files in component_files.items():
            if name == dual:
                continue
            for path in files:
                assert (root/path).is_file() == (name in selected), (name, path)
        journal = json.loads((root/'.visualizers-backup/transaction.json').read_text())
        assert set(row['path'] for row in journal['files']) == {*expected, 'vlc.visualizers.original.exe'}
        assert (root/'owner-keep.txt').read_text() == 'not installer owned'
        return expected

    def restore(root, installed):
        command('restore', root)
        for path, sha in original.items():
            assert digest(root/path) == sha, path
        for path in installed.keys()-original.keys():
            assert not (root/path).exists(), path
        assert not (root/'vlc.visualizers.original.exe').exists()
        assert (root/'owner-keep.txt').read_text() == 'not installer owned'

    for name in components:
        root = work/name
        fixture(root)
        restore(root, install(root, [name]))
        result[name] = 'installed separately and restored'

    root = work/'all-and-change-selection'
    fixture(root)
    install(root, list(components))
    install(root, ['beach'])
    installed = install(root, ['beach', dual])
    restore(root, installed)
    result['all_and_change_selection'] = 'full suite, remove and add DualSubs, and restore'

    root = work/'modified-file'
    fixture(root)
    installed = install(root, [dual])
    edited = root/'plugins/codec/libsubsdec_plugin.dll'
    with edited.open('ab') as stream:
        stream.write(b'user-modification')
    assert 'was modified' in command('restore', root, expected=1)
    assert digest(root/core) == installed[core]
    assert edited.read_bytes().endswith(b'user-modification')
    result['modified_file'] = 'refused removal and kept current VLC intact'

    root = work/'independent-dualsubs'
    fixture(root)
    marker = root/'DualSubs/install-manifest.json'
    marker.parent.mkdir()
    marker.write_text('{}')
    assert 'standalone DualSubs' in command(
        'patch', root, '--payload', payload, '--components', dual, '--recover', 'yes', expected=1,
    )
    assert digest(root/core) == original[core]
    assert marker.read_text() == '{}'
    result['independent_owner'] = 'left the standalone overlay untouched'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', required=True, type=Path)
    parser.add_argument('--stock-vlc', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    report = args.report.resolve()
    report.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='vlc-transaction-', dir=report.parent) as temp:
        work = Path(temp).resolve(strict=True)
        if work.parent != report.parent:
            raise RuntimeError('Unexpected VLC transaction test directory')
        result = run_tests(args.payload.resolve(), args.stock_vlc.resolve(), work)
    document = {'result': 'pass', 'cases': result}
    report.write_text(json.dumps(document, indent=2)+'\n')
    print(json.dumps(document, indent=2))


if __name__ == '__main__':
    main()
