"""Collect compact local validation evidence, omitting generated WAV media."""
from pathlib import Path
import argparse,tarfile
p=argparse.ArgumentParser();p.add_argument('--work',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();work=a.work.resolve()
folders=['matrix-native','matrix-release-launcher-fixed','soak-native-30min','installer-native-test','display-native-test','native-asan-profile']
files=[]
for folder in folders:
    files.extend(f for f in (work/folder).rglob('*') if f.is_file() and f.suffix in ('.log','.json','.jsonl','.png') and '.install' not in f.parts and 'suite' not in f.relative_to(work/folder).parts and 'selected' not in f.relative_to(work/folder).parts)
for name in ['native-cpu-asan.log','native-asan-process.log','geiss-sample.txt','cache-corruption-test.json']:
    if (work/name).exists():files.append(work/name)
with tarfile.open(a.output,'w:gz') as archive:
    for file in files:archive.add(file,arcname=file.relative_to(work).as_posix())
print('Collected',len(files),'validation files')
