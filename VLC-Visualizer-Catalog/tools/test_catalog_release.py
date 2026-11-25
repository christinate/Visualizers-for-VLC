"""Check actual isolated Git catalog packages, provenance and refusal behavior."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
from release import CONTENTS

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();output=args.output.resolve();output.mkdir(parents=True)
    cases=[]

    def git(root,*arguments):
        return subprocess.check_output(['git','-C',str(root),*arguments],stderr=subprocess.STDOUT).decode().strip()

    def commit(root):
        git(root,'add','--all')
        git(root,'-c','user.name=Catalog release test','-c','user.email=catalog-test@invalid',
            'commit','--quiet','-m','isolated fixture')
        return git(root,'rev-parse','HEAD')

    def fixture(name):
        root=output/name;root.mkdir()
        for name in CONTENTS:
            source=ROOT/name;target=root/name
            if source.is_dir():shutil.copytree(source,target,ignore=shutil.ignore_patterns('__pycache__'))
            else:shutil.copy2(source,target)
        shutil.copy2(ROOT/'.gitignore',root/'.gitignore')
        git(root,'init','--quiet');git(root,'config','core.autocrlf','false')
        commit(root)
        return root

    def invoke(root,ok=True,env=None):
        command=[sys.executable,str(root/'tools/release.py'),'--output',str(root/'build/artifact'),
                 '--stage',str(root/'build/validation')]
        child=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace',env=env)
        (root/'build').mkdir(exist_ok=True)
        (root/'build/check.stdout.log').write_text(child.stdout,encoding='utf-8')
        (root/'build/check.stderr.log').write_text(child.stderr,encoding='utf-8')
        assert (child.returncode==0)==ok,(command,child.returncode,child.stdout,child.stderr)
        return child

    def inspect(root,expected):
        path=root/'build/artifact';metadata=json.loads((path/'RELEASE.json').read_text(encoding='utf-8'))
        assert metadata['source_commit']==expected and metadata['status']=='local-candidate-release-validation-pending'
        for name,digest in json.loads((path/'SHA256SUMS.json').read_text()).items():
            assert hashlib.sha256((path/name).read_bytes()).hexdigest()==digest
        archive=path/metadata['archive']
        with zipfile.ZipFile(archive) as bundle:
            assert {'tools/validate.py','tools/release.py','RELEASES.md','CHANGELOG.md'}<=set(bundle.namelist())
            assert bundle.read('VERSION').decode().strip()==metadata['version']
        return metadata

    def mark(name):cases.append(dict(case=name,result='pass'));print(name+': pass',flush=True)

    root=fixture('clean');head=git(root,'rev-parse','HEAD');invoke(root);inspect(root,head)
    # Re-execute the delivered command outside the Git checkout itself.
    child=subprocess.run([sys.executable,str(root/'build/validation/tools/validate.py')],capture_output=True)
    assert child.returncode==0;mark('clean exact-commit package, hashes and delivered validator')

    root=fixture('dirty-tracked');(root/'README.md').write_text('uncommitted change\n');invoke(root,False)
    assert not (root/'build/artifact').exists() and not (root/'build/validation').exists();mark('dirty tracked source refused before output creation')

    root=fixture('dirty-untracked');(root/'unknown.txt').write_text('untracked\n');invoke(root,False)
    assert not (root/'build/artifact').exists();mark('untracked source refused')

    root=fixture('ignored-extras');head=git(root,'rev-parse','HEAD')
    with (root/'.git/info/exclude').open('a') as stream:stream.write('\n/examples/local-only.dat\n')
    (root/'examples/local-only.dat').write_text('must not ship\n');invoke(root);metadata=inspect(root,head)
    with zipfile.ZipFile(root/'build/artifact'/metadata['archive']) as bundle:assert 'examples/local-only.dat' not in bundle.namelist()
    mark('ignored example output excluded from package')

    for name,occupied in [('occupied-output','artifact'),('occupied-stage','validation')]:
        root=fixture(name);path=root/'build'/occupied;path.mkdir(parents=True);(path/'keep').write_text('preserve\n')
        invoke(root,False);assert (path/'keep').read_text()=='preserve\n';assert not (root/'build'/('validation' if occupied=='artifact' else 'artifact')).exists();mark(name+' preserved')

    root=fixture('bad-catalog');data=json.loads((root/'visualizers.json').read_text());data['schema_version']=999
    (root/'visualizers.json').write_text(json.dumps(data));commit(root);invoke(root,False)
    assert (root/'build/artifact/validation.log').is_file() and not (root/'build/artifact/RELEASE.json').exists();mark('invalid committed catalog gets no success metadata')

    root=fixture('head-moves');version=(root/'VERSION').read_text().strip()
    # This injection belongs only to the isolated fixture. Advance its actual
    # source repo during snapshot validation, after the tool captured HEAD.
    injection='''
import os as _os, pathlib as _pathlib, subprocess as _subprocess
_repo=_pathlib.Path(_os.environ['CATALOG_RELEASE_TEST_REPO'])
(_repo/'VERSION').write_text('9.9.9\\n')
(_repo/'README.md').write_text('new HEAD, not the packaged snapshot\\n')
_subprocess.run(['git','-C',str(_repo),'add','VERSION','README.md'],check=True)
_subprocess.run(['git','-C',str(_repo),'-c','user.name=Catalog test','-c','user.email=test@invalid','commit','--quiet','-m','advance during validation'],check=True)
'''
    with (root/'tools/validate.py').open('a',encoding='utf-8') as stream:stream.write(injection)
    head=commit(root);env=dict(os.environ,CATALOG_RELEASE_TEST_REPO=str(root));invoke(root,env=env)
    metadata=inspect(root,head);assert git(root,'rev-parse','HEAD')!=head and metadata['version']==version
    mark('concurrent HEAD and VERSION change cannot relabel snapshot')

    (output/'result.json').write_text(json.dumps(dict(result='pass',cases=cases,scope='actual isolated Git archives; source/JSON only'),indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':main()
