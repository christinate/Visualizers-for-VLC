"""Package one independently built Linux x86_64 or macOS arm64 visualizer."""
from pathlib import Path
import argparse
import hashlib
import json
import platform
import subprocess
from release_guard import candidate_version,fresh_directory,checksums
from unix_packaging import linux_target,linux_runtime,linux_plugin,copy_plan,record,write_package,archive_package,audit_linux
from macos_packaging import macos_target,macos_runtime,relocate_and_sign,audit_macos,install_notes
ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host-build',type=Path,required=True)
    parser.add_argument('--native-build',type=Path,required=True)
    parser.add_argument('--module',type=Path,required=True)
    parser.add_argument('--scene',choices=('terrain','beach','objects','undersea','coralcanyon'),
                        help='Select a visualizer in the Terrain64 source tree')
    parser.add_argument('--classic',type=Path)
    parser.add_argument('--projectm-library',type=Path)
    parser.add_argument('--dawn-install',type=Path,default=ROOT/'sdk/build/dawn-install')
    parser.add_argument('--sdl-library',type=Path,help='Required built SDL2 dylib on macOS')
    parser.add_argument('--stage',type=Path)
    parser.add_argument('--output',type=Path)
    args=parser.parse_args();mac=platform.system()=='Darwin'
    (macos_target if mac else linux_target)();version=candidate_version(ROOT)
    target=dict(system='macos',architecture='arm64') if mac else dict(system='linux',architecture='x86_64')
    index=json.loads((ROOT/'sdk/VERSION.json').read_text())
    for name,expected in index['files'].items():
        if hashlib.sha256((ROOT/'sdk'/name).read_bytes().replace(b'\r\n',b'\n')).hexdigest()!=expected:raise RuntimeError('Modified source SDK: '+name)
    manifest_name='manifest-'+args.scene+'.json' if args.scene in ('beach','objects','undersea','coralcanyon') else 'manifest.json'
    source=json.loads((ROOT/manifest_name).read_text(encoding='utf-8'));identifier=source['id']
    if args.scene and identifier!=args.scene:parser.error('The requested scene is absent from this repository')
    name={'geiss':'PrismFlow64','milkdrop':'MilkDrop2-64','terrain':'Terrain64','beach':'Beach','objects':'Objects','undersea':'UnderSea','coralcanyon':'CoralCanyon'}[identifier]
    if mac:
        if not args.sdl_library:raise RuntimeError('Mac packaging requires --sdl-library')
        from unix_packaging import native_plugin
        runtime=macos_runtime(ROOT/'sdk',args.host_build,args.native_build,args.dawn_install,args.sdl_library)
        plugin,manifest=native_plugin(ROOT,args.module,args.classic,args.projectm_library,manifest_name=manifest_name,**target)
    else:
        runtime=linux_runtime(ROOT/'sdk',args.host_build,args.native_build,args.dawn_install)
        plugin,manifest=linux_plugin(ROOT,args.module,args.classic,args.projectm_library,manifest_name=manifest_name)
    stage=args.stage or ROOT/('build/release-'+version+'-'+target['system'])
    output=args.output or ROOT/'releases'/version/(target['system']+'-'+target['architecture'])
    if stage.exists() or output.exists():raise FileExistsError('Use fresh candidate staging/output paths')
    stage=fresh_directory(stage);copy_plan(runtime,stage);copy_plan(plugin,stage/'plugins'/identifier)
    manifest_file=stage/'plugins'/identifier/'manifest.json';manifest_file.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    if mac:
        relocation_inputs=relocate_and_sign(stage)
        runtime.update({p.relative_to(stage).as_posix():p for p in (stage/'native/VisualizerNative.app').rglob('*') if p.is_file() or p.is_symlink()})
    components={'runtime':[record(stage/path,stage) for path in sorted(runtime)],
                identifier:[record(stage/'plugins'/identifier/path,stage) for path in sorted([*plugin,'manifest.json'])]}
    options=dict(**target,vlc_prefix='${HOME}/Library/Application Support/VLCVisualizers',install_notes=install_notes(version)) if mac else {}
    default='${HOME}/Library/Application Support/'+name if mac else '${XDG_DATA_HOME:-${HOME}/.local/share}/'+name.lower()
    write_package(stage,version,identifier,'renderer',components,default,**options)
    audit=audit_macos(stage,version) if mac else audit_linux(stage)
    if mac:audit['relocation_inputs']=relocation_inputs
    scan=json.loads(subprocess.check_output([str(stage/'VisualizersCtl'),'scan','--suite',str(stage)],text=True))
    if scan['rejected'] or [p['id'] for p in scan['visualizers']]!=[identifier]:raise RuntimeError('Independent package scan failed')
    (stage/'installed-visualizers.json').unlink();(stage/'.scan.lock').unlink()
    output=fresh_directory(output)
    (stage/'dependency-audit.json').write_text(json.dumps(audit,indent=2)+'\n',encoding='utf-8')
    archive=archive_package(stage,output,name,version,**target);checksums(output)
    print(json.dumps(dict(result='candidate-built',archive=str(archive),bytes=archive.stat().st_size,release_ready=False),indent=2))


if __name__=='__main__':main()
