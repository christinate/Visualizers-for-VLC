"""Filesystem-only integration tests against copies of the installed VLC files.

Does not patch the real VLC installation or write uninstall registry entries.
"""
from pathlib import Path
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
import shutil
import subprocess
import uuid
ROOT=Path(__file__).resolve().parents[1]
VLC=Path(os.environ.get('VLC_INSTALL',r'C:\Program Files\VideoLAN\VLC'))
CONTROL=ROOT/'build/bin/Release/VisualizersCtl.exe'
TEST=ROOT/'build'/('patch-test-'+uuid.uuid4().hex)
TEST.mkdir()
def sha(file):
    with file.open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()
def run(*args,success=True):
    result=subprocess.run([str(CONTROL),*map(str,args)],capture_output=True,text=True,timeout=120)
    if (result.returncode==0)!=success:raise AssertionError((result.returncode,result.stdout,result.stderr,args))
    return result
def original(root):
    root.mkdir()
    for name in ('vlc.exe','libvlccore.dll'):
        source=VLC/('vlc.visualizers.original.exe' if name=='vlc.exe' and (VLC/'vlc.visualizers.original.exe').exists() else name)
        shutil.copy2(source,root/name)
    (root/'plugins/gui').mkdir(parents=True)
    shutil.copy2(VLC/'plugins/gui/libqt_plugin.dll',root/'plugins/gui/libqt_plugin.dll')
    (root/'plugins/audio_filter').mkdir(parents=True)
    (root/'plugins/audio_filter/libvlc_visualizers_plugin.dll').write_bytes(b'pre-existing-plugin-must-be-restored')
    return {str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()}
root=TEST/'VLC';before=original(root)
run('patch','--root',root,'--payload',ROOT/'payload')
run('verify','--root',root)
assert sha(root/'vlc.visualizers.original.exe')==before['vlc.exe']
suite=root/'visualizers'
for project,id in [('Geiss64','geiss'),('MilkDrop2-64','milkdrop'),('Terrain64','terrain')]:shutil.copytree(ROOT.parent/project/'payload/plugin',suite/'plugins'/id)
result=run('scan','--suite',suite)
assert len(json.loads(result.stdout)['visualizers'])==3
cache=(suite/'installed-visualizers.json').read_bytes()
geiss=suite/'plugins/geiss/assets/prismflow/index.html';geiss.rename(geiss.with_suffix('.missing'))
# A stale menu click must fail safely, even before another scan removes its row.
failed=subprocess.run([str(suite/'Visualizers.exe'),'--visualizer','geiss','--demo','--self-test','--background','--profile',str(TEST/'profile')],timeout=10)
assert failed.returncode==1
assert (suite/'installed-visualizers.json').read_bytes()==cache
assert len(json.loads(run('scan','--suite',suite).stdout)['visualizers'])==2
geiss.with_suffix('.missing').rename(geiss)
bad=suite/'plugins/invalid';bad.mkdir();value=json.loads((suite/'plugins/geiss/manifest.json').read_text());value['id']='invalid';value['entrypoint']='../geiss/vis_geiss64.dll';(bad/'manifest.json').write_text(json.dumps(value))
scan=json.loads(run('scan','--suite',suite).stdout);assert len(scan['visualizers'])==3 and any(x['package']=='invalid' for x in scan['rejected'])
wrapper=subprocess.run([str(root/'vlc.exe'),'--scan-visualizers'],timeout=15);assert wrapper.returncode==0
# Refuse modified installed files before making any restoration changes.
target=suite/'Visualizers.exe';patched=target.read_bytes();target.write_bytes(patched+b'changed')
snap={str(p.relative_to(root)):sha(p) for p in root.rglob('*') if p.is_file()}
run('restore','--root',root,success=False)
assert all(sha(root/name)==digest for name,digest in snap.items())
target.write_bytes(patched)
run('restore','--root',root)
assert all(sha(root/name)==digest for name,digest in before.items())
assert not (root/'vlc.visualizers.original.exe').exists()
# A file in use must trigger rollback and leave the original VLC executable intact.
lockedRoot=TEST/'locked';lockedBefore=original(lockedRoot)
kernel=C.WinDLL('kernel32',use_last_error=True);kernel.CreateFileW.argtypes=[W.LPCWSTR,W.DWORD,W.DWORD,C.c_void_p,W.DWORD,W.DWORD,W.HANDLE];kernel.CreateFileW.restype=W.HANDLE;kernel.CloseHandle.argtypes=[W.HANDLE]
handle=kernel.CreateFileW(str(lockedRoot/'vlc.exe'),0x80000000,1,None,3,0,None)
assert handle not in (0,C.c_void_p(-1).value)
try:run('patch','--root',lockedRoot,'--payload',ROOT/'payload',success=False)
finally:kernel.CloseHandle(handle)
assert all(sha(lockedRoot/name)==digest for name,digest in lockedBefore.items())
assert not (lockedRoot/'vlc.visualizers.original.exe').exists()
assert not list(lockedRoot.rglob('*.patch.tmp'))
longest=max(len(str(root/item['path'])+'.patch.tmp') for item in json.loads((ROOT/'payload/payload.json').read_text())['files'])
report={'result':'pass','sandbox':str(TEST),'longest_staged_path_chars':longest,'checks':['install hashes','verified original executable copy','persistent scan','removed plugin rejected at launch','rescan removal','traversal rejected','vlc.exe scan command','modified-file restoration refusal','byte-identical restoration','in-use rollback']}
if longest>260:report['checks'].append('payload paths beyond MAX_PATH')
(ROOT/'build/patch-test-result.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
