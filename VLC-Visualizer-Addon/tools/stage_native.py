"""Stage one fresh native Linux suite without changing the system VLC."""
from pathlib import Path
import argparse
import json
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'sdk'))
from release_guard import fresh_directory,candidate_version
from unix_packaging import linux_target,linux_runtime,linux_plugin,copy_plan,tree,audit_linux


def arguments(parser):
    parser.add_argument('--build-root',type=Path,required=True)
    parser.add_argument('--suite',type=Path,required=True)
    parser.add_argument('--native-build',type=Path)
    parser.add_argument('--host-build',type=Path)
    parser.add_argument('--dawn-install',type=Path)
    parser.add_argument('--geiss-module',type=Path)
    parser.add_argument('--terrain-module',type=Path)
    parser.add_argument('--beach-module',type=Path)
    parser.add_argument('--objects-module',type=Path)
    parser.add_argument('--undersea-module',type=Path)
    parser.add_argument('--coralcanyon-module',type=Path)
    parser.add_argument('--classic',type=Path)
    parser.add_argument('--milkdrop',type=Path)
    parser.add_argument('--projectm-library',type=Path)
    parser.add_argument('--vlc-module',type=Path)


def stage(args):
    linux_target();version=candidate_version(ROOT);build=args.build_root.resolve()
    host=args.host_build or build/'build-addon/native/native-runtime'
    native=args.native_build or build/'native-sdk-runtime'
    dawn=args.dawn_install or ROOT/'build/dawn-install'
    plan=linux_runtime(ROOT/'sdk',host,native,dawn)
    addon={'VlcVisualizerLauncher':host/'VlcVisualizerLauncher',
           'vlc-plugins/libvlc_visualizers_plugin.so':args.vlc_module or build/'build-addon/native/libvlc_visualizers_plugin.so'}
    for name in ('README.md','SECURITY.md','STABILITY.md','RELEASES.md'):
        addon[name]=ROOT/name
    for name in ('VLC-COPYING.LIB.txt','VLC-COPYING.txt','SDK-BSD-3-Clause.txt'):
        addon['licenses/addon/'+name]=ROOT/'licenses'/name
    terrain_default=build/'native-terrain-independent/native/dawn'
    terrain_module=args.terrain_module or terrain_default/'terrain_dawn.so'
    inputs={
      'geiss':('Geiss64',args.geiss_module or build/'build-geiss/native/dawn/prismflow_dawn.so',args.classic or build/'build-geiss/native/vis_geiss64.so',None),
      'terrain':('Terrain64',terrain_module,None,None),
      'beach':('Terrain64',args.beach_module or terrain_module.with_name('beach_dawn.so'),None,None),
      'objects':('Terrain64',args.objects_module or terrain_module.with_name('objects_dawn.so'),None,None),
      'undersea':('Terrain64',args.undersea_module or terrain_module.with_name('undersea_dawn.so'),None,None),
      'coralcanyon':('Terrain64',args.coralcanyon_module or terrain_module.with_name('coralcanyon_dawn.so'),None,None),
      'milkdrop':('MilkDrop2-64',args.milkdrop or build/'build-milkdrop/native/vis_milkdrop64.so',None,args.projectm_library or build/'build-milkdrop/native/projectm/src/libprojectM/libprojectM-4.so.4')}
    plugins={}
    for identifier,(name,module,classic,projectm) in inputs.items():
        project=ROOT.parent/name
        if candidate_version(project)!=version:raise RuntimeError('Project candidate versions differ')
        manifest_name='manifest-'+identifier+'.json' if identifier in ('beach','objects','undersea','coralcanyon') else 'manifest.json'
        plugins[identifier]=linux_plugin(project,module,classic,projectm,manifest_name=manifest_name)
    for source in [*plan.values(),*addon.values(),*(p for files,manifest in plugins.values() for p in files.values())]:
        if not source.exists() and not source.is_symlink():raise FileNotFoundError(source)
    suite=fresh_directory(args.suite);copy_plan(plan,suite);copy_plan(addon,suite)
    components={'runtime':list(plan),'addon':list(addon)}
    for identifier,(files,manifest) in plugins.items():
        folder=suite/'plugins'/identifier;copy_plan(files,folder)
        (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        components[identifier]=['plugins/'+identifier+'/'+name for name in [*files,'manifest.json']]
    result=audit_linux(suite)
    ctl=suite/'VisualizersCtl'
    import subprocess
    scan=json.loads(subprocess.check_output([str(ctl),'scan','--suite',str(suite)],text=True))
    if scan['rejected'] or {p['id'] for p in scan['visualizers']}!=set(inputs):raise RuntimeError('Native staged scan failed')
    (suite/'installed-visualizers.json').unlink();(suite/'.scan.lock').unlink()
    result.update(version=version,qa_status='development staging; real players, installer matrix and final source provenance pending')
    return suite,components,result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);arguments(parser);args=parser.parse_args()
    suite,components,result=stage(args)
    report=suite.parent/(suite.name+'-dependency-audit.json')
    with report.open('x',encoding='utf-8') as stream:stream.write(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(result='staged',suite=str(suite),audit=str(report),release_ready=False),indent=2))
