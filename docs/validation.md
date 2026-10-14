# Validation

Historical 0.1.x validation record. Current standalone DLL evidence is in
[standalone-validation.md](standalone-validation.md).

Automated coverage is deliberately narrow: geometry and mouse boundaries;
real system DirectDraw offscreen RGB565; virtual primary/backbuffer Blt and Flip;
RGB565 GDI color output and black bars; COM identity/interface rejection;
surface-busy rejection for Lock/GetDC; GDI updates through ReleaseDC;
window destruction before surface release and during attachment; idempotent
initialization; busy shutdown; final cleanup and import restoration. Both the
explicit import-hook path and actual `ddraw.dll` proxy loading are exercised.
The singleton test verifies per-process isolation, preserved same-process
ERROR_ALREADY_EXISTS behavior, and unchanged unrelated mutex names.

These tests do not establish the game's cursor-flicker behavior. The GDI test
reproduces an intermediate cursor-erasure frame and inspects actual window pixels.
The original installed game EXE was read only. No experimental DLL was injected
into production clients, and Excalibur was not stopped or restarted. A separately
copied test client was launched and stopped during an opt-in startup check.

Local prototype verification on 2026-10-13: x86 MSVC 19.44, Windows SDK
10.0.26100.0. Debug and Release each passed all three CTest cases. Both Debug
loading paths also passed with Diagnostics=1, observing exactly the two deliberate
ShowCursor calls and two SetCursor calls with unchanged results. Twenty concurrent
Release harness processes (ten explicit-init, ten proxy) passed startup/exit
contracts in 8.67 seconds. This short smoke test is not a retail performance
benchmark or evidence of long-running multibox stability. GitHub CI was authored
but has not been run remotely.

On 2026-10-14, the user reported an outer window that resized while the game image
stayed fixed, plus a black square behind the game-drawn cursor, using the isolated
test shortcut. A new real-DirectDraw regression reproduced incorrect windowed
primary `BltFast` scaling in 0.1.0. A separate run against the old DLL also showed
black pixels in a saved cursor background. The corrected blit preserves logical
pixel coordinates and translates only the screen origin. The regression checks
full-frame pixels at 1280×960, 4:3 letterboxing at 1280×720, zero-color-key cursor
blits, saved cursor backgrounds, and mouse mapping through both loading paths.
These synthetic checks are evidence for the blit correction, not proof that the
reported retail cursor artifact or the original flicker is resolved.
Debug and Release builds each passed all five CTest cases after the correction
(10/10 total). No production client was injected, restarted, or stopped.

The user's 0.1.1 retail recheck failed with the same symptoms. A bounded startup
trace then confirmed native windowed GDI `BitBlt` presentations to the attached
HWND, with no wrapped-primary presentations. Version 0.1.2 captures those window
writes, redirects window-source cursor reads to logical pixels, and synchronizes
DirectDraw primary readbacks with that canvas. New GDI regressions fail against
0.1.1 for both enlarged output and black primary readback, and pass with 0.1.2.
They also verify host WM_PAINT execution and native GDI after shutdown. A copied
retail client startup trace now records successful canvas writes and presentation.
It does not establish mouse-motion artifact elimination or resize behavior in
the actual game; those interactive checks still need confirmation.
Version 0.1.2 Debug and Release each passed all seven CTest cases (14/14 total).

The user's 0.1.2 retail recheck failed again. Version 0.1.3 enforces aspect fit
and nearest neighbor, and queues presentation after drawing calls complete. Its
GDI tests use native DirectDraw frame/cursor DCs and a retained startup window DC.
They verify actual enlarged window pixels, exact 2x pixel blocks, fractional
color preservation, centered bars, and no visible cursor-erasure intermediate
from either direct frame writes or the host's WM_PAINT callback.
WM_PRINTCLIENT snapshots remain separate from actual-window checks. Debug tracing
records resized dimensions/clip bounds and separately budgets cursor read/write
events so startup frames do not consume those diagnostics.

The latest isolated retail trace observed paired window-source cursor save and
window-destination restore calls, clipped to 20x25 at the client boundary. No
retail resize was captured. The user could hear but could not see the automatically
launched copy; its exact-path-verified process was closed on request. Desktop
control was unavailable, and no more retail launches were performed. Retail
resizing, black cursor artifacts and flicker remain unresolved acceptance items.

Version 0.1.3 x86 MSVC Debug and Release each passed all seven CTest cases
(14/14 total, 12.16 and 12.42 seconds respectively) after both cursor-presentation
regressions were reproduced as failures and corrected. No game was launched
with the final 0.1.3 build.

## Retail acceptance, on an isolated test installation

The user confirmed 0.1.3 cursor flashing was fixed on the inspected retail client.
Its resize trace showed growing width with height held at 480, so the aspect-fit
renderer correctly left side bars. Version 0.1.4 instead constrains window sizing
itself: all eight edge/corner drags, arbitrary size requests, and maximize produce
exact 4:3 client dimensions. Tests exercise upward corner growth, opposite-edge
anchors, actual no-bar window pixels, corner hit tests, and minimize/restore.
Frame sizes derive from window styles/DPI rather than stale minimized rectangles.
Retail corner resizing and multi-monitor/DPI behavior still need manual rechecks.

Version 0.1.4 Debug and Release each passed all seven CTest cases (14/14 total;
12.45 and 12.27 seconds respectively). The new corner/edge contract failed on
0.1.3 before implementation. A minimize/restore regression was then reproduced
and corrected with style/DPI-based frame sizing. Cursor/GDI/DirectDraw rendering
source files match the user-confirmed 0.1.3 implementation. The isolated shortcut
now loads 0.1.4 on its next launch; no retail client or Excalibur was launched,
closed, restarted or injected during this change.

Record Windows build, game EXE SHA256, DAWindower build,
GPU(s), monitor topology, resolutions, refresh rates and DPI percentages.
Keep comparable baseline recordings with native DirectDraw and legacy Dawnd.
Use the same monitor and focus sequence for each configuration.

1. Launch one test client with the proxy; leave production clients unchanged.
   Verify login and world rendering, colors, text and all cursor shapes.
2. Move pointer continuously across game image, borders and caption.
   Test hover, left/right clicks, double clicks, wheel, captured dragging outside
   the window, and returning. Compare click target to the visible image at
   320×240, 640×480, 960×720 and 1280×960. Drag all corners and sides; verify both
   dimensions grow and no letterbox bars appear. Inspect nearest-neighbor pixels.
3. Resize repeatedly, drag the caption, maximize, minimize/restore and Alt+Tab.
   Watch frame continuity and CPU use. Check live and minimized taskbar previews.
4. Move across primary/secondary monitors in both directions and halfway between.
   Include negative desktop coordinates, 100%/150%/200% DPI, different resolutions
   and refresh rates, and different adapters where available. Verify graphics and
   cursor continue updating and clicks remain aligned.
5. Test the game's windowed/fullscreen selection. DAWindower virtualizes supported
   640×480/16-bit fullscreen requests; physical exclusive fullscreen is not exposed.
6. Launch 2, then 10, then 20 test clients. Switch focus rapidly, drag/resize several,
   close one while others render, and run for several hours. Record per-process CPU,
   working set, GDI/USER handles, frame behavior and any presentation HRESULT failures.
7. Repeat launch/exit 30 times. Check residual processes and growing handle counts.

Debug diagnostics are observers: `ShowCursor` return counts and `SetCursor`
handles are forwarded unchanged. Capture them with a Windows debug-output viewer.
Use Debug tracing briefly; high-frequency cursor output can perturb timing.
The explicit initializer can observe startup calls; proxy startup may begin after
the game's earliest cursor calls. A native baseline trace is still needed to
prove which retail mechanism produces the flicker.

Any synthetic pass should be reported separately from each retail acceptance
result. A lost-surface test needs a genuine loss event; successful Restore on a
system-memory surface alone does not establish recovery from driver reset.
