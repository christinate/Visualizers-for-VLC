"""Exercise actual native schema-2 archives on an owner-local Linux/Mac machine."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import shutil
import subprocess
import tarfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archives',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();output=args.output.resolve();output.mkdir(parents=True)
    mac=platform.system()=='Darwin'
    native=Path('native/VisualizerNative.app/Contents/MacOS/VisualizerNative' if mac else 'native/VisualizerNative')
    dawn=Path('native/libwebgpu_dawn.dylib' if mac else 'native/libwebgpu_dawn.so')
    packages={};checks=[]
    for archive in args.archives.glob('*.tar.gz'):
        with tarfile.open(archive) as stream:
            members=stream.getmembers();top={Path(m.name).parts[0] for m in members}
            assert len(top)==1
            for item in members:
                assert (output/item.name).resolve().is_relative_to(output)
                assert item.isfile() or item.isdir() or item.issym(),item.name
                if item.issym():assert not Path(item.linkname).is_absolute()
            stream.extractall(output,filter='data')
        root=output/next(iter(top));metadata=json.loads((root/'native-payload.json').read_text())
        packages[metadata['package_id']]=root
    assert set(packages)=={'addon','geiss','milkdrop','terrain'}
    def run(*command,ok=True):
        result=subprocess.run([str(x) for x in command],capture_output=True,text=True,timeout=90)
        assert (result.returncode==0)==ok,(command,result.returncode,result.stdout,result.stderr)
        return result
    def digest(path):
        if path.is_symlink():return ('symlink',os.readlink(path))
        with path.open('rb') as stream:return ('file',hashlib.file_digest(stream,'sha256').hexdigest(),path.stat().st_mode&0o777)
    def snapshot(root):return {p.relative_to(root).as_posix():digest(p) for p in root.rglob('*') if p.is_file() or p.is_symlink()}
    def scan(root,ctl):return json.loads(run(ctl,'scan','--suite',root).stdout)
    def mark(test,**details):checks.append(dict(test=test,result='pass',**details));print(test,details,flush=True)
    combined=packages['addon'];bootstrap=combined/'.installer/VisualizersCtl'
    for mask in range(8):
        suite=output/('combined-'+str(mask));suite.mkdir();original=b'original user readme\n';(suite/'README.md').write_bytes(original)
        flags=[];expected=[]
        for i,identifier in enumerate(('geiss','milkdrop','terrain')):
            if mask&(1<<i):expected.append(identifier)
            else:flags.append('--without-'+identifier)
        run(combined/'install.sh','--prefix',suite,*flags)
        result=scan(suite,bootstrap);assert [p['id'] for p in result['visualizers']]==sorted(expected) and not result['rejected']
        assert len(list(suite.rglob(dawn.name)))==2 # live + owned snapshot
        assert (suite/native).is_file()
        if mask==7:
            changed=suite/'plugins/geiss/manifest.json';saved=changed.read_bytes();changed.write_bytes(saved+b' ')
            before=snapshot(suite);run(combined/'uninstall.sh','--prefix',suite,ok=False)
            assert snapshot(suite)==before,'Whole-package refusal changed an earlier component'
            changed.write_bytes(saved);mark('whole-package modified-file preflight')
        run(combined/'uninstall.sh','--prefix',suite)
        assert (suite/'README.md').read_bytes()==original
        assert not (suite/'Visualizers').exists() and not (suite/native).exists()
        for identifier in expected:assert not (suite/'plugins'/identifier/'manifest.json').exists()
        mark('combined opt-outs and exact original restoration',mask=mask)
        shutil.rmtree(suite)
    for identifier in ('geiss','milkdrop','terrain'):
        package=packages[identifier];suite=output/('standalone-'+identifier)
        run(package/'install.sh','--prefix',suite)
        result=scan(suite,package/'.installer/VisualizersCtl');assert [p['id'] for p in result['visualizers']]==[identifier]
        assert (suite/native).is_file() and not (suite/'VlcVisualizerLauncher').exists()
        before=snapshot(suite);run(package/'install.sh','--prefix',suite,ok=False);assert snapshot(suite)==before
        if identifier=='milkdrop':
            link=suite/('plugins/milkdrop/libprojectM-4.4.dylib' if mac else 'plugins/milkdrop/libprojectM-4.so.4');assert link.is_symlink()
            saved=os.readlink(link);link.unlink();link.symlink_to('missing-library.so');before=snapshot(suite)
            run(package/'uninstall.sh','--prefix',suite,ok=False);assert snapshot(suite)==before
            link.unlink();link.symlink_to(saved);mark('changed projectM SONAME symlink refusal')
        run(package/'uninstall.sh','--prefix',suite);assert not (suite/'Visualizers').exists()
        mark('independent standalone and occupied-component refusal',id=identifier);shutil.rmtree(suite)
    core=output/'shared-vlc';run(combined/'install.sh','--prefix',core,'--without-geiss','--without-milkdrop','--without-terrain')
    runtime_hash=digest(core/dawn)
    for identifier in ('geiss','milkdrop','terrain'):
        package=packages[identifier]
        run(package/'install.sh','--mode','vlc','--prefix',core)
        assert digest(core/dawn)==runtime_hash
        journal=json.loads((core/'.install'/identifier/'journal.json').read_text())
        assert journal['package_id']==identifier
        run(package/'uninstall.sh','--prefix',core)
        assert digest(core/dawn)==runtime_hash
        mark('plugin-only VLC install preserves shared runtime',id=identifier)
    run(packages['geiss']/'install.sh','--mode','vlc','--prefix',core)
    owned=core/'plugins/geiss/manifest.json';saved=owned.read_bytes()
    run(combined/'uninstall.sh','--prefix',core)
    assert owned.read_bytes()==saved and not (core/'VlcVisualizerLauncher').exists()
    run(packages['geiss']/'uninstall.sh','--prefix',core);assert not owned.exists()
    mark('addon removal retains independently owned plugin');shutil.rmtree(core)
    suite=output/'invalid-opt-out'
    run(bootstrap,'install','--suite',suite,'--source',combined,'--skip','geissbogus',ok=False)
    assert not suite.exists();mark('unknown opt-out rejected before destination creation')
    report=dict(result='pass',checks=checks,system=platform.system(),scope='actual native archive installation/scanning/ownership; no actual VLC playback, visualizer GPU or physical HDR claim')
    (output/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))


if __name__=='__main__':main()
