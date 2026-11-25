"""Fetch and verify the pinned Windows development SDK for native SDL windows."""
from pathlib import Path
import hashlib
import json
import urllib.request
import zipfile

ROOT=Path(__file__).resolve().parents[1]


def prepare():
    pin=json.loads((ROOT/'dawn/dependencies.lock.json').read_text())['SDL2']
    version=pin['version']
    cache=ROOT/'build/sdl-sdk';cache.mkdir(parents=True,exist_ok=True)
    archive=cache/f'SDL2-devel-{version}-VC.zip'
    if not archive.exists():
        url=pin['windows_sdk_url']
        temporary=archive.with_suffix('.download')
        with urllib.request.urlopen(url,timeout=120) as source,temporary.open('wb') as output:
            while chunk:=source.read(1024*1024):output.write(chunk)
        temporary.replace(archive)
    with archive.open('rb') as stream:
        if hashlib.file_digest(stream,'sha256').hexdigest()!=pin['windows_sdk_sha256']:raise RuntimeError('SDL development SDK checksum mismatch')
    folder=cache/f'SDL2-{version}'
    if not (folder/'cmake/sdl2-config.cmake').exists():
        with zipfile.ZipFile(archive) as source:
            for entry in source.infolist():
                target=(cache/entry.filename).resolve()
                if not target.is_relative_to(cache.resolve()):raise RuntimeError('Invalid SDK archive path')
            source.extractall(cache)
    configs=list(folder.rglob('sdl2-config.cmake'))
    if len(configs)!=1:raise RuntimeError('Expected one SDL CMake configuration')
    return configs[0].parent


if __name__=='__main__':
    print(json.dumps({'SDL2_DIR':str(prepare())},indent=2))
