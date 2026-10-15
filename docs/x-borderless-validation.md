# X, borderless windows and launcher geometry

The later [icon/caption validation](icon-validation.md) supersedes this record's
standard side-inset behavior and adds game icons to both editions. Results below
remain scoped to their recorded binaries.

Candidate source version: 0.2.3, October 14, 2026. This work is separate from the
immutable published 0.2.2 preview. Each edition still distributes exactly one
native x86 DLL; X is an alternative, not a dependency of standard DAWindower.

## Behavior under test

- Standard: native Windows title bar, no side/bottom frame insets, invisible resize hit regions.
- Both: nearest-pixel native 4:3 window sizing and fitted work-area maximize.
- X: caption-only charcoal-to-black gradient, Dark Ages/character title,
  per-button dark tooltips, native-size reset and asynchronous New client.
- A completed bare-popup launcher fullscreen selection owns bounds and style,
  whether applied before or after renderer attachment. X hides its caption there.
- Scoped EXE imports adapt late client MoveWindow/ShowWindow/SetWindowLongA
  resets without blocking direct native launcher operations or user-data writes.

## Reproduced regressions

Before the fixes, isolated native fixtures failed because pre-attach fullscreen
lost its popup style, post-attach fullscreen was
shrunk by windowed aspect constraints, and client MoveWindow reset placement.
The same fixtures passed after the shared frame/sizing and import adapters.
These are real Windows windows and IAT calls, with synthetic game behavior.

The user clarified that both windowed editions must retain title bars. A revised
regression caught the removed standard caption; standard now keeps native
caption painting/buttons while X keeps its own caption. The user also reported
snapping in both editions. Replaying consecutive corner drags reproduced an
outward step changing 641×481 backward to 640×480. Replacing the competing
width/height driver switch with a continuous closest-size 4:3 projection fixes
that feedback. Tests now apply repeated outward/inward corner requests and
walk every side-drag width between native tracking limits (bounded on huge desktops).

The installed 7.41 executable imports MoveWindow, ShowWindow and SetWindowLongA.
Read-only inspection found MoveWindow calls at VA 0x417599 and 0x419467. Its
SetWindowLongA calls at 0x416f43 and 0x418df0 use GWL_USERDATA rather than window
style. They must continue to work. No client SetWindowPos import was found.

## Current candidate packages

Each directory contains only the selected DLL. Static CRT and PE/import checks
passed; no companion binary or configuration is installed.

| Edition | Local candidate | Bytes | SHA256 |
| --- | --- | --- | --- |
| Standard | `dist/0.2.3-caption-smooth/DAWindower.dll` | 168,960 | `7b7e4e7927bfac959465e023c1ade78b2b515b8309cd333293ec865e3e2d7023` |
| X | `dist/0.2.3-x-tooltips/DAWindowerX.dll` | 207,360 | `3c46a50dd9621e76e9cf66bcbc3d6a97c931f659e2d0ebc50a8e79e34dbce7da` |

Release passed all 19 standard and 21 X contracts, including native/custom title bars,
continuous corner/side sizing, real window pixels/cursor composition, fullscreen
ordering, import ownership and X child injection. Standard Debug passed 19/19.
X Debug's full run passed 20/21: its UI launch harness timed out after 15 seconds,
although the fixture subsequently wrote both success records and exited. That
bound did not include the worker's own 15-second loader allowance plus native
fixture setup. Test-only budgets now include setup; the targeted launch contract
passed in Debug (3.64 seconds) and Release (2.83 seconds). Production launch
timeouts were unchanged. All current contracts therefore have passing local
evidence; the initial full-run timing failure remains recorded here.

Earlier local `0.2.3-borderless` and
`0.2.3-x-borderless` packages are superseded; their standard caption removal and
corner driver feedback are not the selected behavior.
The earlier X caption/smooth candidate had SHA256
`14af9e00030f542adf92aa633091a86a40138a37c8a1775673161d5368be8822`;
it established the geometry observations below but lacked working tooltip registration.

## Current actual-client loading and geometry

Two clean isolated copies of the installed 7.41 client were started suspended,
injected by remote LoadLibraryA, checked for a nonzero module handle, then resumed.
No initialization export was called. Both were running together after the helper completed.

| Edition | PID / HWND | Final review placement | Fullscreen round trip |
| --- | --- | --- | --- |
| Standard | 26176 / 0x004a0710 | 800×600 client at (40,40), native caption | 1920×1080 bare popup; minimize/restore and exact windowed return passed |
| X | 9352 / 0x0012052a | 800×600 client at (920,40), X caption | Same checks passed, with no caption/insets during launcher-selected fullscreen |

Client SHA256: `b168856a22a5315d2eb0fc942ca1cb6ef38e81035a445bc2cc9710d3d1a5715f`.
Host Windows build: 26120.6972. Both module lists contained Windows System32
DirectDraw and only the selected DAWindower edition; the isolated game folder
had no adjacent ddraw.dll. The helper also verified all 17 intermediate width
and 17 height steps, with coupled nearest-pixel aspect, before the fullscreen
round trip. Final standard style was 0x16cf0000; X was 0x960f0000.

Raw logs are `runtime-tests/normal-caption-smooth-retail.log` and
`runtime-tests/x-caption-smooth-retail.log`. As in earlier builds, the retail
WM_PRINTCLIENT captures were black and cannot prove visual progression. Manual
title-bar, drag feel, tooltip and game graphics acceptance is recorded separately.

The user confirmed both title bars and smooth resizing on these exact candidates.
They corrected their initial tooltip answer: X's hover effect worked, but no
tooltip appeared. A new native-control regression reproduced a tooltip window
with zero registered tools. The full TOOLINFOW size included a v6-only reserved
field and the older common-controls activation rejected registration. X now uses
the compatible V2 prefix for registration, updates and activation; no manifest
or companion binary is added. The regression checks all five descriptions after
the real hover timer, actual tooltip visibility and dismissal. See Microsoft's
[structure-size compatibility guidance](https://learn.microsoft.com/en-us/windows/win32/controls/common-control-versions).

The corrected tooltip registration, five delayed descriptions and dismissal
passed the native frame fixture in Release (3.93 seconds) and Debug (4.62 seconds).
The synthetic hover cancels/removes the real pointer's unrelated leave event,
then explicitly tests leave dismissal; it does not move the user's mouse.
X's LoadLibrary-only, presentation recovery and child-launch cases also passed
in both configurations after the tooltip-only production change.

## Native-size review and final tooltip acceptance

At the user's request, the helper's new `--launch-native` mode performs no resize
or fullscreen operations. It checks initial game dimensions before moving its
own new window. Standard PID 33656 / HWND 0x008406ba opened at (40,40); corrected
X PID 30324 / HWND 0x004906f2 opened at (760,40). Both started at exactly 640×480
game-client pixels with their captions outside that rectangle, using remote
LoadLibraryA alone and zero initialization calls. Logs:
`runtime-tests/normal-native-review.log` and `runtime-tests/x-native-tooltip-review.log`.

On corrected X SHA256 `3c46a50d…4dbce7da`, the user confirmed:
“All five tooltips appear.” The earlier title-bar/resize confirmation and this
new tooltip observation have separate build scopes; neither certifies every
desktop-interruption event on this candidate.

## Initial X prototype observation

The initial X prototype was loaded into an isolated actual 7.41 client through
remote LoadLibraryA while suspended, followed by ResumeThread, with zero
initialization calls. The user confirmed login-name title, native-size reset,
New client and normal graphics/mouse behavior: “All work correctly.”

That tested prototype was 202,240 bytes, SHA256
`d09227b9729b8334ad0a96c84b5000b4aeb9b73a110a3be7f1c22c2d2015b29d`.
It predates the later borderless/gradient/aspect/fullscreen changes. Do not reuse
that observation as acceptance of the changed candidate.

## Remaining runtime scope

Actual-client LoadLibrary-only startup and native launcher-style fullscreen
geometry checks passed. Manual title-bar and resize feel checks passed. All five
tooltip descriptions were confirmed on the corrected X candidate.
Full launcher workflows have not been exercised by the automated fixtures.
A partially completed pre-attach popup transition with neither maximized state
nor monitor bounds is ambiguous; see the
[launcher contract and limits](integration.md#borderless-windows-and-launcher-fullscreen).

UAC accept/cancel, lock/unlock, refresh/topology changes, logout-name timing and
extended multi-client operation are not separately certified for this candidate.
Previous recovery observations remain scoped to their recorded builds. Use the
[manual acceptance matrix](testing.md#manual-acceptance-matrix) for new evidence.
