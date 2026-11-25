"""Build/record hash-pinned SDL2 for native arm64 Mac release dependencies."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import re
import subprocess
import tarfile
import urllib.request

ROOT=Path(__file__).resolve().parents[1]


def sha(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source-archive','source','build','install'):
        parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--cmake',default='cmake')
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--macos-deployment-target',default='11.0')
    parser.add_argument('--record-existing',action='store_true',help='Verify canonical source, existing CMake configuration and installed output; do not rebuild')
    args=parser.parse_args()
    if platform.system()!='Darwin' or platform.machine()!='arm64':parser.error('Requires arm64 macOS')
    if not 1<=args.jobs<=16 or not re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}',args.macos_deployment_target):parser.error('Invalid jobs/deployment target')
    if tuple(map(int,args.macos_deployment_target.split('.')))<(11,):parser.error('Apple Silicon requires macOS 11+')
    pin=json.loads((ROOT/'dawn/dependencies.lock.json').read_text(encoding='utf-8'))['SDL2']
    archive,source,build,install=map(Path.resolve,(args.source_archive,args.source,args.build,args.install))
    provenance=install/'share/vlc-visualizers/SDL2-build-provenance.json'
    if provenance.exists():raise FileExistsError('Existing SDK provenance is immutable; use fresh paths')
    if not args.record_existing and (build.exists() or install.exists()):raise FileExistsError('Use fresh build/install directories')
    if not archive.exists():
        if args.record_existing:raise FileNotFoundError(archive)
        archive.parent.mkdir(parents=True,exist_ok=True)
        with urllib.request.urlopen(pin['source_url']) as response,archive.open('xb') as target:
            while block:=response.read(1024*1024):target.write(block)
    if sha(archive)!=pin['source_sha256']:raise RuntimeError('SDL source archive differs from its pinned SHA-256')
    expected={}
    with tarfile.open(archive) as stream:
        for member in stream.getmembers():
            path=Path(member.name)
            if path.is_absolute() or '..' in path.parts or not (member.isfile() or member.isdir() or member.issym()):raise RuntimeError('Unexpected SDL source archive member')
            relative=Path(*path.parts[1:])
            if member.isfile():
                expected[relative.as_posix()]=('file',hashlib.sha256(stream.extractfile(member).read()).hexdigest())
            elif member.issym():
                target=Path(member.linkname)
                if target.is_absolute() or not (source/relative.parent/target).resolve().is_relative_to(source):raise RuntimeError('SDL source symlink escapes its tree')
                expected[relative.as_posix()]=('symlink',member.linkname)
        if not source.exists():
            if args.record_existing:raise FileNotFoundError(source)
            source.mkdir(parents=True)
            # Publish regular entries before links, preventing archive links
            # from becoming parents for writes. Preserve contained canonical
            # Android source aliases even though the Mac build does not use them.
            for member in stream.getmembers():
                relative=Path(*Path(member.name).parts[1:])
                if not relative.parts:continue
                target=source/relative
                if member.isdir():target.mkdir(parents=True,exist_ok=True)
                elif member.isfile():
                    target.parent.mkdir(parents=True,exist_ok=True)
                    with target.open('xb') as output:output.write(stream.extractfile(member).read())
                    target.chmod(0o755 if member.mode&0o111 else 0o644)
            for member in stream.getmembers():
                if member.issym():
                    target=source/Path(*Path(member.name).parts[1:]);target.parent.mkdir(parents=True,exist_ok=True);target.symlink_to(member.linkname)
    actual={}
    for p in source.rglob('*'):
        if not p.resolve().is_relative_to(source):raise RuntimeError('SDL source tree contains an escaping link')
        if p.is_symlink():
            if not p.exists():raise RuntimeError('SDL source alias target is missing')
            actual[p.relative_to(source).as_posix()]=('symlink',os.readlink(p))
        elif p.is_file():actual[p.relative_to(source).as_posix()]=('file',sha(p))
    if actual!=expected:raise RuntimeError('SDL source tree differs from the canonical archive')
    configuration=dict(CMAKE_BUILD_TYPE='Release',CMAKE_OSX_ARCHITECTURES='arm64',
        CMAKE_OSX_DEPLOYMENT_TARGET=args.macos_deployment_target,CMAKE_INSTALL_PREFIX=str(install),SDL_SHARED='ON',SDL_STATIC='OFF',SDL_TEST='OFF')
    if not args.record_existing:
        subprocess.run([args.cmake,'-S',str(source),'-B',str(build),*['-D'+k+'='+v for k,v in configuration.items()]],check=True)
        subprocess.run([args.cmake,'--build',str(build),'--parallel',str(args.jobs)],check=True)
        subprocess.run([args.cmake,'--install',str(build)],check=True)
    cache={}
    for line in (build/'CMakeCache.txt').read_text().splitlines():
        if not line or line.startswith(('#','//')) or '=' not in line or ':' not in line.split('=',1)[0]:continue
        key,value=line.split('=',1);cache[key.split(':',1)[0]]=value
    if cache.get('CMAKE_HOME_DIRECTORY')!=str(source) or any(cache.get(k)!=v for k,v in configuration.items()):raise RuntimeError('SDL build configuration differs from recorded inputs')
    library=install/'lib/libSDL2-2.0.0.dylib'
    arch=subprocess.check_output(['lipo','-archs',str(library)],text=True).strip()
    loads=subprocess.check_output(['otool','-l',str(library)],text=True)
    match=re.search(r'\bminos\s+(\d+(?:\.\d+){0,2})',loads)
    if arch!='arm64' or not match or tuple(map(int,match[1].split('.')))!=tuple(map(int,args.macos_deployment_target.split('.'))):raise RuntimeError('SDL installed architecture/deployment target differs')
    result=dict(schema_version=1,system='Darwin',machine='arm64',version=pin['version'],source_url=pin['source_url'],
        source_sha256=pin['source_sha256'],source_files_verified=len(expected),configuration=configuration,
        cmake_cache_sha256=sha(build/'CMakeCache.txt'),artifact=dict(path='lib/'+library.name,bytes=library.stat().st_size,sha256=sha(library),minimum_macos=match[1]))
    provenance.parent.mkdir(parents=True,exist_ok=True)
    with provenance.open('x',encoding='utf-8') as output:output.write(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(result='pass',provenance=str(provenance),artifact=result['artifact'])),flush=True)


if __name__=='__main__':main()
