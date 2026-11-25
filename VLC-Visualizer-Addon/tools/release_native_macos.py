"""Prepare fresh arm64 Mac native suite/renderer archives without publishing."""
from pathlib import Path
import argparse
import json
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'sdk'))
from release_guard import fresh_directory,checksums,candidate_version
from unix_packaging import native_plugin,copy_plan,record,write_package,archive_package
from macos_packaging import macos_target,macos_runtime,relocate_and_sign,audit_macos,install_notes


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('build-root','suite','output','dawn-install','sdl-library'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();macos_target();version=candidate_version(ROOT)
    if args.suite.exists() or args.output.exists():
        raise FileExistsError('Use fresh candidate staging/output paths')
    build=args.build_root.resolve()
    runtime=macos_runtime(ROOT/'sdk',build/'build-addon/native/native-runtime',build/'build-addon/dawn',args.dawn_install,args.sdl_library)
    addon={'VlcVisualizerLauncher':build/'build-addon/native/native-runtime/VlcVisualizerLauncher',
           'vlc-plugins/libvlc_visualizers_plugin.dylib':build/'build-addon/native/libvlc_visualizers_plugin.dylib'}
    for name in ('README.md','SECURITY.md','STABILITY.md','RELEASES.md'):addon[name]=ROOT/name
    for name in ('VLC-COPYING.LIB.txt','VLC-COPYING.txt','SDK-BSD-3-Clause.txt'):
        addon['licenses/addon/'+name]=ROOT/'licenses'/name
    inputs={
        'geiss':('Geiss64',build/'build-addon/dawn/prismflow_dawn.so',build/'build-geiss/native/vis_geiss64.dylib',None),
        'terrain':('Terrain64',build/'build-addon/dawn/terrain_dawn.so',None,None),
        'beach':('Terrain64',build/'build-addon/dawn/beach_dawn.so',None,None),
        'objects':('Terrain64',build/'build-addon/dawn/objects_dawn.so',None,None),
        'undersea':('Terrain64',build/'build-addon/dawn/undersea_dawn.so',None,None),
        'coralcanyon':('Terrain64',build/'build-addon/dawn/coralcanyon_dawn.so',None,None),
        'milkdrop':('MilkDrop2-64',build/'build-milkdrop-version-fixed/native/vis_milkdrop64.dylib',None,
                    build/'build-milkdrop-version-fixed/native/projectm/src/libprojectM/libprojectM-4.4.dylib')}
    plugins={}
    for identifier,(name,module,classic,projectm) in inputs.items():
        project=ROOT.parent/name
        if candidate_version(project)!=version:raise RuntimeError('Candidate versions differ')
        manifest_name='manifest-'+identifier+'.json' if identifier in ('beach','objects','undersea','coralcanyon') else 'manifest.json'
        plugins[identifier]=native_plugin(project,module,classic,projectm,system='macos',architecture='arm64',
                                          manifest_name=manifest_name)
    for source in [*runtime.values(),*addon.values(),*(p for files,manifest in plugins.values() for p in files.values())]:
        if not source.exists() and not source.is_symlink():raise FileNotFoundError(source)
    suite=fresh_directory(args.suite);copy_plan(runtime,suite);copy_plan(addon,suite)
    components={'runtime':list(runtime),'addon':list(addon)}
    for identifier,(files,manifest) in plugins.items():
        package=suite/'plugins'/identifier;copy_plan(files,package)
        (package/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        components[identifier]=['plugins/'+identifier+'/'+name for name in [*files,'manifest.json']]
    before=relocate_and_sign(suite)
    # codesign can create the app resource seal. Include it in file ownership.
    app=suite/'native/VisualizerNative.app'
    components['runtime']=sorted(set(components['runtime'])|{p.relative_to(suite).as_posix() for p in app.rglob('*') if p.is_file() or p.is_symlink()})
    audit=audit_macos(suite,version)
    scan=json.loads(subprocess.check_output([str(suite/'VisualizersCtl'),'scan','--suite',str(suite)],text=True))
    if scan['rejected'] or {p['id'] for p in scan['visualizers']}!=set(inputs):
        raise RuntimeError('Staged Mac catalog failed')
    (suite/'installed-visualizers.json').unlink();(suite/'.scan.lock').unlink()
    output=fresh_directory(args.output)
    (output/'relocation-inputs.json').write_text(json.dumps(before,indent=2)+'\n')
    (output/'dependency-audit.json').write_text(json.dumps(audit,indent=2)+'\n')
    variants=[('VLC-Visualizer-Addon','addon','suite',tuple(components),'VLCVisualizers'),
              ('PrismFlow64','geiss','renderer',('runtime','geiss'),'PrismFlow64'),
              ('MilkDrop2-64','milkdrop','renderer',('runtime','milkdrop'),'MilkDrop2-64'),
              ('Terrain64','terrain','renderer',('runtime','terrain'),'Terrain64'),
              ('Beach','beach','renderer',('runtime','beach'),'Beach'),
              ('Objects','objects','renderer',('runtime','objects'),'Objects'),
              ('UnderSea','undersea','renderer',('runtime','undersea'),'UnderSea'),
              ('CoralCanyon','coralcanyon','renderer',('runtime','coralcanyon'),'CoralCanyon')]
    for name,owner,kind,selected,folder in variants:
        package=fresh_directory(output/('stage-'+name));rows={}
        for component in selected:
            plan={relative:suite/relative for relative in components[component]};copy_plan(plan,package)
            rows[component]=[record(package/relative,package) for relative in sorted(plan)]
        write_package(package,version,owner,kind,rows,'${HOME}/Library/Application Support/'+folder,
                      system='macos',architecture='arm64',vlc_prefix='${HOME}/Library/Application Support/VLCVisualizers',
                      install_notes=install_notes(version))
        package_audit=audit_macos(package,version)
        package_audit['relocation_inputs']={name:row for name,row in before.items() if name in package_audit['binaries']}
        (package/'dependency-audit.json').write_text(json.dumps(package_audit,indent=2)+'\n')
        archive=archive_package(package,output,name,version,system='macos',architecture='arm64')
        print(json.dumps(dict(archive=str(archive),bytes=archive.stat().st_size,release_ready=False)),flush=True)
        # Only delete the generated expanded variant inside this fresh output.
        import shutil
        assert package.resolve().is_relative_to(output.resolve()) and package.name.startswith('stage-')
        shutil.rmtree(package)
    checksums(output)


if __name__=='__main__':main()
