"""Stage/build the Windows native candidate without replacing older artifacts."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys
from qt_runtime import files as qt_files
from prepare_dualsubs import DUALSUBS_COMMIT, INSTALLER_SHA256, OVERLAY_FILES, STOCK_CORE_SHA256
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'sdk'))
from release_guard import candidate_version, fresh_directory, checksums, sha256
from native_packaging import windows_runtime, windows_plugin, copy_plan, tree, audit_windows, file_records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-only', action='store_true')
    parser.add_argument('--stage-only', action='store_true')
    parser.add_argument('--build-root', type=Path, default=ROOT/'build/native-release')
    parser.add_argument('--host-build', type=Path)
    parser.add_argument('--native-build', type=Path)
    parser.add_argument('--geiss-build', type=Path)
    parser.add_argument('--terrain-build', type=Path)
    parser.add_argument('--dawn-install', type=Path, default=ROOT/'build/dawn-install')
    parser.add_argument('--sdl-sdk', type=Path, default=ROOT/'build/sdl-sdk')
    parser.add_argument('--dualsubs-stage', type=Path)
    parser.add_argument('--dualsubs-qt-module', type=Path)
    parser.add_argument('--stage', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Use release_native.py on Linux or release_native_macos.py on macOS')
    if bool(args.dualsubs_stage) != bool(args.dualsubs_qt_module):
        parser.error('Supply both the verified DualSubs stage and its combined Qt module')
    version = candidate_version(ROOT)
    stage = args.stage or ROOT/('build/release-'+version+'-windows')
    output = args.output or ROOT/'releases'/version/'windows-x86_64'
    # Refuse occupied paths before running any build or changing staging files.
    for path in (stage, output):
        if path.exists():
            raise FileExistsError('Choose a fresh candidate path: '+str(path))
    if not args.package_only:
        sdl_pin = json.loads((ROOT/'sdk/dawn/dependencies.lock.json').read_text())['SDL2']['version']
        sdl = args.sdl_sdk/('SDL2-'+sdl_pin)/'cmake'
        subprocess.run(['cmake', '-S', str(ROOT), '-B', str(args.build_root), '-G',
                        os.environ.get('CMAKE_GENERATOR', 'Visual Studio 18 2026'), '-A', 'x64',
                        '-DVIS_BUILD_DAWN_RUNTIME=ON', '-DDawn_DIR='+str(args.dawn_install/'lib/cmake/Dawn'),
                        '-DSDL2_DIR='+str(sdl)], check=True)
        subprocess.run(['cmake', '--build', str(args.build_root), '--config', 'Release', '--parallel', '6',
                        '--target', 'Visualizers', 'VisualizersCtl', 'VlcVisualizerLauncher', 'VisualizerNative',
                        'qt_bootstrap', 'vlc_visualizers'], check=True)
    host = args.host_build or args.build_root/'bin/Release'
    native = args.native_build or args.build_root/'bin/Release'
    runtime = windows_runtime(ROOT/'sdk', host, native, args.dawn_install, args.sdl_sdk)
    sources = {'visualizers/'+name: source for name, source in runtime.items()}
    sources.update({'vlc.exe': host/'VlcVisualizerLauncher.exe',
                    'plugins/audio_filter/libvlc_visualizers_plugin.dll': host/'libvlc_visualizers_plugin.dll'})
    sources.update(qt_files(ROOT, host))
    tree(sources, ROOT/'licenses', 'visualizers/licenses/addon')
    projects = {'geiss': 'Geiss64', 'milkdrop': 'MilkDrop2-64', 'terrain': 'Terrain64',
                'beach': 'Terrain64', 'objects': 'Terrain64', 'undersea': 'Terrain64', 'coralcanyon': 'Terrain64'}
    plans = {}
    for identifier, name in projects.items():
        project = ROOT.parent/name
        if candidate_version(project) != version:
            raise RuntimeError('Renderer/addon versions differ: '+name)
        module = (args.geiss_build if identifier == 'geiss' else args.terrain_build) or project/'build/native-independent/bin/Release'
        manifest_name = 'manifest-'+identifier+'.json' if identifier in ('beach', 'objects', 'undersea', 'coralcanyon') else 'manifest.json'
        plans[identifier] = windows_plugin(project, project/'build/bin/Release', module, manifest_name=manifest_name)
    stage = fresh_directory(stage)
    payload = stage/'payload'
    copy_plan(sources, payload/'files')
    components = {}
    for identifier, plan in plans.items():
        folder = payload/'components'/identifier
        copy_plan(plan, folder)
        components[identifier] = file_records(folder, 'visualizers/plugins/'+identifier+'/', 'components/'+identifier+'/')
    records = file_records(payload/'files', source_prefix='files/')
    manifest = dict(schema_version=1, version=version, supported_vlc=['3.0.24-amd64'],
                    patch_method='reversible-launcher-replacement', files=records, components=components)
    if args.dualsubs_stage:
        dual = args.dualsubs_stage
        provenance = json.loads((dual/'source.json').read_text(encoding='utf-8'))
        if provenance['commit'] != DUALSUBS_COMMIT or provenance['installer_sha256'] != INSTALLER_SHA256:
            raise RuntimeError('Unexpected DualSubs provenance')
        plan = {name: dual/'vlc'/name for name in OVERLAY_FILES}
        for name, source in plan.items():
            if sha256(source) != provenance['files'][name]:
                raise RuntimeError('DualSubs staged file changed: '+name)
        plan['visualizers/qt/bin/libqt_backend.dll'] = args.dualsubs_qt_module
        plan['visualizers/licenses/dualsubs/LICENSE-DualSubs.txt'] = dual/'LICENSE-DualSubs.txt'
        plan['visualizers/licenses/dualsubs/source.json'] = dual/'source.json'
        destination = payload/'components/dualsubs'
        copy_plan(plan, destination)
        components['dualsubs'] = file_records(destination, source_prefix='components/dualsubs/')
        manifest['dualsubs'] = dict(version=provenance['version'], stock_core_sha256=STOCK_CORE_SHA256)
    (payload/'payload.json').write_text(json.dumps(manifest, indent=2)+'\n')
    (payload/'bootstrap').mkdir()
    shutil.copy2(host/'VisualizersCtl.exe', payload/'bootstrap/VisualizersCtl.exe')
    audit = audit_windows(payload, {'files/visualizers/qt/bin/libqt_backend.dll': ['libvlccore.dll']})
    audit['external_import_requirement'] = (
        'The selected interface loads VLC 3.0.24 AMD64. The optional DualSubs component supplies its verified core.'
        if args.dualsubs_stage else 'The private VLC interface loads the selected installed VLC 3.0.24 AMD64 core.'
    )
    dawn = [p for p in payload.rglob('webgpu_dawn.dll')]
    if len(dawn) != 1 or any(p.suffix.lower() == '.exe' for p in (payload/'components').rglob('*')):
        raise RuntimeError('Combined payload must contain one Dawn runtime and plugin-only components')
    audit.update(version=version, payload_bytes=sum(p.stat().st_size for p in payload.rglob('*') if p.is_file()),
                 component_bytes={k: sum(p.stat().st_size for p in (payload/'components'/k).rglob('*') if p.is_file()) for k in components},
                 qa_status='installer execution, Defender, real players and final source provenance pending')
    (stage/'dependency-audit.json').write_text(json.dumps(audit, indent=2)+'\n')
    if args.stage_only:
        print(json.dumps(dict(result='staged', stage=str(stage), bytes=audit['payload_bytes'], release_ready=False), indent=2))
        return
    output = fresh_directory(output)
    artifact = output/('VLC-Visualizer-Addon-'+version+'-setup.exe')
    nsis = os.environ.get('MAKENSIS', r'C:\Program Files (x86)\NSIS\makensis.exe')
    flags = ['/DINCLUDE_DUALSUBS'] if args.dualsubs_stage else []
    subprocess.run([nsis, '/DVERSION='+version, '/DPAYLOAD_DIR='+str(payload.resolve()),
                    '/DOUTPUT_FILE='+str(artifact.resolve()), *flags, str(ROOT/'installer/addon.nsi')], cwd=ROOT, check=True)
    shutil.make_archive(str(output/('VLC-Visualizer-Addon-'+version+'-payload')), 'zip', payload)
    shutil.copy2(stage/'dependency-audit.json', output/'dependency-audit.json')
    checksums(output)
    print(json.dumps(dict(result='candidate-built', installer=str(artifact), sha256=sha256(artifact),
                          bytes=artifact.stat().st_size, release_ready=False), indent=2))


if __name__ == '__main__':
    main()
