"""Small filesystem-only regression for framework links and exact restoration."""
from pathlib import Path
import argparse,json,os,platform,subprocess
p=argparse.ArgumentParser();p.add_argument('--ctl',type=Path,required=True);p.add_argument('--work',type=Path,required=True);a=p.parse_args()
root=a.work.resolve();root.mkdir();source=root/'source';suite=root/'suite'
(source/'target/A').mkdir(parents=True);suite.mkdir()
(source/'target/A/file').write_text('framework data\n')
(source/'target/Current').symlink_to('A',target_is_directory=True)
(source/'alias').symlink_to('target/Current/file')
(suite/'original').write_text('original user data\n');(suite/'alias').symlink_to('original')
manifest={'platform':'macos' if platform.system()=='Darwin' else 'linux','architecture':'arm64' if platform.machine() in ('arm64','aarch64') else 'x86_64','version':'test','components':{'addon':['target/A/file','target/Current','alias']}}
(source/'native-payload.json').write_text(json.dumps(manifest))
def run(*args,ok=True):
 r=subprocess.run([str(a.ctl),*map(str,args)],capture_output=True,text=True)
 assert (r.returncode==0)==ok,(r.returncode,r.stderr)
 return r
run('install','--suite',suite,'--source',source)
assert (suite/'target/Current').is_symlink() and os.readlink(suite/'target/Current')=='A'
assert (suite/'alias').read_text()=='framework data\n'
# Preserve changed links, including dangling links, before any removal.
(suite/'alias').unlink();(suite/'alias').symlink_to('missing')
run('uninstall','--suite',suite,'--component','addon',ok=False)
assert (suite/'target/A/file').exists()
(suite/'alias').unlink();(suite/'alias').symlink_to('target/Current/file')
run('uninstall','--suite',suite,'--component','addon')
assert os.readlink(suite/'alias')=='original' and (suite/'alias').read_text()=='original user data\n'
assert not (suite/'target').exists()
print(json.dumps({'result':'pass','checks':['framework directory links','file links','dangling modified link refusal','original symlink restoration']},indent=2))
