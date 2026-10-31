#!/usr/bin/env python3
"""Prepare bundled ELF library paths and grant CAP_NET_RAW to selected examples."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import shlex


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('examples', nargs='*', default=['15_tactile_display'])
    parser.add_argument('--prepare-only', action='store_true',
                        help='prepare library paths and print the administrator command')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    if not (root / '.ghand-linux-bundle').is_file():
        parser.error('Run the copy in dist/linux/setup_ethercat.py')
    patchelf = shutil.which('patchelf')
    setcap = shutil.which('setcap') or '/usr/sbin/setcap'
    if not patchelf:
        parser.error('patchelf is required (Ubuntu: sudo apt install patchelf)')
    library_path = str(root / 'lib')
    if ':' in library_path or '\n' in library_path:
        parser.error('Bundle path must not contain a colon or newline')
    targets = []
    for name in args.examples:
        target = root / 'bin' / name
        if (Path(name).name != name or target.is_symlink() or not target.is_file()
                or target.read_bytes()[:4] != b'\x7fELF'):
            parser.error(f'Not a bundled ELF example: {name}')
        targets.append(target)
    for target in targets:
        # Absolute DT_RPATH also resolves indirect dependencies in secure-execution
        # mode, where LD_LIBRARY_PATH and untrusted $ORIGIN paths cannot be used.
        subprocess.run([patchelf, '--force-rpath', '--set-rpath', library_path,
                        str(target)], check=True)
    sums = root / 'SHA256SUMS.txt'
    entries = []
    for path in sorted(root.rglob('*')):
        if path.is_file() and path != sums:
            entries.append(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(root)}\n')
    sums.write_text(''.join(entries))
    # setcap expects a capability expression for each file.
    command = [setcap]
    for target in targets:
        command.extend(['cap_net_raw=ep', str(target)])
    if os.geteuid() != 0:
        command.insert(0, 'sudo')
    if args.prepare_only:
        print('Library paths prepared. Run once in your terminal:')
        print(shlex.join(command))
    else:
        subprocess.run(command, check=True)
        subprocess.run(['getcap', *map(str, targets)], check=True)
    print('Run the top-level example launcher as your normal user.')
    print('After moving or rebuilding this bundle, run this setup again.')


if __name__ == '__main__':
    main()
