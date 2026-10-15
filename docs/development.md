# Development setup

## Prerequisites

- Windows 10/11 with Visual Studio 2022 or Build Tools, the Desktop development with
  C++ workload, MSVC x86/x64 tools and a Windows SDK.
- CMake 3.24+ on `PATH`; Visual Studio can install it with the C++ tools.
- An interactive Windows desktop for the full native test suite.
- Optional development checks: Python 3.9+ and clang-format 19. The DLL itself
  uses neither Python nor LLVM at runtime.

Run commands from the repository root. Use Developer PowerShell if CMake is not
available in your normal shell. `CMakePresets.json` selects Visual Studio 2022,
Win32, tests enabled, and the optional historical proxy disabled. Preset schema
5 matches the minimum CMake version; see [CMake's preset reference](https://cmake.org/cmake/help/v3.24/manual/cmake-presets.7.html).

## Normal edit/build/test loop

```powershell
cmake --preset windows-x86
cmake --build --preset release
ctest --preset release

cmake --build --preset debug
ctest --preset debug
```

Both configurations share `build/windows-x86`. Do not run two builds against that
directory at once. A normal configure does not launch the game or touch existing clients.

For a focused regression:

```powershell
ctest --preset release -R 'resize_cursor_contract|native_aspect_resize_contract'
```

For unit checks without an interactive desktop:

```powershell
ctest --preset unit-release
ctest --preset unit-debug
```

These select viewport/aspect geometry and the bounded X name-reader/build-guard checks. They do not establish that the renderer or
recovery works; use the full interactive suite and actual-client checks for that.

The optional X edition has separate `windows-x86-x`, `x-release` and `x-debug`
presets and its own build/install directories. See [DAWindower X](x-edition.md).

## Format and documentation checks

```powershell
./tools/format.ps1
./tools/format.ps1 -Check
python tools/check_docs.py
```

The formatter searches `PATH`, then installed Visual Studio C++ Clang tools.
Install those optional tools or pass a specific executable:

```powershell
./tools/format.ps1 -Check -ClangFormat 'C:/Tools/LLVM/bin/clang-format.exe'
```

The repository uses clang-format 19. CI pins 19.1.5 through the
[development-only Python distribution](https://pypi.org/project/clang-format/19.1.5/).
The style follows LLVM's [configuration options](https://clang.llvm.org/docs/ClangFormatStyleOptions.html)
with expanded control flow and preserved include order. Source docs are UTF-8/LF.
`check_docs.py` checks local links and heading anchors without requesting external
URLs. Generated/local artifacts are named as code paths, not public source links.

## Produce and inspect a package

```powershell
cmake --install build/windows-x86 --config Release
python tools/check_package.py dist/local
python tools/inspect_pe.py dist/local/DAWindower.dll
```

The default install directory contains only `DAWindower.dll`. Package checks
verify file count, x86 PE format, the COFF DLL flag, the system DLL import allowlist
and the three optional exports. The header checks follow Microsoft's
[PE format specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
They do not test startup, rendering or recovery.

Use a new directory when an earlier package is loaded by a client:

```powershell
cmake --install build/windows-x86 --config Release --prefix dist/candidate
python tools/check_package.py dist/candidate
```

Never overwrite an in-use DLL. No restart is performed by the build or install.

## Build only the DLL

Presets include test tools for contributors. A minimal library-only build is:

```powershell
cmake -S . -B build/library -G 'Visual Studio 17 2022' -A Win32 -DBUILD_TESTING=OFF
cmake --build build/library --config Release
cmake --install build/library --config Release --prefix dist/library
```

`DAW_BUILD_PROXY_TESTS=ON` enables historical development comparisons. It is off
by default; CMake never installs that proxy as part of the runtime package.

## Debug diagnostics

Set the environment before launching an isolated client with the Debug DLL:

```powershell
$env:DAW_DIAGNOSTICS = '1'
```

Debug emits at most 2,048 log lines next to the loaded DLL and to the debugger.
Release does not write logs or read configuration. The logging preserves the
caller's LastError. Optional statistics/lifecycle exports are documented in
[integration](integration.md); they are not required for ordinary injection.

## Common setup problems

| Symptom | Check |
| --- | --- |
| CMake rejects the target architecture | Use `-A Win32` or the shared preset; a 64-bit host still builds a 32-bit DLL. |
| Wrong generator cached in a build directory | Configure into a fresh directory with the intended preset/generator. |
| `cmake` or `ctest` not found | Use Developer PowerShell or add the installed CMake `bin` directory to `PATH`. |
| Full tests fail at `OpenInputDesktop` or visible pixel checks | Run on an interactive desktop rather than a restricted sandbox/service session. |
| Link/install reports the DLL is in use | Use a separate candidate path; do not stop existing clients to free it. |
| Formatter version differs | Select clang-format 19 with `-ClangFormat`. |

Continue with [testing](testing.md) and [the code tour](architecture.md).
