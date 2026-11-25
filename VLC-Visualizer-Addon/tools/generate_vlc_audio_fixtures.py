"""Create synthetic, hash-recorded audio fixtures; never launches a visualizer.

Uses installed FFmpeg/ffprobe, without downloads, microphones or user media.
These fixture checks are separate from actual VLC decoding/bridge validation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


CASES = [
    ('wav-mono-8k-u8', '.wav', 8000, 1, 'pcm_u8', []),
    ('wav-stereo-44k1-s16', '.wav', 44100, 2, 'pcm_s16le', []),
    ('wav-stereo-48k-s24', '.wav', 48000, 2, 'pcm_s24le', []),
    ('wav-stereo-96k-f32', '.wav', 96000, 2, 'pcm_f32le', []),
    ('wav-5_1-48k-s16', '.wav', 48000, 6, 'pcm_s16le', []),
    ('flac-stereo-48k-s24', '.flac', 48000, 2, 'flac',
     ['-sample_fmt', 's32', '-bits_per_raw_sample', '24']),
    ('mp3-stereo-44k1', '.mp3', 44100, 2, 'libmp3lame', ['-b:a', '192k']),
    ('aac-stereo-48k', '.m4a', 48000, 2, 'aac', ['-b:a', '192k']),
    ('opus-stereo-48k', '.opus', 48000, 2, 'libopus', ['-b:a', '160k']),
    ('vorbis-stereo-44k1', '.ogg', 44100, 2, 'libvorbis', ['-q:a', '5']),
]


def run(command):
    result = subprocess.run(command, capture_output=True, timeout=60)
    if result.returncode:
        raise RuntimeError(result.stderr.decode('utf-8', 'replace')[-4000:])
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--duration', type=float, default=12)
    parser.add_argument('--ffmpeg', default=shutil.which('ffmpeg'))
    parser.add_argument('--ffprobe', default=shutil.which('ffprobe'))
    args = parser.parse_args()
    if not args.ffmpeg or not args.ffprobe:
        parser.error('Install FFmpeg/ffprobe or provide their executable paths')
    if not 5 <= args.duration <= 60:
        parser.error('Choose a finite fixture duration between 5 and 60 seconds')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for identifier, extension, rate, channels, codec, options in CASES:
        # A 120-bpm amplitude envelope, with distinct carriers per channel.
        # The 5.1 source uses less gain to leave room for player downmixing.
        gain = .15 if channels == 6 else .25
        expressions = [f'{gain}*(1+sin(2*PI*2*t))*sin(2*PI*{220+n*55}*t)'
                       for n in range(channels)]
        layout = {1: 'mono', 2: 'stereo', 6: '5.1'}[channels]
        source = ('aevalsrc='+'|'.join(expressions)+f':s={rate}'
                  +f':d={args.duration}:c={layout}')
        destination = output/(identifier+extension)
        command = [args.ffmpeg, '-nostdin', '-hide_banner', '-loglevel', 'error',
                   '-f', 'lavfi', '-i', source, '-c:a', codec, '-threads', '1',
                   *options, '-n', str(destination)]
        run(command)
        probe = json.loads(run([args.ffprobe, '-v', 'error', '-show_streams',
                                '-show_format', '-of', 'json', str(destination)]))
        streams = probe['streams']
        assert len(streams) == 1 and streams[0]['codec_type'] == 'audio', probe
        stream = streams[0]
        assert stream['channels'] == channels and int(stream['sample_rate']) == rate, stream
        # Decode the whole fixture with the generator tool as a corruption check.
        # This is not a VLC result and is labelled separately in the manifest.
        run([args.ffmpeg, '-nostdin', '-hide_banner', '-loglevel', 'error',
             '-xerror', '-i', str(destination), '-map', '0:a:0', '-f', 'null', '-'])
        row = {'id': identifier, 'file': destination.name,
               'sha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
               'bytes': destination.stat().st_size, 'duration_requested': args.duration,
               'source': source, 'ffmpeg_command': command, 'probe': probe,
               'ffmpeg_decode': 'pass', 'vlc_decode': 'not yet tested'}
        rows.append(row)
        (output/'fixtures.json').write_text(json.dumps({
            'schema_version': 1, 'status': 'fixture preparation only',
            'source': 'synthetic tones with 120-bpm envelope; no recording',
            'ffmpeg_version': run([args.ffmpeg, '-version']).decode('utf-8', 'replace').splitlines()[0],
            'cases': rows}, indent=2)+'\n', encoding='utf-8')
        print(identifier+': prepared and FFmpeg decode checked', flush=True)
    print(json.dumps({'result': 'fixtures-generated', 'count': len(rows),
                      'bytes': sum(row['bytes'] for row in rows), 'vlc_validation': 'pending'}))


if __name__ == '__main__':
    main()
