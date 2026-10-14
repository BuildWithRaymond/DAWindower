# DAWindower

**Dark Ages on your desktop. One DLL.**

DAWindower is a native windowing and rendering compatibility layer for the
32-bit Dark Ages client. It keeps the game's RGB565 graphics, gives windows room
to resize, fixes the software-cursor presentation path, and recovers retained
frames when the Windows desktop is interrupted.

**Windows · x86 · C++17 · MIT · Windows system dependencies only**

**[Download v0.2.2 preview](https://github.com/BuildWithRaymond/DAWindower/releases/tag/v0.2.2)** ·
[GitHub repository](https://github.com/BuildWithRaymond/DAWindower)

[Build](#build-and-test) · [Launcher integration](docs/integration.md) ·
[Contribute](CONTRIBUTING.md) · [Code tour](docs/architecture.md) ·
[Test evidence](docs/smooth-resize-validation.md)

## With love for dawnd

For more than twenty years, dawnd has been part of how this community plays
Dark Ages. It gave players windowed clients, made multiple clients practical,
and kept getting passed along in the launchers people relied on. That kind of
staying power deserves respect.

DAWindower carries that usefulness forward. The goal is familiar Dark Ages
behavior with recoverable rendering, a dependable loading contract, and source
that the next contributor can understand. Thank you to dawnd's creators and to
everyone who kept it in players' hands. [Credits and research acknowledgments](CREDITS.md).

## What you get

- **One standalone `DAWindower.dll`.** Ordinary early `LoadLibraryA` injection;
  no extra initialization call, proxy DLL, configuration file, runtime package,
  or launcher-specific dependency.
- **Smooth resizing.** Width and height change independently in one-pixel steps.
  Graphics fill the client area without black bars; non-4:3 sizes stretch proportions.
- **Faithful game pixels.** The logical frame stays 640×480 RGB565. Nearest-neighbor
  presentation avoids blur, and mouse coordinates use the same scale as the image.
- **A coordinated software cursor.** Frame and cursor writes are presented together
  so the cursor's erased-background intermediate frame stays off screen.
- **Visible resize arrows.** Native Windows arrows appear on edges and corners,
  remain during drag, and hand control back to the game's cursor inside.
- **Automatic recovery per window.** Retained pixels, surface restoration and bounded
  presentation retries handle loss and desktop interruptions. Recovery watches secure
  desktop/UAC, lock/unlock, display/device changes and restore events for background clients too.
- **Multiple clients.** Only the inspected game's singleton mutex is isolated per
  process. Networking, gameplay and automation paths are not hooked.

## What changes from dawnd

Both DLLs share the important baseline: a small 32-bit library, ordinary launcher
injection, windowed play, and support for multiple client processes. Here is where
DAWindower's implementation differs from the inspected legacy binary.

| Area | Inspected dawnd | DAWindower |
| --- | --- | --- |
| Desktop presentation | Its converted-primary path draws into a separate real DirectDraw primary. | A retained RGB565 canvas presents through GDI into each window. |
| Failed presentation | That private-primary path discards the presentation result; no repair path was found for the private surface. | Failed presentations retain the frame and retry with bounded backoff, even without new game writes. |
| Lost surface content | No wrapper-owned pixel backup/repopulation was found in that path. | Completed pixel writes are retained; recovery reapplies color keys and keeps public COM identities stable. |
| Software-cursor updates | Cursor artifacts were reproduced in the tested legacy setup. | Logical cursor reads/writes share the retained frame; presentation coalesces updates. |
| Resizing | Full-window presentation, with 4:3 constraints in the inspected sizing handler. | Full-window presentation with independent, arbitrary client dimensions. |
| Initialization | Import hooks are installed during process attach. | Only small import gates run at attach; full setup finishes synchronously at the first factory call outside loader lock. |
| Contribution | Research examined a bundled binary; original source was not found in the references searched. | MIT source, documented ownership, reproducible builds, and regression tests are included. |

These are scoped findings, not a claim that every dawnd setup fails. The user
reported interruption recovery working with DAWindower where their dawnd setup
did not. The exact UAC, display and monitor event matrix was not individually
logged. See the [original investigation](docs/investigation.md),
[recovery assessment](docs/rendering-recovery-assessment.md), and
[loading/recovery record](docs/standalone-validation.md) for evidence and limits.

## Use it with an existing launcher

Distribute **only `DAWindower.dll`** and replace your launcher's old dawnd resource
or injection path with it. The established sequence stays the same:

1. Create Dark Ages suspended.
2. Inject the absolute DLL path with the usual remote `LoadLibraryA` thread.
3. Wait for a nonzero module handle, release the injection resources, and resume
   the client's main thread.

No call to `DAWindowerInitialize` is required. Finish injection before startup
reaches the singleton check or graphics creation. Talos and Zeus are not required.
Use one compatibility DLL per client; consult the
[integration guide](docs/integration.md) for the precise contract and optional exports.

## Build and test

Install Visual Studio 2022 or its Build Tools with **Desktop development with C++**,
the Windows SDK, and CMake 3.24 or newer. Use a Developer PowerShell with CMake on
`PATH`. The shared preset selects **Win32**, even on a 64-bit machine.

From this repository's root:

```powershell
cmake --preset windows-x86
cmake --build --preset release
ctest --preset release
cmake --install build/windows-x86 --config Release
```

The installed runtime package is:

```text
dist/local/
└── DAWindower.dll
```

The full suite uses native DirectDraw/GDI windows and needs an interactive Windows
desktop. CI runs the no-desktop unit tests, source checks and package checks;
retail acceptance remains a separate runtime check. For Debug builds, focused
tests, formatting, diagnostics and build troubleshooting, start with
[the development guide](docs/development.md) and [testing guide](docs/testing.md).

## Verified behavior and current limits

Version **0.2.2** passed **13/13 contracts in both Debug and Release**. Its
164,352-byte x86 package was loaded into an isolated actual Dark Ages client with
`LoadLibraryA` alone. The user confirmed smooth full-window resizing, correct
game mouse behavior, and visible resize arrows in the tested builds.

Earlier recovery testing also covered simultaneous actual clients and 20
concurrent synthetic clients. The acceptance records distinguish automated
fault injection, actual-client loading, and manual observations. They do not
certify every GPU, monitor arrangement or client version.

Current boundaries:

- The verified client uses v1 `IDirectDraw`/`IDirectDrawSurface` and named factory
  imports. Late injection, newer interfaces and dynamic factory lookup are outside
  the verified loading contract.
- Recovery defers while the client holds native Lock memory or a GetDC lease.
  Uncommitted writes destroyed during that lease cannot be reconstructed; an
  indefinitely retained invalid HDC needs client cooperation.
- Timers need the game's message pump to continue running. Sustained 10–20-client
  performance and a complete physical display-interruption matrix remain unmeasured.

[Current resize/cursor acceptance](docs/smooth-resize-validation.md) ·
[Standalone loading/recovery acceptance](docs/standalone-validation.md) ·
[Release history](CHANGELOG.md)

## Find your first contribution

Start with [CONTRIBUTING.md](CONTRIBUTING.md). Documentation corrections, focused
regressions, reproducible client reports and measured performance work all help.
You do not need the game installed to build or run the synthetic native tests.

| If you want to understand… | Start here |
| --- | --- |
| How loading alone initializes the DLL | [Bootstrap and runtime](docs/architecture.md#loading-and-initialization) |
| Where frames and cursor updates meet | [Frame flow](docs/architecture.md#frames-and-cursor-composition) |
| How recovery preserves objects and pixels | [Recovery ownership](docs/architecture.md#recovery-and-object-lifetime) |
| How resizing and mouse mapping agree | [Window behavior](docs/architecture.md#window-and-input-behavior) |
| Which test protects a change | [Test map](docs/testing.md#automated-test-map) |
| Which reports are current or historical | [Documentation index](docs/README.md) |

DAWindower's original code is licensed under [MIT](LICENSE). The legacy dawnd
binary and the Dark Ages client are not included or covered by that license.
