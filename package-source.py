"""Package the buildable source and isolated verification artifacts; never user data."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib

root = Path(__file__).resolve().parent
destination = root / 'Cuebase-studio-timeline-update-2026-10-12.zip'
files = [root / name for name in (
    'CMakeLists.txt', 'CMakePresets.json', 'Run-Cuebase.cmd', 'Build-Cuebase.cmd', 'README-Build.md',
    'FRAME-EXPORT.md', 'BUILD-WORK.md', 'CUE-BOARD-WORK.md', 'CUE-BOARD-V2-WORK.md', 'CUE-BOARD-GUIDE.md', 'REPLAY-TEAM-WORK.md', 'VERIFICATION.md', 'PRODUCTION-GUIDE.md', 'package-source.py')]
for folder in ('src', 'tests', 'backend'):
    files += sorted(p for p in (root / folder).rglob('*') if p.is_file())
files += [root / 'tools' / 'mediamtx' / name for name in ('mediamtx.exe', 'LICENSE')]
for name in ('test-results.txt', 'test-results.xml', 'stream-results.txt', 'stream-results.xml'):
    files.append(root / 'build-final' / name)
files += sorted((root / 'build-final' / 'artifacts').glob('*.png'))
files.append(root / 'build-final' / 'artifacts' / 'local-receiver-check.txt')
with ZipFile(destination, 'w', ZIP_DEFLATED, compresslevel=6) as archive:
    for path in files:
        relative = path.relative_to(root)
        if relative.parts[0] == 'build-final':
            relative = Path('verification').joinpath(*relative.parts[1:])
        archive.write(path, Path('Cuebase') / relative)
with ZipFile(destination) as archive:
    assert archive.testzip() is None
print(f'{destination.name}: {destination.stat().st_size:,} bytes')
print('SHA256:', hashlib.file_digest(destination.open('rb'), 'sha256').hexdigest())
