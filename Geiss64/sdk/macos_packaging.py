"""Owner-local arm64 Mac payload relocation, signing and dependency audits.

Only fresh staging copies are changed. Ad-hoc signatures establish local code
integrity; they are not Developer ID signing or Apple notarization.
"""
from pathlib import Path
import json
import os
import platform
import plistlib
import subprocess
from release_guard import sha256
from unix_packaging import tree, record


def macos_target():
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        raise RuntimeError('This Mac builder requires an arm64 macOS host; Intel Mac validation is pending')


def macos_runtime(sdk, host, native, dawn, sdl):
    macos_target()
    sdk,host,native,dawn,sdl=map(Path,(sdk,host,native,dawn,sdl))
    pin=json.loads((sdk/'dawn/dependencies.lock.json').read_text())
    provenance=json.loads((dawn/'build-provenance.json').read_text())
    if (provenance['source_revision']!=pin['source_revision'] or provenance['system']!='Darwin'
            or provenance['machine']!='arm64' or provenance['options'].get('DAWN_BUILD_NODE_BINDINGS')!='OFF'
            or provenance['options'].get('DAWN_BUILD_MONOLITHIC_LIBRARY')!='SHARED'):
        raise RuntimeError('Expected pinned shared arm64 Dawn provenance without Node')
    library=dawn/'lib/libwebgpu_dawn.dylib'
    hashes={item['path']:item['sha256'] for item in provenance['artifacts']}
    if sha256(library)!=hashes.get('lib/libwebgpu_dawn.dylib'):
        raise RuntimeError('Dawn library differs from build provenance')
    if sdl.name!='libSDL2-2.0.0.dylib' or not sdl.is_file():
        raise RuntimeError('Supply the built SDL2 arm64 shared library')
    sdl_provenance=sdl.parent.parent/'share/vlc-visualizers/SDL2-build-provenance.json'
    if not sdl_provenance.is_file():
        raise RuntimeError('SDL build provenance is missing; build/verify this SDK with tools/build_macos_sdl.py')
    sdl_input=json.loads(sdl_provenance.read_text())
    if (sdl_input.get('system')!='Darwin' or sdl_input.get('machine')!='arm64'
            or sdl_input.get('version')!=pin['SDL2']['version']
            or sdl_input.get('source_sha256')!=pin['SDL2']['source_sha256']
            or sdl_input.get('artifact',{}).get('path')!='lib/'+sdl.name
            or sdl_input['artifact'].get('sha256')!=sha256(sdl)):
        raise RuntimeError('SDL library/source differs from verified arm64 build provenance')
    plan={name:host/name for name in ('Visualizers','VisualizersCtl')}
    plan.update({'native/libwebgpu_dawn.dylib':library,'native/'+sdl.name:sdl,
                 'native/build-provenance.json':dawn/'build-provenance.json',
                 'native/dependencies.lock.json':sdk/'dawn/dependencies.lock.json',
                 'native/SDL2-build-provenance.json':sdl_provenance})
    tree(plan,native/'VisualizerNative.app','native/VisualizerNative.app')
    tree(plan,sdk/'licenses','licenses/runtime')
    tree(plan,dawn/'share/dawn/notices','licenses/runtime/dawn')
    return plan


def macho_info(path):
    with Path(path).open('rb') as stream:
        magic=stream.read(4)
    if magic not in (b'\xcf\xfa\xed\xfe',b'\xfe\xed\xfa\xcf',b'\xca\xfe\xba\xbe',b'\xbe\xba\xfe\xca'):
        return None
    arch=subprocess.check_output(['lipo','-archs',str(path)],text=True).strip()
    if arch!='arm64':
        raise RuntimeError('Expected arm64 Mach-O: '+str(path)+' ('+arch+')')
    lines=subprocess.check_output(['otool','-l',str(path)],text=True).splitlines()
    dependencies=[];rpaths=[];identity=None;minimum=None;sdk=None
    for index,line in enumerate(lines):
        command=line.strip()
        if command in ('cmd LC_LOAD_DYLIB','cmd LC_LOAD_WEAK_DYLIB','cmd LC_REEXPORT_DYLIB','cmd LC_LOAD_UPWARD_DYLIB','cmd LC_ID_DYLIB'):
            value=lines[index+2].strip().removeprefix('name ').rsplit(' (offset ',1)[0]
            if command=='cmd LC_ID_DYLIB':identity=value
            else:dependencies.append(value)
        elif command=='cmd LC_RPATH':
            rpaths.append(lines[index+2].strip().removeprefix('path ').rsplit(' (offset ',1)[0])
        elif command=='cmd LC_BUILD_VERSION':
            for value in lines[index+2:index+6]:
                fields=value.split()
                if fields and fields[0]=='minos':minimum=fields[1]
                if fields and fields[0]=='sdk':sdk=fields[1]
        elif command=='cmd LC_VERSION_MIN_MACOSX':
            minimum=lines[index+2].split()[1];sdk=lines[index+3].split()[1]
    return dict(architecture=arch,needed=dependencies,search_paths=rpaths,install_name=identity,
                minimum_macos=minimum,build_sdk=sdk)


def macho_files(root):
    result=[]
    for path in sorted(Path(root).rglob('*')):
        if path.is_file() and not path.is_symlink():
            info=macho_info(path)
            if info is not None:result.append((path,info))
    return result


def relocate_and_sign(suite):
    """Require a caller-owned fresh staging directory; never modify input SDKs."""
    macos_target();suite=Path(suite).resolve()
    before={}
    for file,info in macho_files(suite):
        relative=file.relative_to(suite).as_posix();before[relative]=dict(sha256=sha256(file),**info)
        for dependency in info['needed']:
            name=Path(dependency).name
            if name.startswith(('libwebgpu_dawn','libSDL2')):
                target=suite/'native'/name
            elif name.startswith('libprojectM'):
                target=file.parent/name
            else:
                continue
            if not target.is_file() or not target.resolve().is_relative_to(suite):
                raise RuntimeError('Missing or escaping bundled library: '+str(target))
            after='@loader_path/'+os.path.relpath(target,file.parent)
            if dependency!=after:
                subprocess.run(['install_name_tool','-change',dependency,after,str(file)],check=True,capture_output=True)
        for path in info['search_paths']:
            subprocess.run(['install_name_tool','-delete_rpath',path,str(file)],check=True,capture_output=True)
        if info['install_name']:
            subprocess.run(['install_name_tool','-id','@rpath/'+file.name,str(file)],check=True,capture_output=True)
        subprocess.run(['codesign','--force','--sign','-',str(file)],check=True,capture_output=True)
    minimum=max((row['minimum_macos'] for row in before.values() if row['minimum_macos']),
                key=lambda value:tuple(map(int,value.split('.'))),default=None)
    # Bundle requirements must reflect all packaged dependencies, including
    # modules loaded after startup. Change only our fresh staging copies.
    for app in sorted(suite.rglob('*.app'),key=lambda p:len(p.parts),reverse=True):
        if not minimum:raise RuntimeError('Cannot determine staged Mac deployment requirement')
        plist=app/'Contents/Info.plist';info=plistlib.loads(plist.read_bytes())
        info['LSMinimumSystemVersion']=minimum
        plist.write_bytes(plistlib.dumps(info,sort_keys=False))
        subprocess.run(['codesign','--force','--sign','-',str(app)],check=True,capture_output=True)
    return before


def audit_macos(suite, version=None):
    macos_target();suite=Path(suite).resolve();binaries={};vlc=None
    forbidden={'.node','.asar','.pak','.js','.cjs','.mjs','.html','.css','.o','.a'}
    for file in sorted(suite.rglob('*')):
        if not file.is_file() and not file.is_symlink():continue
        relative=file.relative_to(suite).as_posix()
        if relative=='vlc-plugins/libvlc_visualizers_plugin.dylib' and b'menu-control-command.json' in file.read_bytes():
            raise RuntimeError('AppKit test-control module must never enter a release payload')
        if any(p.lower() in ('browser','node_modules','electron') for p in file.relative_to(suite).parts) or file.suffix.lower() in forbidden:
            raise RuntimeError('Non-native release resource: '+relative)
        if file.is_symlink():
            record(file,suite)
            if not file.exists():raise RuntimeError('Broken payload symlink: '+relative)
            continue
        info=macho_info(file)
        if info is None:continue
        if info['search_paths']:
            raise RuntimeError('Unexpected Mach-O search path: '+relative)
        resolved={}
        for dependency in info['needed']:
            if dependency.startswith('@loader_path/'):
                target=(file.parent/dependency.removeprefix('@loader_path/')).resolve()
                if not target.is_relative_to(suite) or not target.is_file():
                    raise RuntimeError('Missing/escaping Mach-O dependency: '+relative+' '+dependency)
                resolved[dependency]=target.relative_to(suite).as_posix()
            elif dependency.startswith(('/usr/lib/','/System/Library/')):
                # macOS system dylibs may exist only in the shared cache.
                resolved[dependency]='macOS system framework/shared cache'
            elif relative=='vlc-plugins/libvlc_visualizers_plugin.dylib' and dependency=='@rpath/libvlccore.dylib':
                core=Path('/Applications/VLC.app/Contents/MacOS/lib/libvlccore.dylib')
                if not core.is_file():raise RuntimeError('Install the matching arm64 VLC application')
                if macho_info(core) is None:raise RuntimeError('Invalid VLC core')
                vlc=dict(path=str(core),sha256=sha256(core))
                resolved[dependency]='VLC process supplies its already-loaded core; '+str(core)
            else:
                raise RuntimeError('Development/unresolved Mach-O dependency: '+relative+' '+dependency)
        subprocess.run(['codesign','--verify','--strict',str(file)],check=True,capture_output=True)
        info.update(resolved=resolved,sha256=sha256(file),signature='ad-hoc, verified locally')
        binaries[relative]=info
    minimum=max((v['minimum_macos'] for v in binaries.values() if v['minimum_macos']),
                key=lambda value:tuple(map(int,value.split('.'))),default=None)
    apps=[]
    for app in sorted(suite.rglob('*.app')):
        info=plistlib.loads((app/'Contents/Info.plist').read_bytes())
        if version is not None:
            numeric=version.split('-',1)[0]
            if info.get('CFBundleShortVersionString')!=numeric or info.get('CFBundleVersion')!=numeric:
                raise RuntimeError('App bundle version differs from candidate: '+str(app))
        if info.get('CFBundleIdentifier')!='org.vlcvisualizers.native' or not info.get('NSMicrophoneUsageDescription'):
            raise RuntimeError('Unexpected production app identity/recording usage description')
        if info.get('LSMinimumSystemVersion')!=minimum:
            raise RuntimeError('App minimum macOS differs from packaged binary/dependency requirements')
        subprocess.run(['codesign','--verify','--deep','--strict',str(app)],check=True,capture_output=True)
        apps.append(app.relative_to(suite).as_posix())
    if len(list(suite.rglob('libwebgpu_dawn.dylib')))!=1:
        raise RuntimeError('Expected one shared Dawn library per package')
    return dict(schema_version=1,platform='macos',architecture='arm64',minimum_macos=minimum,browser_included=False,
                binaries=binaries,bundles=apps,vlc_prerequisite=vlc,
                limits='Static arm64 dependencies and local ad-hoc signature integrity. Not Developer ID signing/notarization; audio permissions, actual VLC and GPU windows require separate checks.')


def install_notes(version):
    return ('VLC Visualizers '+version+' — macOS arm64 development candidate\n\n'
            'Run ./install.sh without sudo; use --prefix PATH for a custom suite.\n'
            'Combined defaults include Prism Flow, MilkDrop/projectM, Rhythm Terrain, Beach, Objects, UnderSea and CoralCanyon.\n'
            'Use --without-geiss, --without-milkdrop, --without-terrain, --without-beach, --without-objects, --without-undersea or --without-coralcanyon.\n'
            'Renderer archives support --mode vlc to add only the plugin to an existing addon.\n'
            'Use the same --prefix with ./uninstall.sh. Close players/visualizers first.\n'
            'Uninstall the previous component before upgrading. Modified files are retained.\n\n'
            'VLC: launch VlcVisualizerLauncher; --scan-visualizers refreshes its catalog.\n'
            'The signed system VLC app is unchanged. Matching arm64 VLC 3 is required.\n'
            'Standalone: ./Visualizers --visualizer ID [--line-in default|ID|exact-label].\n'
            'Allow microphone access for VLC Visualizers when macOS requests it.\n'
            'Command-L/Ctrl-L reselects input; Escape/right-click closes the visualizer.\n'
            'Double-click toggles fullscreen. Audio is processed locally and is not saved.\n\n'
            'Copy a scene INI from its plugin assets into ~/Library/Application Support/VLCVisualizers/profiles\n'
            'and reopen to apply your changes.\n\n'
            'HDR is Experimental, opt-in and off by default. Physical HDR is unverified.\n'
            'Only Apple Silicon is validated. See the audit minimum_macos value\n'
            'for the actual OS floor.\n'
            'Older macOS and Intel Mac support remain pending.\n'
            'See dependency-audit.json for exact dependencies and local signature checks.\n'
            'These ad-hoc signed archives are not Developer ID signed or notarized.\n'
            'Final-source installation, player, capture and stability QA remain release gates.\n')
