"""Stage a verified DualSubs Windows release and merge its Qt menu into ours."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
DUALSUBS_REPOSITORY = 'https://github.com/christinate/DualSubs-for-VLC'
DUALSUBS_VERSION = '0.2.0'
DUALSUBS_COMMIT = 'b3b42d9bc6f1fa3408c6bcc8cfde58fdfb87e07b'
VLC_VERSION = '3.0.24'
INSTALLER_SHA256 = '7505d27c70558cd161f1d62a0d3e66ad76a33fec5cfb8357b544382792a8b4bf'
STOCK_CORE_SHA256 = 'c089f281affe1e85c61edd5996c2b496191f82fa5cdc7ef709b0c09bf754cdbe'
UPSTREAM_MENUS = {
    'menus.cpp': '570cf20fb8950e614fec8a466b7490032e7d8ed7da51c912aa28fbfd469233e0',
    'menus.hpp': 'e8c32c2186ed04bec916089b30b51f65b3d1930aaa6387a25f452f714e93189b',
}
OVERLAY_FILES = (
    'libvlc.dll',
    'libvlccore.dll',
    'plugins/codec/liblibass_plugin.dll',
    'plugins/codec/libsubsdec_plugin.dll',
    'plugins/codec/libsubstx3g_plugin.dll',
    'plugins/codec/libttml_plugin.dll',
    'plugins/codec/libwebvtt_plugin.dll',
)


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--installer', type=Path, required=True)
    parser.add_argument('--dualsubs-source', type=Path, required=True)
    parser.add_argument('--vlc-menus', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seven-zip', default=r'C:\Program Files\7-Zip\7z.exe')
    args = parser.parse_args()
    if sha256(args.installer) != INSTALLER_SHA256:
        raise ValueError('DualSubs installer does not match the pinned release')
    for name, expected in UPSTREAM_MENUS.items():
        if hashlib.sha256((args.vlc_menus/name).read_bytes().replace(b'\r\n', b'\n')).hexdigest() != expected:
            raise ValueError(f'{name} is not from stock VLC {VLC_VERSION}')

    def from_dualsubs(path):
        return subprocess.check_output(['git', '-C', args.dualsubs_source, 'show', f'{DUALSUBS_COMMIT}:{path}'])

    output = args.output.resolve()
    output.mkdir(exist_ok=False)
    qt = output/'qt'
    vlc = output/'vlc'
    shutil.copytree(ROOT/'vendor/vlc/qt', qt)
    shutil.copytree(ROOT/'vendor/vlc/share', output/'share')
    with tempfile.TemporaryDirectory(dir=output) as staging:
        work = Path(staging)
        for name in UPSTREAM_MENUS:
            paths = [qt/name, work/('stock-'+name), work/('dualsubs-'+name)]
            changes = [paths[0].read_bytes(), (args.vlc_menus/name).read_bytes(),
                       from_dualsubs(f'source-overlay/vlc-{VLC_VERSION}/modules/gui/qt/{name}')]
            for path, data in zip(paths, changes):
                path.write_bytes(data.replace(b'\r\n', b'\n'))
            merged = subprocess.check_output(['git', 'merge-file', '-p', *paths])
            paths[0].write_bytes(merged)
        subprocess.run([
            args.seven_zip, 'e', '-y', '-o'+staging, args.installer.resolve(),
            '-ir!DualSubs-Payload.zip', '-ir!payload-manifest.json',
        ], check=True)
        manifest = json.loads((work/'payload-manifest.json').read_text(encoding='utf-8-sig'))
        if manifest['dualSubsVersion'] != DUALSUBS_VERSION or manifest['vlcVersion'] != VLC_VERSION:
            raise ValueError('Unexpected DualSubs payload version')
        digests = {item['relativePath']: item['sha256'].lower() for item in manifest['files']}
        with ZipFile(work/'DualSubs-Payload.zip') as archive:
            names = {raw.replace('\\', '/'): raw for raw in archive.namelist()}
            for name in OVERLAY_FILES:
                data = archive.read(names[name])
                if hashlib.sha256(data).hexdigest() != digests[name]:
                    raise ValueError(f'Unexpected DualSubs payload bytes: {name}')
                destination = vlc/name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
    (output/'LICENSE-DualSubs.txt').write_bytes(from_dualsubs('LICENSE'))
    (output/'source.json').write_text(json.dumps({
        'repository': DUALSUBS_REPOSITORY, 'version': DUALSUBS_VERSION, 'commit': DUALSUBS_COMMIT,
        'vlc': VLC_VERSION, 'installer_sha256': INSTALLER_SHA256,
        'files': {name: digests[name] for name in OVERLAY_FILES},
        'merged_qt_sources': {name: sha256(qt/name) for name in UPSTREAM_MENUS},
    }, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
