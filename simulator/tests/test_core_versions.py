"""Versionvalitsimen eristetyt Git-, prosessi- ja tilasiirtymätestit."""
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/core_versions.py'
spec = importlib.util.spec_from_file_location('core_versions', SCRIPT)
cv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cv)


def git(root, *args):
    return subprocess.check_output(['git', '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', *args], cwd=root, stderr=subprocess.DEVNULL, text=True).strip()


class VersionsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ASkompu ää ')
        self.root = Path(self.temp.name)
        self.repo = self.root / 'upstream'
        self.repo.mkdir()
        git(self.repo, 'init', '-b', 'main')
        (self.repo / 'version.txt').write_text('old')
        git(self.repo, 'add', '.')
        git(self.repo, 'commit', '-m', 'Ensimmäinen versio ääkkösillä')
        self.old = git(self.repo, 'rev-parse', 'HEAD')
        git(self.repo, 'tag', '-a', 'test-preview', '-m', 'Tagi')
        (self.repo / 'version.txt').write_text('new')
        git(self.repo, 'commit', '-am', 'Uusi versio\n\nPitkä kuvaus ja tekniset tiedot.')
        self.new = git(self.repo, 'rev-parse', 'HEAD')
        self.backend = cv.Backend(self.root / 'home', self.root / 'job', str(self.repo))
        self.bundle = self.root / 'source.zip'
        with zipfile.ZipFile(self.bundle, 'w') as z:
            z.writestr('simulator/CMakeLists.txt', 'test fixture')

    def tearDown(self):
        self.temp.cleanup()

    def test_main_tag_exact_sha_and_utf8_metadata(self):
        records = self.backend.versions()
        cache = self.root / 'home/upstream.git'
        self.assertEqual(records[0]['sha'], self.new)
        self.assertEqual(self.backend.resolve(cache, 'refs/tags/test-preview')['sha'], self.old)
        selected = self.backend.resolve(cache, self.old)
        self.assertIn('ääkkösillä', selected['subject'])
        self.assertEqual(selected['tag'], 'test-preview')
        self.assertIn('%C3%A4', (self.root / 'job/versions.tsv').read_text(encoding='utf-8'))
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), self.new)
        self.assertEqual(git(self.repo, 'status', '--porcelain'), '')

    def test_invalid_and_unknown_refs_fail(self):
        cache = self.backend.fetch()
        for ref in ('HEAD~1', '--help', 'shortsha', 'refs/tags/no-such-tag', 'a' * 40):
            with self.subTest(ref=ref), self.assertRaises(cv.VersionError):
                self.backend.resolve(cache, ref)

    def test_cli_resolve_explicit_empty_or_whitespace_ref_fails_before_fetch(self):
        for ref in ('', ' ', '\t\n'):
            with self.subTest(ref=repr(ref)), patch.object(cv.Backend, 'fetch', side_effect=AssertionError('invalid ref must not fetch')) as fetch:
                result = cv.main(['resolve', '--home', str(self.backend.home), '--job', str(self.backend.job), '--ref', ref])
                self.assertEqual(result, 1)
                fetch.assert_not_called()
                state = json.loads((self.backend.job/'state.json').read_text(encoding='utf-8'))
                self.assertEqual(state['phase'], 'failed')
                self.assertEqual(state['failed_phase'], 'starting')
                self.assertIn('40-merkkinen', state['message'])

    def test_cli_resolve_omitted_ref_defaults_to_actual_main(self):
        with patch.object(cv.Backend, 'fetch', return_value=self.repo) as fetch:
            result = cv.main(['resolve', '--home', str(self.backend.home), '--job', str(self.backend.job)])
        self.assertEqual(result, 0)
        fetch.assert_called_once()
        state = json.loads((self.backend.job/'state.json').read_text(encoding='utf-8'))
        self.assertEqual(state['phase'], 'done')
        self.assertEqual(state['sha'], self.new)
        self.assertEqual(state['subject'], 'Uusi versio')

    def test_build_explicit_empty_or_whitespace_ref_fails_before_fetch(self):
        for ref in ('', ' ', '\t\n'):
            with self.subTest(ref=repr(ref)), patch.object(self.backend, 'fetch') as fetch:
                with self.assertRaisesRegex(cv.VersionError, '40-merkkinen'):
                    self.backend.build(ref, self.bundle)
                fetch.assert_not_called()
                self.assertFalse((self.backend.home/'builds').exists())

    def test_remote_failure_is_not_success(self):
        result = cv.main(['list', '--home', str(self.root/'bad-home'), '--job', str(self.root/'bad-job'), '--upstream', str(self.root/'absent')])
        self.assertEqual(result, 1)
        self.assertEqual(json.loads((self.root/'bad-job/state.json').read_text(encoding='utf-8'))['phase'], 'failed')

    def test_lock_prevents_concurrent_operation_and_recovers_dead_owner(self):
        with cv.workspace_lock(self.backend.home):
            with self.assertRaises(cv.VersionError):
                with cv.workspace_lock(self.backend.home):
                    pass
        lock = self.backend.home/'operation.lock'
        lock.mkdir()
        (lock/'owner.json').write_text('{"pid":99999999}')
        with cv.workspace_lock(self.backend.home):
            self.assertTrue(lock.exists())
        self.assertFalse(lock.exists())

    def test_cancel_before_fetch_leaves_repository_unchanged(self):
        (self.backend.job/'cancel').touch()
        with self.assertRaises(cv.Cancelled):
            self.backend.fetch()
        self.assertFalse((self.backend.home/'upstream.git').exists())
        self.assertEqual(git(self.repo, 'status', '--porcelain'), '')

    def test_running_process_is_cancelled_and_reaped(self):
        timer = threading.Timer(.3, lambda: (self.backend.job/'cancel').touch())
        timer.start()
        started = time.monotonic()
        with self.assertRaises(cv.Cancelled):
            self.backend.run([sys.executable, '-c', 'import time; time.sleep(60)'])
        timer.join()
        self.assertLess(time.monotonic()-started, 5)

    @unittest.skipIf(os.name == 'nt', 'POSIX SIGINT/process-group regression')
    def test_cli_sigint_cancels_child_and_removes_workspace_lock(self):
        job = self.root/'signal-job'
        child_pid = job/'child.pid'
        code = '''import importlib.util,sys
from pathlib import Path
spec=importlib.util.spec_from_file_location("cv",sys.argv[1]);cv=importlib.util.module_from_spec(spec);spec.loader.exec_module(cv)
def slow_requirements(self,bundle):
    script="import os,time;from pathlib import Path;Path("+repr(str(self.job/"child.pid"))+").write_text(str(os.getpid()));time.sleep(60)"
    self.run([sys.executable,"-c",script])
    return ""
cv.Backend.requirements=slow_requirements
sys.exit(cv.main(["doctor","--home",sys.argv[2],"--job",sys.argv[3]]))
'''
        process = subprocess.Popen([sys.executable, '-c', code, str(SCRIPT), str(self.backend.home), str(job)],
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic()+5
            while not child_pid.exists() and process.poll() is None and time.monotonic() < deadline:
                time.sleep(.02)
            self.assertTrue(child_pid.exists())
            pid = int(child_pid.read_text(encoding='utf-8'))
            process.send_signal(signal.SIGINT)
            output, error = process.communicate(timeout=8)
            self.assertEqual(process.returncode, 2, output+error)
            self.assertEqual(json.loads((job/'state.json').read_text(encoding='utf-8'))['phase'], 'cancelled')
            self.assertFalse((self.backend.home/'operation.lock').exists())
            self.assertFalse(cv.alive(pid))
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()

    def test_timeout_fails_instead_of_hanging(self):
        with self.assertRaises(cv.VersionError):
            self.backend.run([sys.executable, '-c', 'import time; time.sleep(60)'], timeout=.2)

    def test_wrong_checkout_and_dirty_sources_are_rejected(self):
        clone = self.root/'clone'
        git(self.root, 'clone', str(self.repo), str(clone))
        with self.assertRaises(cv.VersionError):
            self.backend.verify_checkout(clone, self.old)
        (clone/'version.txt').write_text('local change')
        with self.assertRaises(cv.VersionError):
            self.backend.verify_checkout(clone, self.new)

    def fake_toolchain(self, sha, fail=None, wrong_version=False, profile='480x320'):
        """Vain CMake/CTest on korvattu. Git ja SHA-lukitus ovat todellisia."""
        real_run = self.backend.run
        phases, commands = [], []
        real_publish = self.backend.publish
        def publish(phase, message, **fields):
            phases.append(phase)
            real_publish(phase, message, **fields)
        def run(command, cwd=None, **kwargs):
            if str(command[0]) == 'git':
                return real_run(command, cwd=cwd, **kwargs)
            commands.append([str(x) for x in command])
            if command[:2] == ['cmake', '--preset']:
                self.assertIn(f'-DASKOMPU_CORE_REVISION={sha}', command)
                core = Path(next(arg.split('=',1)[1] for arg in command if str(arg).startswith('-DASKOMPU_CORE_ROOT=')))
                self.assertEqual((core/'version.txt').read_text(encoding='utf-8'), 'old' if sha == self.old else 'new')
                (self.repo/'version.txt').write_text('moved main during build ' + str(time.monotonic()))
                git(self.repo, 'commit', '-am', 'Main changed after resolution')
            if fail and command[0] == fail:
                raise cv.VersionError('Simuloitu työkalun virhe')
            if command[0] == 'cmake' and '--install' in command:
                app = Path(command[-1]);exe = app/('askompu-simulaattori.exe' if os.name=='nt' else 'bin/askompu-simulaattori')
                exe.parent.mkdir(parents=True);exe.write_bytes(b'fixture executable')
            if '--versio' in command:
                return f'ASkompu-ydin: {"0"*40 if wrong_version else sha}\nYtimen työpuu: puhdas\nNäyttöprofiili: {profile}'
            return ''
        return phases, commands, patch.object(self.backend, 'run', run), patch.object(self.backend, 'publish', publish)

    def test_build_uses_old_sha_even_when_main_changes_and_preserves_previous(self):
        phases, commands, run_patch, publish_patch = self.fake_toolchain(self.old)
        previous = self.backend.home/'previous.exe';previous.write_text('working')
        with patch.object(self.backend, 'requirements', return_value=''), run_patch, publish_patch:
            ready = self.backend.build(self.old, self.bundle)
        self.assertEqual(ready['sha'], self.old)
        self.assertEqual(phases, ['listing','downloading','configuring','building','testing','installing','validating','ready'])
        self.assertTrue(any(command[:2]==['ctest','--preset'] for command in commands))
        self.assertEqual(previous.read_text(encoding='utf-8'), 'working')
        self.assertNotEqual(git(self.repo,'rev-parse','HEAD'), self.old)
        self.assertTrue((self.backend.home/'builds'/ready['artifact']/'ready.json').exists())

    def test_failed_build_and_wrong_binary_never_become_ready(self):
        for mode in ('compile', 'test', 'wrong-version'):
            with self.subTest(mode=mode):
                phases, _, run_patch, publish_patch = self.fake_toolchain(self.new, 'ctest' if mode=='test' else 'cmake' if mode=='compile' else None, mode=='wrong-version')
                with patch.object(self.backend,'requirements',return_value=''), run_patch, publish_patch:
                    with self.assertRaises(cv.VersionError):
                        self.backend.build(self.new,self.bundle)
                self.assertNotIn('ready',phases)
        self.assertFalse((self.backend.home/'latest-ready.json').exists())

    def test_wrong_or_missing_installed_profile_rejects_ready_and_preserves_previous(self):
        previous = {'artifact': 'a'*32, 'unchanged': True}
        cv.json_write(self.backend.home/'latest-ready.json', previous)
        for actual in ('480x320', None):
            phases, _, run_patch, publish_patch = self.fake_toolchain(self.new, profile=actual)
            fake_run = run_patch.new
            def run(command, **kwargs):
                output = fake_run(command, **kwargs)
                if '--versio' in command and actual is None:
                    return '\n'.join(line for line in output.splitlines() if not line.startswith('Näyttöprofiili:'))
                return output
            with self.subTest(actual=actual), patch.object(self.backend,'requirements',return_value=''), patch.object(self.backend,'run',run), publish_patch:
                with self.assertRaises(cv.VersionError):
                    self.backend.build(self.new, self.bundle, profile='320x170')
            self.assertIn('validating', phases)
            self.assertNotIn('ready', phases)
            self.assertEqual(json.loads((self.backend.home/'latest-ready.json').read_text(encoding='utf-8')), previous)

    def test_source_bundle_cannot_escape_workspace(self):
        with zipfile.ZipFile(self.bundle, 'w') as z:
            z.writestr('../../outside.txt', 'invalid')
        with patch.object(self.backend, 'requirements', return_value=''):
            with self.assertRaises(cv.VersionError):
                self.backend.build(self.old,self.bundle)
        self.assertFalse((self.root/'outside.txt').exists())

    def test_launch_rejects_modified_or_untested_executable(self):
        artifact='a'*32;root=self.backend.home/'builds'/artifact
        root.mkdir(parents=True);exe=root/'bad.exe';exe.write_bytes(b'changed')
        cv.json_write(root/'ready.json', self.ready_record(artifact, exe, binary_sha256='0'*64))
        with self.assertRaises(cv.VersionError):
            self.backend.launch(artifact,Path(sys.executable))
        self.assertFalse((self.backend.home/'history.json').exists())

    def test_cmake_itself_rejects_wrong_sha_and_dirty_external_tree(self):
        core = self.repo / 'src/core'
        core.mkdir(parents=True)
        (core/'ApplicationCore.cpp').write_text('// fixture')
        git(self.repo,'add','.')
        git(self.repo,'commit','-m','CMake source fixture')
        sha = git(self.repo,'rev-parse','HEAD')
        validator = SCRIPT.parents[1]/'cmake/ValidateCore.cmake'
        driver = self.root/'validate.cmake'
        driver.write_text(f'cmake_minimum_required(VERSION 3.25)\ninclude("{validator.as_posix()}")\naskompu_validate_core()\n')
        def check(revision):
            return subprocess.run(['cmake',f'-DASKOMPU_ROOT={self.root}',f'-DASKOMPU_CORE_ROOT={self.repo}',f'-DASKOMPU_CORE_REVISION={revision}','-P',str(driver)],capture_output=True,text=True)
        accepted = check(sha)
        self.assertEqual(accepted.returncode,0,accepted.stdout + accepted.stderr)
        self.assertNotEqual(check(self.old).returncode,0)
        (core/'ApplicationCore.cpp').write_text('// changed after configure')
        self.assertNotEqual(check(sha).returncode,0)

    def test_failed_real_startup_preserves_previous_history(self):
        artifact='b'*32
        root=self.backend.home/'builds'/artifact
        root.mkdir(parents=True)
        source=root/'fail.c';source.write_text('int main(void) { return 1; }')
        (root/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\nset(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)\nproject(StartupFailure LANGUAGES C)\nadd_executable(failure fail.c)\n')
        configure=['cmake','-S',str(root),'-B',str(root/'build')]
        if os.name=='nt':
            configure+=['-G','Visual Studio 17 2022','-A','x64']
        subprocess.run(configure,check=True,capture_output=True)
        subprocess.run(['cmake','--build',str(root/'build'),'--config','Release'],check=True,capture_output=True)
        exe=root/'build'/('Release/failure.exe' if os.name=='nt' else 'failure')
        cv.json_write(root/'ready.json', self.ready_record(artifact, exe))
        history={'previous':'unchanged','current':'also unchanged'}
        cv.json_write(self.backend.home/'history.json',history)
        with self.assertRaises(cv.VersionError):
            self.backend.launch(artifact,Path(sys.executable))
        self.assertEqual(json.loads((self.backend.home/'history.json').read_text(encoding='utf-8')),history)

    def test_cli_rejects_core_cache_override(self):
        result=cv.main(['build','--home',str(self.backend.home),'--job',str(self.backend.job),'--cmake-option=-DASKOMPU_CORE_ROOT=/wrong'])
        self.assertEqual(result,1)
        self.assertEqual(json.loads((self.backend.job/'state.json').read_text(encoding='utf-8'))['phase'],'failed')

    def ready_record(self, artifact, exe, **changes):
        record = {'artifact': artifact, 'executable': str(exe), 'tested': True,
                  'binary_sha256': cv.hashlib.sha256(exe.read_bytes()).hexdigest(), 'profile': '480x320',
                  'source_kind': 'github', 'sha': self.new, 'date': '2026-10-12T12:00:00+03:00', 'tag': ''}
        record.update(changes)
        return record

    def prepare_ready(self, artifact='c'*32, **changes):
        root = self.backend.home/'builds'/artifact
        root.mkdir(parents=True, exist_ok=True)
        exe = root/'app.exe'
        exe.write_bytes(b'tested executable fixture')
        record = self.ready_record(artifact, exe, **changes)
        cv.json_write(root/'ready.json', record)
        return record

    def test_doctor_restores_only_integrity_checked_ready_and_previous(self):
        ready = self.prepare_ready(profile='320x170')
        cv.json_write(self.backend.home/'latest-ready.json', ready)
        previous = self.root/'previous.exe'
        previous.write_bytes(b'previous tested process')
        cv.json_write(self.backend.home/'history.json', {'previous': str(previous),
                      'previous_sha256': cv.hashlib.sha256(previous.read_bytes()).hexdigest()})
        with patch.object(self.backend, 'requirements', return_value=''):
            self.backend.doctor(self.bundle)
        self.assertEqual(self.backend.state['artifact'], ready['artifact'])
        self.assertEqual(self.backend.state['ready_exists'], 1)
        self.assertEqual(self.backend.state['ready_profile'], '320x170')
        self.assertEqual(self.backend.state['ready_date'], ready['date'])
        self.assertEqual(self.backend.state['ready_executable'], ready['executable'])
        self.assertEqual(self.backend.state['previous'], str(previous))
        self.assertEqual(self.backend.state['ready_warning'], '')
        previous.write_bytes(b'previous program changed')
        with patch.object(self.backend, 'requirements', return_value=''):
            self.backend.doctor(self.bundle)
        self.assertEqual(self.backend.state['previous'], '')
        self.assertIn('muuttunut', self.backend.state['previous_warning'])
        self.assertEqual(self.backend.state['artifact'], ready['artifact'])
        self.assertEqual(self.backend.state['requirements_ok'], 1)

    def test_doctor_stale_corrupt_ready_and_history_do_not_disable_tools(self):
        for mode in ('missing', 'changed', 'latest-json', 'ready-json', 'latest-mismatch', 'legacy-history', 'history-json'):
            ready = self.prepare_ready()
            cv.json_write(self.backend.home/'latest-ready.json', ready)
            cv.json_write(self.backend.home/'history.json', {'previous': '/missing/previous', 'previous_sha256': '0'*64})
            if mode == 'missing':
                Path(ready['executable']).unlink()
            elif mode == 'changed':
                Path(ready['executable']).write_bytes(b'modified after testing')
            elif mode == 'latest-json':
                (self.backend.home/'latest-ready.json').write_text('{invalid JSON')
            elif mode == 'ready-json':
                (self.backend.home/'builds'/ready['artifact']/'ready.json').write_text('[]')
            elif mode == 'latest-mismatch':
                altered = dict(ready, profile='320x170')
                cv.json_write(self.backend.home/'latest-ready.json', altered)
            elif mode == 'legacy-history':
                cv.json_write(self.backend.home/'history.json', {'previous': ready['executable']})
            else:
                (self.backend.home/'history.json').write_text('{invalid JSON')
            with self.subTest(mode=mode), patch.object(self.backend, 'requirements', return_value=''):
                self.backend.doctor(self.bundle)
                self.assertEqual(self.backend.state['requirements_ok'], 1)
                self.assertEqual(self.backend.state['phase'], 'done')
                self.assertEqual(self.backend.state['previous'], '')
                self.assertTrue(self.backend.state['previous_warning'])
                if mode not in ('legacy-history', 'history-json'):
                    self.assertEqual(self.backend.state['artifact'], '')
                    self.assertEqual(self.backend.state['ready_exists'], 0)
                    self.assertTrue(self.backend.state['ready_warning'])

    def test_ready_validation_rejects_bad_formats_untested_and_external_executable(self):
        ready = self.prepare_ready()
        external = self.root/'outside.exe'
        external.write_bytes(Path(ready['executable']).read_bytes())
        changes = ({'tested': False}, {'tested': 1}, {'artifact': 'wrong'}, {'sha': 'short'},
                   {'profile': '800x600'}, {'binary_sha256': 'bad'}, {'source_kind': 'other'},
                   {'executable': str(external)}, {'executable': '../outside.exe'},
                   {'source_kind': 'local', 'fingerprint': 'bad', 'local_path': str(self.repo)})
        for change in changes:
            with self.subTest(change=change), patch.object(cv.subprocess, 'Popen') as spawn:
                cv.json_write(self.backend.home/'builds'/ready['artifact']/'ready.json', dict(ready, **change))
                with self.assertRaises(cv.VersionError):
                    self.backend.launch(ready['artifact'], Path(sys.executable))
                spawn.assert_not_called()

    def fake_successful_startup(self, children):
        class Child:
            pid = 999999
            def __init__(self, command, **kwargs):
                self.terminated = False
                children.append(self)
                Path(command[command.index('--startup-ready')+1]).touch()
            def poll(self):
                return 0 if self.terminated else None
            def terminate(self):
                self.terminated = True
            def wait(self, timeout=None):
                return 0
        return Child

    def test_launch_records_digest_and_rollback_rejects_modified_or_legacy_previous(self):
        ready = self.prepare_ready()
        current = self.root/'current.exe'
        current.write_bytes(b'currently running fixture')
        children = []
        with patch.object(cv.subprocess, 'Popen', self.fake_successful_startup(children)), patch.object(cv.time, 'sleep'):
            self.backend.launch(ready['artifact'], current)
        history = json.loads((self.backend.home/'history.json').read_text(encoding='utf-8'))
        self.assertEqual(history['previous_sha256'], cv.hashlib.sha256(current.read_bytes()).hexdigest())
        self.assertEqual(history['current_sha256'], ready['binary_sha256'])
        with patch.object(cv.subprocess, 'Popen', self.fake_successful_startup(children)), patch.object(cv.time, 'sleep'):
            self.backend.launch(None, Path(ready['executable']), rollback=True)
        # Restore the original transition, then mutate its previous binary.
        cv.json_write(self.backend.home/'history.json', history)
        current.write_bytes(b'changed after use')
        with patch.object(cv.subprocess, 'Popen') as spawn, self.assertRaises(cv.VersionError):
            self.backend.launch(None, Path(ready['executable']), rollback=True)
        spawn.assert_not_called()
        self.assertEqual(json.loads((self.backend.home/'history.json').read_text(encoding='utf-8')), history)
        history.pop('previous_sha256')
        cv.json_write(self.backend.home/'history.json', history)
        with patch.object(cv.subprocess, 'Popen') as spawn, self.assertRaises(cv.VersionError):
            self.backend.launch(None, Path(ready['executable']), rollback=True)
        spawn.assert_not_called()

    def test_cancellation_after_startup_ack_terminates_new_process_preserves_history(self):
        ready = self.prepare_ready()
        current = self.root/'current.exe'
        current.write_bytes(b'current fixture')
        history = {'previous': 'untouched', 'current': 'untouched'}
        cv.json_write(self.backend.home/'history.json', history)
        children = []
        def late_cancel(seconds):
            if seconds == .3:
                self.backend.cancelled = True
        with patch.object(cv.subprocess, 'Popen', self.fake_successful_startup(children)), patch.object(cv.time, 'sleep', late_cancel), self.assertRaises(cv.Cancelled):
            self.backend.launch(ready['artifact'], current)
        self.assertTrue(children[0].terminated)
        self.assertEqual(json.loads((self.backend.home/'history.json').read_text(encoding='utf-8')), history)
        self.assertNotEqual(self.backend.state['phase'], 'launched')

    def prepare_local(self, commit=True):
        for relative in cv.LOCAL_REQUIRED:
            file = self.repo / relative
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text('// local source fixture\n')
        (self.repo/'include').mkdir()
        (self.repo/'include/BoardConfig.h').write_text('// configuration\n')
        (self.repo/'LICENSE').write_text('License fixture\n')
        (self.repo/'platformio.ini').write_text('[env:test]\n')
        if commit:
            git(self.repo, 'add', '.')
            git(self.repo, 'commit', '-m', 'Local source fixture')
        return self.repo

    def test_inspect_local_reports_real_head_dirty_fingerprint_without_network_or_writes(self):
        source = self.prepare_local()
        head = git(source, 'rev-parse', 'HEAD')
        index_before = (source/'.git/index').read_bytes()
        index_mtime = (source/'.git/index').stat().st_mtime_ns
        clean = self.backend.inspect_local(source)
        (source/'src/core/Clock.cpp').write_text('// uncommitted feature\n')
        (source/'src/core/LocalFeature.h').write_text('// untracked feature\n')
        expected = cv.local_inventory(source)
        with patch.object(self.backend, 'fetch', side_effect=AssertionError('local must not fetch')):
            dirty = self.backend.inspect_local(source)
        self.assertEqual(dirty['sha'], head)
        self.assertEqual(dirty['dirty'], 1)
        self.assertEqual(clean['dirty'], 0)
        self.assertNotEqual(clean['fingerprint'], dirty['fingerprint'])
        self.assertEqual(dirty['source_kind'], 'local')
        self.assertEqual(dirty['local_path'], str(source.resolve()))
        self.assertEqual(cv.local_inventory(source), expected)
        self.assertEqual((source/'.git/index').read_bytes(), index_before)
        self.assertEqual((source/'.git/index').stat().st_mtime_ns, index_mtime)
        self.assertFalse((self.backend.home/'upstream.git').exists())

    def test_local_status_respects_repository_line_endings(self):
        source = self.prepare_local()
        git(source, 'config', 'core.autocrlf', 'true')
        target = source/'src/core/Clock.cpp'
        target.write_bytes(b'// windows line ending\r\n')
        git(source, 'add', '.')
        git(source, 'commit', '-m', 'CRLF source')
        self.assertEqual(self.backend.inspect_local(source)['dirty'], 0)

    def test_local_capture_is_exact_and_omits_unrelated_builds_git_and_simulator(self):
        source = self.prepare_local()
        for relative in ('simulator/build/binary.exe', '.pio/build/firmware.bin', 'build/cache',
                         'src/build/temporary.cpp', 'src/old.o'):
            path = source / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'unrelated')
        (source/'src/core/Clock.cpp').write_text('// saved uncommitted change')
        before = cv.local_inventory(source)
        core = self.root/'snapshot'
        selected, manifest = self.backend.capture_local(source, core)
        self.assertEqual(cv.verify_snapshot(core, manifest)['fingerprint'], selected['fingerprint'])
        self.assertEqual(cv.local_inventory(core), before)
        self.assertEqual((core/'src/core/Clock.cpp').read_bytes(), (source/'src/core/Clock.cpp').read_bytes())
        self.assertFalse((core/'.git').exists())
        self.assertFalse((core/'simulator').exists())
        self.assertFalse((core/'src/build').exists())
        self.assertEqual(cv.local_inventory(source), before)
        self.assertEqual(selected['dirty'], 1)

    def test_plain_local_directory_does_not_borrow_parent_git_metadata(self):
        source = self.prepare_local()
        nested = source/'local-export'
        nested.mkdir()
        for name in cv.LOCAL_TREES:
            cv.shutil.copytree(source/name, nested/name)
        selected = self.backend.inspect_local(nested)
        self.assertEqual(selected['git'], 0)
        self.assertEqual(selected['sha'], 'tuntematon')

    @unittest.skipIf(os.name == 'nt', 'Windows symlink creation may require developer privileges')
    def test_local_source_rejects_file_and_directory_symlinks(self):
        source = self.prepare_local()
        outside = self.root/'outside.txt'
        outside.write_text('outside')
        link = source/'src/escape.h'
        link.symlink_to(outside)
        with self.assertRaises(cv.VersionError):
            self.backend.inspect_local(source)
        link.unlink()
        link.symlink_to(source/'include', target_is_directory=True)
        with self.assertRaises(cv.VersionError):
            self.backend.capture_local(source, self.root/'snapshot')
        self.assertFalse((self.root/'snapshot').exists())

    def test_source_edit_during_capture_is_rejected_and_no_partial_snapshot_survives(self):
        source = self.prepare_local()
        count = len(cv.local_inventory(source))
        original_read = cv.checked_read
        calls = 0
        def change_during_copy(root, relative):
            nonlocal calls
            calls += 1
            if calls == count * 2 + 1:
                (source/relative).write_text('// developer saved a newer version\n')
            return original_read(root, relative)
        with patch.object(cv, 'checked_read', change_during_copy), self.assertRaises(cv.VersionError):
            self.backend.capture_local(source, self.root/'snapshot')
        self.assertFalse((self.root/'snapshot').exists())
        self.assertEqual(git(source, 'rev-parse', 'HEAD'), self.backend.local_metadata(source)['sha'])

    def cmake_snapshot_check(self, core, manifest):
        validator = SCRIPT.parents[1]/'cmake/ValidateCore.cmake'
        driver = self.root/'validate-local.cmake'
        driver.write_text(f'cmake_minimum_required(VERSION 3.25)\ninclude("{validator.as_posix()}")\naskompu_validate_core()\n')
        return subprocess.run(['cmake', f'-DASKOMPU_ROOT={self.root}', f'-DASKOMPU_CORE_ROOT={core}',
                               f'-DASKOMPU_CORE_SNAPSHOT={manifest}', '-P', str(driver)], capture_output=True, text=True)

    def test_python_and_cmake_verify_snapshot_hashes_and_complete_inventory(self):
        source = self.prepare_local()
        for modification in ('file', 'extra', 'hidden', 'manifest'):
            core = self.root/modification
            _, manifest = self.backend.capture_local(source, core)
            accepted = self.cmake_snapshot_check(core, manifest)
            self.assertEqual(accepted.returncode, 0, accepted.stdout+accepted.stderr)
            if modification == 'file':
                (core/'src/core/Clock.cpp').write_text('// tampered')
            elif modification in ('extra', 'hidden'):
                (core/('.hidden' if modification == 'hidden' else 'extra.cpp')).write_text('extra')
            else:
                data = json.loads(manifest.read_text(encoding='utf-8'))
                data['fingerprint'] = '0'*64
                cv.json_write(manifest, data)
            with self.subTest(modification=modification):
                with self.assertRaises(cv.VersionError):
                    cv.verify_snapshot(core, manifest)
                self.assertNotEqual(self.cmake_snapshot_check(core, manifest).returncode, 0)

    def test_manifest_path_traversal_duplicates_and_symlinks_are_rejected(self):
        source = self.prepare_local()
        for relative in ('../outside.h', '/outside.h', 'src/../outside.h', 'src/bad;name.h', 'src//bad.h', 'src\\bad.h'):
            with self.subTest(relative=relative), self.assertRaises(cv.VersionError):
                cv.local_relative_path(relative)
        core = self.root/'snapshot'
        _, manifest = self.backend.capture_local(source, core)
        data = json.loads(manifest.read_text(encoding='utf-8'))
        data['files'][0]['path'] = '../outside.h'
        cv.json_write(manifest, data)
        with self.assertRaises(cv.VersionError):
            cv.verify_snapshot(core, manifest)
        self.assertNotEqual(self.cmake_snapshot_check(core, manifest).returncode, 0)

    @unittest.skipIf(os.name == 'nt', 'Windows symlink creation may require developer privileges')
    def test_snapshot_symlinked_file_directory_root_and_manifest_are_rejected(self):
        source = self.prepare_local()
        for mode in ('file', 'directory', 'root', 'manifest'):
            core = self.root/('snapshot-'+mode)
            _, manifest = self.backend.capture_local(source, core)
            if mode == 'file':
                path = core/'src/core/Clock.cpp'
                path.unlink()
                path.symlink_to(source/'src/core/Clock.cpp')
            elif mode == 'directory':
                cv.shutil.rmtree(core/'include')
                (core/'include').symlink_to(source/'include', target_is_directory=True)
            elif mode == 'root':
                real = self.root/'real-snapshot'
                core.rename(real)
                core.symlink_to(real, target_is_directory=True)
            else:
                real = core/'manifest-real.json'
                manifest.rename(real)
                manifest.symlink_to(real)
            with self.subTest(mode=mode), self.assertRaises(cv.VersionError):
                cv.verify_snapshot(core, manifest)
            self.assertNotEqual(self.cmake_snapshot_check(core, manifest).returncode, 0)

    def fake_local_toolchain(self, source, tamper=False, wrong_version=False):
        real_run = self.backend.run
        commands, captured = [], {}
        def run(command, cwd=None, **kwargs):
            if str(command[0]) == 'git':
                return real_run(command, cwd=cwd, **kwargs)
            commands.append(command)
            if command[:2] == ['cmake', '--preset']:
                core = Path(next(arg.split('=',1)[1] for arg in command if str(arg).startswith('-DASKOMPU_CORE_ROOT=')))
                manifest = Path(next(arg.split('=',1)[1] for arg in command if str(arg).startswith('-DASKOMPU_CORE_SNAPSHOT=')))
                captured.update(cv.verify_snapshot(core, manifest))
                captured['compiled_profile'] = next(arg.split('=',1)[1] for arg in command if str(arg).startswith('-DASKOMPU_DISPLAY_PROFILE='))
                self.assertEqual((core/'src/core/Clock.cpp').read_text(encoding='utf-8'), '// local source fixture\n')
                (source/'src/core/Clock.cpp').write_text('// changed after immutable capture\n')
                if tamper:
                    (core/'src/core/Clock.cpp').write_text('// forbidden snapshot edit\n')
            if command[0] == 'cmake' and '--install' in command:
                app = Path(command[-1])
                exe = app/('askompu-simulaattori.exe' if os.name=='nt' else 'bin/askompu-simulaattori')
                exe.parent.mkdir(parents=True)
                exe.write_bytes(b'fixture local executable')
            if '--versio' in command:
                state = 'paikallisia muutoksia' if captured['dirty'] else 'puhdas'
                fingerprint = '0'*64 if wrong_version else captured['fingerprint']
                return f'ASkompu-ydin: {captured["sha"]}\nYtimen työpuu: {state}\nYtimen lähde: local\nPaikallinen lähde: {source.resolve()}\nASkompu-lähteiden SHA-256: {fingerprint}\nNäyttöprofiili: {captured["compiled_profile"]}'
            return ''
        return commands, captured, patch.object(self.backend, 'run', run)

    def test_local_build_uses_frozen_dirty_snapshot_and_keeps_all_test_gates(self):
        source = self.prepare_local(commit=False)
        for profile in ('320x170', '480x320'):
            (source/'src/core/Clock.cpp').write_text('// local source fixture\n')
            commands, captured, run_patch = self.fake_local_toolchain(source)
            with patch.object(self.backend, 'requirements', return_value=''), run_patch, patch.object(self.backend, 'fetch', side_effect=AssertionError('No network')):
                ready = self.backend.build(None, self.bundle, local=source, profile=profile)
            self.assertEqual(ready['sha'], self.new)
            self.assertEqual(ready['dirty'], 1)
            self.assertEqual(ready['source_kind'], 'local')
            self.assertEqual(ready['fingerprint'], captured['fingerprint'])
            self.assertEqual(ready['profile'], profile)
            self.assertTrue(any(command[:2] == ['ctest', '--preset'] for command in commands))
            self.assertTrue(any('--tarkista' in command for command in commands))
            self.assertTrue(any(f'-DASKOMPU_DISPLAY_PROFILE={profile}' in command for command in commands))
        self.assertFalse((self.backend.home/'upstream.git').exists())

    def test_local_snapshot_or_installed_fingerprint_tamper_prevents_ready(self):
        source = self.prepare_local()
        for tamper, wrong_version in ((True, False), (False, True)):
            (source/'src/core/Clock.cpp').write_text('// local source fixture\n')
            _, _, run_patch = self.fake_local_toolchain(source, tamper, wrong_version)
            with run_patch, patch.object(self.backend, 'requirements', return_value=''), self.assertRaises(cv.VersionError):
                self.backend.build(None, self.bundle, local=source)
        self.assertFalse((self.backend.home/'latest-ready.json').exists())

    def test_cli_source_flags_are_exclusive_and_source_workspace_is_not_written(self):
        with self.assertRaises(SystemExit):
            cv.main(['build', '--home', str(self.backend.home), '--ref', 'main', '--local', str(self.repo)])
        nested_home = self.repo/'should-not-exist'
        with self.assertRaises(SystemExit):
            cv.main(['inspect-local', '--home', str(nested_home), '--local', str(self.repo)])
        self.assertFalse(nested_home.exists())

    def test_incremental_local_cache_reuses_exact_sources_and_updates_only_changed_files(self):
        source = self.prepare_local()
        (source/'src/core/TemporaryFeature.h').write_text('// before')
        selected, _ = self.backend.capture_local(source, self.root/'snapshot-one')
        cached_core, cached_source = self.backend.prepare_local_cache(self.bundle, self.root/'snapshot-one', selected, '480x320', ())
        stable = cached_core/'src/core/ApplicationCore.cpp'
        stable_time = stable.stat().st_mtime_ns
        changed = cached_core/'src/core/Clock.cpp'
        changed_time = changed.stat().st_mtime_ns
        (source/'src/core/Clock.cpp').write_text('// changed in editor')
        (source/'src/core/TemporaryFeature.h').unlink()
        selected2, _ = self.backend.capture_local(source, self.root/'snapshot-two')
        cached_core2, cached_source2 = self.backend.prepare_local_cache(self.bundle, self.root/'snapshot-two', selected2, '480x320', ())
        self.assertEqual(cached_core2, cached_core)
        self.assertEqual(cached_source2, cached_source)
        self.assertEqual(stable.stat().st_mtime_ns, stable_time)
        self.assertNotEqual(changed.stat().st_mtime_ns, changed_time)
        self.assertEqual(changed.read_text(encoding='utf-8'), '// changed in editor')
        self.assertFalse((cached_core/'src/core/TemporaryFeature.h').exists())
        self.assertEqual(cv.verify_snapshot(cached_core, cached_core/cv.SNAPSHOT_NAME)['fingerprint'], selected2['fingerprint'])
        other_core, _ = self.backend.prepare_local_cache(self.bundle, self.root/'snapshot-two', selected2, '320x170', ())
        self.assertNotEqual(other_core, cached_core)

    def test_local_cache_source_or_snapshot_tampering_is_not_reused(self):
        source = self.prepare_local()
        selected, _ = self.backend.capture_local(source, self.root/'snapshot')
        cached_core, cached_source = self.backend.prepare_local_cache(self.bundle, self.root/'snapshot', selected, '480x320', ())
        (cached_source/'simulator/CMakeLists.txt').write_text('unexpected change')
        with self.assertRaises(cv.VersionError):
            self.backend.prepare_local_cache(self.bundle, self.root/'snapshot', selected, '480x320', ())
        (cached_source/'simulator/CMakeLists.txt').write_text('test fixture')
        (cached_core/'src/core/Clock.cpp').write_text('// unexpected change')
        with self.assertRaises(cv.VersionError):
            self.backend.prepare_local_cache(self.bundle, self.root/'snapshot', selected, '480x320', ())

    def test_cache_write_failure_and_cancellation_leave_previous_core_retryable(self):
        source = self.prepare_local()
        selected, _ = self.backend.capture_local(source, self.root/'old-snapshot')
        cached_core, _ = self.backend.prepare_local_cache(self.bundle, self.root/'old-snapshot', selected, '480x320', ())
        stable_time = (cached_core/'src/core/ApplicationCore.h').stat().st_mtime_ns
        (source/'src/core/ApplicationCore.cpp').write_text('// changed A')
        (source/'src/core/Clock.cpp').write_text('// changed B')
        updated, _ = self.backend.capture_local(source, self.root/'new-snapshot')
        original_write = Path.write_bytes
        for mode in ('write-error', 'cancel'):
            def fail_during_stage(path, data):
                if 'core-update-' in str(path) and path.name == 'Clock.cpp':
                    if mode == 'write-error':
                        raise OSError('simulated disk write failure')
                    self.backend.cancelled = True
                return original_write(path, data)
            with self.subTest(mode=mode), patch.object(Path, 'write_bytes', fail_during_stage):
                with self.assertRaises(OSError if mode == 'write-error' else cv.Cancelled):
                    self.backend.prepare_local_cache(self.bundle, self.root/'new-snapshot', updated, '480x320', ())
            self.backend.cancelled = False
            self.assertEqual(cv.verify_snapshot(cached_core, cached_core/cv.SNAPSHOT_NAME)['fingerprint'], selected['fingerprint'])
            self.assertFalse((cached_core.parent/'core-update.json').exists())
            self.assertEqual((cached_core/'src/core/ApplicationCore.h').stat().st_mtime_ns, stable_time)
        self.backend.prepare_local_cache(self.bundle, self.root/'new-snapshot', updated, '480x320', ())
        self.assertEqual(cv.verify_snapshot(cached_core, cached_core/cv.SNAPSHOT_NAME)['fingerprint'], updated['fingerprint'])
        self.assertEqual((cached_core/'src/core/ApplicationCore.h').stat().st_mtime_ns, stable_time)

    def test_cache_rename_failure_rolls_back_exact_previous_snapshot(self):
        source = self.prepare_local()
        old, _ = self.backend.capture_local(source, self.root/'old-snapshot')
        core, _ = self.backend.prepare_local_cache(self.bundle, self.root/'old-snapshot', old, '480x320', ())
        (source/'src/core/Clock.cpp').write_text('// edited feature')
        updated, _ = self.backend.capture_local(source, self.root/'new-snapshot')
        original_rename = Path.rename
        def fail_after_previous_move(path, destination):
            result = original_rename(path, destination)
            if path == core and Path(destination).name == 'previous':
                raise OSError('simulated directory-swap failure')
            return result
        with patch.object(Path, 'rename', fail_after_previous_move), self.assertRaises(OSError):
            self.backend.prepare_local_cache(self.bundle, self.root/'new-snapshot', updated, '480x320', ())
        self.assertEqual(cv.verify_snapshot(core, core/cv.SNAPSHOT_NAME)['fingerprint'], old['fingerprint'])
        self.assertFalse((core.parent/'core-update.json').exists())
        self.backend.prepare_local_cache(self.bundle, self.root/'new-snapshot', updated, '480x320', ())
        self.assertEqual(cv.verify_snapshot(core, core/cv.SNAPSHOT_NAME)['fingerprint'], updated['fingerprint'])

    def interrupted_cache_update(self, backend, snapshot, selected, stop_after='previous'):
        # os._exit models a hard interruption that cannot run Python finally/rollback.
        code = '''import importlib.util,json,os,sys
from pathlib import Path
spec=importlib.util.spec_from_file_location("cv",sys.argv[1]);cv=importlib.util.module_from_spec(spec);spec.loader.exec_module(cv)
b=cv.Backend(Path(sys.argv[2]),Path(sys.argv[3]))
original=Path.rename
def interrupt(path,destination):
    result=original(path,destination)
    if Path(destination).name==sys.argv[7]:os._exit(71)
    return result
Path.rename=interrupt
b.prepare_local_cache(Path(sys.argv[4]),Path(sys.argv[5]),json.loads(sys.argv[6]),"480x320",())
'''
        result = subprocess.run([sys.executable, '-c', code, str(SCRIPT), str(backend.home), str(backend.job),
                                 str(self.bundle), str(snapshot), json.dumps(selected), stop_after], capture_output=True, text=True)
        self.assertEqual(result.returncode, 71, result.stdout+result.stderr)

    def test_identified_hard_interruption_recovers_before_retrying_verified_update(self):
        source = self.prepare_local()
        for stop_after in ('previous', 'core'):
            backend = cv.Backend(self.root/('home-'+stop_after), self.root/('job-'+stop_after))
            (source/'src/core/Clock.cpp').write_text('// before')
            old_snapshot = self.root/('old-'+stop_after)
            old, _ = backend.capture_local(source, old_snapshot)
            core, _ = backend.prepare_local_cache(self.bundle, old_snapshot, old, '480x320', ())
            stable_time = (core/'src/core/ApplicationCore.cpp').stat().st_mtime_ns
            (source/'src/core/Clock.cpp').write_text('// after '+stop_after)
            new_snapshot = self.root/('new-'+stop_after)
            updated, _ = backend.capture_local(source, new_snapshot)
            self.interrupted_cache_update(backend, new_snapshot, updated, stop_after)
            self.assertTrue((core.parent/'core-update.json').exists())
            backend.prepare_local_cache(self.bundle, new_snapshot, updated, '480x320', ())
            self.assertFalse((core.parent/'core-update.json').exists())
            self.assertEqual(cv.verify_snapshot(core, core/cv.SNAPSHOT_NAME)['fingerprint'], updated['fingerprint'])
            self.assertEqual((core/'src/core/ApplicationCore.cpp').stat().st_mtime_ns, stable_time)

    def test_interruption_marker_does_not_authorize_tampered_snapshot_recovery(self):
        source = self.prepare_local()
        old, _ = self.backend.capture_local(source, self.root/'old-snapshot')
        core, _ = self.backend.prepare_local_cache(self.bundle, self.root/'old-snapshot', old, '480x320', ())
        (source/'src/core/Clock.cpp').write_text('// after')
        updated, _ = self.backend.capture_local(source, self.root/'new-snapshot')
        self.interrupted_cache_update(self.backend, self.root/'new-snapshot', updated)
        marker = json.loads((core.parent/'core-update.json').read_text(encoding='utf-8'))
        stage = core.parent/marker['transaction']/'next'
        (stage/'src/core/Clock.cpp').write_text('// untrusted external mutation')
        with self.assertRaises(cv.VersionError):
            self.backend.prepare_local_cache(self.bundle, self.root/'new-snapshot', updated, '480x320', ())
        self.assertTrue((core.parent/'core-update.json').exists())
        self.assertFalse(core.exists())

    def test_bundle_identity_ignores_zip_timestamps_but_not_source_content(self):
        first = self.backend.bundle_inventory(self.bundle)
        second_bundle = self.root/'source-new-time.zip'
        with zipfile.ZipFile(second_bundle, 'w') as archive:
            info = zipfile.ZipInfo('simulator/CMakeLists.txt', (2020, 1, 2, 3, 4, 5))
            archive.writestr(info, 'test fixture')
        self.assertEqual(self.backend.bundle_inventory(second_bundle), first)
        with zipfile.ZipFile(second_bundle, 'w') as archive:
            archive.writestr('simulator/CMakeLists.txt', 'different simulator')
        self.assertNotEqual(self.backend.bundle_inventory(second_bundle), first)

    def test_cmake_metadata_preserves_utf8_source_origin_and_quoted_local_path(self):
        source = self.prepare_local()
        quoted = self.root / ('ASkompu ää local' if os.name == 'nt' else 'ASkompu ää "local"')
        source.rename(quoted)
        selected, manifest = self.backend.capture_local(quoted, self.root/'snapshot')
        exported = self.root/'exported-simulator'
        (exported/'simulator').mkdir(parents=True)
        origin = self.new + '\nsisältää paikallisia muutoksia\n'
        (exported/'simulator/SOURCE-ORIGIN.txt').write_text(origin, encoding='utf-8')
        output = self.root/'metadata'
        generator = SCRIPT.parents[1]/'cmake/GenerateBuildInformation.cmake'
        result = subprocess.run(['cmake', f'-DASKOMPU_SOURCE_ROOT={exported}', f'-DASKOMPU_CORE_ROOT={manifest.parent}',
                                 f'-DASKOMPU_CORE_REVISION={selected["sha"]}', f'-DASKOMPU_CORE_SNAPSHOT={manifest}',
                                 f'-DASKOMPU_OUTPUT_DIR={output}', '-DASKOMPU_VERSION=1.0.0', '-DASKOMPU_CONFIGURATION=Release',
                                 '-P', str(generator)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        header = (output/'BuildInformation.h').read_text(encoding='utf-8')
        self.assertIn('worktree = "sisältää paikallisia muutoksia";', header)
        self.assertIn('sourceKind = "local";', header)
        self.assertIn('sourcePath = "'+selected['local_path'].replace('\\','\\\\').replace('"','\\"')+'";', header)
        self.assertIn(selected['fingerprint'], header)
        self.assertEqual((output/'SOURCE-ORIGIN.txt').read_text(encoding='utf-8'), origin)
        self.assertIn('Paikallinen lähde: '+selected['local_path'], (output/'BUILDINFO-Release.txt').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
