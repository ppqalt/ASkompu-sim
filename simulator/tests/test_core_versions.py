"""Versionvalitsimen eristetyt Git-, prosessi- ja tilasiirtymätestit."""
import importlib.util
import json
import os
from pathlib import Path
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
        self.assertIn('%C3%A4', (self.root / 'job/versions.tsv').read_text())
        self.assertEqual(git(self.repo, 'rev-parse', 'HEAD'), self.new)
        self.assertEqual(git(self.repo, 'status', '--porcelain'), '')

    def test_invalid_and_unknown_refs_fail(self):
        cache = self.backend.fetch()
        for ref in ('HEAD~1', '--help', 'shortsha', 'refs/tags/no-such-tag', 'a' * 40):
            with self.subTest(ref=ref), self.assertRaises(cv.VersionError):
                self.backend.resolve(cache, ref)

    def test_remote_failure_is_not_success(self):
        result = cv.main(['list', '--home', str(self.root/'bad-home'), '--job', str(self.root/'bad-job'), '--upstream', str(self.root/'absent')])
        self.assertEqual(result, 1)
        self.assertEqual(json.loads((self.root/'bad-job/state.json').read_text())['phase'], 'failed')

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

    def fake_toolchain(self, sha, fail=None, wrong_version=False):
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
                self.assertEqual((core/'version.txt').read_text(), 'old' if sha == self.old else 'new')
                (self.repo/'version.txt').write_text('moved main during build ' + str(time.monotonic()))
                git(self.repo, 'commit', '-am', 'Main changed after resolution')
            if fail and command[0] == fail:
                raise cv.VersionError('Simuloitu työkalun virhe')
            if command[0] == 'cmake' and '--install' in command:
                app = Path(command[-1]);exe = app/('askompu-simulaattori.exe' if os.name=='nt' else 'bin/askompu-simulaattori')
                exe.parent.mkdir(parents=True);exe.write_bytes(b'fixture executable')
            if '--versio' in command:
                return f'ASkompu-ydin: {"0"*40 if wrong_version else sha}\nYtimen työpuu: puhdas'
            return ''
        return phases, commands, patch.object(self.backend, 'run', run), patch.object(self.backend, 'publish', publish)

    def test_build_uses_old_sha_even_when_main_changes_and_preserves_previous(self):
        phases, commands, run_patch, publish_patch = self.fake_toolchain(self.old)
        previous = self.backend.home/'previous.exe';previous.write_text('working')
        with patch.object(self.backend, 'requirements', return_value=''), run_patch, publish_patch:
            ready = self.backend.build(self.old, self.bundle)
        self.assertEqual(ready['sha'], self.old)
        self.assertEqual(phases, ['listing','downloading','configuring','building','testing','ready'])
        self.assertTrue(any(command[:2]==['ctest','--preset'] for command in commands))
        self.assertEqual(previous.read_text(), 'working')
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
        cv.json_write(root/'ready.json', {'executable':str(exe),'tested':True,'binary_sha256':'0'*64})
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
        cv.json_write(root/'ready.json',{'executable':str(exe),'tested':True,'binary_sha256':cv.hashlib.sha256(exe.read_bytes()).hexdigest()})
        history={'previous':'unchanged','current':'also unchanged'}
        cv.json_write(self.backend.home/'history.json',history)
        with self.assertRaises(cv.VersionError):
            self.backend.launch(artifact,Path(sys.executable))
        self.assertEqual(json.loads((self.backend.home/'history.json').read_text()),history)

    def test_cli_rejects_core_cache_override(self):
        result=cv.main(['build','--home',str(self.backend.home),'--job',str(self.backend.job),'--cmake-option=-DASKOMPU_CORE_ROOT=/wrong'])
        self.assertEqual(result,1)
        self.assertEqual(json.loads((self.backend.job/'state.json').read_text())['phase'],'failed')


if __name__ == '__main__':
    unittest.main()
