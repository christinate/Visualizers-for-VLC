"""Build independent Windows native visualizer installers from the pinned SDK."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from release_guard import candidate_version, fresh_directory, checksums, sha256
from native_packaging import windows_runtime, windows_plugin, copy_plan, audit_windows, file_records
ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-only',action='store_true')
    parser.add_argument('--stage-only',action='store_true')
    parser.add_argument('--build-root',type=Path,default=ROOT/'build/native-release')
    parser.add_argument('--host-build',type=Path)
    parser.add_argument('--native-build',type=Path)
    parser.add_argument('--module-build',type=Path)
    parser.add_argument('--scene',choices=('terrain','beach','objects','undersea','coralcanyon'),
                        help='Select a visualizer in the Terrain64 source tree')
    parser.add_argument('--legacy-build',type=Path)
    parser.add_argument('--bridge-root',type=Path,default=ROOT)
    parser.add_argument('--dawn-install',type=Path,default=ROOT/'sdk/build/dawn-install')
    parser.add_argument('--sdl-sdk',type=Path,default=ROOT/'sdk/build/sdl-sdk')
    parser.add_argument('--stage',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    for key,value in vars(args).items():
        if isinstance(value,Path):setattr(args,key,value.resolve())
    if sys.platform!='win32':parser.error('This entry point builds Windows installers')
    snapshot=json.loads((ROOT/'sdk/VERSION.json').read_text(encoding='utf-8'))
    for file,expected in snapshot['files'].items():
        if hashlib.sha256((ROOT/'sdk'/file).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=expected:
            raise RuntimeError('The pinned SDK snapshot was modified: '+file)
    version=candidate_version(ROOT)
    manifest_name='manifest-'+args.scene+'.json' if args.scene in ('beach','objects','undersea','coralcanyon') else 'manifest.json'
    manifest=json.loads((ROOT/manifest_name).read_text(encoding='utf-8'))
    identifier=manifest['id']
    if args.scene and identifier!=args.scene:parser.error('The requested scene is absent from this repository')
    name={'geiss':'PrismFlow64','milkdrop':'MilkDrop2-64','terrain':'Terrain64','beach':'Beach','objects':'Objects','undersea':'UnderSea','coralcanyon':'CoralCanyon'}[identifier]
    stage=args.stage or ROOT/('build/release-'+version+'-windows')
    output=args.output or ROOT/'releases'/version/'windows-x86_64'
    for path in (stage,output):
        if path.exists():raise FileExistsError('Choose a fresh candidate path: '+str(path))
    def run(command):subprocess.run([str(arg) for arg in command],cwd=ROOT,check=True)
    generator=os.environ.get('CMAKE_GENERATOR','Visual Studio 18 2026')
    if not args.package_only:
        pin=json.loads((ROOT/'sdk/dawn/dependencies.lock.json').read_text())
        run(['cmake','-S',ROOT,'-B',args.build_root,'-G',generator,'-A','x64','-DVIS_BUILD_DAWN_RUNTIME=ON',
             '-DDawn_DIR='+str(args.dawn_install/'lib/cmake/Dawn'),'-DSDL2_DIR='+str(args.sdl_sdk/('SDL2-'+pin['SDL2']['version'])/'cmake')])
        run(['cmake','--build',args.build_root,'--config','Release','--parallel','6'])
        for arch,platform in (('x86','Win32'),('x64','x64')):
            bridge_build=args.bridge_root/('build-bridge-'+arch)
            run(['cmake','-S',ROOT/'sdk/winamp','-B',bridge_build,'-G',generator,'-A',platform,'-DVISUALIZER_ID='+identifier])
            run(['cmake','--build',bridge_build,'--config','Release','--parallel','4','--target','winamp_bridge'])
    binary=args.build_root/'bin/Release'
    runtime=windows_runtime(ROOT/'sdk',args.host_build or binary,args.native_build or binary,args.dawn_install,args.sdl_sdk)
    plugin=windows_plugin(ROOT,args.legacy_build or binary,args.module_build or binary,manifest_name=manifest_name)
    bridge_name='vis_'+identifier+'_bridge.dll'
    bridges={arch:args.bridge_root/('build-bridge-'+arch)/'Release'/bridge_name for arch in ('x86','x64')}
    for file in bridges.values():
        if not file.is_file():raise FileNotFoundError(file)
    stage=fresh_directory(stage);payload=stage/'payload'
    copy_plan(runtime,payload/'runtime');copy_plan(plugin,payload/'plugins'/identifier)
    for arch,file in bridges.items():copy_plan({bridge_name:file},payload/'bridges'/arch)
    package=dict(schema_version=1,id=identifier,version=version,
                 runtime=file_records(payload/'runtime',source_prefix='runtime/'),
                 plugin=file_records(payload/'plugins'/identifier,'plugins/'+identifier+'/','plugins/'+identifier+'/'),
                 bridges={arch:dict(path='Plugins/'+bridge_name,source='bridges/'+arch+'/'+bridge_name,sha256=sha256(file)) for arch,file in bridges.items()})
    label=manifest['name']
    if any(c in label for c in '<>:"/\\|?*'):
        raise RuntimeError('Display name is not a valid shortcut filename')
    package['shortcuts']=[dict(path=label+suffix+'.lnk',source='shortcuts/'+label+suffix+'.lnk') for suffix in ('',' settings')]
    (payload/'renderer-payload.json').write_text(json.dumps(package,indent=2)+'\n',encoding='utf-8')
    audit=audit_windows(payload,machine_overrides={'bridges/x86/'+bridge_name:0x14c})
    if len(list(payload.rglob('webgpu_dawn.dll')))!=1:raise RuntimeError('Expected one native runtime')
    audit.update(version=version,id=identifier,payload_bytes=sum(p.stat().st_size for p in payload.rglob('*') if p.is_file()),
                 qa_status='standalone/VLC/Winamp installer, playback and final source provenance checks remain required')
    (stage/'dependency-audit.json').write_text(json.dumps(audit,indent=2)+'\n',encoding='utf-8')
    if args.stage_only:
        print(json.dumps(dict(result='staged',stage=str(stage),release_ready=False),indent=2));return
    output=fresh_directory(output);artifact=output/(name+'-'+version+'-setup.exe')
    nsis=Path(os.environ.get('MAKENSIS',r'C:\Program Files (x86)\NSIS\makensis.exe'))
    run([nsis,'/DVERSION='+version,'/DVIS_ID='+identifier,'/DDISPLAY_NAME='+manifest['name'],'/DPACKAGE_NAME='+name,
         '/DPAYLOAD_DIR='+str(payload.resolve()),'/DOUTPUT_FILE='+str(artifact.resolve()),ROOT/'sdk/renderer-installer.nsi'])
    portable=fresh_directory(stage/'portable');copy_plan(runtime,portable);copy_plan(plugin,portable/'plugins'/identifier)
    (portable/'README.txt').write_text('Standalone native visualizer\n\nRun Visualizers.exe --visualizer '+identifier+'\nUse --line-in default, a device ID or an exact input label at startup.\nCtrl-L reselects input. Escape/right-click closes the window.\nRun Visualizers.exe --settings '+identifier+' for options.\nHDR is Experimental and off by default.\n',encoding='utf-8')
    shutil.make_archive(str(output/(name+'-'+version+'-portable')),'zip',portable)
    shutil.make_archive(str(output/(name+'-'+version+'-payload')),'zip',payload)
    shutil.copy2(stage/'dependency-audit.json',output/'dependency-audit.json');checksums(output)
    print(json.dumps(dict(result='candidate-built',installer=str(artifact),sha256=sha256(artifact),bytes=artifact.stat().st_size,release_ready=False),indent=2))


if __name__=='__main__':main()
