"""Check generated player PCM, capture isolation and owner-exit closure locally."""
from pathlib import Path
import argparse
import json
import os
import platform
import re
import subprocess
import time
from check_dawn_window import control_directory

ROOT=Path(__file__).resolve().parents[1]


def wait_for(path,children,timeout=30):
    until=time.monotonic()+timeout
    while time.monotonic()<until:
        if path.is_file():
            try:return json.loads(path.read_text(encoding='utf-8'))
            except (ValueError,OSError):pass
        if any(child.poll() is not None for child in children):
            raise RuntimeError('Test process exited before ready: '+str(path))
        time.sleep(.05)
    raise RuntimeError('Timed out waiting for '+str(path))


def records(suite,owner):
    found=[]
    for record in list(control_directory().glob('*.json'))[:2048]:
        try:
            if record.stat().st_size>4096:continue
            value=json.loads(record.read_text(encoding='utf-8'))
            if value['protocol']==1 and value['owner_pid']==owner and Path(value['suite']).resolve()==suite.resolve() and re.fullmatch('[0-9a-f]{32}',value['token']):found.append((record,value))
        except (OSError,ValueError,KeyError,TypeError):pass
    return found


def close_owned(suite,owner):
    for record,value in records(suite,owner):record.with_name(record.name+'.close').write_text(value['token'],encoding='ascii')


def ipc_file(pid):
    if platform.system()=='Windows':return None
    runtime=os.environ.get('XDG_RUNTIME_DIR')
    base=Path(runtime)/'vlc-visualizers' if runtime else Path.home()/'.cache/vlc-visualizers/ipc'
    return base/(str(os.getuid())+'-'+str(pid)+'.pcm')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',type=Path,required=True)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--plugin',type=Path,required=True)
    parser.add_argument('--shader',type=Path,default=ROOT.parent/'Terrain64/assets/terrain/terrain.wgsl')
    parser.add_argument('--output-dir',type=Path,default=ROOT/'build/dawn-player-check')
    parser.add_argument('--software',action='store_true')
    args=parser.parse_args();output=args.output_dir.resolve();output.mkdir(parents=True,exist_ok=True)
    run=output/('run-'+str(time.time_ns()));run.mkdir();environment=os.environ.copy()
    if args.software:
        if platform.system()!='Linux':raise RuntimeError('This checker uses the installed Linux lavapipe driver for software testing')
        environment['VK_DRIVER_FILES']='/usr/share/vulkan/icd.d/lvp_icd.json'
        if not Path(environment['VK_DRIVER_FILES']).is_file():raise RuntimeError('Installed lavapipe driver is required')
    reports=[]
    for name,generated,count,fullscreen,closure in [('pcm-siblings',True,2,False,'player-exit'),('missing-bridge',False,1,False,'player-exit'),('fullscreen',True,1,True,'player-exit'),('menu',True,1,False,'menu')]:
        folder=run/name;folder.mkdir();suite=folder/'suite';suite.mkdir();stop=folder/'stop';owner=None;children=[];streams=[]
        try:
            owner_log=(folder/'owner.log').open('w',encoding='utf-8');streams.append(owner_log)
            owner=subprocess.Popen([str(args.probe.resolve()),'--writer' if generated else '--hold',str(folder/'owner.json'),str(stop)],stdout=owner_log,stderr=owner_log)
            identity=wait_for(folder/'owner.json',[owner]);assert identity['pid']==owner.pid
            if name=='pcm-siblings':
                negative=[['--line-in','default'],['--select-input'],['--synthetic'],['--reselection-test']]
                for n,extra in enumerate(negative):
                    invalid=folder/('invalid-source-'+str(n))
                    attempt=subprocess.run([str(args.exe.resolve()),'--terrain-shader',str(args.shader.resolve()),'--vlc-pid',str(owner.pid),'--self-test','--output-dir',str(invalid),*extra],capture_output=True,text=True,timeout=10,env=environment)
                    failure=json.loads((invalid/'result.json').read_text(encoding='utf-8'))
                    assert attempt.returncode==1 and 'Player audio cannot be combined' in failure['error'],failure
                for n,pid in enumerate(['0','-1','4294967296','12suffix']):
                    invalid=folder/('invalid-pid-'+str(n))
                    attempt=subprocess.run([str(args.exe.resolve()),'--self-test','--output-dir',str(invalid),'--vlc-pid',pid],capture_output=True,text=True,timeout=10,env=environment)
                    assert attempt.returncode==1 and 'Invalid' in attempt.stderr,attempt.stderr
            for n in range(count):
                child_folder=folder/str(n);child_folder.mkdir()
                command=[str(args.exe.resolve()),'--terrain-shader',str(args.shader.resolve()),'--plugin',str(args.plugin.resolve()),'--vlc-pid',str(owner.pid),'--suite',str(suite),
                         '--self-test','--player-test','--close-test',closure,'--seconds','40','--output-dir',str(child_folder),'--render-width','192','--render-height','128']
                if fullscreen:command.append('--startup-fullscreen')
                stdout=(child_folder/'stdout.log').open('w',encoding='utf-8');stderr=(child_folder/'stderr.log').open('w',encoding='utf-8');streams.extend([stdout,stderr])
                children.append(subprocess.Popen(command,stdout=stdout,stderr=stderr,env=environment))
            for n in range(count):
                ready=wait_for(folder/str(n)/'ready.json',children)
                assert ready['frames']>=30 and ready['input_picker_opens']==0,ready
                if generated:assert ready['player_pcm_windows']>0 and ready['player_pcm_peak']>.1,ready
                else:assert ready['player_pcm_windows']==0 and ready['player_pcm_peak']==0,ready
            if closure=='player-exit':
                assert len(records(suite,owner.pid))==count,'Missing player-owned window registration'
                stop.write_text('stop\n',encoding='ascii');assert owner.wait(timeout=5)==0
                mapping=ipc_file(owner.pid)
                assert mapping is None or not mapping.exists(),'Generated player IPC file was not released'
            for n,child in enumerate(children):
                assert child.wait(timeout=10)==0,(name,n)
                report=json.loads((folder/str(n)/'result.json').read_text(encoding='utf-8'))
                assert report['result']=='pass' and report['closed_by']==closure and report['owner_pid']==owner.pid,report
                assert not report['recording_started'] and report['input_picker_opens']==0 and report['player_input_requests_blocked']==1,report
                assert report['source']=='media player PCM' and report['maximum_frames_in_flight']<=2 and report['final_fullscreen']==fullscreen,report
                if generated:assert report['player_pcm_windows']>0 and report['player_pcm_peak']>.1,report
                else:assert report['player_pcm_windows']==0 and report['player_pcm_peak']==0,report
                reports.append({'case':name,'window':n,'observed':report})
            assert not records(suite,owner.pid),'Player-owned control record was not released'
            if closure=='menu':assert owner.poll() is None,'Closing the visualizer stopped its player'
            print(name+': pass',flush=True)
        finally:
            close_owned(suite,owner.pid if owner else 0)
            for child in children:
                try:child.wait(timeout=3)
                except subprocess.TimeoutExpired:child.kill();child.wait(timeout=5)
            stop.write_text('stop\n',encoding='ascii')
            if owner:
                try:owner.wait(timeout=5)
                except subprocess.TimeoutExpired:owner.kill();owner.wait(timeout=5)
            for stream in streams:stream.close()
    (output/'summary.json').write_text(json.dumps({'result':'pass','generated_pcm_only':True,'run':str(run),'cases':reports},indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
