from pathlib import Path
import argparse
import json
import shutil
import subprocess
import time
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--seconds',type=int,default=1800);parser.add_argument('--suite',type=Path);args=parser.parse_args()
suite=args.suite or ROOT/'build/soak-suite'
if not args.suite:
    suite.mkdir(parents=True,exist_ok=True)
    for name in ('Visualizers.exe','VisualizersCtl.exe'):shutil.copy2(ROOT/'build/bin/Release'/name,suite/name)
    for name,id in [('Geiss64','geiss'),('MilkDrop2-64','milkdrop')]:shutil.copytree(ROOT.parent/name/'payload/plugin',suite/'plugins'/id,dirs_exist_ok=True)
subprocess.run([str(suite/'VisualizersCtl.exe'),'scan','--suite',str(suite)],check=True,stdout=subprocess.DEVNULL)
processes={};start=time.monotonic()
for id in ('geiss','milkdrop'):
    profile=ROOT/'build/soak'/id;profile.mkdir(parents=True,exist_ok=True)
    for name in ('metrics.jsonl','last-run.log'):
        if (profile/name).exists():(profile/name).unlink()
    processes[id]=subprocess.Popen([str(suite/'Visualizers.exe'),'--visualizer',id,'--demo','--background','--stress','--self-test','--profile',str(profile),'--quit-after',str(args.seconds*1000)])
    print(id,'pid',processes[id].pid,flush=True)
results={}
for id,process in processes.items():
    try:code=process.wait(timeout=max(20,args.seconds+30-(time.monotonic()-start)))
    except subprocess.TimeoutExpired:process.kill();process.wait();code=-1
    profile=ROOT/'build/soak'/id
    metrics=[json.loads(line) for line in (profile/'metrics.jsonl').read_text().splitlines()]
    steady=metrics[min(len(metrics)-1,12):]
    results[id]={'exit':code,'samples':len(metrics),'elapsed_seconds':round(time.monotonic()-start,2),'private_bytes_min':min(m['private_bytes'] for m in steady),'private_bytes_max':max(m['private_bytes'] for m in steady),'handles_min':min(m['handles'] for m in steady),'handles_max':max(m['handles'] for m in steady),'gdi_min':min(m['gdi'] for m in steady),'gdi_max':max(m['gdi'] for m in steady),'log':(profile/'last-run.log').read_text()}
(ROOT/'build/soak-result.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2),flush=True)
raise SystemExit(any(row['exit']!=0 for row in results.values()))
