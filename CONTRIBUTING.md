# Contributing to DAWindower

Welcome. The project is small enough to follow from DLL load to final pixels,
and contributions should help keep it that way. A documentation correction, a
reliable regression or a measured client report can be as useful as a new feature.

## Get oriented

1. Read the [README](README.md) for supported behavior and limits.
2. Follow the [code tour](docs/architecture.md) for ownership and call flow.
3. Build using the [development guide](docs/development.md).
4. Find the relevant contract in the [test map](docs/testing.md#automated-test-map).

Visual Studio 2022 C++ tools, the Windows SDK and CMake 3.24+ are enough to build.
No retail game assets or third-party runtime are needed for synthetic tests.
Python 3.9+ and clang-format 19 are development-only source/package check tools.

## Useful places to start

- Clarify a confusing API contract, ownership comment or build instruction.
- Turn a reproducible rendering failure into a focused regression.
- Record an individual UAC, lock/unlock or display-transition test for foreground
  and background clients, with precise build/client fingerprints.
- Measure sustained multiclient CPU, memory, GDI handles and recovery time.
- Expand supported API coverage only when an actual client requires it.

Avoid speculative renderer rewrites. The current architecture is native RGB565
offscreen rendering plus a retained GDI canvas; a new backend needs measured evidence.

## Keep the loading and runtime contract intact

Every runtime contribution must preserve:

- One distributable native **x86 `DAWindower.dll`**, with a static CRT and Windows
  system dependencies. No companion binary or configuration file is required.
- Early ordinary `LoadLibraryA` injection, with no launcher initialization call
  or dependency on a particular launcher.
- Full initialization outside loader lock; the first graphics object must be wrapped.
- Logical 640×480 RGB565 pixels, color keys, mouse mapping, cursor composition,
  public COM identity and attachment/lifetime rules.
- Independent per-window recovery with bounded work. No sleeps, spins or worker
  races added to the game's startup/rendering path.
- No hooks into networking, gameplay or automation.

The [architecture guide](docs/architecture.md) explains where these contracts live.

## Write code for the next reader

Use the project's `.clang-format` and `.editorconfig`. C++ uses four-space
indentation, a 100-column target and expanded control flow. Include order is
preserved because Windows headers have ordering requirements.

Prefer a descriptive local name over an unexplained abbreviation. Explain why
a lifetime, lock, HRESULT or coordinate rule matters; do not narrate every line.
Keep native API semantics and ABI names recognizable. Small methods are useful
when they clarify a responsibility, not when they hide a straightforward call.

Place code with the responsibility listed in the [source map](docs/architecture.md#runtime-source-map).
Keep shared window behavior in `src/window/` and X-only launch work in `src/x/`.
Use named values for internal roles and array bounds; keep native Windows/COM
method names intact. Name declaration parameters so callers can understand
pointer ownership, coordinate spaces and input/output roles without opening the implementation.

Comments should explain constraints: loader-lock safety, lock order, borrowed
handles, reentrant callbacks, or why a completed game operation cannot be retried.
Remove dead helpers and reuse shared setup rather than retaining parallel copies.
Preserve dated acceptance evidence; label superseded designs and update current
guides when the source contract changes.

New bug fixes should include a regression that fails for the original problem.
Use existing fault-injection helpers for loss/recovery behavior. Do not add tests
that merely repeat the implementation or claim synthetic coverage proves retail behavior.

```powershell
./tools/format.ps1
./tools/format.ps1 -Check
python tools/check_docs.py
```

## Validate in proportion to the change

For docs-only changes, run the documentation link check. For C++ changes, format
the touched code and run the affected tests, then the full relevant configuration
on an interactive Windows desktop. Changes to initialization, COM lifetimes,
cursor handling or recovery should pass both Debug and Release.

```powershell
cmake --preset windows-x86
cmake --build --preset release
ctest --preset release
cmake --build --preset debug
ctest --preset debug
```

For a candidate package:

```powershell
cmake --install build/windows-x86 --config Release
python tools/check_package.py dist/local
```

Actual-client tests are separate and explicitly operated on isolated copies.
Never inject into, stop or restart another person's running client as part of
a test. In this workspace, **Excalibur must not be stopped or restarted without
the user's explicit permission**. The retail helper only launches `Darkages.exe`;
it is development code and never a distribution requirement.

See [testing](docs/testing.md) for the manual acceptance matrix and CI limits.

## Send a focused pull request

Describe the concrete problem, the resulting behavior and how it was verified.
Include the client/DLL fingerprint when runtime compatibility is involved, and
separate automated results from manual observations and untested cases.

Keep behavior changes separate from wide formatting sweeps. Update the README,
integration guide or changelog when a public contract changes. Include only
original code or properly compatible, attributed source; game assets and legacy
DLL binaries do not belong in the repository.

For bug reports, include reproduction steps, Windows/client build, monitor/DPI
setup, injection sequence, and whether background clients are affected. The
repository's issue and PR templates provide a starting point.
