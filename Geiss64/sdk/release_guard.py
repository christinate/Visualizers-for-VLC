"""Immutable candidate output helpers; never replace a previously built artifact."""
from pathlib import Path
import hashlib
import json
import re


def sha256(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def candidate_version(root):
    root = Path(root)
    version = (root/'VERSION').read_text().strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?', version):
        raise ValueError('Invalid candidate version')
    security = root/'release-status.json'
    if security.exists() and version in json.loads(security.read_text()).get('blocked_versions', {}):
        raise RuntimeError('Candidate is blocked for security review: '+version)
    return version


def fresh_directory(path):
    path = Path(path).resolve()
    # mkdir without exist_ok is intentional, even for an empty old directory.
    path.mkdir(parents=True)
    return path


def checksums(directory):
    directory = Path(directory)
    output = directory/'SHA256SUMS.json'
    records = {p.relative_to(directory).as_posix(): sha256(p)
               for p in sorted(directory.rglob('*')) if p.is_file() and p != output}
    with output.open('x', encoding='utf-8') as stream:
        stream.write(json.dumps(records, indent=2)+'\n')
    return records
