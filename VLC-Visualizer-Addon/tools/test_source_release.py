"""Exercise each actual source helper with isolated local Git repositories."""
from pathlib import Path
import argparse
import contextlib
import hashlib
import io
import json
import runpy
import shutil
import subprocess
import sys
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PROJECTS = ('VLC-Visualizer-Addon', 'Geiss64', 'MilkDrop2-64', 'Terrain64', 'VLC-Visualizer-Catalog')


def git(repo, *args, runner=None):
    return (runner or subprocess.run)(['git', '-C', str(repo), *args],
                                     check=True, capture_output=True).stdout


def commit(repo, runner=None):
    git(repo, 'add', '--all', runner=runner)
    git(repo, '-c', 'user.name=Source Archive Validation',
        '-c', 'user.email=source-validation@vlc-visualizers.invalid',
        '-c', 'commit.gpgsign=false', 'commit', '-m', 'Local fixture', runner=runner)
    return git(repo, 'rev-parse', 'HEAD', runner=runner).decode().strip()


def verify(folder, expected_commit, expected_source=b'original source\n'):
    metadata = json.loads((folder/'RELEASE.json').read_text(encoding='utf-8'))
    assert metadata['source_commit'] == expected_commit
    archive = folder/metadata['archive']
    assert hashlib.sha256(archive.read_bytes()).hexdigest() == metadata['archive_sha256']
    assert archive.stat().st_size == metadata['archive_bytes']
    for name, digest in json.loads((folder/'SHA256SUMS.json').read_text()).items():
        assert hashlib.sha256((folder/name).read_bytes()).hexdigest() == digest
    prefix = metadata['project']+'-'+metadata['version']+'/'
    with zipfile.ZipFile(archive) as bundle:
        assert bundle.comment.decode('ascii') == expected_commit
        assert bundle.read(prefix+'example.txt') == expected_source
        assert bundle.read(prefix+'VERSION').decode().strip() == metadata['version']
        assert bundle.read(prefix+'README.md')
        assert all('.git/' not in name and '/releases/' not in name for name in bundle.namelist())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    records = []
    for project in PROJECTS:
        repo = output/project
        (repo/'tools').mkdir(parents=True)
        helper = repo/'tools/source_release.py'
        shutil.copy2(ROOT.parent/project/'tools/source_release.py', helper)
        (repo/'VERSION').write_text('0.5.0-rc.1\n')
        (repo/'README.md').write_text('Isolated owner-local source archive fixture\n')
        (repo/'example.txt').write_bytes(b'original source\n')
        (repo/'.gitignore').write_text('/releases/\n/build/\n__pycache__/\n')
        git(repo, 'init')
        git(repo, 'config', 'core.autocrlf', 'true')
        git(repo, 'config', 'core.hooksPath', str(repo/'.no-hooks'))
        snapshot = commit(repo)
        releases = repo/'releases'
        releases.mkdir()
        for name in ('RELEASE.json', 'SHA256SUMS.json', 'prior-0.4.0.zip'):
            (releases/name).write_bytes(('preserved '+name).encode())
        preserved = {p.name: p.read_bytes() for p in releases.iterdir() if p.is_file()}

        def run(case, destination=None, success=True):
            command = [sys.executable, str(helper)]
            if destination:
                command.extend(['--output', str(destination)])
            result = subprocess.run(command, capture_output=True, timeout=30)
            assert (result.returncode == 0) == success, (project, case, result.stderr)
            for name, data in preserved.items():
                assert (releases/name).read_bytes() == data, (project, case, name)
            records.append({'project': project, 'case': case, 'result': 'pass'})
            return result

        default = releases/'0.5.0-rc.1/source'
        run('default archive preserves all earlier release bytes')
        verify(default, snapshot)
        original_output = {p.name: p.read_bytes() for p in default.iterdir()}
        run('existing output refused', success=False)
        assert original_output == {p.name: p.read_bytes() for p in default.iterdir()}
        custom = repo/'build/custom output 音声'
        run('custom Unicode/spaced output', custom)
        verify(custom, snapshot)
        empty = repo/'build/empty-existing'
        empty.mkdir()
        run('empty existing directory refused', empty, False)
        assert not list(empty.iterdir())
        dirty = repo/'build/dirty'
        (repo/'example.txt').write_text('uncommitted change\n')
        run('dirty tracked source refused', dirty, False)
        assert not dirty.exists()
        git(repo, 'checkout', '--', 'example.txt')
        (repo/'untracked.txt').write_text('untracked\n')
        run('untracked source refused', dirty, False)
        assert not dirty.exists()
        (repo/'untracked.txt').unlink()
        policy = repo/'release-status.json'
        policy.write_text(json.dumps({'blocked_versions': {'0.5.0-rc.1': {}}}))
        commit(repo)
        run('security-held candidate refused', repo/'build/blocked', False)
        assert not (repo/'build/blocked').exists()
        policy.write_text('{}\n')
        (repo/'VERSION').write_text('../invalid\n')
        commit(repo)
        run('invalid committed VERSION refused', repo/'build/invalid', False)
        assert not (repo/'build/invalid').exists()
        (repo/'VERSION').write_text('0.5.0-rc.1\n')
        snapshot = commit(repo)

        # Move actual HEAD at the archive subprocess boundary. The real Git
        # archive must still contain the earlier fixed commit and its VERSION.
        original_run = subprocess.run
        moved = False

        def move_head(command, *positional, **keywords):
            nonlocal moved
            if command[0] == 'git' and 'archive' in command:
                assert not moved and command[-1] == snapshot
                (repo/'example.txt').write_text('later commit\n')
                (repo/'VERSION').write_text('0.6.0-rc.1\n')
                commit(repo, runner=original_run)
                moved = True
            return original_run(command, *positional, **keywords)

        race_output = repo/'build/head-moved'
        saved_argv = sys.argv
        try:
            sys.argv = [str(helper), '--output', str(race_output)]
            with patch('subprocess.run', side_effect=move_head), contextlib.redirect_stdout(io.StringIO()):
                runpy.run_path(str(helper), run_name='__main__')
        finally:
            sys.argv = saved_argv
        assert moved and git(repo, 'rev-parse', 'HEAD').decode().strip() != snapshot
        verify(race_output, snapshot)
        for name, data in preserved.items():
            assert (releases/name).read_bytes() == data
        records.append({'project': project, 'case': 'concurrent HEAD/VERSION change retains fixed source commit', 'result': 'pass'})
        print(project+': 9 source archive cases pass', flush=True)
    (output/'result.json').write_text(json.dumps({'result': 'pass', 'cases': records,
        'scope': 'Actual Git/archive helpers in isolated local repositories; no actual project releases written'}, indent=2)+'\n')


if __name__ == '__main__':
    main()
