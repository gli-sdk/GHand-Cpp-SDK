#!/usr/bin/env python3
"""Build and bundle the Linux examples (requires CMake, Qt 6, patchelf and ldd)."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def run(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=root / 'build-dist-linux')
    parser.add_argument('--output', type=Path, default=root / 'dist/linux')
    parser.add_argument('--qtpaths', default='/usr/lib/qt6/bin/qtpaths6')
    parser.add_argument('--jobs', default='4')
    args = parser.parse_args()
    patchelf = shutil.which("patchelf")
    if not patchelf:
        parser.error("patchelf is required (Ubuntu: sudo apt install patchelf)")
    build, output = args.build_dir.resolve(), args.output.resolve()
    if output.exists() and not (output / '.ghand-linux-bundle').is_file():
        parser.error(f'Refusing to replace unmanaged directory: {output}')
    plugins = Path(run(args.qtpaths, '--query', 'QT_INSTALL_PLUGINS').strip())
    prefix = run(args.qtpaths, '--query', 'QT_INSTALL_PREFIX').strip()
    subprocess.run(['cmake', '-S', str(root), '-B', str(build),
                    '-DCMAKE_BUILD_TYPE=Release', '-DGHAND_BUILD_TESTS=OFF',
                    '-DCMAKE_SKIP_RPATH=ON', f'-DCMAKE_PREFIX_PATH={prefix}'], check=True)
    subprocess.run(['cmake', '--build', str(build), '--parallel', args.jobs], check=True)
    if not (build / 'examples/15_tactile_display').is_file():
        raise RuntimeError('Qt 6 Widgets was not found; example 15 was not built')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.linux-', dir=output.parent) as temp:
        stage = Path(temp) / 'linux'
        for directory in ('bin', 'lib', 'plugins', 'LICENSES/packages'):
            (stage / directory).mkdir(parents=True)
        sources = []
        for exe in sorted((build / 'examples').iterdir()):
            if not exe.is_file() or not os.access(exe, os.X_OK):
                continue
            if exe.read_bytes()[:4] != b'\x7fELF' or '.so' in exe.name:
                continue
            shutil.copy2(exe, stage / 'bin' / exe.name)
            subprocess.run([patchelf, '--force-rpath', '--set-rpath',
                            '$ORIGIN/../lib', str(stage / 'bin' / exe.name)], check=True)
            sources.append(exe)
            launcher = stage / exe.name
            launcher.write_text('#!/bin/sh\nset -eu\n'
                                'HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                                'export QT_PLUGIN_PATH="$HERE/plugins"\n'
                                'export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"\n'
                                'cd "$HERE"\n'
                                f'exec "$HERE/bin/{exe.name}" "$@"\n')
            launcher.chmod(0o755)
        for category in ('platforms', 'imageformats', 'platforminputcontexts',
                         'xcbglintegrations', 'egldeviceintegrations'):
            directory = plugins / category
            if directory.exists():
                shutil.copytree(directory, stage / 'plugins' / category)
                sources.extend(directory.rglob('*.so'))
        if not (stage / 'plugins/platforms/libqxcb.so').exists():
            raise RuntimeError('Qt xcb platform plugin is required')
        shutil.copy2(build / 'libghand.so', stage / 'lib/libghand.so.3')
        sources.append(build / 'libghand.so')
        # Keep the host loader and glibc together; bundle other shared dependencies.
        system = re.compile(r'^(?:libc|libm|libpthread|libdl|librt|libresolv|libutil|libanl)\.so\.')
        env = dict(os.environ, LD_LIBRARY_PATH=str(build))
        bundled = {}
        # Qt loads Xcursor with dlopen, so it is absent from ldd's dependency tree.
        cache = run('/sbin/ldconfig', '-p')
        cursor = re.search(r'^\s*libXcursor\.so\.1 .* => (\S+)', cache, re.M)
        if not cursor:
            raise RuntimeError('libXcursor.so.1 is required for the X11 runtime')
        cursor_path = Path(cursor.group(1)).resolve()
        shutil.copy2(cursor_path, stage / 'lib/libXcursor.so.1')
        bundled['libXcursor.so.1'] = cursor_path
        sources.append(cursor_path)
        for source in sources:
            listing = subprocess.check_output(['ldd', str(source)], env=env, text=True)
            if 'not found' in listing:
                raise RuntimeError(f'Unresolved dependencies for {source}:\n{listing}')
            for name, location in re.findall(r'^\s*(\S+) => (/\S+)', listing, re.M):
                if system.match(name) or name.startswith('libghand.so'):
                    continue
                resolved = Path(location).resolve()
                if name in bundled and bundled[name] != resolved:
                    raise RuntimeError(f'Conflicting library: {name}')
                if name not in bundled:
                    shutil.copy2(resolved, stage / 'lib' / name)
                    bundled[name] = resolved
        shutil.copytree(root / 'config', stage / 'config')
        shutil.copytree(root / 'LICENSES', stage / 'LICENSES', dirs_exist_ok=True)
        for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md'):
            shutil.copy2(root / name, stage / name)
        (stage / 'bin/qt.conf').write_text('[Paths]\nPrefix=..\nPlugins=plugins\nLibraries=lib\n')
        packages = {}
        for source in set(bundled.values()) | {p.resolve() for p in sources if str(p).startswith('/usr/')}:
            candidates = [str(source)]
            if str(source).startswith('/usr/lib/'):
                candidates.append(str(source)[4:])
            for candidate in candidates:
                try:
                    owner = run('dpkg-query', '-S', candidate).split(': ', 1)[0]
                    metadata = run('dpkg-query', '-W', '-f=${binary:Package}\t${Version}\t${source:Package}\t${source:Version}\n', owner)
                    packages[owner] = metadata
                    copyright_file = Path('/usr/share/doc') / owner.split(':')[0] / 'copyright'
                    if copyright_file.exists():
                        shutil.copy2(copyright_file, stage / 'LICENSES/packages' / (owner.replace(':', '_') + '.copyright'))
                    break
                except subprocess.CalledProcessError:
                    continue
        (stage / 'PACKAGES.tsv').write_text('binary\tversion\tsource\tsource_version\n' + ''.join(packages[k] for k in sorted(packages)))
        shutil.copytree('/usr/share/common-licenses', stage / 'LICENSES/common-licenses')
        shutil.copy2(root / 'scripts/setup_ethercat.py', stage / 'setup_ethercat.py')
        (stage / '.ghand-linux-bundle').write_text('Generated by scripts/package_linux.py\n')
        (stage / 'README.md').write_text(
            '# Linux release\n\nFor example 15, complete the one-time permission setup below, then run `./15_tactile_display`.\n'
            'Copy this entire directory; the entry scripts locate bundled libraries and config.\n'
            'Built on ' + run('uname', '-sm').strip() + ', glibc ' + run('getconf', 'GNU_LIBC_VERSION').strip() + '.\n'
            'Use a compatible Linux system with a graphical session for example 15.\n'
            'glibc, the ELF loader, graphics/device drivers and fonts remain system dependencies.\n'
            'EtherCAT requires raw socket privileges and a connected hand.\n'
            'Once after deployment: `python3 setup_ethercat.py` (requires patchelf and setcap).\n'
            'The setup patches the actual ELF with an absolute DT_RPATH, then invokes sudo\n'
            'only for setcap cap_net_raw=ep. Run `./15_tactile_display` as your normal user.\n'
            'For another example: `python3 setup_ethercat.py 01_basic_connection`.\n'
            'Moving or replacing the bundle requires running setup again. Setup refreshes checksums.\n'
            'Use --prepare-only to prepare paths and print the authorization command.\n'
            'Qt and other bundled shared libraries can be replaced in lib/ and plugins/.\n'
            'Package/source versions are in PACKAGES.tsv; Debian/Ubuntu copyright notices\n'
            'and source locations are in LICENSES/packages/. Obtain matching distribution\n'
            'sources with `apt-get source <source>=<source_version>` using deb-src repositories.\n')
        sums = []
        for path in sorted(stage.rglob('*')):
            if path.is_file():
                sums.append(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(stage)}\n')
        (stage / 'SHA256SUMS.txt').write_text(''.join(sums))
        if output.exists():
            shutil.rmtree(output)
        shutil.move(str(stage), output)
    print(f'Linux bundle: {output}')


if __name__ == '__main__':
    main()
