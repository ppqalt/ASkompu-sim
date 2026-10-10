"""Build the optional host bridge against a synthetic selected local core.

The fixture uses the old, pre-redesign ASkompu core and plain rectangles only.
This verifies the adapter contract; it makes no hardware font/fidelity claim.
"""
import importlib.util
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest


SIMULATOR = Path(__file__).resolve().parents[1]
REPOSITORY = SIMULATOR.parent
OLD_CORE_REVISION = '8e69703'
spec = importlib.util.spec_from_file_location('shared_fixture_core_versions',
                                            SIMULATOR / 'tools/core_versions.py')
cv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cv)


CANVAS_HEADER = r'''#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
namespace ui::test {
class FramebufferCanvas {
 public:
  FramebufferCanvas(uint16_t width, uint16_t height)
      : width_(width), height_(height), pixels_(size_t(width) * height) {}
  const std::vector<uint16_t>& pixels() const { return pixels_; }
  void clear(uint16_t color) { std::fill(pixels_.begin(), pixels_.end(), color); }
  void fillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                uint16_t color) {
    for (int row = std::max<int>(0, y); row < std::min<int>(height_, y + height); ++row)
      for (int column = std::max<int>(0, x); column < std::min<int>(width_, x + width); ++column)
        pixels_[size_t(row) * width_ + column] = color;
  }
 private:
  uint16_t width_, height_;
  std::vector<uint16_t> pixels_;
};
}
'''

RENDERER_HEADER = r'''#pragma once
#include <cstdint>
#include "core/DisplayModel.h"
namespace ui {
template<class Surface> class DisplayRenderer {
 public:
  static void renderScreen(Surface& canvas, uint16_t width, uint16_t height,
                           uint16_t background, const core::DisplayModel& model,
                           uint16_t foreground) {
    canvas.clear(background);
    canvas.fillRect(width / 2, height / 2, 3, 5, foreground);
    // A visible marker changes with the model. No font or design asset is used.
    canvas.fillRect(model.timeEntry.hour, 2, 2, 2, foreground);
  }
};
}
'''


class SelectedSharedCoreBuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('git') or not shutil.which('cmake'):
            raise unittest.SkipTest('Git and CMake are required for the selected-core build fixture')
        check = subprocess.run(['git', 'cat-file', '-e', f'{OLD_CORE_REVISION}^{{commit}}'],
                               cwd=REPOSITORY, capture_output=True)
        if check.returncode:
            raise unittest.SkipTest('The pinned pre-redesign core commit is not present in this source archive')

    def command(self, arguments, cwd=None):
        result = subprocess.run(arguments, cwd=cwd, capture_output=True, text=True,
                                timeout=120)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result.stdout

    def prepare_snapshot(self, root):
        source = root / 'paikallinen lähde'
        source.mkdir()
        archive = subprocess.check_output(
            ['git', 'archive', OLD_CORE_REVISION, '--', 'src', 'include', 'test',
             'platformio.ini'], cwd=REPOSITORY)
        with tarfile.open(fileobj=io.BytesIO(archive)) as files:
            for member in files:
                target = source / member.name
                self.assertTrue(target.resolve().is_relative_to(source.resolve()))
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    self.assertTrue(member.isfile(), 'Old fixture contains unsupported archive data')
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(files.extractfile(member).read())
        # Establish that neither the baseline nor the fixture carries redesign assets.
        self.assertFalse((source / 'src/ui/ModernDisplayRenderer.h').exists())
        self.assertFalse((source / 'src/ui/DisplayStyle.h').exists())
        self.assertFalse((source / 'src/ui/fonts').exists())
        self.assertFalse((source / 'src/ui/DisplayRenderer.h').exists())
        canvas = source / 'test/portable_display'
        canvas.mkdir()
        (canvas / 'FramebufferCanvas.h').write_text(CANVAS_HEADER)
        (canvas / 'FramebufferCanvas.cpp').write_text('#include "FramebufferCanvas.h"\n')
        (source / 'src/ui/DisplayRenderer.h').write_text(RENDERER_HEADER)
        # This optional shared-renderer source is deliberately empty: the fixture
        # has no formatting logic or fonts and links only the adapter contract.
        (source / 'src/ui/DisplayFormatting.cpp').write_text('// Synthetic adapter fixture only.\n')
        backend = cv.Backend(root / 'home', root / 'job')
        before = cv.local_inventory(source)
        selected, manifest = backend.capture_local(source, root / 'snapshot')
        self.assertEqual(cv.local_inventory(source), before)
        self.assertEqual(cv.verify_snapshot(root / 'snapshot', manifest)['fingerprint'],
                         selected['fingerprint'])
        return root / 'snapshot', manifest, selected

    def build_with(self, cc, cxx):
        if not shutil.which(cc) or not shutil.which(cxx):
            self.skipTest(f'{cc}/{cxx} unavailable')
        with tempfile.TemporaryDirectory(prefix='ASkompu shared ää ') as temporary:
            root = Path(temporary)
            snapshot, manifest, selected = self.prepare_snapshot(root)
            build = root / 'build'
            self.command(['cmake', '-S', str(SIMULATOR), '-B', str(build),
                          '-DASKOMPU_BUILD_GUI=OFF', '-DBUILD_TESTING=ON',
                          '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                          f'-DCMAKE_C_COMPILER={shutil.which(cc)}',
                          f'-DCMAKE_CXX_COMPILER={shutil.which(cxx)}',
                          f'-DASKOMPU_CORE_ROOT={snapshot}',
                          f'-DASKOMPU_CORE_REVISION={selected["sha"]}',
                          f'-DASKOMPU_CORE_SNAPSHOT={manifest}'])
            self.command(['cmake', '--build', str(build), '--target',
                          'shared_display_tests', '-j2'])
            commands = json.loads((build / 'compile_commands.json').read_text())
            bridge = next(item for item in commands
                          if item['file'].endswith('/ui/SharedDisplayFramebuffer.cpp'))
            self.assertIn('ASKOMPU_HAS_SHARED_DISPLAY_RENDERER=1', bridge['command'])
            self.assertIn(str(snapshot), bridge['command'])
            self.assertFalse(any('ModernDisplayRenderer' in item['file'] or
                                 'RobotoDisplayFont' in item['file'] for item in commands))
            self.command(['ctest', '--test-dir', str(build), '--output-on-failure',
                          '-R', '^yhteinen_nayttorenderer$'])
            self.assertEqual(cv.verify_snapshot(snapshot, manifest)['fingerprint'],
                             selected['fingerprint'])

    def test_gcc_selected_local_shared_renderer(self):
        self.build_with('gcc', 'g++')

    def test_clang_selected_local_shared_renderer(self):
        self.build_with('clang', 'clang++')


if __name__ == '__main__':
    unittest.main()
