"""Verify native installer ownership, opt-outs, rescanning and refusal paths."""
from pathlib import Path
import argparse,json,os,subprocess,sys
parser=argparse.ArgumentParser();parser.add_argument('--package',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
package=args.package.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
suite=out/'suite';suite.mkdir(exist_ok=True);original=b'original readme must be restored\n';(suite/'README.md').write_bytes(original)
def run(*argv,ok=True):
    result=subprocess.run(list(map(str,argv)),stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=60)
    assert (result.returncode==0)==ok,(result.returncode,result.stderr)
    return result
run(package/'install.sh','--prefix',suite)
ctl=package/'.installer/VisualizersCtl';scan=json.loads(run(suite/'VlcVisualizerLauncher','--scan-visualizers').stdout);assert len(scan['visualizers'])==3
# Modification is a concrete refusal, preserving every other installed file.
file=suite/'plugins/geiss/manifest.json';saved=file.read_bytes();file.write_bytes(saved+b' ')
run(ctl,'uninstall','--suite',suite,'--component','geiss',ok=False);assert (suite/'plugins/geiss'/scan['visualizers'][0]['entrypoint']).exists();file.write_bytes(saved)
# Missing packages are rejected again at a click/check, before a rescan.
engine=suite/'plugins/geiss'/scan['visualizers'][0]['entrypoint'];engine.rename(engine.with_suffix('.missing'));run(ctl,'check','--suite',suite,'--package',suite/'plugins/geiss',ok=False)
assert len(json.loads(run(ctl,'scan','--suite',suite).stdout)['visualizers'])==2;engine.with_suffix('.missing').rename(engine)
run(package/'uninstall.sh','--prefix',suite);assert (suite/'README.md').read_bytes()==original
assert not (suite/'VlcVisualizerLauncher').exists();assert not engine.exists()
selected=out/'selected';run(package/'install.sh','--prefix',selected,'--without-geiss');assert [e['id'] for e in json.loads(run(selected/'VlcVisualizerLauncher','--scan-visualizers').stdout)['visualizers']]==['milkdrop','terrain'];run(package/'uninstall.sh','--prefix',selected)
report={'result':'pass','platform':sys.platform,'checks':['default bundles','launcher scan','modified-file refusal','missing engine revalidation','rescan removal','exact original restoration','optional component omitted','owned-file uninstall']}
(out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
