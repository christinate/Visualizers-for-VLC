"""Build/run the native foundation checks, without launching a player or installer."""
from pathlib import Path
import argparse, json, subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sdk = parser.add_mutually_exclusive_group(required=True)
    sdk.add_argument('--dawn-dir', type=Path)
    sdk.add_argument('--sdk-result', type=Path, help='JSON emitted by prepare_dawn.py')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/dawn-check')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--sdl-dir', type=Path, help='Also build the native window target with this SDL2 CMake package')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 32:
        parser.error('--jobs must be between 1 and 32')
    dawn_dir = args.dawn_dir
    if args.sdk_result:
        dawn_dir = Path(json.loads(args.sdk_result.read_text())['Dawn_DIR'])
    build = args.build_dir.resolve()
    configure = [args.cmake, '-S', str(ROOT / 'dawn'), '-B', str(build),
                 '-DCMAKE_BUILD_TYPE=Release', '-DDawn_DIR=' + str(dawn_dir.resolve())]
    if args.sdl_dir:
        configure.extend(['-DVIS_BUILD_NATIVE_WINDOW=ON', '-DSDL2_DIR=' + str(args.sdl_dir.resolve())])
    subprocess.run(configure, check=True)
    subprocess.run([args.cmake, '--build', str(build), '--config', 'Release',
                    '--parallel', str(args.jobs)], check=True)
    ctest = Path(args.cmake).with_name('ctest' + ('.exe' if str(args.cmake).endswith('.exe') else '')) if Path(args.cmake).is_absolute() else 'ctest'
    subprocess.run([str(ctest), '--test-dir', str(build), '-C', 'Release',
                    '--output-on-failure'], check=True)


if __name__ == '__main__':
    main()
