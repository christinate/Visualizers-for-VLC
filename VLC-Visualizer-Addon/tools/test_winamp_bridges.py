"""Exercise Winamp's capture ABI without requiring a Winamp UI installation.
Build both renderer release payloads first. Each renderer is staged with its
own independent runtime, matching its standalone Winamp installer layout.
"""
from pathlib import Path
import json
import os
import subprocess
import shutil
import time
ROOT=Path(__file__).resolve().parents[1]
results=[]
for project,id in [('Geiss64','geiss'),('MilkDrop2-64','milkdrop')]:
    suite=ROOT.parent/project/'build/winamp-test-suite'
    shutil.copytree(ROOT.parent/project/'payload/runtime',suite,dirs_exist_ok=True)
    shutil.copytree(ROOT.parent/project/'payload/plugin',suite/'plugins'/id,dirs_exist_ok=True)
    subprocess.run([str(suite/'VisualizersCtl.exe'),'scan','--suite',str(suite)],check=True,stdout=subprocess.DEVNULL)
    for arch in ['x86','x64']:
        profile=ROOT/'build/bridge-tests'/f'{id}-{arch}'
        profile.mkdir(parents=True,exist_ok=True)
        log=profile/'last-run.log'
        if log.exists():log.unlink()
        env=dict(os.environ,VLC_VISUALIZERS_BRIDGE_HOST=str(suite/'Visualizers.exe'),VLC_VISUALIZERS_BRIDGE_TEST_PROFILE=str(profile))
        build=ROOT.parent/project/f'build-bridge-{arch}/Release'
        probe=subprocess.run([str(build/'bridge_probe.exe'),str(build/f'vis_{id}_bridge.dll')],env=env,capture_output=True,text=True,timeout=15)
        assert probe.returncode==0,(probe.returncode,probe.stdout,probe.stderr)
        for attempt in range(30):
            if log.exists() and 'exit=0' in log.read_text():break
            time.sleep(.1)
        output=log.read_text()
        assert 'exit=0' in output and 'audio_peak=0.000000' not in output,output
        results.append({'id':id,'architecture':arch,'exit':probe.returncode,'probe':probe.stdout.strip(),'renderer_log':output})
        print(id,arch,'pass',flush=True)
(ROOT/'build/bridge-test-result.json').write_text(json.dumps(results,indent=2)+'\n')
