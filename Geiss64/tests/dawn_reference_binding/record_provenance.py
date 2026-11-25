"""Record an owner-local development binding; never part of an installed payload."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--dawn-source', type=Path, required=True)
    parser.add_argument('--dawn-install', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    pin = json.loads((Path(__file__).resolve().parents[2] /
                      'sdk/dawn/dependencies.lock.json').read_text(encoding='utf-8'))
    sdk_file = args.dawn_install / 'build-provenance.json'
    sdk = json.loads(sdk_file.read_text(encoding='utf-8'))
    revision = subprocess.check_output(
        ['git', '-C', str(args.dawn_source), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != pin['source_revision'] or sdk['source_revision'] != revision:
        raise RuntimeError('Source, SDK and project Dawn pins must agree')
    subprocess.run(['git', '-C', str(args.dawn_source), 'diff', '--exit-code',
                    'HEAD', '--', '.'], check=True, stdout=subprocess.DEVNULL)
    if args.module.suffix != '.node' or not args.module.is_file():
        raise RuntimeError('Expected a built Dawn Node binding')
    libraries = [p for folder in ('bin', 'lib', 'lib64')
                 for p in (args.dawn_install / folder).glob('*webgpu_dawn*')
                 if p.is_file() and p.suffix != '.lib']
    if not libraries:
        raise RuntimeError('Installed shared Dawn library is missing')
    expected = {item['path']: item['sha256'] for item in sdk['artifacts']}
    for library in libraries:
        relative = library.relative_to(args.dawn_install).as_posix()
        if expected.get(relative) != sha(library):
            raise RuntimeError('Shared Dawn library differs from SDK provenance: ' + relative)
    result = {
        'scope': 'Development-only original-JavaScript oracle; excluded from production payloads',
        'dawn_revision': revision,
        'module_file': str(args.module.resolve()),
        'module_sha256': sha(args.module),
        'sdk_provenance_sha256': sha(sdk_file),
        'shared_dawn': [{'file': str(p.resolve()), 'sha256': sha(p)} for p in libraries],
    }
    with args.output.open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'result': 'pass', 'module_sha256': result['module_sha256']}))


if __name__ == '__main__':
    main()
