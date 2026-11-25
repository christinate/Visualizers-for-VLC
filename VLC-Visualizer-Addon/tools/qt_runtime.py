"""Files owned by the Windows inline Qt interface distribution."""
from pathlib import Path
import os
def files(root:Path, host_build=None):
    qt=Path(os.environ.get('VLC_VISUALIZERS_QT',root/'build/Qt/6.8.3/mingw_64'))
    interface=Path(os.environ.get('VLC_VISUALIZERS_QT_MODULE',root/'build-qt-mingw/libqt_plugin.dll'))
    host_build=Path(host_build) if host_build is not None else root/'build/bin/Release'
    result={'plugins/gui/libqt_plugin.dll':host_build/'libqt_plugin.dll','visualizers/qt/bin/libqt_backend.dll':interface}
    # VLC's dependency search is System32-only. Its small bootstrap explicitly
    # opens our private backend with DLL_LOAD_DIR; no PATH or policy changes.
    for name in ('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','Qt6Svg.dll','Qt6Network.dll','libgcc_s_seh-1.dll','libstdc++-6.dll','libwinpthread-1.dll','d3dcompiler_47.dll'):
        result['visualizers/qt/bin/'+name]=qt/'bin'/name
    for folder in ('platforms','styles','imageformats','iconengines'):
        for path in (qt/'plugins'/folder).glob('*.dll'):
            result['visualizers/qt/plugins/'+folder+'/'+path.name]=path
    for source in result.values():
        if not source.is_file():raise FileNotFoundError('Build the matching MinGW Qt interface first: '+str(source))
    return result
