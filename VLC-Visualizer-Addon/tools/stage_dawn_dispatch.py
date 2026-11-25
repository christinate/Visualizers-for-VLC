"""Stage a fresh native-dispatch integration suite, not a release installer.

The original engine binaries remain separate. This deliberately omits the
browser; their standalone capture helper is still a D45 migration gate.
"""
from pathlib import Path
import argparse
import json
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dawn-build', type=Path, required=True)
    parser.add_argument('--host-build', type=Path, required=True)
    parser.add_argument('--suite', type=Path, required=True)
    parser.add_argument('--classic', type=Path, required=True)
    parser.add_argument('--milkdrop', type=Path, required=True)
    parser.add_argument('--dawn-install', type=Path)
    parser.add_argument('--sdl-library', type=Path)
    args = parser.parse_args()
    suite = args.suite.resolve()
    # Never merge an old Electron payload or silently delete a prior candidate.
    if suite.exists() and any(suite.iterdir()):
        raise RuntimeError('Use a new empty integration-suite directory')
    suite.mkdir(parents=True, exist_ok=True)
    system = {'Windows': 'windows', 'Darwin': 'macos', 'Linux': 'linux'}[platform.system()]
    windows, mac = system == 'windows', system == 'macos'
    arch = 'arm64' if platform.machine().lower() in ('arm64', 'aarch64') else 'x86_64'
    build, host = args.dawn_build.resolve(), args.host_build.resolve()
    runtime = suite/'native'
    runtime.mkdir()
    executable_suffix = '.exe' if windows else ''
    for name in ('Visualizers', 'VisualizersCtl', 'VlcVisualizerLauncher'):
        shutil.copy2(host/(name+executable_suffix), suite)
    native_binary = build/('VisualizerNative'+executable_suffix)
    if mac:
        shutil.copytree(build/'VisualizerNative.app', runtime/'VisualizerNative.app')
        native_binary = runtime/'VisualizerNative.app/Contents/MacOS/VisualizerNative'
        if not args.dawn_install or not args.sdl_library:
            raise RuntimeError('Mac staging requires --dawn-install and --sdl-library')
        for file in (args.dawn_install/'lib').glob('libwebgpu_dawn*.dylib'):
            shutil.copy2(file, runtime/file.name)
        shutil.copy2(args.sdl_library, runtime/args.sdl_library.name)
    else:
        shutil.copy2(native_binary, runtime)
        if windows:
            for name in ('webgpu_dawn.dll', 'd3dcompiler_47.dll', 'SDL2.dll'):
                shutil.copy2(build/name, runtime/name)
        else:
            if not args.dawn_install:
                raise RuntimeError('Linux staging requires --dawn-install')
            libraries = list(args.dawn_install.glob('lib*/libwebgpu_dawn.so*'))
            if not libraries:
                raise RuntimeError('Source-built shared Dawn runtime not found')
            for file in libraries:
                shutil.copy2(file, runtime/file.name)
    suffix = '.dll' if windows else '.so'
    version = (ROOT/'VERSION').read_text().strip()
    revision = json.loads((ROOT/'dawn/dependencies.lock.json').read_text())['source_revision']
    for identifier, name, module in (('geiss', 'Prism Flow', 'prismflow_dawn'), ('terrain', 'Rhythm Terrain', 'terrain_dawn')):
        package = suite/'plugins'/identifier
        package.mkdir(parents=True)
        shutil.copy2(build/(module+suffix), package)
        assets = ROOT.parent/'Geiss64/native/prismflow/shaders' if identifier == 'geiss' else ROOT.parent/'Terrain64/assets/terrain'
        (package/'assets').mkdir()
        # Native runtime payload: shaders/manifests only, no JS/HTML reference.
        for file in assets.iterdir():
            if file.is_file() and file.suffix in ('.wgsl', '.json'):
                shutil.copy2(file, package/'assets'/file.name)
        project = 'Geiss64' if identifier == 'geiss' else 'Terrain64'
        manifest = json.loads((ROOT.parent/project/'manifest-dawn.json').read_text(encoding='utf-8'))
        if manifest['dawn_revision'] != revision or manifest['id'] != identifier or manifest['api_version'] != 4:
            raise RuntimeError('Renderer development manifest disagrees with the pinned runtime')
        manifest.update(version=version, platform=system, architecture=arch, entrypoint=module+suffix)
        if identifier == 'geiss':
            shutil.copy2(args.classic, package/args.classic.name)
            manifest['classic'] = dict(entrypoint=args.classic.name, adapter='geiss' if windows else 'opengl')
        (package/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    milk = suite/'plugins/milkdrop'
    milk.mkdir()
    shutil.copy2(args.milkdrop, milk/args.milkdrop.name)
    shutil.copytree(ROOT.parent/'MilkDrop2-64/assets', milk/'assets')
    # projectM uses its existing native shared library next to the module.
    if not windows:
        for file in args.milkdrop.parent.rglob('libprojectM*'):
            if file.is_file() and ('.dylib' in file.name if mac else '.so' in file.name):
                shutil.copy2(file, milk/file.name)
    manifest = dict(schema_version=1 if windows else 2, api_version=1 if windows else 2,
                    id='milkdrop', name='MilkDrop 2' if windows else 'MilkDrop 2 (projectM native)', version=version,
                    platform=system, architecture=arch, adapter='winamp' if windows else 'opengl', entrypoint=args.milkdrop.name)
    (milk/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    if mac:
        # Only our new integration copies are changed/signed; installed VLC and
        # preserved release artifacts remain untouched.
        def change(file, before, after):
            subprocess.run(['install_name_tool', '-change', before, after, str(file)], check=True)
        binaries = [native_binary, suite/'Visualizers', suite/'VisualizersCtl', suite/'VlcVisualizerLauncher',
                    *runtime.glob('*.dylib'), *suite.glob('plugins/*/*.so'), *suite.glob('plugins/*/*.dylib')]
        for file in binaries:
            dependencies = subprocess.check_output(['otool', '-L', str(file)], text=True).splitlines()[1:]
            for line in dependencies:
                dependency = line.strip().split(' (compatibility')[0]
                name = Path(dependency).name
                if 'libwebgpu_dawn' in name or 'libSDL2' in name:
                    if not (runtime/name).is_file():
                        raise RuntimeError('Missing runtime library: '+name)
                    relative = __import__('os').path.relpath(runtime/name, file.parent)
                    change(file, dependency, '@loader_path/'+relative)
                elif 'libprojectM' in name and file.parent == milk:
                    change(file, dependency, '@loader_path/'+name)
            subprocess.run(['codesign', '--force', '--sign', '-', str(file)], check=True, capture_output=True)
        subprocess.run(['codesign', '--force', '--sign', '-', str(runtime/'VisualizerNative.app')], check=True, capture_output=True)
    result = subprocess.run([str(suite/('VisualizersCtl'+executable_suffix)), 'scan', '--suite', str(suite)],
                            capture_output=True, text=True, check=True)
    registry = json.loads(result.stdout)
    if registry['rejected'] or {entry['id'] for entry in registry['visualizers']} != {'geiss', 'milkdrop', 'terrain'}:
        raise RuntimeError('Staged native catalog failed: '+result.stdout)
    print(json.dumps(dict(result='pass', suite=str(suite), bytes=sum(p.stat().st_size for p in suite.rglob('*') if p.is_file()),
                          release_payload=False, browser_included=False), indent=2))


if __name__ == '__main__':
    main()
