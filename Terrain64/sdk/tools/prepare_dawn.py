"""Fetch a pinned development SDK. Never downloads or runs anything in an installer."""
from pathlib import Path
import argparse,hashlib,json,platform,tarfile,urllib.request
ROOT=Path(__file__).resolve().parents[1]

def digest(file):
    with file.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def prepare(target=None,cache=None):
    pin=json.loads((ROOT/'dawn/dependencies.lock.json').read_text(encoding='utf-8'))
    if target is None:
        system=platform.system();machine=platform.machine().lower()
        target='windows-x64' if system=='Windows' and machine in ('amd64','x86_64') else 'macos-arm64' if system=='Darwin' and machine in ('arm64','aarch64') else 'linux-x86_64' if system=='Linux' and machine in ('amd64','x86_64') else ''
    if target not in pin['assets']:raise RuntimeError('No pinned SDK for this target: '+target)
    cache=Path(cache or ROOT/'build/dawn-sdk').resolve();cache.mkdir(parents=True,exist_ok=True)
    asset=pin['assets'][target];archive=cache/asset['file']
    if not archive.exists():
        url='https://github.com/google/dawn/releases/download/'+pin['release_tag']+'/'+asset['file']
        temporary=archive.with_suffix(archive.suffix+'.download')
        request=urllib.request.Request(url,headers={'User-Agent':'VLC-Visualizers-local-build'})
        with urllib.request.urlopen(request,timeout=120) as source,temporary.open('wb') as output:
            while chunk:=source.read(1024*1024):output.write(chunk)
        temporary.replace(archive)
    if digest(archive)!=asset['sha256']:raise RuntimeError('Dawn SDK checksum mismatch: '+str(archive))
    sdk=cache/(target+'-'+pin['source_revision'][:12]);marker=sdk/'.verified.json'
    if not marker.exists():
        sdk.mkdir(parents=True,exist_ok=True)
        with tarfile.open(archive,'r:gz') as stream:stream.extractall(sdk,filter='data')
        marker.write_text(json.dumps({'target':target,'source_revision':pin['source_revision'],'archive_sha256':asset['sha256']},indent=2)+'\n',encoding='utf-8')
    else:
        saved=json.loads(marker.read_text())
        if saved['archive_sha256']!=asset['sha256'] or saved['source_revision']!=pin['source_revision']:raise RuntimeError('Unexpected SDK cache provenance')
    configs=list(sdk.rglob('DawnConfig.cmake'))+list(sdk.rglob('dawn-config.cmake'))
    if len(configs)!=1:raise RuntimeError('Expected one relocatable Dawn CMake package, found '+str(len(configs)))
    return sdk,configs[0].parent

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--target');parser.add_argument('--cache',type=Path);args=parser.parse_args()
    sdk,config=prepare(args.target,args.cache)
    print(json.dumps({'sdk':str(sdk),'Dawn_DIR':str(config)},indent=2))
