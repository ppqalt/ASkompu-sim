"""Varmista tarkistustyökalun eristys paikallisilla, kertakäyttöisillä Git-fixtureilla."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools' / 'check_upstream.py'


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], text=True, stderr=subprocess.STDOUT).strip()


def commit(repo, message):
    git(repo, 'add', '.')
    git(repo, '-c', 'user.name=Testi', '-c', 'user.email=test@example.invalid', 'commit', '-m', message)


class UpstreamCheckTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='askompu-check-test-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.upstream = self.root / 'upstream'
        self.upstream.mkdir()
        git(self.upstream, 'init', '-b', 'main')
        (self.upstream / 'common.txt').write_text('alku\n')
        sim = self.upstream / 'simulator'
        sim.mkdir()
        (sim / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\nproject(Fixture NONE)\nenable_testing()\nadd_test(NAME fixture COMMAND "${CMAKE_COMMAND}" -E true)\n')
        (sim / 'CMakePresets.json').write_text(json.dumps({'version': 6,
            'configurePresets': [{'name': 'linux-release', 'generator': 'Unix Makefiles', 'binaryDir': '${sourceDir}/build'}],
            'buildPresets': [{'name': 'linux-release', 'configurePreset': 'linux-release'}],
            'testPresets': [{'name': 'linux-release', 'configurePreset': 'linux-release'}]}))
        commit(self.upstream, 'Testifixturen pohja')
        self.source = self.root / 'source'
        git(self.upstream, 'clone', str(self.upstream), str(self.source))
        (self.source / 'fork.txt').write_text('fork\n')
        commit(self.source, 'Testifixturen haaramuutos')

    def state(self):
        return tuple(git(self.source, *args) for args in [('rev-parse', 'HEAD'), ('status', '--porcelain'), ('show-ref',), ('remote', '-v')])

    def check_tool(self, expected):
        before = self.state()
        result = subprocess.run([sys.executable, str(SCRIPT), '--source', str(self.source), '--upstream-url', str(self.upstream)], text=True, capture_output=True)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        self.assertEqual(before, self.state())
        return result

    def test_merge_build_and_tests_leave_source_untouched(self):
        (self.upstream / 'common.txt').write_text('upstream muutos\n')
        commit(self.upstream, 'Testifixturen upstream-muutos')
        result = self.check_tool(0)
        self.assertIn('100% tests passed', result.stdout)
        self.assertEqual((self.source / 'common.txt').read_text(), 'alku\n')

    def test_conflict_fails_without_modifying_source(self):
        for repo, content in [(self.upstream, 'upstream\n'), (self.source, 'fork\n')]:
            (repo / 'common.txt').write_text(content)
            commit(repo, 'Testifixturen ristiriita')
        self.assertIn('common.txt', self.check_tool(3).stderr)

    def test_dirty_source_is_rejected_without_changes(self):
        (self.source / 'common.txt').write_text('keskeneräinen\n')
        self.check_tool(2)
        self.assertEqual((self.source / 'common.txt').read_text(), 'keskeneräinen\n')


if __name__ == '__main__':
    unittest.main()
