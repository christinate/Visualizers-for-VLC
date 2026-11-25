"""Exercise malformed installed-cache data through the real VLC menu."""
from pathlib import Path
import argparse,json,os,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--suite',type=Path,required=True);p.add_argument('--audio',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();suite=a.suite.resolve();cache=suite/'installed-visualizers.json';saved=cache.read_bytes();rows=[]
try:
    for bad in [b'{',b'{"visualizers":42}',b'{"visualizers":[null,{}, {"id":"bad","name":42}]}']:
        cache.write_bytes(bad);report=suite/'menu-test.json'
        if report.exists():report.unlink()
        env=dict(os.environ,VLC_VISUALIZERS_SELF_TEST='1',VLC_VISUALIZERS_TEST_SUITE=str(suite));args=[str(suite/'VlcVisualizerLauncher'),'--ignore-config','--aout=dummy','--play-and-exit','--run-time=3']
        if sys.platform!='darwin':args+=['--no-qt-privacy-ask']
        args.append(str(a.audio.resolve()));result=subprocess.run(args,env=env,capture_output=True,text=True,timeout=30)
        assert result.returncode==0,(result.returncode,result.stderr);menu=json.loads(report.read_text());assert menu['attached'],menu
        rows.append({'malformed_cache':bad.decode(),'vlc_exit':result.returncode,'menu':menu})
finally:cache.write_bytes(saved)
a.output.write_text(json.dumps({'result':'pass','cases':rows},indent=2)+'\n');print('PASS: malformed JSON, wrong array type and malformed entries preserve VLC menu/process')
