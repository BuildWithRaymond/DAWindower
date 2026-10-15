# Actual-client screenshots

Captured October 14, 2026, from isolated Dark Ages **7.41** clients on Windows
build **26120.6972**. Each client was created suspended, injected with its chosen
DLL using remote `LoadLibraryA`, and resumed with **zero initialization calls**.
Loaded-module paths and hashes were checked before capture.

Both show the actual main menu before login. The animated menu ring was captured
at different moments, so its colors differ. No character name, account details,
credentials, chat or unrelated desktop content appears in these images.

These captures show the [restored game icons and corrected standard caption spacing](../icon-validation.md).
Standard is PID 16032; X is PID 28076. Both loaded module hashes match the final
icon/caption candidates recorded below.

## Original captures

| Image | Whole window | Game area | Visible difference |
| --- | --- | --- | --- |
| [DAWindower](standard-native.png) | 642×512 | 640×480 | Game icon with native caption spacing and Windows controls. |
| [DAWindower X](x-native.png) | 640×516 | 640×480 | Game icon, black gradient caption, New client, native-size reset and Windows controls. |

Standard has a **31-pixel native caption** and a one-pixel visible side/bottom
edge on this desktop. Its game starts at (1,31) within the capture. X has a
**36-pixel caption**; its game starts at (0,36). Game dimensions match exactly.
Original PNG files are direct screen captures cropped to each window's visible
bounds. They have no retouching, overlays or resampling.

## README comparison

[The comparison panel](editions-comparison.png) adds a dark backdrop and labels
around the two original captures. Both game areas align. The embedded screenshot
regions were verified pixel-for-pixel against the originals:

- Standard: top-left (47,297), dimensions 642×512.
- X: top-left (752,292), dimensions 640×516.
- Game areas: top-left (48,328) and (752,328), each 640×480.
- Complete panel: 1440×980; click it in the README to view full resolution.

The screenshots show window appearance. They do not demonstrate rendering
recovery, character-title timing after login, or long-running performance.
See the [validation record](../icon-validation.md) for runtime evidence
and its build scopes.

## Captured DLLs

| Edition | SHA256 |
| --- | --- |
| Standard | `76631219ca22076bf5d6ef4917a66778377418901c5e4e97d1f7f597d34906e1` |
| X | `bf520eb31b91fb2eafaf85c82cd02b910365643446d026e96eb27494b5a89c48` |

Dark Ages artwork appears as captured from the installed client. The project's
MIT license covers its original code.
