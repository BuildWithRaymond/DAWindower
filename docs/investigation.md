# Baseline investigation — 2026-10-13

## Artifacts and scope

The workspace began empty. Read-only reference checkouts were placed in the
sibling `research/` directory; none are dependencies or included in distribution.
Examined Echo's `Resources/dawnd.dll`, historical Talos's same resource, and a
local Excalibur working-copy DLL. All three have SHA256:

`6772726967951c472f7bae53aeb7d70ba2e38605af5f814f9b130c6f04b8e0d9`

Binary: 27,911 bytes, PE32/i386 DLL, preferred base 0x67e40000, entry RVA 0x1060,
PE timestamp 2007-11-03 18:10:06 UTC. A timestamp is metadata, not proof of build
provenance. GCC 3.4.5/MinGW SJLJ strings are present. The sole export `DAWnd_v3`
is RVA 0x5020 in writable, non-executable, zero-initialized `.bss`. No instruction
references that address in the disassembly. There is no callable export ABI to copy.

The installed `KRU/Dark Ages/Darkages.exe` was inspected without execution or
changes: SHA256 `b168856a22a5315d2eb0fc942ca1cb6ef38e81035a445bc2cc9710d3d1a5715f`.
This fingerprint does not authenticate it as an unmodified official download.
Its graphics setup at VA 0x4496b0 calls DirectDrawCreate and operates the **v1**
IDirectDraw vtable (SetCooperativeLevel at byte offset 0x50, three-argument
SetDisplayMode at 0x54, CreateSurface at 0x18). Reports calling the runtime
"DirectDraw 7" do not establish use of IDirectDraw7 COM interfaces.

## Confirmed Dawnd hook map

RVAs below are relative to the DLL base; names are analysis labels, not symbols.

| RVA | Confirmed behavior |
| --- | --- |
| 0x1060 | MinGW CRT DLL entry; dispatches project initialization at 0x1180 |
| 0x1180 | On process attach, hooks five EXE imports; disables thread callbacks; does work under loader lock |
| 0x1270 / 0x12ae | Reads import directory; matches DLL/name-resolved function pointer; uses VirtualQuery/VirtualProtect and replaces an IAT slot |
| 0x1c74 | RegisterClassA: saves one original WNDPROC globally, installs 0x19da, extracts EXE icon |
| 0x1cfe | CreateWindowExA: forces WS_OVERLAPPEDWINDOW, adjusts initial outer size, uses default placement |
| 0x171e | DirectDrawCreate: calls original, creates a clipper via vtable offset 0x10; overwrites returned object's vtable slots 0x18/0x30/0x50/0x54 |
| 0x16c2 | SetCooperativeLevel: stores HWND, binds clipper via SetHWnd, calls original with DDSCL_NORMAL |
| 0x16b6 | SetDisplayMode: returns DD_OK without changing display mode; **RET 0x18** is suspicious for the v1 four-stack-argument call, which expects RET 0x10 |
| 0x1668 | GetDisplayMode: forwards call, overwrites pixel depth/masks to 16-bit RGB565, returns success |
| 0x150c | CreateSurface: detects caps 0x218 (PRIMARYSURFACE/FLIP/COMPLEX), replaces with 640×480 offscreen surface; forces RGB565; creates separate real primary and sets its clipper |
| 0x14b8 | Hooks that surface's **BltFast** slot 0x1c; calls original; for the stored virtual front surface invokes presentation |
| 0x13d0 | Presentation: GetClientRect + ClientToScreen; stretches fixed 640×480 source with real primary Blt/DDBLT_WAIT |
| 0x19da / 0x1800 | WNDPROC: handles activation, repaint/resize presentation, remaps mouse coordinates into 640×480, constrains WM_SIZING to 4:3 |
| 0x1bbb / 0x1be8 | WM_MOUSEMOVE/WM_NCMOUSEMOVE boundary ShowCursor(FALSE/TRUE), guarded by one global visibility flag derived from returned counter; does not hook ShowCursor itself |
| 0x1c3a | CreateMutexA: calls original and immediately ReleaseMutex; no renamed per-client mutex or wait loop |
| 0x1dd0 | RegQueryValueExA: forwards call; for value name `DisplayMode`, writes zero into first output byte |
| 0x1fxx–0x22xx | Atom operations belong to MinGW/libgcc shared exception-runtime pointer machinery, identified by `-LIBGCCW32-EH-3-SJLJ-GTHR-MINGW32` strings |

No fixed game-code addresses were found in the relevant hook path. Confirmed
assumptions are API names, vtable offsets, surface caps, 640×480, RGB565 and the
registry value name. Single process-global HWND/surface/original-function slots
and direct shared-vtable writes are fragile around multiple DirectDraw objects
and other hooks. Error results are not consistently checked. No project detach
path restores the IAT/vtables, releases retained graphics resources or restores
the original WNDPROC. These are static limitations; they do not prove a particular
reported freeze. The display-mode cleanup mismatch also needs controlled runtime
tracing before attributing any crash to it.

The installed EXE's mutex name is `Nexon.SingleInstance`. A separate Win32 probe
using a unique analysis-only name confirmed the old hook's error side effect:
creating an existing mutex gives ERROR_ALREADY_EXISTS (183), then releasing it
without ownership fails with ERROR_NOT_OWNER (288). That explains how Dawnd can
mask the game's singleton check; the exact game intent is inferred from the name
and startup check, not from an exported Dawnd contract. DAWindower instead suffixes
only this verified name with the process ID, retaining same-process mutex semantics
and leaving unrelated mutexes unchanged. It never releases a caller's mutex.

## Cursor evidence and remaining uncertainty

[DDrawCompat #231](https://github.com/narzoul/DDrawCompat/issues/231) reports Dark
Ages cursor flicker removed by DDrawCompat. The maintainer explains its black
thumbnails as redirected layered presentation with WS_EX_TOOLWINDOW, and reports
thumbnail fixes in v0.5.0. This is a wrapper presentation issue, not evidence
that ShowCursor counters caused the original flicker.

Current [DDrawCompat cursor handling](https://github.com/narzoul/DDrawCompat/blob/2c9a07fdf9308e2b0b117886a7e363b149ee1bc7/DDrawCompat/Gdi/Cursor.cpp)
tracks requested cursor handles, substitutes a transparent native cursor during
emulation, and schedules cursor overlay updates. Its
[presentation path](https://github.com/narzoul/DDrawCompat/blob/2c9a07fdf9308e2b0b117886a7e363b149ee1bc7/DDrawCompat/DDraw/RealPrimarySurface.cpp)
selects cursor emulation, while Resource/ShaderBlitter composite cursor content.
The successful architecture coordinates presentation and cursor ownership; it
does not spam ShowCursor(TRUE). It does not isolate which one of these changes
fixes Dark Ages.

Installed EXE has ShowCursor calls in state initialization/destruction and
state transitions (e.g. VAs 0x414833, 0x414b4f, 0x417814, 0x417883, 0x46688b),
and SetCursor calls in UI handling (e.g. 0x56fb36, 0x5700fc, 0x570112). Static
call sites do not establish per-frame counter oscillation. No live retail trace
was collected. Leading hypothesis: legacy native-primary presentation/cursor
ownership interaction; a counter imbalance remains a separate possibility.
The actual retail cause remains unconfirmed.

[DDrawCompat #599](https://github.com/narzoul/DDrawCompat/issues/599) reports
secondary-monitor presentation freeze, returning to normal on primary. Later
tests in that discussion reproduce **without Dawnd**, with GdiInterops=none and
several DDrawCompat versions. The maintainer explicitly rejects the D3D9 swap-chain
explanation: DDrawCompat does not use D3D9 and generally supports cross-monitor
presentation. No specific root cause was established; do not blame Dawnd or a
nonexistent D3D9 adapter binding as confirmed fact.

## Prototype decision and licenses

Options considered: reuse DDrawCompat's complete wrapper; only subclass cursor
messages; or isolate native offscreen rendering from desktop-primary presentation.
The first carries the unresolved #599 interaction; the second has no evidence
it controls the underlying race. The prototype chooses the third: native RGB565
offscreen surfaces, a virtual primary and GDI presentation on the existing window.
It copies one fixed frame, preserves cursor API semantics, uses one transform
for paint/input and does not recreate surfaces on resize or monitor movement.
This tests a plausible mechanism without rebuilding the game's renderer.

[Chaos.Client](https://github.com/Sichii/Chaos.Client/tree/05c7050f7144d1e7010f9ee761935783ad5ff2d8)
confirms a 640×480 virtual canvas, pixel filtering and virtual mouse scaling
concepts; its MonoGame pipeline is not retail-client evidence. Its license is
AGPL-3.0; no code was copied. DDrawCompat is 0BSD, but no source is reused either.
No license grant for the legacy Dawnd binary/source was found. Searches of public
GitHub repository metadata/README references, Talos and forks, Echo, and Saraet/
DAWnd_v3 references found bundled binaries and documentation, not original Dawnd
source. This bounded search does not prove that source never existed.

Reference commits: DDrawCompat `2c9a07f`, Echo `36d0eeb`, Chaos.Client `05c7050`,
historical Talos `3992da2`. Original DAWindower code is MIT. No Dawnd binary,
AGPL code or reference checkout is packaged. Only Windows system DLLs are used
at runtime. Modern Avalonia Talos code was unavailable; see integration notes.

## Windowed presentation correction — 2026-10-14

The retail graphics initializer selects `DDSCL_NORMAL` (8) or exclusive fullscreen
(0x13) at VA 0x449752/0x44975e and calls v1 `SetCooperativeLevel` at 0x44978b.
Windowed primary creation uses caps 0x200, without a flip chain. Rendering calls
v1 `BltFast` at 0x44a24e, using a fixed source rectangle and stored destination
offsets. Sprite blits at 0x452683 use flags 0x11 (`WAIT | SRCCOLORKEY`); the sprite
setup at 0x44ab03 sets source color key zero. These are static observations of the
installed EXE, not a live cursor trace.

DAWindower 0.1.0 converted windowed `BltFast` into a stretching `Blt` and normalized
destination pixels against the resized window. That shrank a fixed 640×480 frame
inside the virtual surface before the final window enlargement. A real-DirectDraw
test reproduces the fixed-size output, and a separate old-DLL run reads black
pixels from a saved cursor background outside the shrunken frame. Version 0.1.1
translates the screen origin and retains pixel coordinates; scaling occurs only
at presentation. Cursor keying is forwarded unchanged. Both corrected behaviors
pass the synthetic regression. Retail artifact and flicker confirmation remains
pending. `BltFast`'s surface-pixel and transparency semantics are documented by
[Microsoft](https://learn.microsoft.com/en-us/previous-versions/ms785072(v=vs.85)).

## Actual retail windowed path — 2026-10-14

The user's 0.1.1 recheck still showed fixed-size graphics and a black cursor
artifact. Diagnostic startup runs on the isolated copy confirmed DAWindower's
module path and DDSCL_NORMAL. The client created a non-flipping primary, then
repeatedly presented 640×480 memory-DC frames with GDI BitBlt/SRCCOPY to the attached
HWND. Those calls bypassed the virtual DirectDraw primary. The earlier synthetic
blit correction therefore did not cover the real windowed presentation path.

Version 0.1.2 adds an EXE-only BitBlt import hook scoped to attached windows. Both
window writes and window-source cursor readbacks use one retained RGB565 DIB.
Native DirectDraw primary pixels synchronize lazily before read/write access,
avoiding an unrelated black primary during GDI presentation. Per-window locks
protect the canvas; unrelated GDI calls remain native. Host WM_PAINT callbacks
are forwarded before repainting the retained frame. The legacy DisplayMode hook
is not reproduced, and registry values are not changed.

Old-DLL GDI and DirectDraw-readback regressions fail, the corrected cases pass,
and a new retail startup trace records canvas BitBlt and final presentation.
This proves activation of the observed path, not retail cursor-motion or resize
acceptance. Raster operations and DIB memory synchronization follow Microsoft's
[BitBlt](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-bitblt)
and [CreateDIBSection](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createdibsection)
contracts.

## Cursor transaction and actual output — 2026-10-14

The 0.1.2 retail recheck still failed. A longer isolated trace now observes paired
GDI cursor background reads from the HWND and writes back to it, clipped to 20x25
at the boundary. It did not observe primary-surface GetDC/readback calls in that
run. Cached primary DCs therefore are not established as this failure's cause.
The inspected EXE imports no GetCursorPos; mouse-message coordinates remain the
supported input path. These observations apply only to this EXE and trace.

A new actual-window regression reproduces 0.1.2 visibly erasing a software cursor
when a full background frame is presented before its replacement cursor. Version
0.1.3 queues one presentation through a registered window message, coalescing
updates until the owner dispatches it. Host paint callbacks still run; callbacks
that write a frame also defer presentation rather than exposing a cursor erase. Tests now
use native DirectDraw frame/cursor DCs, inspect real window pixels with a retained
startup DC, and check 4:3 bars and nearest-neighbor colors. Scaling options that
permit distortion or leave avoidable unused space are removed.

This identifies and corrects a presentation defect independently; it does not
prove the user's retail cursor flicker/artifact or resizing is fixed. The user
could not see the automatically launched trace client, so it was closed and no
further game launches were attempted. Retail interactive validation remains open.

## Aspect-locked window sizing — 2026-10-14

The user subsequently reported the 0.1.3 flashing-cursor fix was perfect. Its
retail resize trace showed width changing while client height remained 480; this
explains why the image stayed limited by height and side bars appeared.
Version 0.1.4 keeps the confirmed cursor path and handles
[WM_SIZING](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-sizing),
window-position size requests and maximize limits. Client dimensions are 4k by 3k,
so presentation fills the window without distortion or letterboxing. Edge/corner
dragging anchors the opposite side, and corner direction uses displacement from
the initial drag rectangle to avoid oscillation. Border dimensions use window
styles and [DPI-aware frame calculation](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-adjustwindowrectexfordpi),
with an older-system fallback. This also corrected a restore regression exposed
by using minimized rectangles as border measurements. The resizing change has
been independently tested; retail confirmation remains pending.
