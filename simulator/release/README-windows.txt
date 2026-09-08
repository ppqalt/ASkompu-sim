ASkompu Simulator 1.0.0

Extract the entire ZIP, then double-click askompu-simulaattori.exe.
Normal simulation needs no Python, Git, CMake, PlatformIO or compiler.
Keep all included files together.

The interface is in Finnish. Set the clock with Up/Down; Right advances
to minutes and then accepts. Choose a speed and press Jatka (Run).
Ohje opens help. Space pauses/resumes; arrows operate the device,
P = VP/point, A = AT, R = reverse, S = stop, N = step, Esc = long Left.
TRIP performs the configured internal trip reset.

Ytimen versio is an OPTIONAL local source-build workflow for upstream
Mikky100/ASkompu commits. It requires Python 3.10+, Git, CMake 3.25+,
CTest, Visual Studio 2022 C++ Build Tools and the Windows SDK.
It is not a tool-free updater. The included backend script and source ZIP
are required for this workflow. Builds and tests run in a separate workspace.
Restart begins a fresh session; you can return to the previous program.

All simulation state is held in memory. Saved scenarios and replay are not
included. The application is unsigned; organizational security policies may
prevent execution. Do not disable security protections to run it.

Windows CI validates the package using SDL's dummy video driver.
Manual Windows 11 testing of 1.0.0 and the full new core build/restart/rollback
workflow remains outstanding. This is a desktop simulator and development
tool, not an official firmware release or a certified navigation instrument.

Downloads, known limitations and support:
https://github.com/ppqalt/ASkompu-sim/releases/tag/v1.0.0
Include BUILDINFO.txt when reporting a problem.
ASkompu copyright (c) 2026 Mikky100. See LISENSSIT for licenses.
