"""Check saved fullscreen/window sizes through actual SDL windows."""
from pathlib import Path
import argparse,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--suite',type=Path,required=True);p.add_argument('--output',type=Path,required=True);args=p.parse_args();suite=args.suite.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True);rows=[]
for id in ('geiss','milkdrop'):
    for full in (False,True):
        profile=out/(id+('-full' if full else '-window'));profile.mkdir(exist_ok=True);(profile/(id+'.json')).write_text(json.dumps({'width':512,'height':384,'fps':30,'fullscreen':full}))
        result=subprocess.run([str(suite/'Visualizers'),'--visualizer',id,'--demo','--background','--self-test','--profile',str(profile),'--quit-after','12000'],capture_output=True,text=True,timeout=35)
        log=(profile/'last-run.log').read_text();assert result.returncode==0,(result.returncode,result.stderr,log)
        assert 'startup_fullscreen='+str(int(full)) in log and 'restore/borderless=pass' in log and 'round-trip=pass' in log,log
        rows.append({'renderer':id,'startup_fullscreen':full,'exit':result.returncode,'log':log})
(out/'result.json').write_text(json.dumps(rows,indent=2)+'\n');print('PASS: both renderers start windowed/fullscreen and restore 512x384 geometry')
