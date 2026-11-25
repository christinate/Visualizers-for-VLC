from pathlib import Path
import hashlib
import json
import shutil
ROOT=Path(__file__).resolve().parents[1]
SDK=ROOT/'sdk/runtime'
for directory in ('include','vendor/nlohmann'):
    shutil.copytree(ROOT/directory,SDK/directory,dirs_exist_ok=True)
(SDK/'src').mkdir(exist_ok=True)
for name in ('host.cpp','addonctl.cpp'):shutil.copy2(ROOT/'src'/name,SDK/'src'/name)
# Do not refresh the preserved browser reference snapshots. Native packaging
# does not read them; removing the old renderer packagers is separate D45 work.
native=ROOT/'sdk/dawn';native.mkdir(exist_ok=True)
native_files='''Runtime.cmake dependencies.lock.json wayland-extended-srgb.patch Info.plist.in windows.manifest NATIVE-API.md
context.cpp context.hpp shader_cache.cpp shader_cache.hpp plugin.cpp plugin.hpp music_analysis.cpp music_analysis.hpp
player_audio.cpp player_audio.hpp audio_capture.cpp audio_capture.hpp capture_permission_mac.mm
settings.cpp settings.hpp input_picker.hpp input_picker_win.cpp input_picker_mac.mm input_picker_qt.cpp
settings_ui.hpp settings_ui_win.cpp settings_ui_mac.mm settings_ui_qt.cpp
window.cpp window.hpp presentation.hpp presentation_mac.hpp presentation_mac.mm window_demo.cpp production.cpp capture_service.cpp capture_service.hpp diagnostic_text.hpp
image_decode.cpp image_decode.hpp image_decode_win.cpp image_decode_mac.mm image_decode_qt.cpp image_pixels.hpp
text_raster.cpp text_raster.hpp text_raster_win.cpp text_raster_mac.mm text_raster_qt.cpp'''.split()
for name in native_files:shutil.copy2(ROOT/'dawn'/name,native/name)
shutil.copy2(ROOT/'dawn/SDK-CMakeLists.txt',native/'CMakeLists.txt')
shutil.copytree(ROOT/'vendor/miniaudio',SDK/'vendor/miniaudio',dirs_exist_ok=True)
(ROOT/'sdk/tools').mkdir(exist_ok=True)
for name in ('build_dawn.py','build_macos_sdl.py','prepare_dawn.py','prepare_windows_sdl.py'):
    shutil.copy2(ROOT/'tools'/name,ROOT/'sdk/tools'/name)
shutil.copy2(ROOT/'licenses/GPL-3.0.txt',ROOT/'sdk/LICENSE.GPL-3.0.txt')
(ROOT/'sdk/licenses').mkdir(exist_ok=True)
for name in ('GPL-3.0.txt','SDK-BSD-3-Clause.txt','nlohmann-json-MIT.txt','SDL2-zlib.txt'):
    shutil.copy2(ROOT/'licenses'/name,ROOT/'sdk/licenses'/name)
shutil.copy2(ROOT/'vendor/miniaudio/LICENSE',ROOT/'sdk/licenses/miniaudio-LICENSE.txt')
def source_file(path):
    relative=path.relative_to(ROOT/'sdk')
    return path.is_file() and path.name!='VERSION.json' and not any(part.startswith('build') or part in ('__pycache__','.git') for part in relative.parts[:-1])
files={p.relative_to(ROOT/'sdk').as_posix():hashlib.sha256(p.read_bytes().replace(b'\r\n',b'\n')).hexdigest() for p in (ROOT/'sdk').rglob('*') if source_file(p)}
(ROOT/'sdk/VERSION.json').write_text(json.dumps({'version':(ROOT/'VERSION').read_text().strip(),'hash_normalization':'LF line endings for SDK text files','files':files},indent=2)+'\n')
for name in ('Geiss64','MilkDrop2-64','Terrain64'):
    project=ROOT.parent/name
    def ignore_generated(directory,names):
        return [name for name in names if name in ('__pycache__','.git') or (name.startswith('build') and (Path(directory)/name).is_dir())]
    shutil.copytree(ROOT/'sdk',project/'sdk',dirs_exist_ok=True,ignore=ignore_generated)
    shutil.copy2(ROOT/'sdk/renderer-installer.nsi',project/'installer/renderer-installer.nsi')
print('Pinned native/runtime/bridge source SDK snapshots in all renderer projects')
