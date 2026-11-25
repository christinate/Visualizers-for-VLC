"""Stage local Windows integration tests without touching an installed player."""
from pathlib import Path
import json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
suite=ROOT/'build/browser-suite';suite.mkdir(parents=True,exist_ok=True)
for name in ('Visualizers.exe','VisualizersCtl.exe'):
    shutil.copy2(ROOT/'build/bin/Release'/name,suite/name)
app=suite/'browser/app';app.mkdir(parents=True,exist_ok=True)
for name in ('main.cjs','preload.cjs','package.json'):shutil.copy2(ROOT/'browser'/name,app/name)
shutil.copytree(ROOT/'browser/ui',app/'ui',dirs_exist_ok=True)
(app/'native').mkdir(exist_ok=True)
shutil.copy2(ROOT/'build/browser-native/Release/visualizer_audio.node',app/'native/visualizer_audio.node')
runtime=suite/'browser/runtime'
if not runtime.exists():
    shutil.copytree(ROOT/'build/browser-runtime/win32-x64',runtime)
for project,id in [('Geiss64','geiss'),('MilkDrop2-64','milkdrop'),('Terrain64','terrain')]:
    source=ROOT.parent/project;dest=suite/'plugins'/id;dest.mkdir(parents=True,exist_ok=True)
    if (source/'payload/plugin').exists():shutil.copytree(source/'payload/plugin',dest,dirs_exist_ok=True)
    if (source/'assets').exists():shutil.copytree(source/'assets',dest/'assets',dirs_exist_ok=True)
    if (source/'manifest.json').exists():shutil.copy2(source/'manifest.json',dest/'manifest.json')
subprocess.run([str(suite/'VisualizersCtl.exe'),'scan','--suite',str(suite)],check=True)
print(suite)
