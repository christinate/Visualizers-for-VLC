"""Archive a clean, fixed source commit without replacing earlier release evidence."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NAME = 'VLC-Visualizer-Addon'


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def main():
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, help='Fresh destination; default releases/VERSION/source')
    args = parser.parse_args()
    commit = git('rev-parse', 'HEAD').decode().strip()
    if git('status', '--porcelain', '--untracked-files=normal').strip():
        raise SystemExit('Commit the release source before archiving it')
    version = git('show', commit+':VERSION').decode('utf-8').strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?', version):
        raise SystemExit('Invalid candidate version in the source commit')
    if (ROOT/'release-status.json').is_file():
        policy = json.loads(git('show', commit+':release-status.json').decode('utf-8'))
        if version in policy.get('blocked_versions', {}):
            raise SystemExit('Candidate is blocked for security review: '+version)
    output = (args.output or ROOT/'releases'/version/'source').resolve()
    # This intentionally refuses an existing directory, including an empty one.
    output.mkdir(parents=True)
    prefix = NAME+'-'+version+'/'
    archive = output/(NAME+'-'+version+'-source.zip')
    with archive.open('xb') as stream:
        subprocess.run(['git', '-c', 'core.autocrlf=false', '-c', 'core.eol=lf',
                        '-C', str(ROOT), 'archive', '--format=zip',
                        '--prefix='+prefix, commit], stdout=stream, check=True)
    with zipfile.ZipFile(archive) as bundle:
        if bundle.read(prefix+'VERSION').decode('utf-8').strip() != version:
            raise RuntimeError('Archived VERSION does not match source metadata')
    with archive.open('rb') as stream:
        digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    metadata = {'schema_version': 1, 'project': NAME, 'version': version,
                'source_commit': commit, 'archive': archive.name,
                'archive_sha256': digest, 'archive_bytes': archive.stat().st_size,
                'status': 'local-candidate-release-validation-pending'}
    release = output/'RELEASE.json'
    with release.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(metadata, indent=2)+'\n')
    with release.open('rb') as stream:
        metadata_digest = hashlib.file_digest(stream, 'sha256').hexdigest()
    with (output/'SHA256SUMS.json').open('x', encoding='utf-8') as stream:
        stream.write(json.dumps({archive.name: digest, release.name: metadata_digest}, indent=2)+'\n')
    print('Archived source commit '+commit+' with candidate-only checksums: '+str(archive))


if __name__ == '__main__':
    main()
