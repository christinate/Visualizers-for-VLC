"""Verify the actual shared source snapshots; never downloads or publishes."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
expected=(ROOT/'sdk/VERSION.json').read_bytes()
index=json.loads(expected)
for project in (ROOT,ROOT.parent/'Geiss64',ROOT.parent/'MilkDrop2-64',ROOT.parent/'Terrain64'):
    sdk=project/'sdk'
    assert (sdk/'VERSION.json').read_bytes()==expected, 'SDK index differs: '+str(project)
    for relative,digest in index['files'].items():
        file=sdk/relative
        assert file.is_file(), 'Missing SDK source: '+str(file)
        assert hashlib.sha256(file.read_bytes().replace(b'\r\n',b'\n')).hexdigest()==digest, 'Modified SDK source: '+str(file)
    assert (sdk/'tools/build_dawn.py').is_file(), 'Missing independent source builder'
    assert 'scene_dawn.hpp' not in (sdk/'dawn/window_demo.cpp').read_text(encoding='utf-8'), 'Runtime depends on a renderer implementation'
    print(project.name+': '+str(len(index['files']))+' matching SDK source hashes')
