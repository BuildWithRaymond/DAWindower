# Free resizing and visible resize cursors — 2026-10-14

Raw files named below are ignored local development artifacts, not published
source links. Reproduction steps are in [testing](testing.md).

## Rendering behavior

Version 0.2.1 removed the 4:3 sizing constraints and the 4-by-3 dimension grid.
Windows now accepts independent width and height changes in one-pixel steps,
subject to normal native tracking limits. The entire 640×480 RGB565 image is
scaled into the client rectangle with nearest-neighbor sampling. No letterboxing
or cropping is used. Non-4:3 windows stretch the image's proportions.

This reproduces the inspected original dawnd presentation behavior: its private
presentation routine at RVA 0x13d0 stretches the source into the complete client
rectangle. Mouse mapping uses the same independent scaling on both axes.
The titlebar redesign and extra buttons were deferred at the user's request.

## Test-first evidence and actual client

Before the sizing correction, a requested 640×503 client became 640×480, and
the 1280×720 viewport test failed because the old viewport was only 960 pixels
wide. Afterward, Release passed all 12 contracts, including one-pixel width and
height sequences, portrait windows, all eight native edge/corner sizing messages,
full-client GDI pixels, nearest-neighbor sampling and mouse-coordinate boundaries.
See Release test log (`runtime-tests/smooth-resize-release-tests.log`).

Actual isolated Dark Ages process **17324** loaded the exact version 0.2.1 file
from `dist/0.2.1/DAWindower.dll` by remote LoadLibraryA alone before resuming the
main thread. No initializer export was called. All widths 1000–1016 at height
703 and heights 703–719 at width 1016 were accepted. Only the injected DLL and
system DirectDraw appeared among the enumerated graphics modules.
See actual-client log (`runtime-tests/retail-smooth-resize.log`).

Version 0.2.1 was 164,352 bytes, SHA256:
`c19d01763f7d071d0f4bf812cbbbf021ab4f57221656d37b8dc176c645dd8701`.
The inspected EXE SHA256 was
`b168856a22a5315d2eb0fc942ca1cb6ef38e81035a445bc2cc9710d3d1a5715f`.
The user confirmed: “Smooth, fills window, cursor correct.” This confirms the
game cursor and resizing on that build, not frame-arrow visibility: the user
subsequently reported the resize cursor was effectively absent.

## Frame cursor correction

The retail trace records `ShowCursor(0) -> -1` on the window thread. The game
hides the Windows cursor for its software cursor. Previously the frame subclass
had no cursor handling, leaving resize pointers subject to that hidden count and
the host's cursor messages. A new LoadLibrary-only regression reproduced an
absent frame arrow with a hidden Windows cursor and a host that clears shapes.
See failing regression (`runtime-tests/resize-cursor-before.log`).

Version 0.2.2 selects the user's Windows horizontal, vertical or diagonal resize
cursor for each native resize hit code. It adds only the visibility increments
needed to make that cursor visible, with a hard bound of 64 owned increments.
Existing positive counts are preserved; repeated hover does not accumulate calls.
The exact owned increments are balanced on return to the client, nonclient leave,
drag completion, cancellation, deactivation and window destruction. During a
captured resize drag, the selected native arrow stays active. Client-area cursor
messages otherwise reach the host, preserving the software-cursor pipeline.

The counter and message contracts follow Microsoft's
[ShowCursor](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showcursor),
[WM_SETCURSOR](https://learn.microsoft.com/en-us/windows/win32/menurc/wm-setcursor)
and [TRACKMOUSEEVENT](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-trackmouseevent)
documentation. No cursor asset, dependency, configuration file or launcher call
was added. Abnormal applications that hide the OS cursor more than 64 levels
cannot be forced visible by this bounded mechanism; the observed client hides
it once.

## Version 0.2.2 package and runtime

The installed package is `dist/0.2.2/DAWindower.dll`, **164,352 bytes**,
PE32/i386, static CRT. SHA256:
`635881c4a99d8cb6e49065addbdd4e42fcb70b449ae67369ca062dd7251536c3`.
Installation produced exactly one file. Imports remain exclusively USER32,
GDI32, COMCTL32, WTSAPI32 and KERNEL32; native DirectDraw comes from Windows.
See PE inspection (`runtime-tests/resize-cursor-pe.json`).

Release and Debug each passed **13/13** contracts, including the new cursor test. It covers
correct system shapes on all eight edges/corners, visibility with negative and
already-positive game counts, 1,000 repeated hover events without count drift,
additional application hide calls, captured drag and every cleanup route.
Existing GDI cursor composition, mouse mapping, smooth resizing, initialization,
surface restoration and presentation recovery remain covered.
See Release log (`runtime-tests/resize-cursor-release-tests.log`) and
Debug log (`runtime-tests/resize-cursor-debug-tests.log`). The final Debug
build completed with zero warnings and zero errors.

Actual isolated retail process **21756** loaded this exact package by remote
LoadLibraryA, with **zero initialization calls**, before resuming startup. It
accepted the same one-pixel width/height sequences and ended at 1016×719. The
loaded module list contained the final package and Windows system DirectDraw.
See actual-client log (`runtime-tests/retail-resize-cursor.log`).
The user tested this new retail client and confirmed: “Arrows visible; game
cursor correct.” The requested check included edge/corner hover, dragging and
return to the game's own cursor. The automated cursor regression uses a native
synthetic host rather than reading the retail process's private cursor counter.

## Scope of evidence

The [version 0.2.0 record](standalone-validation.md) retains earlier actual
multiclient and interruption-recovery evidence and its limits. Full hardware
topology coverage and sustained 10–20-retail-client performance remain unmeasured.
WM_PRINTCLIENT captures are not reliable proof of retail frame progression on
this client. Existing clients and Excalibur were not restarted or modified.
