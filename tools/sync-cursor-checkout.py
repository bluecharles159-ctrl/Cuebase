"""Safely move the user's older Cursor/Visual Studio checkout to the verified integration.

Dry-run by default. Keeps original edits in a named git stash, plus the versioned
reference source. Refuses new or changed Cursor work rather than overwriting it.
"""
import argparse
from datetime import datetime, timezone
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / 'reference-implementations/cursor-2026-10-27'

def git(directory, *args):
    result = subprocess.run(['git', '-C', str(directory), *args], capture_output=True, text=True, check=True)
    return result.stdout.strip()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--destination', required=True, type=Path)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    target = args.destination.resolve()
    if target == ROOT or not (target / '.git').is_dir():
        raise SystemExit('Destination must be the separate existing Cuebase checkout.')
    if git(target, 'remote', 'get-url', 'origin') != git(ROOT, 'remote', 'get-url', 'origin'):
        raise SystemExit('Repositories have different origins; no changes made.')
    branch = 'codex/cursor-integration'
    revision = git(ROOT, 'rev-parse', 'HEAD')
    if git(target, 'branch', '--show-current') == branch:
        # Subsequent updates are fast-forward only. Never stash or overwrite new work.
        if git(target, 'status', '--porcelain', '--untracked-files=all'):
            raise SystemExit('Editor checkout has local changes. Preserve and review them before updating.')
        print(f'Clean integration checkout: {target}\nUpdate revision: {revision}')
        if args.apply:
            git(target, 'fetch', str(ROOT), revision)
            git(target, 'merge', '--ff-only', revision)
            print('Editor checkout updated:', git(target, 'rev-parse', 'HEAD'))
        else:
            print('Dry run passed. Apply will allow only a fast-forward update.')
        return
    if git(target, 'rev-parse', '--short=7', 'HEAD') != '8a45503':
        raise SystemExit('Cursor checkout has new commits. Review them before synchronizing.')
    expected = {'src/' + path.name for path in REFERENCE.iterdir() if path.is_file()}
    status = subprocess.run(['git', '-C', str(target), 'status', '--porcelain', '-z'], capture_output=True, check=True).stdout
    changed = {entry.decode('utf-8')[3:].replace('\\', '/') for entry in status.split(b'\0') if entry}
    if changed != expected:
        raise SystemExit('Cursor changed-file list differs from the reviewed snapshot; no changes made.\n' + '\n'.join(sorted(changed)))
    for relative in expected:
        if (target / relative).read_bytes() != (REFERENCE / Path(relative).name).read_bytes():
            # Git may normalize only line endings when recording the preserved source.
            normalize = lambda data: data.replace(b'\r\n', b'\n')
            if normalize((target / relative).read_bytes()) != normalize((REFERENCE / Path(relative).name).read_bytes()):
                raise SystemExit(f'Cursor has newer edits in {relative}; no changes made.')
    if git(target, 'branch', '--list', branch):
        raise SystemExit('Integration branch already exists; inspect it before updating.')
    print(f'Reviewed {len(expected)} Cursor files. Target: {target}\nIntegration revision: {revision}')
    if not args.apply:
        print('Dry run passed. No files changed.')
        return
    git(target, 'fetch', str(ROOT), revision)
    label = 'Cursor originals before Cuebase integration ' + datetime.now(timezone.utc).isoformat()
    print(git(target, 'stash', 'push', '--include-untracked', '-m', label))
    print('Recovery stash:', git(target, 'rev-parse', 'refs/stash'))
    git(target, 'switch', '-c', branch, revision)
    if git(target, 'rev-parse', 'HEAD') != revision:
        raise SystemExit('Unexpected checkout revision. Original edits remain in the stash.')
    print('Visual Studio checkout updated:', revision)
    print('Original Cursor files are also preserved in reference-implementations/cursor-2026-10-27/.')

if __name__ == '__main__':
    main()
