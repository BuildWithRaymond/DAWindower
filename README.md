<div align="center">

# DAWindower

**Dark Ages on your desktop. One DLL per client.**

An open-source alternative to `dawnd.dll`, with automatic rendering recovery
and an optional edition with extra window controls.

[![Windows build](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml?query=branch%3Amain)

[Get started](#get-started) · [Repository cleanup](#repository-cleanup-and-organization) · [Launcher guide](docs/integration.md) · [Contribute](CONTRIBUTING.md)

</div>

[![Real Dark Ages clients with matching black gradient captions: DAWindower with three window controls on the left, DAWindower X with extra client controls on the right](docs/images/editions-comparison.png)](docs/images/editions-comparison.png)

<p align="center"><sub>Real 7.41 clients. Shared gradient caption. Matching 640×480 game areas. <a href="docs/images/README.md">Capture details and build versions.</a></sub></p>

## Familiar game. A stronger foundation.

- **Recover automatically.** Per-window rendering recovery after desktop
  interruptions, including background clients.
- **Resize naturally.** Native 640×480 startup, smooth 4:3 resizing and matching
  mouse coordinates.
- **Make it your own.** MIT-licensed C++17, readable source and native regression
  tests. No hooks into networking, gameplay or automation.

Native **32-bit Windows**. Each edition is **one standalone DLL**, using only
Windows system components. No proxy DLL, configuration files or extra runtime.

## Choose your edition

**Standard stays barebones:** familiar dawnd-style rendering and windowing for
launchers and other projects, with fixes for known rendering, recovery and window
bugs. **New features and extra controls go to X.** Core bug fixes and renderer
maintenance remain shared between both editions.

| Edition | What you get |
| --- | --- |
| **DAWindower**<br>`DAWindower.dll` | Black gradient title bar with the client-supplied title and minimize, maximize/restore and close controls. |
| **DAWindower X**<br>`DAWindowerX.dll` | Same title behavior, **+** New client, **1:1** native-size reset and button tooltips. |

Same renderer and recovery. Choose **one edition per client**.
Both editions preserve titles supplied by the game or launcher, including character
names when provided. Captions refresh from Windows title text, including updates
from another process; neither edition reads character-name buffers from game memory.
[X controls and title behavior →](docs/x-edition.md)

## Repository cleanup and organization

The latest maintenance pass makes the codebase easier to read, navigate and
contribute to:

- **Clearer code.** Descriptive callback and API parameter names, named bootstrap
  roles, and notes explaining ownership, lifetimes and runtime contracts.
- **Less duplication.** Removed unused helpers and includes, consolidated shared
  frame setup and build source lists, and clarified the PE inspection tool.
- **Better navigation.** Expanded repository and tool maps, refreshed contributor
  and integration guides, and clearly labeled superseded design records.

The cleanup passed **90 native regression checks** across both editions in
Release and Debug, along with formatting, documentation-link and package checks.
**Existing clients do not need a DLL update for this cleanup.**

[Explore the code and tool maps →](docs/architecture.md) · [Contributor guide →](CONTRIBUTING.md)

## Get started

**Current source: 0.2.3 candidate.** [Build either edition](docs/development.md)
or get candidate packages from [GitHub Actions](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml?query=branch%3Amain).

**Earlier download:** [v0.2.2 standard preview](https://github.com/BuildWithRaymond/DAWindower/releases/tag/v0.2.2)
predates X and the current resize, title-bar and presentation changes.

For launcher developers, replace your dawnd resource or injection path with
the chosen DLL:

1. Create Dark Ages suspended.
2. Inject the absolute DLL path with **`LoadLibraryA`** and wait for success.
3. Resume the client. **No extra initialization call.**

No specific launcher is required. [Integration details](docs/integration.md)
cover ordering, fixed filenames and fullscreen behavior.

Tested with actual Dark Ages **7.41** clients.
[Verification and limits →](docs/images/README.md#runtime-checks)

## With love for dawnd

For more than twenty years, dawnd has helped this community play Dark Ages in
windows and run multiple clients. Thank you to **Saraet, creator of dawnd.dll**,
and everyone who kept it in players' hands.

DAWindower carries that legacy forward with recoverable rendering, open source
and optional X controls. [Full credits →](CREDITS.md)

---

[Contribute](CONTRIBUTING.md) · [Code tour](docs/architecture.md) · [Testing](docs/testing.md) · [All docs](docs/README.md) · [MIT license](LICENSE)
