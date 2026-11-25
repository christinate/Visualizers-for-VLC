"""Pinned, hash-verified local Electron runtime; no npm/network code in installers."""
from pathlib import Path
import hashlib,json,os,platform,shutil,stat,subprocess,sys,urllib.request,zipfile
VERSION='45.3.3'
HASHES={
 'win32-x64':'49dd2d3875944f08de03d34595cd30093c588b9edcabb41a0ac481028a3a5a8a',
 'darwin-arm64':'25ae0ce40ee462433c1029b8fcc485949fc8a89394b62b70887d7793898edc89',
 'linux-x64':'0a63ce0e2b6a69d6ced7194949934b8414a6dadeca82910f88dc8720a1433269'}
def digest(file):
 with open(file,'rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def download(url,file):
 file.parent.mkdir(parents=True,exist_ok=True)
 temporary=file.with_suffix(file.suffix+'.download')
 with urllib.request.urlopen(url,timeout=90) as source,open(temporary,'wb') as output:shutil.copyfileobj(source,output)
 temporary.replace(file)
def prepare(root,cache=None):
 root=Path(root).resolve();sdk=root/'sdk'
 system=sys.platform;arch='arm64' if platform.machine() in ('arm64','aarch64') else 'x64';target=system+'-'+arch
 if target not in HASHES:raise RuntimeError('No verified browser runtime for '+target)
 cache=Path(cache or root/'build/browser-runtime');archive=cache/f'electron-v{VERSION}-{target}.zip'
 if not archive.exists():download(f'https://github.com/electron/electron/releases/download/v{VERSION}/{archive.name}',archive)
 if digest(archive)!=HASHES[target]:raise RuntimeError('Electron archive SHA256 mismatch: '+str(archive))
 unpacked=cache/(target+'-v3')
 if not (unpacked/'.verified.json').exists():
  unpacked.mkdir(parents=True,exist_ok=True)
  with zipfile.ZipFile(archive) as z:
   for item in z.infolist():
    path=unpacked/item.filename
    if not path.resolve().is_relative_to(unpacked.resolve()):raise RuntimeError('Archive escapes runtime directory')
    mode=item.external_attr>>16
    if stat.S_ISLNK(mode):
     link=z.read(item).decode('utf-8')
     if Path(link).is_absolute() or not (path.parent/link).resolve().is_relative_to(unpacked.resolve()):raise RuntimeError('Archive symlink escapes runtime directory')
     path.parent.mkdir(parents=True,exist_ok=True)
     if path.is_symlink():path.unlink()
     path.symlink_to(link)
    else:z.extract(item,unpacked)
    if os.name!='nt' and not stat.S_ISLNK(mode) and mode:os.chmod(path,mode&0o777)
  (unpacked/'.verified.json').write_text(json.dumps({'version':VERSION,'archive_sha256':HASHES[target]})+'\n')
 native=root/'build/browser-native-sdk';command=['cmake','-S',str(sdk/'browser/native'),'-B',str(native),'-DAUDIO_INCLUDE_DIR='+str(sdk/'runtime/include')]
 if system=='win32':
  lib=cache/'node.lib'
  if not lib.exists():download(f'https://artifacts.electronjs.org/headers/dist/v{VERSION}/win-x64/node.lib',lib)
  if digest(lib)!='c4ba898a42b8e5e37eeecae6e89c9831d59e0e2d9a279b4189fc2b349ac63156':raise RuntimeError('Node import library SHA256 mismatch')
  command+=['-G',os.environ.get('CMAKE_GENERATOR','Visual Studio 18 2026'),'-A','x64','-DNODE_LIBRARY='+str(lib)]
 subprocess.run(command,check=True)
 subprocess.run(['cmake','--build',str(native),'--config','Release','--parallel','4'],check=True)
 module=native/('Release/visualizer_audio.node' if system=='win32' else 'visualizer_audio.node')
 return unpacked,module
def stage(root,destination,cache=None):
 root=Path(root).resolve();destination=Path(destination);runtime,module=prepare(root,cache)
 # copytree preserves links, but does not replace links on a repeated stage.
 for file in runtime.rglob('*'):
  if file.is_symlink():
   target=destination/'runtime'/file.relative_to(runtime)
   if target.is_symlink():
    if os.readlink(target)!=os.readlink(file):raise RuntimeError('Modified staged runtime link: '+str(target))
    target.unlink()
 shutil.copytree(runtime,destination/'runtime',dirs_exist_ok=True,symlinks=True)
 app=destination/'app';app.mkdir(parents=True,exist_ok=True)
 for name in ('main.cjs','preload.cjs','package.json'):shutil.copy2(root/'sdk/browser'/name,app/name)
 shutil.copytree(root/'sdk/browser/ui',app/'ui',dirs_exist_ok=True)
 (app/'native').mkdir(exist_ok=True);shutil.copy2(module,app/'native/visualizer_audio.node')
 return destination
