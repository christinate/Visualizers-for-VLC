"""Verify recorded release artifacts and Git tags."""
import argparse
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'sdk'))
from release_guard import sha256

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--allow-pruned-archives', action='store_true',
                    help='Permit missing superseded versioned archives and installers')
args = parser.parse_args()
baseline = json.loads((ROOT/'dawn/PRESERVED-RELEASES.json').read_text())
count = 0
pruned = 0
for name, expected in baseline['projects'].items():
    project = ROOT.parent/name
    tags = subprocess.check_output(['git', '-C', str(project), 'show-ref', '--tags'], text=True).splitlines()
    for tag in expected['tags']:
        if tag not in tags:
            raise RuntimeError('Preserved Git tag changed: '+name+' '+tag)
    for relative, record in expected['artifacts'].items():
        path = project/relative
        if (args.allow_pruned_archives and not path.exists() and path.parent == project/'releases'
                and any(f'-0.{minor}.0-rc.1' in path.name for minor in (2, 3, 4))
                and path.name.endswith(('.exe', '.zip', '.tar.gz'))):
            pruned += 1
            continue
        if not path.is_file() or path.stat().st_size != record['bytes'] or sha256(path) != record['sha256']:
            raise RuntimeError('Preserved release artifact changed: '+str(path))
        count += 1
print('Preserved '+str(count)+' artifacts/metadata/checksum files and all recorded tags across five repositories')
if pruned:
    print(f'{pruned} superseded local archives and installers were pruned')
