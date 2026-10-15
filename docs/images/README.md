# Actual-client screenshots

Captured October 14, 2026, from isolated Dark Ages **7.41** clients on Windows
build **26120.6972**. Each client was created suspended, injected with its chosen
DLL using remote `LoadLibraryA`, and resumed with **zero initialization calls**.
Loaded-module paths and hashes were checked before capture.

Both show the actual main menu before login. The animated menu ring was captured
at different moments, so its colors differ. No character name, account details,
credentials, chat or unrelated desktop content appears in these images.

Both editions now use the [same caption renderer](../../src/window/caption.cpp).
Standard is PID 25000; X is PID 16020. Both loaded module hashes match the shared
caption candidates recorded below.

## Original captures

| Image | Whole window | Game area | Visible difference |
| --- | --- | --- | --- |
| [DAWindower](standard-native.png) | 640×516 | 640×480 | Game icon, shared gradient caption, host title and three window controls. |
| [DAWindower X](x-native.png) | 640×516 | 640×480 | Game icon, black gradient caption, New client, native-size reset and Windows controls. |

Both have a **36-pixel caption**, with game pixels reaching both sides and the
bottom. The game starts at (0,36) in each capture. Game dimensions match exactly.
Original PNG files are direct screen captures cropped to each window's visible
bounds. They have no retouching, overlays or resampling.

## README comparison

[The comparison panel](editions-comparison.png) adds a dark backdrop and labels
around the two original captures. Both game areas align. The embedded screenshot
regions were verified pixel-for-pixel against the originals:

- Standard: top-left (48,292), dimensions 640×516.
- X: top-left (752,292), dimensions 640×516.
- Game areas: top-left (48,328) and (752,328), each 640×480.
- Complete panel: 1440×980; click it in the README to view full resolution.

## Runtime checks

The captured standard Release build passed **21/21** contracts; X passed
**22/22**. Both packages passed the one-file PE32/i386, system-import and export
checks. The shared caption tests check that native Windows caption rendering
is disabled beneath the gradient, along with tooltips, button input and GDI lifetime.

Both actual clients loaded with `LoadLibraryA` alone. Each passed intermediate
width/height resizing, native launcher-style fullscreen, minimize/restore and
return to exactly 640×480 before capture. Both were also moved across several
desktop positions for visual review. Local logs are
`shared-caption-standard-retail.log` and `shared-caption-x-retail.log`.

The screenshots show window appearance. They do not certify recovery from UAC
or physical monitor changes, character-title timing after login, or long-running
performance. Earlier recovery observations retain their original scope in the
[standalone](../standalone-validation.md) and [X](../x-borderless-validation.md) records.

## Captured DLLs

| Edition | SHA256 |
| --- | --- |
| Standard | `91805735a195914ce69653415821c8329e9ec50f79e9a246c7a70faeea12bd02` |
| X | `626ffec32363ced0314bb6a7b13b02aa7f762be2885ac157ad455fe610323d91` |

Dark Ages artwork appears as captured from the installed client. The project's
MIT license covers its original code.
