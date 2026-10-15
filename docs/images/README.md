# Actual-client screenshots

Captured October 14, 2026, from isolated Dark Ages **7.41** clients on Windows
build **26120.6972**. Each client was created suspended, injected with its chosen
DLL using remote `LoadLibraryA`, and resumed with **zero initialization calls**.
Loaded-module paths and hashes were checked before capture.

Both show the actual main menu before login. The animated menu ring was captured
at different moments, so its colors differ. No character name, account details,
credentials, chat or unrelated desktop content appears in these images.

These captures predate the [game-icon restoration and standard caption-spacing fix](../icon-validation.md).
Their exact binary hashes remain recorded below; they are not screenshots of the
later candidate.

## Original captures

| Image | Whole window | Game area | Visible difference |
| --- | --- | --- | --- |
| [DAWindower](standard-native.png) | 640×511 | 640×480 | Native Windows title bar and controls. |
| [DAWindower X](x-native.png) | 640×516 | 640×480 | Black gradient caption, New client, native-size reset and Windows controls. |

The different total heights come from the **31-pixel native caption** and
**36-pixel X caption** on this desktop. The game dimensions match exactly.
Original PNG files are direct screen captures cropped to each window's visible
bounds. They have no retouching, overlays or resampling.

## README comparison

[The comparison panel](editions-comparison.png) adds a dark backdrop and labels
around the two original captures. Both game areas align. The embedded screenshot
regions were verified pixel-for-pixel against the originals:

- Standard: top-left (48,297), dimensions 640×511.
- X: top-left (752,292), dimensions 640×516.
- Complete panel: 1440×980; click it in the README to view full resolution.

The screenshots show window appearance. They do not demonstrate rendering
recovery, character-title timing after login, or long-running performance.
See the [validation record](../x-borderless-validation.md) for runtime evidence
and its build scopes.

## Captured DLLs

| Edition | SHA256 |
| --- | --- |
| Standard | `7b7e4e7927bfac959465e023c1ade78b2b515b8309cd333293ec865e3e2d7023` |
| X | `3c46a50dd9621e76e9cf66bcbc3d6a97c931f659e2d0ebc50a8e79e34dbce7da` |

Dark Ages artwork appears as captured from the installed client. The project's
MIT license covers its original code.
