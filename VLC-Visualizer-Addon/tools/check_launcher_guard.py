"""Read-only launcher verification against isolated valid/corrupt backups.

Never executes the backed-up program or patches an installed VLC. Every case
uses --verify-original; input files are linked or copied without modification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--launcher', type=Path, required=True)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--core', type=Path, required=True)
    parser.add_argument('--old-launcher', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sources = [args.launcher.resolve(), args.original.resolve(), args.core.resolve(), args.old_launcher.resolve()]
    hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    output = args.output.resolve()
    output.mkdir()
    rows = []
    def link_source(source, destination):
        try:
            os.link(source, destination)
        except OSError:
            # Protected installed/system files may be readable but unlinkable.
            shutil.copy2(source, destination)
    cases = ['valid', 'unicode-spaced-path', 'old-wrapper-backup', 'current-wrapper-backup',
             'same-file-backup', 'missing-backup', 'empty-backup', 'invalid-backup',
             'dll-as-backup', 'missing-core', 'mismatched-core', 'mixed-scan-verify']
    for name in cases:
        folder = output/(name+' \u65e5\u672c\u8a9e' if name == 'unicode-spaced-path' else name)
        folder.mkdir()
        launcher, original, core = folder/'vlc.exe', folder/'vlc.visualizers.original.exe', folder/'libvlccore.dll'
        shutil.copy2(sources[0], launcher)
        if name == 'old-wrapper-backup':
            link_source(sources[3], original)
        elif name == 'current-wrapper-backup':
            shutil.copy2(launcher, original)
        elif name == 'same-file-backup':
            os.link(launcher, original)
        elif name == 'empty-backup':
            original.write_bytes(b'')
        elif name == 'invalid-backup':
            original.write_bytes(b'not an executable')
        elif name == 'dll-as-backup':
            link_source(sources[2], original)
        elif name != 'missing-backup':
            link_source(sources[1], original)
        if name == 'mismatched-core':
            link_source(Path(os.environ['SystemRoot'])/'System32/version.dll', core)
        elif name != 'missing-core':
            link_source(sources[2], core)
        command = [str(launcher), '--verify-original']
        if name == 'mixed-scan-verify':
            command.append('--scan-visualizers')
        p = subprocess.run(command, capture_output=True, timeout=5)
        expected = 0 if name in ['valid', 'unicode-spaced-path'] else 1
        assert p.returncode == expected, (name, p.returncode, p.stderr)
        assert not (folder/'visualizers').exists(), 'Verification created a scan directory'
        row = {'case': name, 'exit': p.returncode,
               'stdout': p.stdout.decode('utf-8', 'replace'), 'stderr': p.stderr.decode('utf-8', 'replace')}
        rows.append(row)
        print(name+': pass', flush=True)
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == digest for p, digest in hashes.items())
    report = {'result': 'pass', 'cases': rows, 'input_sha256': hashes,
              'scope': 'Actual launcher --verify-original path; no GUI, backed-up program execution, install or security verdict'}
    (output/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'result': 'pass', 'cases': len(rows)}))


if __name__ == '__main__':
    main()
