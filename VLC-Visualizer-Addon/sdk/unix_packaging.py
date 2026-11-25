"""Native Linux payload plans and user-directory package archives.

No browser cache is consulted. macOS staging/signing remains a deferred gate.
"""
from pathlib import Path
import hashlib
import json
import os
import platform
import re
import shutil
import stat
import subprocess
import tarfile
from release_guard import fresh_directory, sha256


def linux_target():
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise RuntimeError('This native package builder currently validates Linux x86_64; macOS packaging/signing is deferred in M09')


def tree(plan, source, prefix):
    source=Path(source)
    if not source.is_dir():raise FileNotFoundError(source)
    for path in sorted(source.rglob('*')):
        if path.is_file() or path.is_symlink():plan[prefix+'/'+path.relative_to(source).as_posix()]=path


def copy_plan(plan, destination):
    destination=Path(destination)
    for relative,source in sorted(plan.items()):
        relative=Path(relative)
        if relative.is_absolute() or '..' in relative.parts:raise ValueError('Invalid native payload path')
        source=Path(source);target=destination/relative
        if not source.exists() and not source.is_symlink():raise FileNotFoundError(source)
        if target.exists() or target.is_symlink():raise FileExistsError(target)
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target,follow_symlinks=False)
        if not target.is_symlink():
            # Archive extraction commonly removes group/other write bits.
            # Record canonical release permissions rather than build umasks.
            target.chmod(0o755 if source.stat().st_mode & 0o111 else 0o644)


def linux_runtime(sdk, host_build, native_build, dawn_install):
    linux_target()
    sdk,host_build,native_build,dawn_install=map(Path,(sdk,host_build,native_build,dawn_install))
    pin=json.loads((sdk/'dawn/dependencies.lock.json').read_text())
    provenance=json.loads((dawn_install/'build-provenance.json').read_text())
    if provenance['source_revision']!=pin['source_revision'] or provenance['system']!='Linux' or provenance['machine']!='x86_64':
        raise RuntimeError('Pinned source-built Linux Dawn provenance is required')
    if provenance['options'].get('DAWN_BUILD_NODE_BINDINGS')!='OFF' or provenance['options'].get('DAWN_BUILD_MONOLITHIC_LIBRARY')!='SHARED':
        raise RuntimeError('Expected one shared Dawn library without Node bindings')
    patch=pin['linux_source_patch']
    patch_file=sdk/'dawn'/patch['path']
    if provenance.get('source_patches')!=[patch] or sha256(patch_file)!=patch['sha256']:
        raise RuntimeError('Linux Dawn provenance must include the pinned Wayland presentation patch')
    library=dawn_install/'lib/libwebgpu_dawn.so'
    recorded={item['path']:item['sha256'] for item in provenance['artifacts']}
    if sha256(library)!=recorded.get('lib/libwebgpu_dawn.so'):raise RuntimeError('Dawn library differs from build provenance')
    plan={name:host_build/name for name in ('Visualizers','VisualizersCtl')}
    plan.update({'native/VisualizerNative':native_build/'VisualizerNative','native/libwebgpu_dawn.so':library,
                 'native/build-provenance.json':dawn_install/'build-provenance.json',
                 'native/dependencies.lock.json':sdk/'dawn/dependencies.lock.json',
                 'native/wayland-extended-srgb.patch':patch_file})
    tree(plan,sdk/'licenses','licenses/runtime')
    tree(plan,dawn_install/'share/dawn/notices','licenses/runtime/dawn')
    return plan


def library_chain(plan, source, prefix):
    source=Path(source);base=source.parent.resolve();seen=set()
    while True:
        if source.name in seen or not source.resolve().is_relative_to(base):raise RuntimeError('Library symlink cycle/escape')
        seen.add(source.name);plan[prefix+'/'+source.name]=source
        if not source.is_symlink():break
        target=os.readlink(source)
        if Path(target).is_absolute() or Path(target).parent!=Path('.'):raise RuntimeError('Library links must stay beside their target')
        source=source.parent/target


def linux_plugin(project, module, classic=None, projectm=None, *, manifest_name='manifest.json'):
    linux_target()
    return native_plugin(project,module,classic,projectm,manifest_name=manifest_name)


def native_plugin(project, module, classic=None, projectm=None, *, system='linux', architecture='x86_64',
                  manifest_name='manifest.json'):
    """Shared payload layout; callers validate the actual target and binaries."""
    project=Path(project);module=Path(module)
    source=json.loads((project/manifest_name).read_text(encoding='utf-8'))
    identifier=source['id'];version=(project/'VERSION').read_text().strip()
    if source['version']!=version:raise RuntimeError('Renderer manifest/version mismatch')
    plan={module.name:module,'LICENSE.txt':project/'LICENSE'}
    tree(plan,project/'licenses','licenses')
    if identifier in ('geiss','terrain','beach','objects','undersea','coralcanyon'):
        pin=json.loads((project/'sdk/dawn/dependencies.lock.json').read_text())
        if source['adapter']!='dawn' or source['api_version']!=4 or source['dawn_revision']!=pin['source_revision']:raise RuntimeError('Expected a pinned API 4 renderer manifest')
        manifest=dict(source,platform=system,architecture=architecture,entrypoint=module.name)
        scene='undersea' if identifier=='coralcanyon' else identifier
        assets=project/('native/prismflow/shaders' if identifier=='geiss' else 'assets/'+scene)
        for file in sorted(assets.iterdir()):
            if file.is_file() and (file.suffix in ('.wgsl','.json') or file.name==identifier+'.ini'):plan['assets/'+file.name]=file
        if identifier!='geiss' and 'assets/'+identifier+'.ini' not in plan:
            raise RuntimeError('The packaged scene INI is missing')
        if identifier=='geiss':
            if classic is None:raise RuntimeError('Prism Flow package requires its Classic compatibility module')
            classic=Path(classic);plan[classic.name]=classic;manifest['classic']={'adapter':'opengl','entrypoint':classic.name}
            for name in ('LICENSE.txt','NOTICE.txt'):plan['licenses/prismflow/'+name]=project/'native/prismflow'/name
    elif identifier=='milkdrop':
        manifest=dict(schema_version=2,api_version=2,id=identifier,name='MilkDrop 2 (projectM native)',version=version,
                      platform=system,architecture=architecture,adapter='opengl',entrypoint=module.name,
                      settings=dict(display=True,fullscreen=True,hdr_output='sdr',hdr_experimental=True))
        tree(plan,project/'assets','assets')
        if projectm is None:raise RuntimeError('Native MilkDrop requires the projectM SONAME library')
        library_chain(plan,projectm,'.')
        plan={name.removeprefix('./'):path for name,path in plan.items()}
    else:raise RuntimeError('Unknown built-in renderer')
    return plan,manifest


def record(path, root):
    relative=path.relative_to(root).as_posix()
    if path.is_symlink():
        target=os.readlink(path)
        if Path(target).is_absolute() or not path.resolve().is_relative_to(root.resolve()):raise RuntimeError('Payload symlink escapes package')
        return dict(path=relative,kind='symlink',target=target,
                    sha256=hashlib.sha256(b'symlink\0'+os.fsencode(target)).hexdigest())
    permissions=stat.S_IMODE(path.stat().st_mode)
    if permissions & ~0o777:raise RuntimeError('Special file permissions are not supported in native payloads')
    return dict(path=relative,kind='file',bytes=path.stat().st_size,mode=permissions,sha256=sha256(path))


def records(root):
    root=Path(root)
    return [record(path,root) for path in sorted(root.rglob('*')) if path.is_file() or path.is_symlink()]


def elf_info(path):
    with Path(path).open('rb') as stream:header=stream.read(20)
    if len(header)<20 or header[:4]!=b'\x7fELF':return None
    if header[4]!=2 or header[5]!=1 or int.from_bytes(header[18:20],'little')!=62:raise RuntimeError('Wrong native ELF architecture: '+str(path))
    output=subprocess.check_output(['readelf','-d',str(path)],text=True)
    needed=re.findall(r'\(NEEDED\).*?\[(.*?)\]',output)
    search=re.findall(r'\((?:RUNPATH|RPATH)\).*?\[(.*?)\]',output)
    if any('/' in name for name in needed):raise RuntimeError('Absolute/development ELF dependency: '+str(path))
    for value in search:
        if any(not entry.startswith('$ORIGIN') for entry in value.split(':')):raise RuntimeError('Nonrelocatable ELF search path: '+str(path)+' '+value)
    return dict(needed=needed,search_paths=search)


def audit_linux(suite):
    linux_target();suite=Path(suite).resolve();binaries={};external={}
    forbidden={'.node','.asar','.pak','.js','.cjs','.mjs','.html','.css','.o','.a'}
    for file in sorted(suite.rglob('*')):
        if not file.is_file() and not file.is_symlink():continue
        relative=file.relative_to(suite).as_posix()
        if any(part.lower() in ('browser','node_modules','electron') for part in file.relative_to(suite).parts) or file.suffix.lower() in forbidden:raise RuntimeError('Non-native release resource: '+relative)
        if file.is_symlink():record(file,suite);continue
        info=elf_info(file)
        if info is None:continue
        for search in info['search_paths']:
            for entry in search.split(':'):
                resolved=Path(entry.replace('$ORIGIN',str(file.parent))).resolve()
                if not resolved.is_relative_to(suite):raise RuntimeError('ELF search path escapes suite: '+relative)
        env=os.environ.copy();env.pop('LD_LIBRARY_PATH',None);env.pop('LD_PRELOAD',None)
        result=subprocess.run(['ldd',str(file)],env=env,capture_output=True,text=True)
        if result.returncode or 'not found' in result.stdout:raise RuntimeError('Unresolved ELF dependency for '+relative+': '+result.stdout+result.stderr)
        resolved={}
        for line in result.stdout.splitlines():
            match=re.match(r'\s*(\S+)\s+=>\s+(/\S+)\s+\(',line)
            if not match:continue
            name,path=match.groups();actual=Path(path).resolve();resolved[name]=str(actual)
            if not actual.is_relative_to(suite):
                if not any(actual.is_relative_to(Path(base)) for base in ('/usr/lib','/lib','/lib64')):raise RuntimeError('Development dependency leaked into staged ELF: '+str(actual))
                external[name]=dict(path=str(actual),sha256=sha256(actual))
        info['resolved_transitive']=resolved;binaries[relative]=info
    dawn=list(suite.rglob('libwebgpu_dawn.so'))
    if len(dawn)!=1:raise RuntimeError('Expected one shared Dawn library per suite')
    return dict(schema_version=1,platform='linux',architecture='x86_64',browser_included=False,binaries=binaries,system_dependencies=external,
                limits='Static ELF and trusted-binary ldd audit. Dynamic Vulkan/Qt/audio driver loads and actual windows need separate runtime checks.')


def write_package(package, version, owner, kind, components, default_prefix, *, system='linux', architecture='x86_64', vlc_prefix=None, install_notes=None):
    package=Path(package)
    manifest=dict(schema_version=2,version=version,package_id=owner,kind=kind,platform=system,architecture=architecture,components=components)
    (package/'native-payload.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    installer=package/'.installer';installer.mkdir()
    shutil.copy2(package/'VisualizersCtl',installer/'VisualizersCtl')
    install='''#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mode=MODE
suite="${VLC_VISUALIZERS_SUITE:-PREFIX}"
prefix_set=0
skip=""
while [ "$#" -gt 0 ]; do
 case "$1" in
 --prefix) [ "$#" -ge 2 ] || exit 2; suite=$2; prefix_set=1; shift 2;;
 --mode) [ "$#" -ge 2 ] || exit 2; mode=$2; shift 2;;
 --without-geiss) skip="${skip}${skip:+,}geiss";shift;;
 --without-milkdrop) skip="${skip}${skip:+,}milkdrop";shift;;
 --without-terrain) skip="${skip}${skip:+,}terrain";shift;;
 --without-beach) skip="${skip}${skip:+,}beach";shift;;
 --without-objects) skip="${skip}${skip:+,}objects";shift;;
 --without-undersea) skip="${skip}${skip:+,}undersea";shift;;
 --without-coralcanyon) skip="${skip}${skip:+,}coralcanyon";shift;;
 *) printf 'Unknown option: %s\\n' "$1" >&2;exit 2;;
 esac
done
if [ "$mode" = vlc ] && [ "$prefix_set" -eq 0 ] && [ -z "${VLC_VISUALIZERS_SUITE:-}" ]; then
 suite="VLC_PREFIX"
fi
"$base/.installer/VisualizersCtl" install --suite "$suite" --source "$base" --skip "$skip" --mode "$mode"
printf '\\nInstalled to %s\\n' "$suite"
'''.replace('MODE','suite' if kind=='suite' else 'standalone').replace('VLC_PREFIX',vlc_prefix or '${XDG_DATA_HOME:-${HOME}/.local/share}/vlc-visualizers').replace('PREFIX',default_prefix)
    uninstall='''#!/bin/sh
set -eu
base=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
suite="${VLC_VISUALIZERS_SUITE:-PREFIX}"
if [ "$#" -gt 0 ]; then [ "$1" = --prefix ] && [ "$#" -eq 2 ] || exit 2; suite=$2; fi
"$base/.installer/VisualizersCtl" uninstall --suite "$suite" --source "$base"
printf 'Removed package-owned files; preferences, extra presets and verified backups remain in %s\\n' "$suite"
'''.replace('PREFIX',default_prefix)
    for name,text in (('install.sh',install),('uninstall.sh',uninstall)):
        (package/name).write_text(text,encoding='utf-8');(package/name).chmod(0o755)
    (package/'INSTALL.txt').write_text(
        'VLC Visualizers '+version+' — Linux x86_64\n\n'
        'Run ./install.sh without sudo. Use --prefix PATH for a custom suite.\n'
        'Renderer archives support --mode vlc to add the plugin to an existing addon.\n'
        'Use the same --prefix with ./uninstall.sh. Close players and visualizers first.\n'
        'Uninstall the previous component before upgrading. Modified files are retained.\n\n'
        'Combined defaults include Prism Flow, MilkDrop/projectM, Rhythm Terrain, Beach, Objects, UnderSea and CoralCanyon.\n'
        'Use --without-geiss, --without-milkdrop, --without-terrain, --without-beach, --without-objects, --without-undersea or --without-coralcanyon.\n'
        'Launch VLC through VlcVisualizerLauncher. --scan-visualizers refreshes its catalog.\n'
        'Standalone: Visualizers --visualizer ID [--line-in default|ID|exact-label]\n'
        'Use Ctrl-L to reselect input. Escape/right-click closes the visualizer.\n\n'
        'Copy a scene INI from its plugin assets into the vlc-visualizers/profiles directory\n'
        'under XDG_CONFIG_HOME (normally ~/.config) and reopen to apply your changes.\n\n'
        'Baseline: Mint 22 / Ubuntu 24.04, SDL2, Qt6, OpenSSL 3, OpenGL/Vulkan/audio drivers.\n'
        'The VLC module uses the matching Qt5 ABI. See dependency-audit.json for details.\n'
        'HDR is Experimental and off by default. Physical HDR is unverified.\n',encoding='utf-8')
    if install_notes is not None:
        (package/'INSTALL.txt').write_text(install_notes,encoding='utf-8')


def archive_package(package, output, name, version, *, system='linux', architecture='x86_64'):
    destination=Path(output)/(name+'-'+version+'-'+system+'-'+architecture+'.tar.gz')
    with destination.open('xb') as raw:
        with tarfile.open(fileobj=raw,mode='w:gz') as archive:archive.add(package,arcname=name+'-'+version)
    return destination
