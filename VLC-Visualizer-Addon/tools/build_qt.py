"""Build the inline VLC interface with the pinned MinGW/Qt MSVCRT toolchain.

Acquire SDKs first (aqtinstall 3.3.0):
python -m aqt install-qt windows desktop 6.8.3 win64_mingw -O build/Qt --modules qtsvg
python -m aqt install-tool windows desktop tools_mingw1310 -O build/Qt
"""
from pathlib import Path
import argparse
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--qt', type=Path, default=ROOT/'build/Qt/6.8.3/mingw_64')
parser.add_argument('--compiler', type=Path, default=ROOT/'build/Qt/Tools/mingw1310_64/bin')
parser.add_argument('--vlc', type=Path, default=Path(r'C:\Program Files\VideoLAN\VLC'))
parser.add_argument('--source', type=Path, default=ROOT/'vendor/vlc/qt')
parser.add_argument('--build-dir', type=Path, default=ROOT/'build-qt-mingw')
args = parser.parse_args()
env = dict(os.environ, PATH=os.pathsep.join((str(args.compiler.resolve()), str((args.qt/'bin').resolve()), os.environ['PATH'])))
command = [
    'cmake', '-S', str(ROOT/'integration/qt'), '-B', str(args.build_dir.resolve()), '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER='+str((args.compiler/'gcc.exe').resolve()),
    '-DCMAKE_CXX_COMPILER='+str((args.compiler/'g++.exe').resolve()),
    '-DCMAKE_PREFIX_PATH='+str(args.qt.resolve()), '-DVLC_QT_SOURCE='+str(args.source.resolve()),
    '-DVLC_CORE_LIBRARY='+str((args.vlc/'libvlccore.dll').resolve()),
    '-DVLC_PUBLIC_LIBRARY='+str((args.vlc/'libvlc.dll').resolve()),
]
subprocess.run(command, env=env, check=True)
subprocess.run(['cmake', '--build', str(args.build_dir.resolve()), '--parallel', '6'], env=env, check=True)
