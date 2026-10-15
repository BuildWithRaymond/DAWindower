# DAWindower

**Dark Ages on your desktop. One DLL per client.**

DAWindower is an open-source alternative to `dawnd.dll` for the 32-bit Dark Ages
client. It keeps the familiar launcher integration and windowed game experience,
with a renderer designed to recover after desktop interruptions. The optional
**DAWindower X** edition adds a dark title bar and useful client controls.

**Windows · x86 · C++17 · MIT · Windows system dependencies only**

[![Windows build and source checks](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml?query=branch%3Amain)

[Choose an edition](#choose-an-edition) · [Downloads and status](#downloads-and-status) ·
[Launcher integration](#use-it-with-an-existing-launcher) · [Build](#build-and-test) ·
[Contribute](CONTRIBUTING.md) · [Test evidence](docs/x-borderless-validation.md)

## With love for dawnd

For more than twenty years, dawnd has been part of how this community plays
Dark Ages. It gave players windowed clients, made multiple clients practical,
and kept getting passed along in the launchers people relied on. That kind of
staying power deserves respect.

Thank you to **Saraet, creator of dawnd.dll**, and to everyone who kept it in
players' hands. DAWindower carries that usefulness forward with recoverable
rendering and source the next contributor can understand.
[Credits and research acknowledgments](CREDITS.md).

## Choose an edition

Both editions share the same renderer, recovery, mouse mapping and loading
contract. Choose **one DLL** for each client; X is an alternative to standard,
not an add-on that needs both files.

| | DAWindower | DAWindower X |
| --- | --- | --- |
| Distributable | `DAWindower.dll` | `DAWindowerX.dll` |
| Title bar | Native Windows title bar and controls | Charcoal-to-black gradient with custom controls |
| Character title | The client's existing title | `Dark Ages · CharacterName` on the supported client; otherwise `Dark Ages` |
| New client button | Use your launcher | **+** starts another client from the same game folder with X injected |
| Native-size button | Resize through Windows | **1:1** returns the game area to **640×480** |
| Button tooltips | Native Windows behavior | Descriptions for all five title-bar buttons |

Ordinary windowed clients **keep their title bars**. Both editions remove visible
side/bottom frame insets while retaining invisible resize grips and visible
resize arrows. Windows start at native **640×480** game dimensions, resize
smoothly at the native **4:3 aspect ratio**, and maximize to the largest 4:3 game
area that fits the monitor work area. There are no preset size steps.

X's title-bar layout:

```text
Dark Ages · CharacterName          [+] [1:1]   [−] [□] [×]
```

X's New client button starts the game directly. It does not carry over launcher
arguments, credentials or automatic login. Character-name support currently
targets the inspected **7.41** client; unknown layouts display `Dark Ages`.
See [X controls and client support](docs/x-edition.md).

An explicit launcher-selected borderless fullscreen mode keeps its monitor
bounds and hides the title bar. That mode fills the monitor, including non-4:3
displays; ordinary windowed resizing keeps 4:3. See the
[fullscreen integration contract](docs/integration.md#borderless-windows-and-launcher-fullscreen)
for ordering requirements and limits.

## Why use DAWindower?

- **Recovery belongs to each window.** Completed pixel writes are retained, lost
  surfaces can be restored, and failed presentations retry with bounded backoff.
  Recovery watches secure-desktop/UAC, lock/unlock, display/device and restore
  events, including for background clients.
- **Keep the existing loading contract.** Early `LoadLibraryA` injection alone
  initializes either edition. There is no required initializer call, separate
  `ddraw.dll` proxy, configuration file, companion executable or external runtime.
  Talos, Zeus and other specific launchers are not required.
- **Source you can maintain.** MIT-licensed C++17, documented ownership, native
  regression tests and one-file package checks make changes inspectable and
  reproducible.
- **Optional controls without a different renderer.** X adds character titles,
  native-size reset and New client while sharing standard's graphics and recovery.

The game still draws a **640×480 RGB565** logical frame. Presentation uses GDI
and nearest-neighbor scaling, with mouse coordinates mapped to that same game
area. Networking, gameplay and automation paths are not hooked.

### What this changes from dawnd

dawnd already provides windowed play, multiple clients, cursor handling,
full-window graphics, aspect-ratio constraints and resize arrows. DAWindower
preserves that baseline. Its main differences are recovery, open-source
maintenance and the optional X controls.

The recovery comparison comes from the inspected legacy binary:

| Area | Inspected dawnd path | DAWindower |
| --- | --- | --- |
| Presentation | Draws a converted primary into a separate real DirectDraw primary. | Presents a retained RGB565 canvas through GDI into each window. |
| Presentation failure | Discards the private-primary presentation result; no repair path was found for that surface. | Retains the frame and retries with bounded backoff, even without new game writes. |
| Surface contents | No wrapper-owned pixel backup/repopulation was found in that path. | Retains completed writes and reapplies color keys while keeping public COM identities stable. |
| Maintenance | Original source was not found in the references searched. | MIT source, contributor documentation and regression tests are included. |

These findings do not mean every dawnd setup fails. Manual testing reported
recovery working with DAWindower where the tester's dawnd setup did not; the
individual UAC/display events were not logged. Read the
[recovery assessment](docs/rendering-recovery-assessment.md) and
[loading/recovery evidence](docs/standalone-validation.md) for the scope of that
comparison.

## Downloads and status

**Current `main`: 0.2.3 candidate source, including both editions.** The title bars,
4:3 resize behavior, launcher-fullscreen handling and X controls described above
are in this source. Build instructions for both editions are below.

**Published binary: [v0.2.2 standard preview](https://github.com/BuildWithRaymond/DAWindower/releases/tag/v0.2.2).**
That download predates X and the current title-bar, aspect-ratio and fullscreen
changes. It is an earlier preview, not a binary of current `main`. A 0.2.3 release
has not been published.

For current source, [GitHub Actions](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml?query=branch%3Amain)
builds both editions in Debug and Release, checks source/docs, and retains
Release candidate artifacts. CI artifacts are build outputs; actual-client
acceptance is recorded separately in the
[current validation record](docs/x-borderless-validation.md).

## Use it with an existing launcher

Replace your launcher's dawnd resource or injection path with **either
`DAWindower.dll` or `DAWindowerX.dll`**. Distribute only the chosen DLL. The
established sequence stays the same:

1. Create Dark Ages suspended.
2. Inject the absolute DLL path with the usual remote `LoadLibraryA` thread.
3. Wait for a nonzero module handle, release the injection resources, and resume
   the client's main thread.

**No `DAWindowerInitialize` call is needed.** Finish injection before startup
reaches the singleton check or graphics creation. Use one compatibility DLL per
client. The [integration guide](docs/integration.md) covers exact ordering,
fixed `dawnd.dll` filenames, launcher-selected fullscreen and optional exports.

## Build and test

Install Visual Studio 2022 or Build Tools with **Desktop development with C++**,
the Windows SDK, and **CMake 3.24+**. Use a Developer PowerShell with CMake on
`PATH`. Both presets select **Win32**, even on a 64-bit machine.

Run the commands for your edition from the repository root.

### Standard

```powershell
cmake --preset windows-x86
cmake --build --preset release
ctest --preset release
cmake --install build/windows-x86 --config Release
python tools/check_package.py dist/local --edition standard
```

Distribute **`dist/local/DAWindower.dll`**.

### X

```powershell
cmake --preset windows-x86-x
cmake --build --preset x-release
ctest --preset x-release
cmake --install build/windows-x86-x --config Release
python tools/check_package.py dist/local-x --edition x
```

Distribute **`dist/local-x/DAWindowerX.dll`**.

Each install directory contains exactly one DLL. The CRT is statically linked;
only Windows system components are runtime dependencies. The optional Python
package-check command validates the build output; Python is a development tool,
not a requirement for players or launchers.

Full tests create native DirectDraw/GDI windows and require an interactive
Windows desktop. Without one, use `ctest --preset unit-release` or
`ctest --preset x-unit-release`. You do not need Dark Ages installed to build
or run the synthetic tests. See the [development guide](docs/development.md)
for Debug builds and formatting, and the [testing guide](docs/testing.md) for
focused checks and actual-client acceptance.

## Verified behavior and current limits

The current validation record separates automated tests, actual-client checks
and manual observations:

| Evidence | Recorded result |
| --- | --- |
| Standard native contracts | 19/19 in Debug and Release. |
| X native contracts | 21/21 in Release. Debug initially passed 20/21; the remaining child-launch test passed after correcting its fixture timeout. Tooltip-related checks passed in both configurations after the final fix. |
| Actual 7.41 client loading | Both editions loaded through remote `LoadLibraryA` alone, with zero initialization calls and no adjacent proxy DLL. |
| Actual window behavior | Native 640×480 startup, intermediate aspect sizes, launcher-style fullscreen and minimize/restore were checked. Manual testing confirmed both title bars and smooth resizing. |
| X tooltip acceptance | All five corrected tooltips were confirmed on the actual client. |
| Hosted CI | Builds both editions in Debug/Release and runs no-desktop unit, source, documentation and package checks. |

Exact candidate hashes, build scopes and the initial X prototype's character
title/New client/native-reset observations are in the
[current validation record](docs/x-borderless-validation.md). Earlier recovery
tests included simultaneous actual clients and 20 concurrent synthetic clients;
those observations remain scoped to their recorded builds.

Current boundaries:

- Actual AoSda launch ordering and the complete UAC, lock/unlock, refresh-rate
  and monitor-change matrix have not been rerun on this candidate. Native
  launcher-style fixtures do not certify a specific launcher's full workflow.
- The verified graphics client uses v1 `IDirectDraw`/`IDirectDrawSurface` and
  named factory imports. Late injection, dynamic factory lookup and newer
  interfaces are outside the verified loading contract.
- Recovery defers while the client holds native Lock memory or a GetDC lease.
  Uncommitted writes destroyed during that lease cannot be reconstructed; an
  indefinitely retained invalid HDC needs client cooperation.
- Recovery timers need the game's message pump. Sustained 10–20-client
  performance and every hardware/client combination remain unmeasured.

## Find your first contribution

Start with [CONTRIBUTING.md](CONTRIBUTING.md). Documentation corrections,
focused regressions, reproducible client reports and measured performance work
all help. Keep fixes small, preserve the single-DLL contract, and distinguish
observed behavior from assumptions in test reports.

| If you want to understand… | Start here |
| --- | --- |
| How loading alone initializes the DLL | [Bootstrap and runtime](docs/architecture.md#loading-and-initialization) |
| Where frames and cursor updates meet | [Frame flow](docs/architecture.md#frames-and-cursor-composition) |
| How recovery preserves objects and pixels | [Recovery ownership](docs/architecture.md#recovery-and-object-lifetime) |
| How resizing and mouse mapping agree | [Window behavior](docs/architecture.md#window-and-input-behavior) |
| How X's title bar and controls work | [X edition](docs/x-edition.md#code-and-validation) |
| Which test protects a change | [Test map](docs/testing.md#automated-test-map) |
| Which reports are current or historical | [Documentation index](docs/README.md) |

DAWindower's original code is licensed under [MIT](LICENSE). The legacy dawnd
binary and the Dark Ages client are not included or covered by that license.
