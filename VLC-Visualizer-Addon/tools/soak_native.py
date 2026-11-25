"""Native renderer stress run with explicit deadlines and saved measurements."""
from pathlib import Path
import argparse,json,subprocess,time,sys,platform
parser=argparse.ArgumentParser();parser.add_argument('--suite',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);parser.add_argument('--seconds',type=int,default=1800);args=parser.parse_args()
suite=args.suite.resolve();output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
processes={};started=time.monotonic()
for id in ('geiss','milkdrop'):
    profile=output/id;profile.mkdir(exist_ok=True)
    for file in ('metrics.jsonl','last-run.log'):
        if (profile/file).exists():(profile/file).unlink()
    (profile/(id+'.json')).write_text(json.dumps({'width':640,'height':480,'fps':30,'fullscreen':False}))
    stream=open(output/(id+'-process.log'),'w')
    process=subprocess.Popen([str(suite/'Visualizers'),'--visualizer',id,'--demo','--background','--stress','--self-test','--profile',str(profile),'--quit-after',str(args.seconds*1000)],stdout=stream,stderr=stream)
    processes[id]=(process,stream,profile);print(id,'pid',process.pid,flush=True)
results={}
for id,(process,stream,profile) in processes.items():
    try:code=process.wait(timeout=max(20,args.seconds+40-(time.monotonic()-started)))
    except subprocess.TimeoutExpired:process.terminate();process.wait(timeout=10);code=-1
    stream.close();metrics=[json.loads(line) for line in (profile/'metrics.jsonl').read_text().splitlines()] if (profile/'metrics.jsonl').exists() else []
    steady=metrics[min(12,max(0,len(metrics)-1)):]
    log=(profile/'last-run.log').read_text() if (profile/'last-run.log').exists() else ''
    results[id]={'exit':code,'samples':len(metrics),'elapsed_seconds':round(time.monotonic()-started,2),'resident_bytes_min':min((r['resident_bytes'] for r in steady),default=0),'resident_bytes_max':max((r['resident_bytes'] for r in steady),default=0),'renderer_log':log}
report={'platform':sys.platform,'architecture':platform.machine(),'seconds_requested':args.seconds,'renderers':results}
(output/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2),flush=True)
raise SystemExit(any(r['exit']!=0 for r in results.values()))
