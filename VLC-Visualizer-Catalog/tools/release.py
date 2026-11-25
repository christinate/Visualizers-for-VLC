"""Package and validate one clean catalog commit without replacing prior output."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import stat
import subprocess
import sys
import zipfile

ROOT=Path(__file__).resolve().parents[1]
CONTENTS=('README.md','WEBGPU.md','DAWN.md','VALIDATION-DAWN.md','VALIDATION-RELEASE.md','RELEASES.md',
          'CHANGELOG.md','visualizers.json','VERSION','LICENSE','schemas','examples','tools')


def git(*arguments):
    return subprocess.check_output(['git','-C',str(ROOT),*arguments])


def sha(path):
    with path.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    if hasattr(sys.stdout,'reconfigure'):sys.stdout.reconfigure(encoding='utf-8')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,help='Fresh catalog artifact directory')
    parser.add_argument('--stage',type=Path,help='Fresh extracted validation directory')
    args=parser.parse_args()
    commit=git('rev-parse','HEAD').decode().strip()
    if git('status','--porcelain','--untracked-files=normal').strip():
        raise SystemExit('Commit the release source before packaging it')
    version=git('show',commit+':VERSION').decode('utf-8').strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?',version):
        raise SystemExit('Invalid candidate version in the source commit')
    output=(args.output or ROOT/'releases'/version/'catalog').resolve()
    staging=(args.stage or ROOT/'build'/('release-'+version)).resolve()
    if output.is_relative_to(staging) or staging.is_relative_to(output):
        raise SystemExit('Use separate fresh output and validation directories')
    if output.exists() or staging.exists():
        raise SystemExit('Prior output/staging must not be overwritten; use fresh paths')
    staging.mkdir(parents=True)
    output.mkdir(parents=True)
    archive=output/('VLC-Visualizer-Catalog-'+version+'.zip')
    # Read only the captured commit. Git's ignored/untracked worktree files,
    # platform line-ending settings and a concurrent HEAD move cannot change it.
    with archive.open('xb') as stream:
        subprocess.run(['git','-c','core.autocrlf=false','-c','core.eol=lf','-C',str(ROOT),
                        'archive','--format=zip',commit,*CONTENTS],stdout=stream,check=True)
    with zipfile.ZipFile(archive) as bundle:
        if bundle.read('VERSION').decode('utf-8').strip()!=version:
            raise RuntimeError('Archived version differs from the source commit')
        for item in bundle.infolist():
            target=staging/item.filename
            if ('\\' in item.filename or not target.resolve().is_relative_to(staging)
                    or stat.S_ISLNK(item.external_attr>>16)):
                raise RuntimeError('Unsupported catalog archive path/link: '+item.filename)
        bundle.extractall(staging)
        entries=len(bundle.infolist())
    # Validate the files and validator that recipients receive, not the live
    # worktree. Keep failed snapshots/logs but do not stamp them successful.
    validation=output/'validation.log'
    with validation.open('x',encoding='utf-8') as stream:
        result=subprocess.run([sys.executable,str(staging/'tools/validate.py')],
                              stdout=stream,stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError('Archived catalog validation failed; inspect '+str(validation))
    metadata=dict(schema_version=1,project='VLC-Visualizer-Catalog',version=version,
                  source_commit=commit,archive=archive.name,archive_sha256=sha(archive),
                  archive_bytes=archive.stat().st_size,archive_entries=entries,
                  validation='tools/validate.py from this archive: pass',
                  status='local-candidate-release-validation-pending')
    release=output/'RELEASE.json'
    with release.open('x',encoding='utf-8') as stream:stream.write(json.dumps(metadata,indent=2)+'\n')
    with (output/'SHA256SUMS.json').open('x',encoding='utf-8') as stream:
        stream.write(json.dumps({p.name:sha(p) for p in (archive,release,validation)},indent=2)+'\n')
    print('Catalog commit '+commit+' prepared and validated: '+str(archive))


if __name__=='__main__':main()
