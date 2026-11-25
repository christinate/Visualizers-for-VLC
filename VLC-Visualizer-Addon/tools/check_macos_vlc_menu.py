"""Actual Mac VLC AppKit activation/PCM fixtures; no microphone or audio saved."""
from pathlib import Path
import argparse
import json
import math
import os
import platform
import shutil
import struct
import subprocess
import time
import wave


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-suite',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if platform.system()!='Darwin':raise RuntimeError('Requires macOS with installed arm64 VLC 3')
    source=args.source_suite.resolve();output=args.output.resolve()
    if source.is_relative_to(output) or output.is_relative_to(source):raise RuntimeError('Use a separate fresh output directory')
    output.mkdir(parents=True);audio=output/'generated-stereo.wav'
    with wave.open(str(audio),'wb') as stream:
        stream.setnchannels(2);stream.setsampwidth(2);stream.setframerate(48000)
        data=bytearray()
        for n in range(48000):
            for hz in (220,330):data+=struct.pack('<h',int(12000*math.sin(2*math.pi*hz*n/48000)))
        stream.writeframes(data*8)
    rows=[]
    for name,identifier,original in [('terrain','terrain',False),('prismflow','geiss',False),('projectm','milkdrop',True),('geiss-classic','geiss',True)]:
        folder=output/name;folder.mkdir();suite=folder/'suite';shutil.copytree(source,suite,symlinks=True)
        profile=suite/'menu-test-profile';profile.mkdir()
        (profile/(identifier+'.json')).write_text(json.dumps(dict(width=320,height=240,fps=60,engine='classic' if original else 'modern')))
        subprocess.run([str(suite/'VisualizersCtl'),'scan','--suite',str(suite)],check=True,capture_output=True)
        env=dict(os.environ,VLC_VISUALIZERS_SELF_TEST='1',VLC_VISUALIZERS_TEST_SUITE=str(suite),VLC_VISUALIZERS_TEST_RENDERER=identifier)
        with (folder/'vlc.log').open('w') as stream:
            child=subprocess.run([str(suite/'VlcVisualizerLauncher'),'--ignore-config','--aout=dummy','--play-and-exit',str(audio)],env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=45)
        menu=json.loads((suite/'menu-test.json').read_text());order=menu['menu_order']
        assert child.returncode==0 and menu['entries']==3 and order.count('Visualizers')==1,menu
        assert order.index('Visualizers')==order.index('View')+1 and order.index('Visualizers')<order.index('Help'),order
        report=profile/'last-run.log' if original else profile/('native-'+identifier)/'result.json'
        deadline=time.monotonic()+15
        while time.monotonic()<deadline:
            if report.exists() and (not original or 'exit=' in report.read_text()):break
            time.sleep(.1)
        if original:
            content=report.read_text()
            assert 'exit=0' in content and 'restore/borderless=pass' in content and 'round-trip=pass' in content,content
            peak=float(content.split('audio_peak=')[1].split()[0]);assert peak>0,content
            observed=dict(result='pass',audio_peak=peak,log=content)
        else:
            observed=json.loads(report.read_text())
            assert observed['result']=='pass' and observed['frames']>=90 and observed['closed_by']=='escape',observed
            assert observed['player_pcm_peak']>0 and observed['player_pcm_windows']>0 and not observed['recording_started'],observed
        rows.append(dict(case=name,result='pass',vlc_exit=child.returncode,menu=menu,observed=observed))
        print(name+': pass',flush=True)
    (output/'result.json').write_text(json.dumps(dict(result='pass',cases=rows,
        scope='Actual VLC decode and AppKit menu delegate activation fixtures. Original engines also check fullscreen restoration and key round-trip. No manual pointer, capture permission, settings dialog, menu Close or physical HDR claim.'),indent=2)+'\n')


if __name__=='__main__':main()
