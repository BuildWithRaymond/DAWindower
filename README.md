<div align="center">

# DAWindower

**Dark Ages on your desktop. One DLL per client.**

An open-source alternative to `dawnd.dll`, with automatic rendering recovery
and an optional edition with extra window controls.

[![Windows build](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml?query=branch%3Amain)

[Get started](#get-started) · [Launcher guide](docs/integration.md) · [Contribute](CONTRIBUTING.md)

</div>

[![Real Dark Ages clients: DAWindower with native Windows controls on the left, DAWindower X with its dark title bar and extra controls on the right](docs/images/editions-comparison.png)](docs/images/editions-comparison.png)

<p align="center"><sub>Real 7.41 clients. Matching 640×480 game areas. <a href="docs/images/README.md">Capture details and build versions.</a></sub></p>

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

| Edition | What you get |
| --- | --- |
| **DAWindower**<br>`DAWindower.dll` | Familiar native Windows title bar and controls. |
| **DAWindower X**<br>`DAWindowerX.dll` | Black gradient title bar, character name, **+** New client, **1:1** native-size reset and button tooltips. |

Same renderer and recovery. Choose **one edition per client**.
[X controls and supported clients →](docs/x-edition.md)

## Get started

**Current source: 0.2.3 candidate.** [Build either edition](docs/development.md)
or get candidate packages from [GitHub Actions](https://github.com/BuildWithRaymond/DAWindower/actions/workflows/ci.yml?query=branch%3Amain).

**Earlier download:** [v0.2.2 standard preview](https://github.com/BuildWithRaymond/DAWindower/releases/tag/v0.2.2)
predates X and the current resize/title-bar changes.

For launcher developers, replace your dawnd resource or injection path with
the chosen DLL:

1. Create Dark Ages suspended.
2. Inject the absolute DLL path with **`LoadLibraryA`** and wait for success.
3. Resume the client. **No extra initialization call.**

No specific launcher is required. [Integration details](docs/integration.md)
cover ordering, fixed filenames and fullscreen behavior.

Tested with actual Dark Ages **7.41** clients.
[Verification and limits →](docs/icon-validation.md)

## With love for dawnd

For more than twenty years, dawnd has helped this community play Dark Ages in
windows and run multiple clients. Thank you to **Saraet, creator of dawnd.dll**,
and everyone who kept it in players' hands.

DAWindower carries that legacy forward with recoverable rendering, open source
and optional X controls. [Full credits →](CREDITS.md)

---

[Contribute](CONTRIBUTING.md) · [Code tour](docs/architecture.md) · [Testing](docs/testing.md) · [All docs](docs/README.md) · [MIT license](LICENSE)
