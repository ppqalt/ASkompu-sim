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
import re
import shutil
import signal
import subprocess
import sys
import time
import uuid
from urllib.parse import quote
import zipfile

UPSTREAM = 'https://github.com/Mikky100/ASkompu.git'
SHA = re.compile(r'^[0-9a-fA-F]{40}$')
TERMINAL = {'ready', 'done', 'failed', 'cancelled', 'launched'}


class Cancelled(Exception):
    pass


class VersionError(Exception):
    pass


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
        env = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='never', GIT_CONFIG_NOSYSTEM='1')
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

    def build(self, ref, bundle: Path, cmake_options=()):
        missing = self.requirements(bundle)
        if missing:
            raise VersionError('Rakennuksesta puuttuu: ' + missing)
        cache = self.fetch()
        selected = self.resolve(cache, ref)
        sha = selected['sha']
        artifact = uuid.uuid4().hex
        root = self.home / 'builds' / artifact
        root.mkdir(parents=True)
        self.publish('downloading', 'Ladataan lukitun version lähdekoodia', artifact=artifact, **selected)
        core = root / 'core'
        self.git('clone', '--no-local', '--no-hardlinks', '--no-checkout', '--', cache, core)
        self.git('checkout', '--detach', sha, cwd=core)
        self.verify_checkout(core, sha)
        self.check_cancel()
        source = root / 'source'
        with zipfile.ZipFile(bundle) as archive:
            for member in archive.infolist():
                target = (source / member.filename).resolve()
                if not target.is_relative_to(source.resolve()) or member.file_size > 32 * 1024 * 1024:
                    raise VersionError('Simulaattorin lähdepaketissa on virheellinen polku tai tiedostokoko.')
            if sum(m.file_size for m in archive.infolist()) > 128 * 1024 * 1024:
                raise VersionError('Simulaattorin lähdepaketti ylittää kokorajan.')
            archive.extractall(source)
        preset = 'windows-release' if os.name == 'nt' else 'linux-release'
        simulator = source / 'simulator'
        build = simulator / 'build' / preset
        self.publish('configuring', 'Valmistellaan erillistä rakennusta')
        self.run(['cmake', '--preset', preset, f'-DASKOMPU_CORE_ROOT={core}', f'-DASKOMPU_CORE_REVISION={sha}', *cmake_options], cwd=simulator)
        self.verify_checkout(core, sha)
        self.publish('building', 'Käännetään simulaattoria valitulla ASkompu-ytimellä')
        self.run(['cmake', '--build', '--preset', preset], cwd=simulator)
        self.publish('testing', 'Suoritetaan kaikki moottori-, ohjain- ja tuotantotestit')
        self.run(['ctest', '--preset', preset], cwd=simulator)
        self.verify_checkout(core, sha)
        stage = root / 'app'
        self.run(['cmake', '--install', build, '--config', 'Release', '--component', 'Runtime', '--prefix', stage], cwd=simulator)
        exe = stage / ('askompu-simulaattori.exe' if os.name == 'nt' else 'bin/askompu-simulaattori')
        environment = dict(os.environ, SDL_VIDEODRIVER='dummy')
        version = self.run([exe, '--versio'], cwd=root, env=environment, timeout=30)
        if f'ASkompu-ydin: {sha}' not in version or 'Ytimen työpuu: puhdas' not in version:
            raise VersionError('Valmis ohjelma raportoi väärän ytimen. Uudelleenkäynnistys estetty.')
        self.run([exe, '--tarkista'], cwd=root, env=environment, timeout=120)
        self.check_cancel()
        ready = dict(selected, artifact=artifact, executable=str(exe), binary_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(), tested=True)
        json_write(root / 'ready.json', ready)
        json_write(self.home / 'latest-ready.json', ready)
        self.publish('ready', 'ASkompu-ydin valmis', executable=str(exe), **selected)
        return ready

    def launch(self, artifact, current: Path, rollback=False):
        history_path = self.home / 'history.json'
        if rollback:
            history = json.loads(history_path.read_text(encoding='utf-8'))
            exe = Path(history['previous'])
        else:
            if not re.fullmatch('[0-9a-f]{32}', artifact or ''):
                raise VersionError('Valmiin rakennuksen tunniste puuttuu.')
            root = self.home / 'builds' / artifact
            ready = json.loads((root / 'ready.json').read_text(encoding='utf-8'))
            exe = Path(ready['executable'])
            if not ready.get('tested') or not exe.resolve().is_relative_to(root.resolve()) or hashlib.sha256(exe.read_bytes()).hexdigest() != ready['binary_sha256']:
                raise VersionError('Valmis rakennus on muuttunut tai testaamatta. Käynnistys estetty.')
        if not exe.is_file() or not current.is_file() or exe.resolve() == current.resolve():
            raise VersionError('Käynnistettävää tai edellistä ohjelmaa ei löydy.')
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
            if process.poll() is not None:
                raise VersionError('Uusi ohjelma sulkeutui käynnistyksessä. Nykyinen ohjelma säilyy käytössä.')
            json_write(history_path, {'current': str(exe.resolve()), 'previous': str(current.resolve()), 'pid': process.pid})
            self.publish('launched', 'Uusi simulaattori on käynnissä. Aloitetaan uusi sessio.', executable=str(exe))
        except BaseException:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    process.kill()
            raise
        finally:
            ready_file.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=['doctor', 'list', 'resolve', 'build', 'launch', 'rollback'])
    parser.add_argument('--home', type=Path, required=True, help='Erillinen hallittu työtila')
    parser.add_argument('--job', type=Path, help='Tilaviestien ja lokien hakemisto')
    parser.add_argument('--upstream', default=UPSTREAM, help='Git-repo; paikallinen repo sopii testeihin')
    parser.add_argument('--ref', default='main', help='main, refs/tags/TAGI tai täysi SHA')
    parser.add_argument('--bundle', type=Path, default=Path(__file__).resolve().parents[1] / 'simulator-source.zip')
    parser.add_argument('--artifact')
    parser.add_argument('--current', type=Path)
    parser.add_argument('--cmake-option', action='append', default=[], help='Kehittäjän CMake-lisävalinta; ei saa ohittaa ytimen lukitusta')
    args = parser.parse_args(argv)
    backend = Backend(args.home, args.job or args.home / 'jobs' / uuid.uuid4().hex, args.upstream)
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: setattr(backend, 'cancelled', True))
    try:
        backend.publish('starting', 'Valmistellaan toimintoa', operation=args.operation)
        with workspace_lock(backend.home):
            if args.operation == 'doctor':
                missing = backend.requirements(args.bundle)
                previous = ''
                if (backend.home / 'history.json').exists():
                    previous = json.loads((backend.home / 'history.json').read_text(encoding='utf-8')).get('previous', '')
                latest = ''
                if (backend.home / 'latest-ready.json').exists():
                    latest = json.loads((backend.home / 'latest-ready.json').read_text(encoding='utf-8')).get('artifact', '')
                ready_info = {}
                if latest:
                    ready_info = json.loads((backend.home / 'latest-ready.json').read_text(encoding='utf-8'))
                backend.publish('done', 'Rakennustyökalut löytyivät' if not missing else 'Puuttuvat työkalut: ' + missing,
                                requirements_ok=int(not missing), previous=previous, artifact=latest,
                                ready_sha=ready_info.get('sha', ''), ready_tag=ready_info.get('tag', ''))
            elif args.operation == 'list':
                backend.versions()
            elif args.operation == 'resolve':
                if not SHA.fullmatch(args.ref) and args.ref != 'main' and not args.ref.startswith('refs/tags/'):
                    raise VersionError('Anna main, refs/tags/TAGI tai täysi 40-merkkinen commit-SHA.')
                selected = backend.resolve(backend.fetch(), args.ref)
                backend.publish('done', 'Versio lukittu täyteen commit-SHA:han', **selected)
            elif args.operation == 'build':
                if any(not option.startswith('-DFETCHCONTENT_SOURCE_DIR_') for option in args.cmake_option):
                    raise VersionError('Lisävalinnoilla saa antaa vain lukittujen riippuvuuksien lähdehakemistoja.')
                backend.build(args.ref, args.bundle, args.cmake_option)
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
