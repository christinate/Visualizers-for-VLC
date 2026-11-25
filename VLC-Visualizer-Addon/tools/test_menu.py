from pathlib import Path
import json
import math
import os
import shutil
import subprocess
import time
import wave
import struct
from qt_runtime import files as qt_files
ROOT=Path(__file__).resolve().parents[1]
suite=ROOT/'build/menu-suite';suite.mkdir(parents=True,exist_ok=True)
plugins=ROOT/'build/menu-module';plugins.mkdir(exist_ok=True)
shutil.copy2(ROOT/'build/bin/Release/libvlc_visualizers_plugin.dll',plugins)
for name in ('Visualizers.exe','VisualizersCtl.exe'):shutil.copy2(ROOT/'build/bin/Release'/name,suite/name)
for project,id in [('Geiss64','geiss'),('MilkDrop2-64','milkdrop')]:shutil.copytree(ROOT.parent/project/'payload/plugin',suite/'plugins'/id,dirs_exist_ok=True)
subprocess.run([str(suite/'VisualizersCtl.exe'),'scan','--suite',str(suite)],check=True,stdout=subprocess.DEVNULL)
for file in (suite/'menu-test.json',suite/'menu-test-profile/last-run.log'):
    if file.exists():file.unlink()
env=dict(os.environ,VLC_PLUGIN_PATH=str(plugins),VLC_VISUALIZERS_SELF_TEST='1',VLC_VISUALIZERS_TEST_SUITE=str(suite))
installed=Path(os.environ.get('VLC_INSTALL',r'C:\Program Files\VideoLAN\VLC'))
private=ROOT/'build/inline-vlc'
if not private.exists():shutil.copytree(installed,private)
for destination,source in qt_files(ROOT).items():
    target=private/destination;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
if not (private/'vlc.visualizers.original.exe').exists():shutil.copy2(private/'vlc.exe',private/'vlc.visualizers.original.exe')
shutil.copy2(ROOT/'build/bin/Release/VlcVisualizerLauncher.exe',private/'vlc.exe')
player=private/'vlc.exe'
shutil.copy2(ROOT/'build/bin/Release/libvlc_visualizers_plugin.dll',private/'plugins/audio_filter/libvlc_visualizers_plugin.dll')
audio=ROOT/'build/menu-audio.wav'
if not audio.exists():
    with wave.open(str(audio),'wb') as output:
        output.setparams((2,2,44100,0,'NONE','not compressed'))
        output.writeframes(b''.join(struct.pack('<hh',int(10000*math.sin(n*.063)),int(9000*math.sin(n*.082))) for n in range(44100*10)))
args=[str(player),'--ignore-config','--no-one-instance','--no-media-library','--no-qt-privacy-ask','--extraintf=visualizers_menu','--aout=dummy','--file-logging','--verbose=2','--logfile='+str(ROOT/'build/menu-vlc.log'),'--play-and-exit','--run-time=9',str(audio)]
args.insert(1,'--reset-plugins-cache')
output=open(ROOT/'build/menu-process.log','w')
process=subprocess.Popen(args,env=env,stdout=output,stderr=output)
try:code=process.wait(timeout=120)
except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=5);raise RuntimeError('VLC menu test did not exit normally')
output.close()
print('VLC exit code:',code,flush=True)
assert code==0,code
result=json.loads((suite/'menu-test.json').read_text())
assert result['attached'] and result['entries']==2,result
order=result['menu_order'];assert order.index('View')+1==order.index('Visualizers')==order.index('Help')-1,order
log=(suite/'menu-test-profile/last-run.log').read_text()
assert 'native key round-trip=pass' in log and 'audio_peak=0.000000' not in log,log
(ROOT/'build/menu-test-result.json').write_text(json.dumps({'exit':code,'menu':result,'renderer_log':log},indent=2)+'\n')
print('PASS: native Visualizers menu attached to VLC, found both packages, and launched a renderer with VLC PCM')
