# ASkompu Simulator 1.1.0

Native Finnish-language desktop simulator using the real ASkompu core.
The 1.1.0 release is built against Mikky100/ASkompu 0.1.2, commit
`427e4026ebc7c510558e847b01ac7bc98c4147bd`. The display is the simulator's
independent preview, not pixel-identical firmware rendering.
See the [project overview](../README.md) for screenshots and downloads.

## Run a package

Windows: extract the entire ZIP and open askompu-simulaattori.exe.
Linux: extract the archive and run bin/askompu-simulaattori.
Normal simulation requires no development tools.

Set the clock using the arrow keys (Right advances and accepts), choose a
speed and press **Jatka**. **Ohje** opens help. Arrow keys operate the device;
P = VP/point, A = AT, R = reverse, S = stop, N = step, Space = pause/resume,
Escape = long Left. Shortcuts are suppressed while editing a text field.
TRIP uses the core's internal trip-reset setting.

State is in memory only. Restart starts a new session; no saved scenarios,
replay or state transfer are included.

## Build and test

CMake 3.25+, a C/C++17 compiler and network access for pinned dependency
downloads are required. Run commands from this directory.

On Debian/Ubuntu, install the Linux build prerequisites:

    sudo apt-get install build-essential cmake git python3 libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxss-dev
    cmake --preset linux-release
    cmake --build --preset linux-release
    ctest --preset linux-release
    python3 -m unittest discover -s tests -p 'test_*.py' -v
    ./build/linux-release/askompu-simulaattori
    cpack --preset linux-release

Use linux-clang for Clang Release, linux-debug for GCC Debug,
or linux-tests for a build without the GUI.

Windows needs Visual Studio 2022 C++ desktop tools, Windows SDK and CMake:

    cmake --preset windows-release
    cmake --build --preset windows-release
    ctest --preset windows-release
    cpack --preset windows-release

Packages appear under build/PRESET/packages. Windows bundles static SDL
and MSVC runtime libraries; packaging rejects unexpected DLL dependencies.
Linux packages use system C/C++ libraries and are not distribution-independent.

## Try a local ASkompu version

Open **ASkompu-versio → Paikallinen kansio**, choose the project root and
press **Tarkista kansio**. Select 320×170 or 480×320, then **Rakenna ja testaa**.
When the checks pass, **Avaa rakennettu versio** starts the new session.
Saved uncommitted edits and new source files are included; the original
directory is never edited. Unsaved editor buffers are not included.

Each build locks a fingerprinted source snapshot. Repeated local builds reuse
the compiler cache for the same source directory, display profile and simulator
sources, while still running the tests and installed-program checks. Restart
begins a fresh session. The first build can download pinned dependencies.

The simulator uses **REAL** when the selected ASkompu sources provide the shared
renderer and host surface. The original display is retained as **Esikatselu**
for older compatible cores: their real application logic runs, but the display
is the simulator's independent preview. This is the temporary LEGACY fallback,
not a claim of exact firmware graphics. No modern firmware graphics are bundled
in this branch. REAL is the future production path.

REAL draws a native RGB565 framebuffer at both supported display sizes.
**Nearest / Linear** affect only desktop texture filtering. New visuals come
from the selected ASkompu tree, never a second simulator styling implementation.

## Optional GitHub core selection

**ASkompu-versio → GitHub** fetches main, tags and recent commits from Mikky100/ASkompu.
Select a version or open **Tarkka commit** for a full SHA. The backend locks the
commit, checks out clean sources, builds in a separate workspace, runs tests
and checks the installed binary before offering restart. A readiness handshake
keeps the old window until the new program starts. A previous program can be
launched again from the selector.

The optional workflow needs Python 3.10+, Git, CMake/CTest and the platform
build tools above. Missing tools do not prevent normal simulation.
The package includes tools/core_versions.py and simulator-source.zip;
keep them with the application. Managed builds consume disk space and are not
automatically removed. Not every historical upstream version is compatible.

Detailed architecture, CLI usage and cache handling:
[CORE_VERSIONS.md (Finnish)](docs/CORE_VERSIONS.md).
Windows build, dependency auditing and manual checks:
[WINDOWS.md (Finnish)](docs/WINDOWS.md).

## GUI checks

    SDL_VIDEODRIVER=dummy ./build/linux-release/askompu-simulaattori --tarkista

The core-selector check uses --tarkista-ytimenvalitsin, an isolated
--core-home and optionally --core-upstream pointing to a local Git fixture.
Add **--local-core PATH** to check the local-source page and folder picker.
Omit the dummy driver for a visible desktop test. The optional
--testaa-ytimen-rakennus adds a real build, cancellation and restart; it leaves
the successfully launched program open.

Release CI checks GCC/Clang and MSVC builds, tests, packaging and extracted
package execution. Windows CI uses SDL's dummy driver, not a manual Windows 11
desktop session. See the release notes for actual results and limitations.

ASkompu copyright © 2026 Mikky100, MIT. See the original LICENSE and
[third-party notices](THIRD_PARTY.md). This fork is a desktop development tool,
not an official firmware release or a certified vehicle navigation device.
