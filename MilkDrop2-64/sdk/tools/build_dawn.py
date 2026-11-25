"""Build the pinned native Dawn dependency with the local C++ toolchain.

This is a developer tool. Installers never fetch or build dependencies.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(arguments, **kwargs):
    return subprocess.run([str(arg) for arg in arguments], check=True, **kwargs)


def git(source, *arguments):
    return run(['git', '-c', 'core.longpaths=true', '-C', source, *arguments],
               capture_output=True, text=True).stdout.strip()


def verify_source(source, patch=None, apply_patch=False):
    if patch is None:
        git(source, 'diff', '--exit-code', 'HEAD', '--', '.')
        return

    patch_file = ROOT / 'dawn' / patch['path']
    if hashlib.sha256(patch_file.read_bytes()).hexdigest() != patch['sha256']:
        raise RuntimeError('Dawn Wayland patch checksum differs from the pinned input')
    files = patch['files']
    changed = set(git(source, 'diff', '--name-only', 'HEAD', '--', '.').splitlines())
    if not changed and apply_patch:
        for name, hashes in files.items():
            content = (source / name).read_bytes().replace(b'\r\n', b'\n')
            if hashlib.sha256(content).hexdigest() != hashes['before']:
                raise RuntimeError(f'Dawn patch input differs from the pinned source: {name}')
        git(source, 'apply', '--check', '--unidiff-zero', str(patch_file))
        git(source, 'apply', '--unidiff-zero', str(patch_file))
        changed = set(git(source, 'diff', '--name-only', 'HEAD', '--', '.').splitlines())
    if changed != set(files):
        raise RuntimeError('Dawn source changes differ from the pinned Wayland patch')
    for name, hashes in files.items():
        content = (source / name).read_bytes().replace(b'\r\n', b'\n')
        if hashlib.sha256(content).hexdigest() != hashes['after']:
            raise RuntimeError(f'Patched Dawn source differs from the pinned output: {name}')


def source_checkout(source, pin, patch=None):
    revision = pin['source_revision']
    if not source.exists():
        # Set long-path handling before checkout, not after a failed checkout.
        run(['git', 'clone', '--config', 'core.longpaths=true', '--filter=blob:none',
             '--no-checkout', pin['source_repository'], source])
        run(['git', '-C', source, 'fetch', '--depth=1', 'origin', revision])
        run(['git', '-C', source, 'checkout', '--detach', revision])
    if git(source, 'rev-parse', 'HEAD') != revision:
        raise RuntimeError('Existing Dawn checkout is not at the pinned revision; use a new --source directory')
    # Keep unrelated user changes in place. Existing patched builds can be resumed.
    verify_source(source, patch, apply_patch=True)


def options(system):
    enabled = {'Windows': 'D3D12', 'Darwin': 'METAL', 'Linux': 'VULKAN'}[system]
    result = {
        'DAWN_FETCH_DEPENDENCIES': 'ON', 'DAWN_ENABLE_INSTALL': 'ON',
        'DAWN_BUILD_MONOLITHIC_LIBRARY': 'SHARED', 'BUILD_SHARED_LIBS': 'OFF',
        'DAWN_BUILD_SAMPLES': 'OFF', 'DAWN_BUILD_TESTS': 'OFF',
        'DAWN_BUILD_NODE_BINDINGS': 'OFF', 'DAWN_BUILD_BENCHMARKS': 'OFF',
        'DAWN_BUILD_PROTOBUF': 'OFF', 'DAWN_USE_GLFW': 'OFF',
        'DAWN_USE_X11': 'ON' if system == 'Linux' else 'OFF',
        'DAWN_USE_WAYLAND': 'ON' if system == 'Linux' else 'OFF',
        'TINT_BUILD_CMD_TOOLS': 'OFF', 'TINT_BUILD_TESTS': 'OFF',
        'TINT_BUILD_BENCHMARKS': 'OFF', 'TINT_BUILD_IR_BINARY': 'OFF',
        'TINT_BUILD_SPV_READER': 'OFF', 'TINT_BUILD_WGSL_READER': 'ON',
        'TINT_BUILD_WGSL_WRITER': 'OFF', 'TINT_BUILD_NULL_WRITER': 'OFF',
        'TINT_BUILD_GLSL_WRITER': 'OFF', 'TINT_BUILD_GLSL_VALIDATOR': 'OFF',
        'TINT_BUILD_HLSL_WRITER': 'ON' if system == 'Windows' else 'OFF',
        'TINT_BUILD_MSL_WRITER': 'ON' if system == 'Darwin' else 'OFF',
        'TINT_BUILD_SPV_WRITER': 'ON' if system == 'Linux' else 'OFF',
        'CMAKE_BUILD_TYPE': 'Release',
    }
    for backend in ('D3D11', 'D3D12', 'METAL', 'VULKAN', 'DESKTOP_GL', 'OPENGLES', 'NULL'):
        result['DAWN_ENABLE_' + backend] = 'ON' if backend == enabled else 'OFF'
    if system == 'Windows':
        # Only WebGPU's C ABI crosses modules; keep each CRT allocation in its
        # owning module and avoid an additional VC runtime installer payload.
        result['CMAKE_MSVC_RUNTIME_LIBRARY'] = 'MultiThreaded'
        result['ABSL_MSVC_STATIC_RUNTIME'] = 'ON'
    return result


def collect_windows_sdk_notices(compiler, install):
    """Match the copied runtime to its SDK before collecting that SDK's notices."""
    digest = hashlib.sha256(compiler.read_bytes()).hexdigest()
    roots = []
    if os.environ.get('WindowsSdkDir'):
        roots.append(Path(os.environ['WindowsSdkDir']))
    program_files = os.environ.get('ProgramFiles(x86)')
    if program_files:
        roots.append(Path(program_files) / 'Windows Kits/10')
    matches = []
    for root in dict.fromkeys(path.resolve() for path in roots):
        for candidate in (root / 'bin').glob('*/x64/d3dcompiler_47.dll'):
            if hashlib.sha256(candidate.read_bytes()).hexdigest() == digest:
                version = candidate.parent.parent.name
                if not all(part.isdecimal() for part in version.split('.')):
                    continue
                license_dir = root / 'Licenses' / version
                names = ('sdk_license.rtf', 'sdk_third_party_notices.rtf')
                if all((license_dir / name).is_file() for name in names):
                    matches.append((version, candidate, license_dir, names))
    if not matches:
        raise RuntimeError('Cannot match the shader compiler to SDK license files; set WindowsSdkDir to the SDK root')
    version, candidate, license_dir, names = max(matches, key=lambda item: tuple(int(part) for part in item[0].split('.')))
    destination = install / 'share/dawn/notices/windows-sdk'
    destination.mkdir(parents=True, exist_ok=True)
    notices = []
    for name in names:
        shutil.copy2(license_dir / name, destination / name)
        notices.append({'path': (destination / name).relative_to(install).as_posix(),
                        'sha256': hashlib.sha256((destination / name).read_bytes()).hexdigest()})
    result = {'sdk_version': version, 'compiler_source': str(candidate),
              'compiler_sha256': digest, 'notices': notices,
              'status': 'SDK notices collected; final redistribution audit remains required'}
    (install / 'share/dawn/windows-sdk-source.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    return result


def provenance(source, build, install, pin, configuration, patch=None):
    repositories = [source]
    for directory, dirs, files in os.walk(source / 'third_party'):
        if '.git' in dirs or '.git' in files:
            repositories.append(Path(directory))
        dirs[:] = [entry for entry in dirs if entry not in ('.git', 'test', 'tests')]
    revisions = []
    notice_dir = install / 'share/dawn/notices'
    for repo in repositories:
        label = 'dawn' if repo == source else repo.relative_to(source).as_posix()
        try:
            origin = git(repo, 'remote', 'get-url', 'origin')
        except subprocess.CalledProcessError:
            # Upstream's shallow fetcher can initialize a repo without a remote.
            origin = None
        git_dir = Path(git(repo, 'rev-parse', '--absolute-git-dir'))
        fetch_head = git_dir / 'FETCH_HEAD'
        revisions.append({'path': label, 'revision': git(repo, 'rev-parse', 'HEAD'),
                          'origin': origin,
                          'fetch_head': fetch_head.read_text(encoding='utf-8').strip() if fetch_head.is_file() else None})
        verify_source(repo, patch if repo == source else None)
        # Preserve upstream top-level notices, including license directories.
        for entry in repo.iterdir():
            if entry.name.lower().startswith(('license', 'copying', 'notice', 'copyright')) or entry.name == 'README.chromium':
                destination = notice_dir / label / entry.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                if entry.is_dir():
                    shutil.copytree(entry, destination, dirs_exist_ok=True)
                elif entry.is_file():
                    shutil.copy2(entry, destination)
        # Some compiled header dependencies are vendored in Dawn's own Git
        # tree rather than separate repositories (for example RenderDoc API).
        for tracked in git(repo, 'ls-files').splitlines():
            entry=Path(tracked)
            if entry.name.lower().startswith(('license','copying','notice','copyright')) or entry.name=='README.chromium':
                original=repo/entry
                if original.is_file():
                    destination=notice_dir/label/'tracked'/entry
                    destination.parent.mkdir(parents=True,exist_ok=True)
                    shutil.copy2(original,destination)
    artifacts = []
    for folder in ('bin', 'lib', 'lib64'):
        for entry in (install / folder).rglob('*'):
            if entry.is_file():
                with entry.open('rb') as stream:
                    digest = hashlib.file_digest(stream, 'sha256').hexdigest()
                artifacts.append({'path': entry.relative_to(install).as_posix(),
                                  'bytes': entry.stat().st_size, 'sha256': digest})
    result = {'source_revision': pin['source_revision'], 'system': platform.system(),
              'machine': platform.machine(), 'options': configuration,
              'dependencies': revisions, 'artifacts': artifacts,
              'notice_status': 'Collected upstream top-level and tracked vendored notices; final runtime license audit is still required.'}
    if patch is not None:
        result['source_patches'] = [patch]
    (install / 'build-provenance.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    shutil.copy2(build / 'CMakeCache.txt', install / 'share/dawn/CMakeCache.txt')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'build/dawn-source')
    parser.add_argument('--build', type=Path, default=ROOT / 'build/dawn-native')
    parser.add_argument('--install', type=Path, default=ROOT / 'build/dawn-install')
    parser.add_argument('--cmake', default='cmake')
    parser.add_argument('--generator')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--macos-deployment-target',
                        help='Explicit minimum macOS for a fresh Darwin SDK build (for example 11.0)')
    parser.add_argument('--dependency-prefix',type=Path,action='append',default=[],
                        help='Private development dependency prefix (repeatable)')
    args = parser.parse_args()
    if not 1 <= args.jobs <= 32:
        parser.error('--jobs must be between 1 and 32')
    if platform.system() not in ('Windows', 'Darwin', 'Linux'):
        parser.error('Unsupported native platform')
    if args.macos_deployment_target:
        if platform.system()!='Darwin' or not re.fullmatch(r'[0-9]+(?:\.[0-9]+){0,2}',args.macos_deployment_target):
            parser.error('--macos-deployment-target requires Darwin and a numeric OS version')
        if platform.machine()=='arm64' and tuple(map(int,args.macos_deployment_target.split('.'))) < (11,):
            parser.error('Apple Silicon requires macOS 11 or later')
    if platform.system() == 'Windows':
        # Dawn's dependency fetcher invokes Git itself. Apply long-path support
        # to those child commands too, without changing global Git settings.
        index = int(os.environ.get('GIT_CONFIG_COUNT', '0'))
        os.environ['GIT_CONFIG_COUNT'] = str(index + 1)
        os.environ['GIT_CONFIG_KEY_' + str(index)] = 'core.longpaths'
        os.environ['GIT_CONFIG_VALUE_' + str(index)] = 'true'
    pin = json.loads((ROOT / 'dawn/dependencies.lock.json').read_text())
    source, build, install = args.source.resolve(), args.build.resolve(), args.install.resolve()
    patch = pin['linux_source_patch'] if platform.system() == 'Linux' else None
    source_checkout(source, pin, patch)
    configuration = options(platform.system())
    if args.macos_deployment_target:
        configuration['CMAKE_OSX_DEPLOYMENT_TARGET']=args.macos_deployment_target
        configuration['CMAKE_OSX_ARCHITECTURES']=platform.machine()
    if args.dependency_prefix:
        configuration['CMAKE_PREFIX_PATH']=';'.join(str(path.resolve()) for path in args.dependency_prefix)
        includes=';'.join(str(path.resolve()/'include') for path in args.dependency_prefix)
        configuration['CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES']=includes
        configuration['CMAKE_C_STANDARD_INCLUDE_DIRECTORIES']=includes
    configuration['CMAKE_INSTALL_PREFIX'] = str(install)
    configure = [args.cmake, '-S', source, '-B', build]
    if args.generator:
        configure.extend(['-G', args.generator])
    configure.extend('-D' + name + '=' + value for name, value in configuration.items())
    run(configure)
    # Upstream's fetcher does not check every Git subprocess return code.
    # Verify the source tree and pinned gitlinks after fetching, before build.
    verify_source(source, patch)
    run([args.cmake, '--build', build, '--config', 'Release', '--parallel', args.jobs])
    run([args.cmake, '--install', build, '--config', 'Release', '--prefix', install])
    if platform.system() == 'Windows':
        # Dawn copies the current SDK's FXC DLL into its build output but does
        # not install it. Keep it beside the runtime, as its loader expects.
        compiler = build / 'Release/d3dcompiler_47.dll'
        if not compiler.is_file():
            raise RuntimeError('Dawn did not stage the Windows SDK shader compiler')
        (install / 'bin').mkdir(parents=True, exist_ok=True)
        shutil.copy2(compiler, install / 'bin/d3dcompiler_47.dll')
        collect_windows_sdk_notices(compiler, install)
    provenance(source, build, install, pin, configuration, patch)
    configs = list(install.rglob('DawnConfig.cmake'))
    if len(configs) != 1:
        raise RuntimeError('Expected exactly one installed Dawn CMake package')
    print(json.dumps({'Dawn_DIR': str(configs[0].parent),
                      'provenance': str(install / 'build-provenance.json')}, indent=2))


if __name__ == '__main__':
    main()
