"""Stage a reproducible Windows test bundle, then archive only after verification.

Run with the installed MSYS2 UCRT64 Python. No user libraries or private settings
are read. Deployment uses Qt's tool plus a recursive PE import dependency audit.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
TOOLCHAIN = Path(os.environ.get('CUEBASE_TOOLCHAIN', 'C:/msys64/ucrt64'))
BUILD = ROOT / os.environ.get('CUEBASE_BUILD_DIR', 'build-final')
BIN = TOOLCHAIN / 'bin'
NAME = os.environ.get('CUEBASE_PACKAGE_NAME', 'Cuebase-Windows-Test-2026-10-25')
if not NAME or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_' for c in NAME):
    raise ValueError('Package name must contain only letters, digits, hyphens or underscores')
STAGE = ROOT / '.delivery' / NAME
APP = STAGE / 'app'

def copy(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)

def imports(path):
    data = path.read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError(f'Invalid Windows binary: {path}')
    count = struct.unpack_from('<H', data, pe+6)[0]
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    optional = pe+24
    is64 = struct.unpack_from('<H', data, optional)[0] == 0x20b
    directories = optional + (112 if is64 else 96)
    section_start = optional+optional_size
    sections = []
    for index in range(count):
        pos = section_start + index*40
        virtual_size, rva, size, offset = struct.unpack_from('<IIII', data, pos+8)
        sections.append((rva, max(size, virtual_size), offset))
    def raw(rva):
        for start, size, offset in sections:
            if start <= rva < start+size:
                return offset+rva-start
        raise ValueError(f'Invalid import address in {path}: {rva}')
    def cstring(rva):
        start = raw(rva)
        return data[start:data.index(b'\0', start)].decode('ascii')
    result = set()
    rva = struct.unpack_from('<I', data, directories+8)[0]
    if rva:
        pos = raw(rva)
        while any(data[pos:pos+20]):
            result.add(cstring(struct.unpack_from('<I', data, pos+12)[0]))
            pos += 20
    # Delay imports are needed by some multimedia/graphics backends as well.
    rva = struct.unpack_from('<I', data, directories+13*8)[0]
    if rva:
        pos = raw(rva)
        while any(data[pos:pos+32]):
            attrs, name = struct.unpack_from('<II', data, pos)
            if not attrs & 1:
                raise ValueError(f'Unexpected VA-based delay imports: {path}')
            result.add(cstring(name))
            pos += 32
    return sorted(result)

def dependencies():
    available = {p.name.lower(): p for p in BIN.glob('*.dll')}
    system = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'System32'
    seen = set()
    audit = {}
    pending = list(APP.rglob('*.exe')) + list(APP.rglob('*.dll'))
    while pending:
        path = pending.pop()
        relative = path.relative_to(APP).as_posix()
        if relative in seen:
            continue
        seen.add(relative)
        audit[relative] = imports(path)
        for name in audit[relative]:
            low = name.lower()
            if low.startswith(('api-ms-', 'ext-ms-')):
                continue
            local = APP/name
            if local.exists():
                pending.append(local)
            elif low in available:
                copy(available[low], local)
                pending.append(local)
            elif not (system/name).exists():
                raise RuntimeError(f'Missing dependency {name} required by {relative}')
    (STAGE/'verification').mkdir(exist_ok=True)
    (STAGE/'verification'/'runtime-imports.json').write_text(json.dumps(audit, indent=2), encoding='utf-8')

def stage():
    if STAGE.exists():
        raise RuntimeError(f'Stage already exists; inspect it before replacing: {STAGE}')
    APP.mkdir(parents=True)
    for name in ('Cuebase.exe', 'CuebaseRender.exe', 'CuebaseEditorInteractionTests.exe', 'CuebaseComponentTests.exe'):
        copy(BUILD/name, APP/name)
    for name in ('ffmpeg.exe', 'ffprobe.exe'):
        copy(BIN/name, APP/name)
    env = dict(os.environ, PATH=str(BIN)+os.pathsep+os.environ.get('PATH', ''))
    command = [str(BIN/'windeployqt6.exe'), '--qtpaths', str(BIN/'qtpaths6.exe'), '--release', '--no-translations', '--no-quick-import', '--compiler-runtime', '--include-plugins', 'qoffscreen', '--dir', str(APP), str(APP/'Cuebase.exe'), str(APP/'CuebaseEditorInteractionTests.exe')]
    result = subprocess.run(command, env=env, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (STAGE/'qt-deployment.log').write_text(result.stdout, encoding='utf-8')
    if result.returncode:
        raise RuntimeError('Qt deployment failed; see qt-deployment.log')
    (APP/'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\n', encoding='utf-8')
    dependencies()
    shutil.copytree(TOOLCHAIN/'share'/'licenses', STAGE/'third-party-licenses')
    source = STAGE/'source'
    for folder in ('src', 'tests', 'assets', 'backend'):
        for path in (ROOT/folder).rglob('*'):
            if path.is_file() and path.suffix.lower() not in {'.pem', '.key', '.zip'} and not path.name.startswith('.env'):
                copy(path, source/path.relative_to(ROOT))
    for path in ROOT.glob('*.md'):
        copy(path, source/path.name)
    for name in ('CMakeLists.txt', 'CMakePresets.json', 'Run-Cuebase.cmd', 'Build-Cuebase.cmd', 'package-source.py'):
        copy(ROOT/name, source/name)
    copy(Path(__file__), source/'tools'/Path(__file__).name)
    for name in ('mediamtx.exe', 'mediamtx.yml', 'LICENSE', 'README.md'):
        if (ROOT/'tools'/'mediamtx'/name).exists():
            copy(ROOT/'tools'/'mediamtx'/name, STAGE/'optional-local-receiver'/name)
    (STAGE/'Start Cuebase.cmd').write_text('@echo off\ncd /d "%~dp0app"\nstart "Cuebase" "Cuebase.exe"\n', encoding='utf-8')
    (STAGE/'README.txt').write_text('Cuebase Windows test build - 25 October 2026\n\nExtract the whole ZIP, then double-click Start Cuebase.cmd. Do not run it inside the ZIP.\nThe app folder includes Qt, media codecs and FFmpeg; MSYS2 is not required to launch it.\nYour existing Cuebase profile is used when launching the app. Back up important projects before testing.\n\nThis is a development checkpoint, not completion of the full requested feature list.\nSee source/CLAUDE-REQUESTS.md and source/COMPLETION-CHECKLIST.md for exact scope.\nThe source folder contains the complete app source, tests, assets and backend setup.\nSupabase credentials are not configured in this package.\nThe optional-local-receiver folder is only for explicitly starting a local MediaMTX receiver.\nThird-party license notices are included. No Windows/macOS/Linux production release is claimed.\n', encoding='utf-8')
    print(STAGE, flush=True)

def verify():
    env = dict(os.environ)
    env['PATH'] = str(Path(env.get('SystemRoot', 'C:/Windows'))/'System32')
    env['QT_PLUGIN_PATH'] = str(APP)
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(APP/'platforms')
    env.pop('QML2_IMPORT_PATH', None)
    for binary, output in [('CuebaseEditorInteractionTests.exe', 'portable-editor-results.txt'), ('CuebaseComponentTests.exe', 'portable-component-results.txt')]:
        report = STAGE/'verification'/output
        result = subprocess.run([str(APP/binary), '-platform', 'offscreen', '-o', str(report)+',txt'], cwd=ROOT, env=env, timeout=180)
        if result.returncode:
            raise RuntimeError(f'Packaged runtime test failed: {binary}; see {report}')
    print('Packaged runtime tests passed with MSYS2 removed from PATH', flush=True)

def archive():
    required = STAGE/'verification'/'portable-editor-results.txt'
    if not required.exists() or '0 failed' not in required.read_text(encoding='utf-8'):
        raise RuntimeError('Portable editor verification must pass before archiving')
    # Refresh documentation/results only; binaries must be verified after staging.
    for path in ROOT.glob('*.md'):
        copy(path, STAGE/'source'/path.name)
    for name in ('editor-interactions-results.txt', 'editor-regression-results.txt', 'component-verified-results.txt'):
        if (ROOT/name).exists():
            copy(ROOT/name, STAGE/'verification'/name)
    for name in ('animation-noise-controls.png', 'component-browser.png', 'component-update-review.png'):
        if (ROOT/'artifacts'/name).exists():
            copy(ROOT/'artifacts'/name, STAGE/'verification'/name)
    files = sorted(p for p in STAGE.rglob('*') if p.is_file() and p.name != 'SHA256SUMS.txt')
    manifest = '\n'.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(STAGE).as_posix() for p in files)+'\n'
    (STAGE/'SHA256SUMS.txt').write_text(manifest, encoding='utf-8')
    destination = ROOT/(NAME+'.zip')
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for path in sorted(STAGE.rglob('*')):
            if path.is_file():
                archive.write(path, Path(NAME)/path.relative_to(STAGE))
    with zipfile.ZipFile(destination) as check:
        if check.testzip() is not None:
            raise RuntimeError('ZIP integrity check failed')
    digest = hashlib.sha256(destination.read_bytes()).hexdigest()
    print(json.dumps({'path':str(destination),'bytes':destination.stat().st_size,'sha256':digest}), flush=True)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('stage', 'verify', 'archive'))
    args = parser.parse_args()
    {'stage': stage, 'verify': verify, 'archive': archive}[args.action]()
