#!/usr/bin/env python3
"""ASkompu-ytimen versionvalitsin. Sama backend GUI:lle ja komentoriville.

Ei kirjoita lähdetyöpuuhun. Kaikki Git-, CMake-, CTest- ja restart-toiminnot
ovat tässä tiedostossa. GUI lukee atomisesti julkaistun TSV-tilan; CLI saa JSONin.
"""
from __future__ import annotations

import argparse
import contextlib
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import stat
import subprocess
import sys
import time
import uuid
from urllib.parse import quote
import zipfile

UPSTREAM = 'https://github.com/Mikky100/ASkompu.git'
SHA = re.compile(r'^[0-9a-fA-F]{40}$')
TERMINAL = {'ready', 'done', 'failed', 'cancelled', 'launched'}
LOCAL_TREES = ('src', 'include', 'test')
LOCAL_FILES = ('LICENSE', 'LICENSE.txt', 'COPYING', 'platformio.ini')
LOCAL_REQUIRED = (
    'src/core/ApplicationCore.cpp', 'src/core/ApplicationCore.h', 'src/core/DisplayModel.h',
    'src/core/Clock.cpp', 'src/domain/CalibrationSetting.cpp',
    'src/domain/CompetitionEngine.cpp', 'src/domain/CompetitionRecords.cpp',
    'src/domain/EventRepository.cpp', 'src/domain/MotionMath.cpp',
    'src/domain/RouteOrder.cpp', 'src/domain/SpeedCalculator.cpp', 'src/domain/TripCounter.cpp',
    'src/route/RouteOrderCodec.cpp', 'src/route/RouteOrderEditor.cpp',
    'src/input/ButtonInterpreter.cpp', 'src/ui/DisplayLayout.cpp', 'src/ui/RouteOrderFormatting.cpp',
    'test/test_motion_math/test_main.cpp', 'test/test_competition/test_main.cpp',
    'test/test_competition_records/test_main.cpp', 'test/test_display_layout/test_main.cpp',
    'test/test_board_profiles/test_main.cpp',
)
SNAPSHOT_NAME = 'snapshot-manifest.json'
SNAPSHOT_LIMIT = 128 * 1024 * 1024
LOCAL_EXCLUDED_DIRS = {'.git', '.pio', 'simulator', 'build', '__pycache__'}
LOCAL_EXCLUDED_SUFFIXES = {'.o', '.obj', '.pyc', '.exe', '.elf', '.a', '.so', '.dll', '.zip', '.tar', '.gz'}


class Cancelled(Exception):
    pass


class VersionError(Exception):
    pass


def local_relative_path(value):
    """Only source paths that are also safe CMake list members are accepted."""
    if (not isinstance(value, str) or not value or any(ord(c) < 32 for c in value)
            or any(c in value for c in ('\\', ';', ':'))):
        raise VersionError('Paikallisessa lähteessä on virheellinen tiedostopolku.')
    parts = value.split('/')
    if any(part in ('', '.', '..') for part in parts):
        raise VersionError('Paikallisen lähteen polku poistuu lähdehakemistosta.')
    if not ((parts[0] in LOCAL_TREES and len(parts) > 1) or value in LOCAL_FILES):
        raise VersionError('Tilannekuvassa on ASkompu-lähteisiin kuulumaton tiedosto: ' + value)
    return value


def source_fingerprint(files):
    return hashlib.sha256(''.join(f'{f["path"]}:{f["sha256"]}\n' for f in files).encode('utf-8')).hexdigest()


def checked_read(root: Path, relative):
    path = root / local_relative_path(relative)
    # Reject symlinks (including ancestors); never follow a link outside the source.
    for ancestor in (path, *path.parents):
        if ancestor == root:
            break
        if ancestor.is_symlink():
            raise VersionError('Paikallinen lähde sisältää symbolisen linkin: ' + relative)
    if not path.resolve().is_relative_to(root):
        raise VersionError('Paikallisen lähteen polku poistuu lähdehakemistosta.')
    before = path.stat(follow_symlinks=False)
    if not stat.S_ISREG(before.st_mode) or before.st_size > 32 * 1024 * 1024:
        raise VersionError('Paikallisessa lähteessä on virheellinen tiedostotyyppi tai koko: ' + relative)
    descriptor = os.open(path, os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0))
    with os.fdopen(descriptor, 'rb') as stream:
        opened = os.fstat(stream.fileno())
        if not stat.S_ISREG(opened.st_mode) or (before.st_dev, before.st_ino) != (opened.st_dev, opened.st_ino):
            raise VersionError('Paikallinen lähde muuttui tarkistuksen aikana. Yritä uudelleen.')
        data = stream.read(32 * 1024 * 1024 + 1)
        after = os.fstat(stream.fileno())
    if (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (after.st_size, after.st_mtime_ns, after.st_ctime_ns) or len(data) != before.st_size:
        raise VersionError('Paikallinen lähde muuttui tarkistuksen aikana. Yritä uudelleen.')
    return data


def local_inventory(root: Path):
    paths = []
    def visit(directory):
        if directory.is_symlink():
            raise VersionError('Paikallinen lähde sisältää symbolisen linkin: ' + str(directory.relative_to(root)))
        for child in sorted(directory.iterdir()):
            if child.name in LOCAL_EXCLUDED_DIRS or child.suffix.lower() in LOCAL_EXCLUDED_SUFFIXES:
                continue
            if child.is_symlink():
                raise VersionError('Paikallinen lähde sisältää symbolisen linkin: ' + str(child.relative_to(root)))
            if child.is_dir():
                visit(child)
            else:
                paths.append(child.relative_to(root).as_posix())
    for name in LOCAL_TREES:
        directory = root / name
        if not directory.is_dir():
            raise VersionError('ASkompu-lähdehakemistosta puuttuu ' + name)
        visit(directory)
    for name in LOCAL_FILES:
        if (root / name).exists() or (root / name).is_symlink():
            paths.append(name)
    files, total = [], 0
    for relative in sorted(paths):
        data = checked_read(root, relative)
        total += len(data)
        if total > SNAPSHOT_LIMIT:
            raise VersionError('Paikallinen ASkompu-lähde ylittää tilannekuvan kokorajan.')
        files.append({'path': relative, 'sha256': hashlib.sha256(data).hexdigest(), 'size': len(data)})
    missing = set(LOCAL_REQUIRED).difference(paths)
    if missing:
        raise VersionError('Hakemisto ei sisällä yhteensopivaa ASkompu-lähderakennetta. Puuttuu: ' + ', '.join(sorted(missing)))
    return files


def validate_snapshot_manifest(manifest):
    if not isinstance(manifest, dict):
        raise VersionError('Tuntematon paikallisen tilannekuvan muoto.')
    if manifest.get('manifest_version') != 1 or manifest.get('source_kind') != 'local':
        raise VersionError('Tuntematon paikallisen tilannekuvan muoto.')
    if (type(manifest.get('git')) is not int or manifest['git'] not in (0, 1) or
        type(manifest.get('dirty')) is not int or manifest['dirty'] not in (0, 1) or
        not isinstance(manifest.get('sha'), str) or
        not (SHA.fullmatch(manifest['sha']) or manifest['sha'] == 'tuntematon' and not manifest['git']) or
        not isinstance(manifest.get('local_path'), str) or not os.path.isabs(manifest['local_path'])):
        raise VersionError('Tilannekuvan alkuperätiedot ovat virheelliset.')
    expected = manifest.get('files')
    if not isinstance(expected, list) or not expected:
        raise VersionError('Tilannekuvan tiedostoluettelo puuttuu.')
    for record in expected:
        if not isinstance(record, dict):
            raise VersionError('Virheellinen tilannekuvan tiedostotunniste.')
        local_relative_path(record.get('path'))
        if not isinstance(record.get('sha256'), str) or not re.fullmatch('[0-9a-f]{64}', record['sha256']) or type(record.get('size')) is not int or record['size'] < 0:
            raise VersionError('Virheellinen tilannekuvan tiedostotunniste.')
    paths = [f['path'] for f in expected]
    if paths != sorted(set(paths)) or source_fingerprint(expected) != manifest.get('fingerprint'):
        raise VersionError('Tilannekuvan manifesti ei vastaa lähdetiivistettä.')
    return expected


def verify_snapshot(core: Path, manifest_path: Path):
    """Verify both hashes and complete inventory before build/test/install."""
    if core.is_symlink() or manifest_path.is_symlink() or manifest_path.resolve() != core.resolve() / SNAPSHOT_NAME:
        raise VersionError('Tilannekuvan manifesti ei ole sen omassa juurihakemistossa.')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    expected = validate_snapshot_manifest(manifest)
    paths = [f['path'] for f in expected]
    actual = local_inventory(core.resolve())
    all_paths = []
    for path in core.rglob('*'):
        if path.is_symlink():
            raise VersionError('Tilannekuvaan on lisätty symbolinen linkki.')
        if path.is_file():
            all_paths.append(path.relative_to(core).as_posix())
    if actual != expected or sorted(all_paths) != sorted(paths + [SNAPSHOT_NAME]):
        raise VersionError('Paikallinen tilannekuva muuttui. Rakennus estetty.')
    return manifest


def atomic(path: Path, text: str):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    temp.write_text(text, encoding='utf-8')
    try:
        for attempt in range(20):
            try:
                os.replace(temp, path)
                return
            except PermissionError:  # Windows: GUI voi juuri lukea vanhaa tilaa.
                if attempt == 19:
                    raise
                time.sleep(.025)
    finally:
        temp.unlink(missing_ok=True)


def json_write(path, value):
    atomic(path, json.dumps(value, ensure_ascii=False, indent=2))


def alive(pid: int):
    if os.name == 'nt':
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.OpenProcess.restype = ctypes.c_void_p
        handle = kernel.OpenProcess(0x1000, False, pid)
        if not handle:
            return ctypes.get_last_error() == 5
        result = ctypes.c_ulong()
        kernel.GetExitCodeProcess(ctypes.c_void_p(handle), ctypes.byref(result))
        kernel.CloseHandle(ctypes.c_void_p(handle))
        return result.value == 259
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


@contextlib.contextmanager
def workspace_lock(home: Path):
    lock = home / 'operation.lock'
    for attempt in range(2):
        try:
            lock.mkdir()
            break
        except FileExistsError:
            try:
                owner = json.loads((lock / 'owner.json').read_text())
                if alive(int(owner['pid'])):
                    raise VersionError('Toinen ytimen toiminto on jo käynnissä. Odota sen valmistumista.')
            except (OSError, ValueError, KeyError):
                # Älä poista lukkoa prosessin ollessa vasta kirjoittamassa omistajaa.
                if time.time() - lock.stat().st_mtime < 10:
                    raise VersionError('Ytimen työtila on varattu. Yritä hetken kuluttua uudelleen.')
            if attempt:
                raise VersionError('Ytimen työtilan lukkoa ei voitu varata.')
            stale = home / ('stale-lock-' + uuid.uuid4().hex)
            lock.rename(stale)
            shutil.rmtree(stale)
    json_write(lock / 'owner.json', {'pid': os.getpid()})
    try:
        yield
    finally:
        shutil.rmtree(lock)


class WindowsJob:
    """KILL_ON_JOB_CLOSE: myös CMake/MSBuildin lapsiprosessit peruuntuvat."""
    def __init__(self, process):
        from ctypes import wintypes as w
        class Basic(ctypes.Structure):
            _fields_ = [('process_time', ctypes.c_int64), ('job_time', ctypes.c_int64),
                        ('flags', w.DWORD), ('min_ws', ctypes.c_size_t), ('max_ws', ctypes.c_size_t),
                        ('active', w.DWORD), ('affinity', ctypes.c_size_t), ('priority', w.DWORD), ('scheduling', w.DWORD)]
        class IO(ctypes.Structure):
            _fields_ = [(name, ctypes.c_uint64) for name in ('read_ops', 'write_ops', 'other_ops', 'read_bytes', 'write_bytes', 'other_bytes')]
        class Extended(ctypes.Structure):
            _fields_ = [('basic', Basic), ('io', IO), ('process_memory', ctypes.c_size_t),
                        ('job_memory', ctypes.c_size_t), ('peak_process', ctypes.c_size_t), ('peak_job', ctypes.c_size_t)]
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.kernel.CreateJobObjectW.restype = w.HANDLE
        self.handle = self.kernel.CreateJobObjectW(None, None)
        info = Extended()
        info.basic.flags = 0x2000
        if not self.handle or not self.kernel.SetInformationJobObject(w.HANDLE(self.handle), 9, ctypes.byref(info), ctypes.sizeof(info)):
            self.close()
            raise VersionError('Windowsin prosessiryhmää ei voitu luoda turvallista peruutusta varten.')
        if not self.kernel.AssignProcessToJobObject(w.HANDLE(self.handle), w.HANDLE(process._handle)):
            self.close()
            raise VersionError('Rakennusprosessia ei voitu liittää Windowsin peruutusryhmään.')

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(ctypes.c_void_p(self.handle))
            self.handle = None


class Backend:
    def __init__(self, home: Path, job: Path, upstream=UPSTREAM):
        self.home = home.resolve()
        self.job = job.resolve()
        self.home.mkdir(parents=True, exist_ok=True)
        self.job.mkdir(parents=True, exist_ok=True)
        self.upstream = upstream
        self.state = {'phase': 'starting', 'message': 'Valmistellaan toimintoa', 'started': time.time()}
        self.log = self.job / 'operation.log'
        self.cancelled = False

    def check_cancel(self):
        if self.cancelled or (self.job / 'cancel').exists():
            raise Cancelled('Toiminto peruutettiin. Nykyinen simulaattori säilyi ennallaan.')

    def publish(self, phase, message, **fields):
        self.state.update(phase=phase, message=message, **fields)
        self.state['updated'] = time.time()
        json_write(self.job / 'state.json', self.state)
        atomic(self.job / 'state.tsv', ''.join(f'{key}\t{quote(str(value), safe="")}\n' for key, value in self.state.items()))

    def run(self, command, cwd=None, timeout=1800, env=None):
        self.check_cancel()
        with self.log.open('ab') as log:
            log.write(('\n$ ' + subprocess.list2cmdline([str(x) for x in command]) + '\n').encode())
            log.flush()
            start = log.tell()
            options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {'start_new_session': True}
            process = subprocess.Popen([str(x) for x in command], cwd=cwd, stdout=log,
                                       stderr=subprocess.STDOUT, env=env, **options)
            job = None
            began = time.monotonic()
            try:
                if os.name == 'nt':
                    job = WindowsJob(process)
                while process.poll() is None:
                    self.check_cancel()
                    if time.monotonic() - began > timeout:
                        raise VersionError('Toiminnon aikaraja ylittyi. Katso tekninen loki ja yritä uudelleen.')
                    try:
                        process.wait(timeout=.08)
                    except subprocess.TimeoutExpired:
                        pass
            finally:
                if job:
                    job.close()
                if process.poll() is None:
                    if os.name == 'nt':
                        process.kill()
                    else:
                        os.killpg(process.pid, signal.SIGTERM)
                    try:
                        process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        if os.name != 'nt':
                            os.killpg(process.pid, signal.SIGKILL)
                        else:
                            process.kill()
                        process.wait()
            log.flush()
        with self.log.open('rb') as stream:
            stream.seek(start)
            output = stream.read(8 * 1024 * 1024).decode('utf-8', errors='replace')
        self.check_cancel()
        if process.returncode:
            raise VersionError(f'{Path(str(command[0])).name} epäonnistui (paluuarvo {process.returncode}). Katso tekninen loki.')
        return output.strip()

    def git(self, *arguments, cwd=None):
        env = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='never', GIT_CONFIG_NOSYSTEM='1', GIT_OPTIONAL_LOCKS='0')
        return self.run(['git', '-c', 'core.autocrlf=false', '-c', 'core.hooksPath=',
                         '-c', 'protocol.ext.allow=never', *arguments], cwd=cwd, timeout=180, env=env)

    def requirements(self, bundle: Path | None):
        required = [('Git', 'git'), ('CMake 3.25+', 'cmake'), ('CTest', 'ctest')]
        if os.name != 'nt':
            required += [('GCC C++', 'g++'), ('Make', 'make')]
        found = [(name, bool(shutil.which(tool))) for name, tool in required]
        found.insert(0, ('Python 3.10+', sys.version_info >= (3, 10)))
        if shutil.which('cmake'):
            version = self.run(['cmake', '--version'], timeout=15).splitlines()[0]
            match = re.search(r'(\d+)\.(\d+)', version)
            found = [(n, ok and bool(match and tuple(map(int, match.groups())) >= (3, 25))) if n.startswith('CMake') else (n, ok) for n, ok in found]
        if os.name == 'nt':
            vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
            vs = self.run([vswhere, '-latest', '-version', '[17.0,18.0)', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], timeout=15) if vswhere.exists() else ''
            found.append(('Visual Studio 2022 C++', bool(vs)))
            kits = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Windows Kits/10'
            import winreg
            for view in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY):
                try:
                    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\Windows Kits\Installed Roots', 0, winreg.KEY_READ | view) as key:
                        kits = Path(winreg.QueryValueEx(key, 'KitsRoot10')[0])
                        break
                except OSError:
                    pass
            sdk = any((include / 'um/Windows.h').is_file() and
                      (kits / 'Lib' / include.name / 'um/x64/kernel32.lib').is_file()
                      for include in (kits / 'Include').glob('*'))
            found.append(('Windows SDK x64', sdk))
        found.append(('Simulaattorin lähdepaketti', bool(bundle and bundle.is_file())))
        atomic(self.job / 'requirements.tsv', ''.join(f'{int(ok)}\t{quote(name, safe="")}\n' for name, ok in found))
        missing = ', '.join(name for name, ok in found if not ok)
        return missing

    def fetch(self):
        self.publish('listing', 'Haetaan versioita GitHubista')
        cache = self.home / 'upstream.git'
        if not cache.exists():
            temporary = self.home / ('fetch-' + uuid.uuid4().hex)
            try:
                self.git('clone', '--mirror', '--', self.upstream, temporary)
                temporary.rename(cache)
            finally:
                if temporary.exists():
                    shutil.rmtree(temporary)
        else:
            actual = self.git('remote', 'get-url', 'origin', cwd=cache)
            if actual != self.upstream:
                raise VersionError('Työtilan upstream-osoite ei vastaa valittua lähdettä. Käytä eri työtilaa.')
            self.git('fetch', '--prune', '--force', 'origin', '+refs/heads/*:refs/heads/*', '+refs/tags/*:refs/tags/*', cwd=cache)
        return cache

    def resolve(self, cache, ref):
        if SHA.fullmatch(ref):
            ref = ref.lower()
        elif ref == 'main':
            ref = 'refs/heads/main'
        elif not ref.startswith('refs/tags/'):
            raise VersionError('Anna main, refs/tags/TAGI tai täysi 40-merkkinen commit-SHA.')
        try:
            full = self.git('rev-parse', '--verify', '--end-of-options', ref + '^{commit}', cwd=cache)
        except VersionError as error:
            raise VersionError('Valittua committia tai tagia ei löydy lähdereposta. Tarkista tunniste ja päivitä lista.') from error
        if not SHA.fullmatch(full):
            raise VersionError('Version ratkaisusta ei saatu täyttä commit-SHA:ta.')
        return self.describe(cache, full)

    def describe(self, cache, full):
        raw = self.git('show', '-s', '--format=%H%x00%cI%x00%s%x00%b', full, cwd=cache).split('\0', 3)
        if len(raw) != 4 or raw[0] != full:
            raise VersionError('Commitin tiedot eivät vastaa valittua SHA:ta.')
        tags = self.git('tag', '--points-at', full, cwd=cache).splitlines()
        return {'sha': raw[0], 'date': raw[1], 'subject': raw[2], 'body': raw[3], 'tag': ', '.join(tags)}

    def versions(self):
        cache = self.fetch()
        records = []
        main = self.resolve(cache, 'main')
        records.append(dict(main, kind='main', label='Uusin upstream main'))
        tags = self.git('for-each-ref', '--sort=-creatordate', '--format=%(refname)', '--count=100', 'refs/tags/', cwd=cache).splitlines()
        for tag in tags:
            self.check_cancel()
            records.append(dict(self.resolve(cache, tag), kind='tag', label=tag.removeprefix('refs/tags/')))
        recent = self.git('log', '-60', '--format=%H', 'refs/heads/main', cwd=cache).splitlines()
        for sha in recent:
            self.check_cancel()
            records.append(dict(self.describe(cache, sha), kind='commit', label=sha[:10]))
        json_write(self.job / 'versions.json', records)
        atomic(self.job / 'versions.tsv', ''.join('\t'.join(quote(str(row[k]), safe='') for k in ('kind', 'label', 'sha', 'date', 'subject', 'body', 'tag')) + '\n' for row in records))
        self.publish('done', 'Versiolista päivitetty', count=len(records))
        return records

    def verify_checkout(self, core, sha):
        if self.git('rev-parse', 'HEAD', cwd=core) != sha or self.git('status', '--porcelain', '--untracked-files=normal', cwd=core):
            raise VersionError('Lähdekoodi ei vastaa lukittua puhdasta SHA:ta. Rakennus estetty.')

    def local_metadata(self, root):
        metadata = {'sha': 'tuntematon', 'date': 'tuntematon', 'tag': '', 'subject': root.name,
                    'body': '', 'dirty': 0, 'git': 0, 'source_kind': 'local',
                    'local_path': str(root), 'path': str(root)}
        if not shutil.which('git'):
            return metadata
        try:
            git_root = self.git('rev-parse', '--show-toplevel', cwd=root)
        except VersionError:
            return metadata
        # A directory inside an unrelated repository must not borrow its HEAD.
        if Path(git_root).resolve() != root:
            return metadata
        sha = self.git('rev-parse', '--verify', 'HEAD', cwd=root)
        if not SHA.fullmatch(sha):
            raise VersionError('Paikallisen Git-lähteen HEAD ei ole kelvollinen commit.')
        metadata.update(self.describe(root, sha))
        metadata.update(git=1, dirty=int(bool(self.git('status', '--porcelain', '--untracked-files=normal', cwd=root))))
        return metadata

    def inspect_local(self, path: Path, publish=True):
        root = path.expanduser().resolve()
        if not root.is_dir():
            raise VersionError('Paikallista ASkompu-lähdehakemistoa ei löydy.')
        if self.home.is_relative_to(root) or self.job.is_relative_to(root):
            raise VersionError('Rakennuksen työtilan ja lokien pitää sijaita ASkompu-lähteen ulkopuolella.')
        files = local_inventory(root)
        selected = dict(self.local_metadata(root), fingerprint=source_fingerprint(files),
                        file_count=len(files), compatibility_ok=1,
                        compatibility_message='Lähderakenne tunnistettu; API-yhteensopivuus varmistuu rakennuksessa ja testeissä')
        if publish:
            self.publish('done', 'Paikallinen ASkompu-lähde tarkistettu', **selected)
        return selected

    def capture_local(self, path: Path, core: Path):
        root = path.expanduser().resolve()
        selected = self.inspect_local(root, publish=False)
        files = local_inventory(root)
        if source_fingerprint(files) != selected['fingerprint']:
            raise VersionError('Paikallinen lähde muuttui tarkistuksen aikana. Yritä uudelleen.')
        core.mkdir(parents=True)
        try:
            for record in files:
                self.check_cancel()
                data = checked_read(root, record['path'])
                if len(data) != record['size'] or hashlib.sha256(data).hexdigest() != record['sha256']:
                    raise VersionError('Paikallinen lähde muuttui kopioinnin aikana. Tallenna tiedostot ja yritä uudelleen.')
                target = core / record['path']
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
            after = self.inspect_local(root, publish=False)
            if after != selected:
                raise VersionError('Paikallinen lähde muuttui kopioinnin aikana. Tallenna tiedostot ja yritä uudelleen.')
            manifest = dict(selected, manifest_version=1, files=files)
            manifest_path = core / SNAPSHOT_NAME
            json_write(manifest_path, manifest)
            verify_snapshot(core, manifest_path)
            return selected, manifest_path
        except BaseException:
            shutil.rmtree(core)
            raise

    def extract_bundle(self, bundle: Path, source: Path):
        self.bundle_inventory(bundle)
        with zipfile.ZipFile(bundle) as archive:
            archive.extractall(source)

    def bundle_inventory(self, bundle):
        files, names = [], set()
        with zipfile.ZipFile(bundle) as archive:
            if sum(m.file_size for m in archive.infolist()) > SNAPSHOT_LIMIT:
                raise VersionError('Simulaattorin lähdepaketti ylittää kokorajan.')
            for member in archive.infolist():
                parts = member.filename.rstrip('/').split('/')
                if (member.filename in names or any(p in ('', '.', '..') for p in parts) or
                    any(c in member.filename for c in ('\\', ';', ':', '\n', '\r')) or
                    member.file_size > 32 * 1024 * 1024 or stat.S_ISLNK(member.external_attr >> 16)):
                    raise VersionError('Simulaattorin lähdepaketissa on virheellinen polku tai tiedostokoko.')
                names.add(member.filename)
                if not member.is_dir():
                    files.append({'path': member.filename, 'sha256': hashlib.sha256(archive.read(member)).hexdigest()})
        return sorted(files, key=lambda f: f['path'])

    def prepare_local_cache(self, bundle, snapshot, selected, profile, cmake_options):
        """Reuse only a verified simulator/source/profile combination, never the user tree."""
        self.check_cancel()
        identity = {'local_path': selected['local_path'], 'profile': profile,
                    # ZIP directory timestamps change on rebuild; key the exact source contents.
                    'bundle_fingerprint': source_fingerprint(self.bundle_inventory(bundle)),
                    'platform': platform.system() + '-' + platform.machine(),
                    'cmake_options': list(cmake_options)}
        key = hashlib.sha256(json.dumps(identity, sort_keys=True, ensure_ascii=False).encode()).hexdigest()
        cache = self.home / 'local-cache' / key
        core, source = cache/'core', cache/'source'
        if cache.is_symlink():
            raise VersionError('Paikallisen rakennuksen välimuistin polku muuttui.')
        if cache.exists():
            if cache.is_symlink() or json.loads((cache/'identity.json').read_text()) != identity:
                raise VersionError('Paikallisen rakennuksen välimuistin alkuperätiedot muuttuivat.')
            self.recover_local_cache_update(cache, identity)
            previous = verify_snapshot(core, core/SNAPSHOT_NAME)
            previous_files = {f['path']: f for f in previous['files']}
            self.verify_cached_bundle(bundle, source)
        else:
            # Publish the initial cache only after its source and core are complete.
            cache.parent.mkdir(parents=True, exist_ok=True)
            creating = cache.with_name('creating-' + key + '-' + uuid.uuid4().hex)
            try:
                creating.mkdir()
                self.extract_bundle(bundle, creating/'source')
                shutil.copytree(snapshot, creating/'core')
                json_write(creating/'identity.json', identity)
                verify_snapshot(creating/'core', creating/'core'/SNAPSHOT_NAME)
                self.verify_cached_bundle(bundle, creating/'source')
                self.check_cancel()
                creating.rename(cache)
                return core, source
            finally:
                if creating.exists():
                    shutil.rmtree(creating)
        manifest = verify_snapshot(snapshot, snapshot/SNAPSHOT_NAME)
        if previous == manifest:
            return core, source
        current_files = {f['path']: f for f in manifest['files']}
        transaction = cache/('core-update-' + uuid.uuid4().hex)
        staged = transaction/'next'
        marker = cache/'core-update.json'
        try:
            # copy2 preserves unchanged mtimes. Build/dependency directories stay in place.
            shutil.copytree(core, staged)
            for relative in previous_files.keys() - current_files.keys():
                self.check_cancel()
                (staged/relative).unlink()
            for relative, record in current_files.items():
                self.check_cancel()
                if previous_files.get(relative) == record:
                    continue
                target = staged/relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(checked_read(snapshot, relative))
            json_write(staged/SNAPSHOT_NAME, manifest)
            verify_snapshot(staged, staged/SNAPSHOT_NAME)
            self.check_cancel()
            json_write(marker, {'transaction': transaction.name, 'identity': identity,
                               'previous_manifest_sha256': hashlib.sha256((core/SNAPSHOT_NAME).read_bytes()).hexdigest(),
                               'next_manifest_sha256': hashlib.sha256((staged/SNAPSHOT_NAME).read_bytes()).hexdigest()})
            core.rename(transaction/'previous')
            staged.rename(core)
            verify_snapshot(core, core/SNAPSHOT_NAME)
            self.verify_cached_bundle(bundle, source)
            self.check_cancel()
            marker.unlink()
        except BaseException:
            if marker.exists():
                # If recovery itself fails, retain the marker and backups for a safe retry.
                self.recover_local_cache_update(cache, identity)
            raise
        finally:
            if transaction.exists() and not marker.exists():
                shutil.rmtree(transaction)
        return core, source

    def recover_local_cache_update(self, cache, identity):
        """Roll back only a fully identified interrupted directory swap; reject tampering."""
        marker = cache/'core-update.json'
        if marker.is_symlink():
            raise VersionError('Paikallisen rakennuksen palautusmerkintä muuttui.')
        if not marker.exists():
            return
        update = json.loads(marker.read_text(encoding='utf-8'))
        name = update.get('transaction') if isinstance(update, dict) else None
        if (not isinstance(name, str) or not re.fullmatch('core-update-[0-9a-f]{32}', name)
                or update.get('identity') != identity
                or any(not isinstance(update.get(k), str) or not re.fullmatch('[0-9a-f]{64}', update[k])
                       for k in ('previous_manifest_sha256', 'next_manifest_sha256'))):
            raise VersionError('Paikallisen rakennuksen palautusmerkintä ei ole kelvollinen.')
        transaction = cache/name
        if transaction.is_symlink() or not transaction.is_dir() or any(p.name not in ('previous', 'next', 'aborted') for p in transaction.iterdir()):
            raise VersionError('Paikallisen rakennuksen palautushakemisto muuttui.')
        previous_hash = update['previous_manifest_sha256']
        next_hash = update['next_manifest_sha256']
        found = {}
        candidates = {'core': cache/'core', 'previous': transaction/'previous',
                      'next': transaction/'next', 'aborted': transaction/'aborted'}
        for role, path in candidates.items():
            if not path.exists() and not path.is_symlink():
                continue
            manifest = verify_snapshot(path, path/SNAPSHOT_NAME)
            digest = hashlib.sha256((path/SNAPSHOT_NAME).read_bytes()).hexdigest()
            allowed = (previous_hash, next_hash) if role == 'core' else (previous_hash,) if role == 'previous' else (next_hash,)
            if digest not in allowed or manifest['local_path'] != identity['local_path']:
                raise VersionError('Keskeneräisen rakennuksen lähdetilannekuva muuttui; automaattinen palautus estetty.')
            found[role] = digest
        if 'previous' in found:
            if 'core' in found:
                if found['core'] != next_hash or 'aborted' in found:
                    raise VersionError('Paikallisen rakennuksen palautustila on ristiriitainen.')
                candidates['core'].rename(candidates['aborted'])
            candidates['previous'].rename(candidates['core'])
        elif found.get('core') != previous_hash:
            raise VersionError('Keskeneräisen rakennuksen aiempaa varmennettua tilannekuvaa ei löydy.')
        restored = verify_snapshot(candidates['core'], candidates['core']/SNAPSHOT_NAME)
        if hashlib.sha256((candidates['core']/SNAPSHOT_NAME).read_bytes()).hexdigest() != previous_hash:
            raise VersionError('Paikallisen rakennuksen palautus epäonnistui.')
        marker.unlink()
        shutil.rmtree(transaction)
        self.publish('downloading', 'Keskeneräinen paikallinen lähdepäivitys palautettiin turvallisesti', recovered_fingerprint=restored['fingerprint'])

    def verify_cached_bundle(self, bundle, source):
        # Generated build/ contents are not simulator sources. Bundle members must remain exact.
        if source.is_symlink() or source.parent.is_symlink():
            raise VersionError('Simulaattorin lähdevälimuisti sisältää virheellisen polun.')
        expected = self.bundle_inventory(bundle)
        actual = []
        def visit(directory):
            for path in sorted(directory.iterdir()):
                relative = path.relative_to(source).as_posix()
                if relative == 'simulator/build':
                    continue
                if path.is_symlink():
                    raise VersionError('Simulaattorin lähdevälimuisti sisältää virheellisen polun.')
                if path.is_dir():
                    visit(path)
                else:
                    actual.append({'path': relative, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        visit(source)
        if sorted(actual, key=lambda f: f['path']) != expected:
            raise VersionError('Simulaattorin lähdevälimuisti muuttui. Rakennus estetty.')

    def build(self, ref, bundle: Path, cmake_options=(), local: Path | None = None, profile='480x320'):
        if profile not in ('320x170', '480x320'):
            raise VersionError('Tuntematon näyttöprofiili.')
        if local is not None and ref is not None:
            raise VersionError('Valitse Git-versio tai paikallinen lähde, ei molempia.')
        if local is None:
            ref = ref if ref is not None else 'main'
            if not SHA.fullmatch(ref) and ref != 'main' and not ref.startswith('refs/tags/'):
                raise VersionError('Anna main, refs/tags/TAGI tai täysi 40-merkkinen commit-SHA.')
        missing = self.requirements(bundle)
        if missing:
            raise VersionError('Rakennuksesta puuttuu: ' + missing)
        artifact = uuid.uuid4().hex
        root = self.home / 'builds' / artifact
        root.mkdir(parents=True)
        core = root / 'core'
        manifest_path = None
        if local is not None:
            self.publish('downloading', 'Otetaan paikallisista lähteistä muuttumaton tilannekuva', artifact=artifact, profile=profile)
            selected, manifest_path = self.capture_local(local, core)
            self.publish('downloading', 'Paikallinen lähdetilannekuva lukittu', **selected)
        else:
            cache = self.fetch()
            selected = self.resolve(cache, ref)
            self.publish('downloading', 'Ladataan lukitun version lähdekoodia', artifact=artifact, profile=profile, source_kind='github', **selected)
            self.git('clone', '--no-local', '--no-hardlinks', '--no-checkout', '--', cache, core)
            self.git('checkout', '--detach', selected['sha'], cwd=core)
        sha = selected['sha']
        original_snapshot = core if manifest_path else None
        source = root / 'source'
        if manifest_path:
            core, source = self.prepare_local_cache(bundle, original_snapshot, selected, profile, cmake_options)
            manifest_path = core/SNAPSHOT_NAME
        else:
            self.extract_bundle(bundle, source)
        def verify():
            if manifest_path:
                original = verify_snapshot(original_snapshot, original_snapshot/SNAPSHOT_NAME)
                verified = verify_snapshot(core, manifest_path)
                if any({k: record.get(k) for k in selected} != selected for record in (original, verified)):
                    raise VersionError('Paikallisen tilannekuvan alkuperätiedot muuttuivat. Rakennus estetty.')
                self.verify_cached_bundle(bundle, source)
            else:
                self.verify_checkout(core, sha)
        verify()
        self.check_cancel()
        preset = 'windows-release' if os.name == 'nt' else 'linux-release'
        simulator = source / 'simulator'
        build = simulator / 'build' / preset
        self.publish('configuring', 'Valmistellaan erillistä rakennusta')
        configure = ['cmake', '--preset', preset, f'-DASKOMPU_CORE_ROOT={core}', f'-DASKOMPU_CORE_REVISION={sha}', f'-DASKOMPU_DISPLAY_PROFILE={profile}']
        if manifest_path:
            configure.append(f'-DASKOMPU_CORE_SNAPSHOT={manifest_path}')
        self.run([*configure, *cmake_options], cwd=simulator)
        verify()
        self.publish('building', 'Käännetään simulaattoria valitulla ASkompu-ytimellä')
        self.run(['cmake', '--build', '--preset', preset], cwd=simulator)
        verify()
        self.publish('testing', 'Suoritetaan kaikki moottori-, ohjain- ja tuotantotestit')
        self.run(['ctest', '--preset', preset], cwd=simulator)
        verify()
        stage = root / 'app'
        self.publish('installing', 'Asennetaan erillistä testattua ohjelmaa')
        self.run(['cmake', '--install', build, '--config', 'Release', '--component', 'Runtime', '--prefix', stage], cwd=simulator)
        verify()
        exe = stage / ('askompu-simulaattori.exe' if os.name == 'nt' else 'bin/askompu-simulaattori')
        environment = dict(os.environ, SDL_VIDEODRIVER='dummy')
        self.publish('validating', 'Varmistetaan asennetun ohjelman versio, näyttöprofiili ja käynnistyminen')
        version = self.run([exe, '--versio'], cwd=root, env=environment, timeout=30)
        expected_worktree = ('paikallisia muutoksia' if selected.get('dirty') else 'puhdas') if selected.get('git', 1) else 'ei Git-tietoja'
        expected = [f'ASkompu-ydin: {sha}', f'Ytimen työpuu: {expected_worktree}', f'Näyttöprofiili: {profile}']
        if manifest_path:
            expected += ['Ytimen lähde: local', f'Paikallinen lähde: {selected["local_path"]}', f'ASkompu-lähteiden SHA-256: {selected["fingerprint"]}']
        if any(line not in version.splitlines() for line in expected):
            raise VersionError('Valmis ohjelma raportoi väärän ytimen. Uudelleenkäynnistys estetty.')
        self.run([exe, '--tarkista'], cwd=root, env=environment, timeout=120)
        self.check_cancel()
        verify()
        ready = dict(selected, profile=profile, source_kind='local' if local else 'github', artifact=artifact, executable=str(exe), binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), tested=True)
        json_write(root / 'ready.json', ready)
        json_write(self.home / 'latest-ready.json', ready)
        self.publish('ready', 'ASkompu-ydin valmis', executable=str(exe), profile=profile, **selected)
        return ready

    def validated_ready(self, artifact):
        """One integrity gate for restored GUI readiness and forward launching."""
        if not isinstance(artifact, str) or not re.fullmatch('[0-9a-f]{32}', artifact):
            raise VersionError('Valmiin rakennuksen tunniste ei ole kelvollinen.')
        root = self.home/'builds'/artifact
        if root.is_symlink() or root.parent.is_symlink():
            raise VersionError('Valmiin rakennuksen polku muuttui.')
        ready = json.loads((root/'ready.json').read_text(encoding='utf-8'))
        if (not isinstance(ready, dict) or ready.get('artifact') != artifact or ready.get('tested') is not True
                or ready.get('profile') not in ('320x170', '480x320')
                or ready.get('source_kind') not in ('local', 'github')
                or not isinstance(ready.get('sha'), str)
                or not (SHA.fullmatch(ready['sha']) or ready['sha'] == 'tuntematon' and ready['source_kind'] == 'local')
                or not isinstance(ready.get('date'), str)
                or not isinstance(ready.get('binary_sha256'), str) or not re.fullmatch('[0-9a-f]{64}', ready['binary_sha256'])
                or not isinstance(ready.get('executable'), str)):
            raise VersionError('Valmiin rakennuksen varmennustiedot puuttuvat tai ovat muuttuneet. Rakenna versio uudelleen.')
        if ready['source_kind'] == 'local' and (not isinstance(ready.get('local_path'), str) or not os.path.isabs(ready['local_path'])
                or not isinstance(ready.get('fingerprint'), str) or not re.fullmatch('[0-9a-f]{64}', ready['fingerprint'])):
            raise VersionError('Valmiin paikallisen rakennuksen lähdetiedot eivät ole kelvolliset.')
        exe = Path(ready['executable'])
        if (not exe.is_absolute() or not exe.resolve().is_relative_to(root.resolve()) or not exe.is_file()
                or hashlib.sha256(exe.read_bytes()).hexdigest() != ready['binary_sha256']):
            raise VersionError('Valmis ohjelma puuttuu tai on muuttunut. Rakenna versio uudelleen.')
        return ready

    def validated_previous(self):
        history = json.loads((self.home/'history.json').read_text(encoding='utf-8'))
        if (not isinstance(history, dict) or not isinstance(history.get('previous'), str)
                or not isinstance(history.get('previous_sha256'), str)
                or not re.fullmatch('[0-9a-f]{64}', history['previous_sha256'])):
            raise VersionError('Edellisen ohjelman eheystieto puuttuu. Vanhaa palautustietoa ei voi käyttää turvallisesti.')
        exe = Path(history['previous'])
        if not exe.is_absolute() or not exe.is_file() or hashlib.sha256(exe.read_bytes()).hexdigest() != history['previous_sha256']:
            raise VersionError('Edellinen ohjelma puuttuu tai on muuttunut. Palautus estetty.')
        return exe, history['previous_sha256']

    def doctor(self, bundle):
        missing = self.requirements(bundle)
        ready, previous = {}, ''
        ready_warning, previous_warning = '', ''
        if (self.home/'latest-ready.json').exists():
            try:
                latest = json.loads((self.home/'latest-ready.json').read_text(encoding='utf-8'))
                if not isinstance(latest, dict):
                    raise VersionError('Viimeisimmän rakennuksen tiedot eivät ole kelvolliset.')
                ready = self.validated_ready(latest.get('artifact'))
                if ready != latest:
                    raise VersionError('Viimeisimmän rakennuksen tiedot eivät vastaa varmennettua ohjelmaa.')
            except (VersionError, OSError, ValueError) as error:
                ready = {}
                ready_warning = 'Aiempi valmis rakennus ei ole käytettävissä: ' + str(error)
        if (self.home/'history.json').exists():
            try:
                exe, _ = self.validated_previous()
                previous = str(exe)
            except (VersionError, OSError, ValueError) as error:
                previous_warning = str(error)
        self.publish('done', 'Rakennustyökalut löytyivät' if not missing else 'Puuttuvat työkalut: ' + missing,
                     requirements_ok=int(not missing), previous=previous, previous_warning=previous_warning,
                     artifact=ready.get('artifact', ''), ready_exists=int(bool(ready)), ready_warning=ready_warning,
                     ready_sha=ready.get('sha', ''), ready_tag=ready.get('tag', ''), ready_date=ready.get('date', ''),
                     ready_profile=ready.get('profile', ''), ready_executable=ready.get('executable', ''),
                     ready_source_kind=ready.get('source_kind', ''), ready_local_path=ready.get('local_path', ''),
                     ready_fingerprint=ready.get('fingerprint', ''), ready_dirty=ready.get('dirty', 0))

    def launch(self, artifact, current: Path, rollback=False):
        history_path = self.home / 'history.json'
        if rollback:
            exe, launched_digest = self.validated_previous()
        else:
            ready = self.validated_ready(artifact)
            exe = Path(ready['executable'])
            launched_digest = ready['binary_sha256']
        if not exe.is_file() or not current.is_file() or exe.resolve() == current.resolve():
            raise VersionError('Käynnistettävää tai edellistä ohjelmaa ei löydy.')
        current_digest = hashlib.sha256(current.read_bytes()).hexdigest()
        self.publish('launching', 'Käynnistetään uutta simulaattoria; nykyinen pysyy vielä auki')
        ready_file = self.job / ('startup-' + uuid.uuid4().hex)
        env = dict(os.environ)
        env.pop('SDL_VIDEODRIVER', None)
        log = (self.job / 'startup.log').open('ab')
        command = [str(exe), '--core-home', str(self.home), '--startup-ready', str(ready_file)]
        if self.upstream != UPSTREAM:
            command += ['--core-upstream', self.upstream]
        options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {'start_new_session': True}
        process = subprocess.Popen(command, cwd=exe.parent, stdout=log, stderr=subprocess.STDOUT, env=env, **options)
        log.close()
        began = time.monotonic()
        try:
            while not ready_file.exists():
                self.check_cancel()
                if process.poll() is not None or time.monotonic() - began > 30:
                    raise VersionError('Uusi ikkuna ei käynnistynyt. Nykyinen ohjelma säilyy käytössä; katso käynnistysloki.')
                time.sleep(.08)
            time.sleep(.3)
            self.check_cancel()
            if process.poll() is not None:
                raise VersionError('Uusi ohjelma sulkeutui käynnistyksessä. Nykyinen ohjelma säilyy käytössä.')
            json_write(history_path, {'current': str(exe.resolve()), 'current_sha256': launched_digest,
                                      'previous': str(current.resolve()), 'previous_sha256': current_digest, 'pid': process.pid})
            self.publish('launched', 'Uusi simulaattori on käynnissä. Aloitetaan uusi sessio.', executable=str(exe))
        except BaseException:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            raise
        finally:
            ready_file.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['doctor', 'list', 'resolve', 'inspect-local', 'build', 'launch', 'rollback'])
    parser.add_argument('--home', type=Path, required=True, help='Erillinen hallittu työtila')
    parser.add_argument('--job', type=Path, help='Tilaviestien ja lokien hakemisto')
    parser.add_argument('--upstream', default=UPSTREAM, help='Git-repo; paikallinen repo sopii testeihin')
    source = parser.add_mutually_exclusive_group()
    source.add_argument('--ref', help='main, refs/tags/TAGI tai täysi SHA (oletus main)')
    source.add_argument('--local', type=Path, help='Paikallinen ASkompu-lähde; myös tallennetut committoimattomat muutokset')
    parser.add_argument('--profile', choices=['320x170', '480x320'], default='480x320', help='Rakennettavan simulaattorin natiivi näyttöprofiili')
    parser.add_argument('--bundle', type=Path, default=Path(__file__).resolve().parents[1] / 'simulator-source.zip')
    parser.add_argument('--artifact')
    parser.add_argument('--current', type=Path)
    parser.add_argument('--cmake-option', action='append', default=[], help='Kehittäjän CMake-lisävalinta; ei saa ohittaa ytimen lukitusta')
    args = parser.parse_args(argv)
    if args.local:
        local_root = args.local.expanduser().resolve()
        if args.home.resolve().is_relative_to(local_root) or (args.job and args.job.resolve().is_relative_to(local_root)):
            parser.error('--home ja --job eivät saa sijaita paikallisen ASkompu-lähteen sisällä')
    backend = Backend(args.home, args.job or args.home / 'jobs' / uuid.uuid4().hex, args.upstream)
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: setattr(backend, 'cancelled', True))
    try:
        backend.publish('starting', 'Valmistellaan toimintoa', operation=args.operation)
        with workspace_lock(backend.home):
            if args.local and args.operation not in ('inspect-local', 'build'):
                raise VersionError('--local on käytössä vain inspect-local- ja build-toiminnoissa.')
            if args.operation == 'doctor':
                backend.doctor(args.bundle)
            elif args.operation == 'list':
                backend.versions()
            elif args.operation == 'resolve':
                ref = args.ref if args.ref is not None else 'main'
                if not SHA.fullmatch(ref) and ref != 'main' and not ref.startswith('refs/tags/'):
                    raise VersionError('Anna main, refs/tags/TAGI tai täysi 40-merkkinen commit-SHA.')
                selected = backend.resolve(backend.fetch(), ref)
                backend.publish('done', 'Versio lukittu täyteen commit-SHA:han', **selected)
            elif args.operation == 'inspect-local':
                if not args.local:
                    raise VersionError('Paikallisen ASkompu-lähteen polku puuttuu (--local).')
                backend.inspect_local(args.local)
            elif args.operation == 'build':
                if any(not option.startswith('-DFETCHCONTENT_SOURCE_DIR_') for option in args.cmake_option):
                    raise VersionError('Lisävalinnoilla saa antaa vain lukittujen riippuvuuksien lähdehakemistoja.')
                backend.build(args.ref, args.bundle, args.cmake_option, args.local, args.profile)
            else:
                if not args.current:
                    raise VersionError('Nykyisen ohjelman polku puuttuu.')
                backend.launch(args.artifact, args.current.resolve(), args.operation == 'rollback')
        print(json.dumps(backend.state, ensure_ascii=False))
        return 0
    except Cancelled as error:
        backend.publish('cancelled', str(error))
        print(json.dumps(backend.state, ensure_ascii=False))
        return 2
    except (VersionError, OSError, ValueError, zipfile.BadZipFile) as error:
        phase = backend.state['phase']
        prefix = 'Valittu ydin ei ehkä ole yhteensopiva simulaattorin kanssa. ' if phase in ('configuring', 'building', 'testing') else ''
        if phase == 'listing':
            prefix = 'Versioiden haku epäonnistui. Tarkista verkkoyhteys, lähderepo ja version tunniste. '
        backend.publish('failed', prefix + str(error), failed_phase=phase)
        print(json.dumps(backend.state, ensure_ascii=False))
        return 1


if __name__ == '__main__':
    sys.exit(main())
