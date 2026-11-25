"""Bounded concurrent Mac renderer soak with generated audio and RSS samples."""
from pathlib import Path
import argparse
import json
import platform
import subprocess
import time
from check_dawn_player import records


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--seconds',type=int,default=600)
    args=parser.parse_args()
    if platform.system()!='Darwin' or not 120<=args.seconds<=3600:
        raise RuntimeError('Requires macOS and a bounded 120–3600 second duration')
    suite=args.suite.resolve();output=args.output.resolve();output.mkdir(parents=True)
    subprocess.run([str(suite/'VisualizersCtl'),'scan','--suite',str(suite)],check=True,capture_output=True)
    children=[];streams=[];samples=[];rows=[]
    report=dict(result='running',seconds=args.seconds,audio='generated only; no microphone or audio file',samples=samples,cases=rows)
    def save():
        temporary=output/'progress.tmp'
        temporary.write_text(json.dumps(report,indent=2)+'\n')
        temporary.replace(output/'result.json')
    try:
        for name,identifier,original in [('terrain','terrain',False),('prismflow','geiss',False),('geiss-classic','geiss',True),('projectm','milkdrop',True)]:
            folder=output/name;folder.mkdir();profile=folder/'profile';profile.mkdir()
            (profile/(identifier+'.json')).write_text(json.dumps(dict(width=320,height=240,fps=30,hdr=False,engine='classic' if original else 'modern')))
            exe=suite/'Visualizers' if original else suite/'native/VisualizerNative.app/Contents/MacOS/VisualizerNative'
            cmd=[str(exe),'--suite',str(suite),'--visualizer',identifier,'--demo','--background','--profile',str(profile)]
            if original:
                cmd+=['--quit-after',str(args.seconds*1000),'--stress']
                if identifier=='geiss':cmd+=['--classic']
            else:
                cmd+=['--seconds',str(args.seconds),'--render-width','192','--render-height','128','--output-dir',str(folder)]
            stream=(folder/'process.log').open('w');streams.append(stream)
            child=subprocess.Popen(cmd,stdout=stream,stderr=subprocess.STDOUT);children.append((name,original,child,folder))
            rows.append(dict(case=name,pid=child.pid,command=cmd))
        started=time.monotonic();next_sample=0
        while any(child.poll() is None for _,_,child,_ in children):
            elapsed=time.monotonic()-started
            if elapsed>args.seconds+30:raise RuntimeError('Renderer did not finish its bounded duration')
            for name,_,child,_ in children:
                if child.poll() is not None and (child.returncode or elapsed<args.seconds-5):
                    raise RuntimeError(name+' ended early: '+str(child.returncode))
            if elapsed>=next_sample:
                for name,_,child,_ in children:
                    if child.poll() is not None:continue
                    observed=subprocess.run(['ps','-p',str(child.pid),'-o','rss=,vsz=,pcpu=,time=,state='],capture_output=True,text=True)
                    if observed.returncode==0 and observed.stdout.strip():
                        values=observed.stdout.split()
                        samples.append(dict(case=name,elapsed_seconds=round(elapsed,3),rss_kib=int(values[0]),vsz_kib=int(values[1]),cpu_percent=float(values[2]),cpu_time=values[3],state=values[4]))
                next_sample=elapsed+30;save()
                print(json.dumps(dict(elapsed_seconds=round(elapsed),running=[name for name,_,p,_ in children if p.poll() is None])),flush=True)
            time.sleep(.25)
        for row,(name,original,child,folder) in zip(rows,children):
            assert child.wait()==0,name
            if original:
                text=(folder/'profile/last-run.log').read_text()
                assert 'exit=0' in text and float(text.split('audio_peak=')[1].split()[0])>.1,text
                row['observed_log']=text
            else:
                observed=json.loads((folder/'result.json').read_text())
                assert observed['result']=='pass' and observed['closed_by']=='duration' and not observed['recording_started'],observed
                assert observed['frames']>args.seconds*15 and observed['maximum_frames_in_flight']<=2,observed
                if name=='terrain' and args.seconds>=300:assert observed['camera_input_mask']==15,observed
                row['observed']=observed
            row.update(result='pass',exit=child.returncode)
        report.update(result='pass',scope='Concurrent generated-audio windows and sampled RSS/VSZ/CPU only. No continuous peaks, GPU/private-memory, real media/microphone, indefinite stability or universal leak-freedom claim.')
    except Exception as error:
        report.update(result='fail',error=repr(error));raise
    finally:
        owned={child.pid for _,_,child,_ in children if child.poll() is None}
        for file,identity in records(suite,0):
            if identity['pid'] in owned:file.with_name(file.name+'.close').write_text(identity['token'])
        for _,_,child,_ in children:
            if child.poll() is None:
                try:child.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    child.terminate();child.wait(timeout=5)
        for stream in streams:stream.close()
        save()


if __name__=='__main__':main()
