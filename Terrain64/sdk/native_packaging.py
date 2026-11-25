"""Explicit native payload plans, with pinned dependency verification.

The Windows release entry points share this module through the source SDK.
No dependency download, browser cache, N-API binary or package merge is used.
"""
from pathlib import Path
import json
import shutil
import struct
import zipfile
from release_guard import sha256


def copy_plan(plan, destination):
    destination = Path(destination)
    for name, source in sorted(plan.items()):
        relative = Path(name)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError('Invalid payload path: '+name)
        target = destination/relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            raise FileExistsError('Payload paths must be unique: '+str(target))
        shutil.copy2(source, target)


def tree(plan, source, prefix):
    source = Path(source)
    if not source.is_dir():
        raise FileNotFoundError(source)
    for file in sorted(source.rglob('*')):
        if file.is_file():
            plan[prefix+'/'+file.relative_to(source).as_posix()] = file


def require_files(plan):
    for name, path in plan.items():
        if not Path(path).is_file():
            raise FileNotFoundError(name+': '+str(path))
    return plan


def pe_imports(file):
    """Read normal and delay imports without loading/executing the PE image."""
    data = Path(file).read_bytes()
    if data[:2] != b'MZ':
        raise ValueError('Missing PE header: '+str(file))
    pe = struct.unpack_from('<I', data, 60)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, count, _, _, _, optional_size, _ = struct.unpack_from('<HHIIIHH', data, pe+4)
    optional = pe+24
    magic = struct.unpack_from('<H', data, optional)[0]
    directory = optional+(112 if magic == 0x20b else 96 if magic == 0x10b else 0)
    if directory == optional:
        raise ValueError('Invalid PE optional header')
    image_base = struct.unpack_from('<Q' if magic == 0x20b else '<I', data, optional+(24 if magic == 0x20b else 28))[0]
    sections = [struct.unpack_from('<8sIIIIIIHHI', data, optional+optional_size+i*40) for i in range(count)]
    def offset(rva):
        for _, size, address, raw_size, raw, *_ in sections:
            if address <= rva < address+max(size, raw_size):
                value = raw+rva-address
                if value >= len(data):
                    break
                return value
        raise ValueError('PE import RVA outside file')
    def string(rva):
        start = offset(rva)
        end = data.find(b'\0', start, start+4096)
        if end < 0:
            raise ValueError('Unterminated PE import name')
        return data[start:end].decode('ascii').lower()
    imports = set()
    for index, size, name_offset in ((1, 20, 12), (13, 32, 4)):
        rva, length = struct.unpack_from('<II', data, directory+index*8)
        if not rva:
            continue
        cursor = offset(rva)
        for i in range(min(length//size+1, 4096)):
            entry = data[cursor+i*size:cursor+(i+1)*size]
            if len(entry) != size:
                raise ValueError('Truncated PE import table')
            if not any(entry):
                break
            name = struct.unpack_from('<I', entry, name_offset)[0]
            if index == 13 and not (struct.unpack_from('<I', entry)[0] & 1):
                name -= image_base
            imports.add(string(name))
        else:
            raise ValueError('Unterminated PE import table')
    return machine, sorted(imports)


# Explicit Windows OS dependencies. Redistributable project dependencies must
# be present in the payload; a development-machine PATH is never accepted.
SYSTEM_DLLS = set('''advapi32.dll avrt.dll bcrypt.dll cfgmgr32.dll comctl32.dll
comdlg32.dll crypt32.dll d2d1.dll d3d11.dll d3d12.dll d3d9.dll
ddraw.dll dsound.dll dwmapi.dll dwrite.dll dxgi.dll dxguid.dll gdi32.dll hid.dll
imm32.dll iphlpapi.dll kernel32.dll ksuser.dll mf.dll mfplat.dll mfreadwrite.dll
mmdevapi.dll mpr.dll msacm32.dll msvcrt.dll netapi32.dll normaliz.dll ntdll.dll
ole32.dll oleacc.dll oleaut32.dll opengl32.dll powrprof.dll propsys.dll psapi.dll
rpcrt4.dll secur32.dll setupapi.dll shcore.dll shell32.dll shlwapi.dll user32.dll userenv.dll
uxtheme.dll version.dll windowscodecs.dll winhttp.dll winmm.dll winspool.drv
wintrust.dll ws2_32.dll wtsapi32.dll usp10.dll authz.dll dnsapi.dll dwmapi.dll
wininet.dll wldap32.dll'''.split())


def audit_windows(directory, external_imports=None, machine_overrides=None):
    directory = Path(directory)
    files = [p for p in directory.rglob('*') if p.is_file()]
    forbidden_names = {'browser', 'node_modules', 'electron.exe', 'chrome.exe', 'node.exe'}
    forbidden_suffixes = {'.node', '.asar', '.pak', '.js', '.cjs', '.mjs', '.css', '.html', '.pdb', '.exp', '.ps1', '.bat', '.cmd'}
    for file in files:
        suffix_forbidden = file.suffix.lower() in forbidden_suffixes
        if file.suffix.lower() == '.lib':
            # MinGW distributes text notices named COPYING.lib. Actual COFF
            # archives/development libraries remain forbidden in the payload.
            notice = 'licenses' in file.relative_to(directory).parts and 'copying' in file.name.lower()
            if notice:
                with file.open('rb') as stream:
                    if stream.read(8) == b'!<arch>\n':
                        raise RuntimeError('Import archive masquerading as a license notice: '+str(file))
                file.read_text(encoding='utf-8')
            suffix_forbidden = not notice
        if any(part.lower() in forbidden_names for part in file.relative_to(directory).parts) or suffix_forbidden:
            raise RuntimeError('Unexpected non-native payload file: '+str(file))
    dlls = {p.name.lower() for p in files if p.suffix.lower() == '.dll'}
    binaries, failures = {}, {}
    external_imports = external_imports or {}
    machine_overrides = machine_overrides or {}
    for file in files:
        if file.suffix.lower() not in ('.exe', '.dll'):
            continue
        machine, imports = pe_imports(file)
        relative = file.relative_to(directory).as_posix()
        if machine != machine_overrides.get(relative, 0x8664):
            raise RuntimeError('Wrong payload binary architecture: '+str(file))
        unresolved = [name for name in imports if name not in dlls and name not in SYSTEM_DLLS and name not in external_imports.get(relative, ())
                      and not name.startswith(('api-ms-win-', 'ext-ms-win-'))]
        if unresolved:
            failures[relative] = unresolved
        binaries[relative] = dict(machine='x86' if machine == 0x14c else 'amd64', imports=imports)
    if failures:
        raise RuntimeError('Unpackaged dependencies: '+json.dumps(failures, indent=2))
    return dict(schema_version=1, platform='windows', architecture='x86_64',
                browser_included=False, binaries=binaries, external_imports=external_imports,
                limitation='Static normal/delay import audit; runtime resolution and dynamic loads require clean-machine execution.')


def windows_runtime(sdk, host_build, native_build, dawn_install, sdl_sdk):
    sdk, host_build, native_build, dawn_install, sdl_sdk = map(Path, (sdk, host_build, native_build, dawn_install, sdl_sdk))
    pin = json.loads((sdk/'dawn/dependencies.lock.json').read_text())
    provenance = json.loads((dawn_install/'build-provenance.json').read_text())
    if provenance['source_revision'] != pin['source_revision'] or provenance['system'] != 'Windows':
        raise RuntimeError('Pinned source-built Windows Dawn provenance is required')
    if provenance['options'].get('DAWN_BUILD_NODE_BINDINGS') != 'OFF' or provenance['options'].get('DAWN_BUILD_MONOLITHIC_LIBRARY') != 'SHARED':
        raise RuntimeError('Dawn must be a shared library without Node bindings')
    recorded = {item['path']: item['sha256'] for item in provenance['artifacts']}
    for name in ('webgpu_dawn.dll', 'd3dcompiler_47.dll'):
        if sha256(dawn_install/'bin'/name) != recorded.get('bin/'+name):
            raise RuntimeError('Dawn artifact differs from build provenance: '+name)
        if sha256(native_build/name) != recorded['bin/'+name]:
            raise RuntimeError('Native runtime links a different Dawn/FXC build: '+name)
    archive = sdl_sdk/f"SDL2-devel-{pin['SDL2']['version']}-VC.zip"
    if sha256(archive) != pin['SDL2']['windows_sdk_sha256']:
        raise RuntimeError('SDL SDK archive checksum mismatch')
    with zipfile.ZipFile(archive) as source:
        dll = [name for name in source.namelist() if name.endswith('/lib/x64/SDL2.dll')]
        if len(dll) != 1 or __import__('hashlib').sha256(source.read(dll[0])).hexdigest() != sha256(native_build/'SDL2.dll'):
            raise RuntimeError('SDL runtime differs from the pinned SDK')
    plan = {name+'.exe': host_build/(name+'.exe') for name in ('Visualizers', 'VisualizersCtl')}
    for name in ('VisualizerNative.exe', 'webgpu_dawn.dll', 'd3dcompiler_47.dll', 'SDL2.dll'):
        plan['native/'+name] = native_build/name
    tree(plan, sdk/'licenses', 'licenses')
    tree(plan, dawn_install/'share/dawn/notices', 'licenses/dawn')
    plan['native/build-provenance.json'] = dawn_install/'build-provenance.json'
    plan['native/dependencies.lock.json'] = sdk/'dawn/dependencies.lock.json'
    return require_files(plan)


def windows_plugin(project, legacy_build, module_build, *, manifest_name='manifest.json'):
    project, legacy_build, module_build = map(Path, (project, legacy_build, module_build))
    manifest_path = project/manifest_name
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    if manifest['version'] != (project/'VERSION').read_text().strip():
        raise RuntimeError('Plugin manifest/version mismatch')
    identifier = manifest['id']
    plan = {'manifest.json': manifest_path, 'LICENSE.txt': project/'LICENSE'}
    tree(plan, project/'licenses', 'licenses')
    if identifier in ('geiss', 'terrain', 'beach', 'objects', 'undersea', 'coralcanyon'):
        pin = json.loads((project/'sdk/dawn/dependencies.lock.json').read_text())
        if manifest['adapter'] != 'dawn' or manifest['api_version'] != 4 or manifest['dawn_revision'] != pin['source_revision']:
            raise RuntimeError('A pinned native API 4 manifest is required')
        plan[manifest['entrypoint']] = module_build/manifest['entrypoint']
        scene = 'undersea' if identifier == 'coralcanyon' else identifier
        source = project/('native/prismflow/shaders' if identifier == 'geiss' else 'assets/'+scene)
        assets = [p for p in source.iterdir() if p.is_file() and
                  (p.suffix in ('.wgsl', '.json') or p.name == identifier+'.ini')]
        if not any(p.suffix == '.wgsl' for p in assets):
            raise RuntimeError('Native shader assets are missing')
        plan.update({'assets/'+p.name: p for p in assets})
        if identifier != 'geiss' and 'assets/'+identifier+'.ini' not in plan:
            raise RuntimeError('The packaged scene INI is missing')
    elif identifier == 'milkdrop':
        plan[manifest['entrypoint']] = legacy_build/manifest['entrypoint']
        tree(plan, project/'assets', 'assets')
        for name in ('D3DX9_31.dll', 'D3DX9_43.dll', 'D3DCompiler_43.dll'):
            plan[name] = project/'vendor/d3dx/release/bin/x64'/name
    else:
        raise RuntimeError('Unknown built-in plugin')
    if identifier == 'geiss':
        plan[manifest['classic']['entrypoint']] = legacy_build/manifest['classic']['entrypoint']
        for name in ('LICENSE.txt', 'NOTICE.txt'):
            plan['licenses/prismflow/'+name] = project/'native/prismflow'/name
    return require_files(plan)


def file_records(directory, destination_prefix='', source_prefix=''):
    directory = Path(directory)
    return [dict(path=destination_prefix+p.relative_to(directory).as_posix(),
                 source=source_prefix+p.relative_to(directory).as_posix(), sha256=sha256(p))
            for p in sorted(directory.rglob('*')) if p.is_file()]
