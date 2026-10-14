# Rendering recovery assessment — 2026-10-14

**Historical assessment:** this records the pre-0.2.0 prototype. Implemented work
and current evidence are in [standalone-validation.md](standalone-validation.md)
and [standalone-recovery-design.md](standalone-recovery-design.md).
Raw disassembly paths name local research artifacts outside the source repository.

**Recommendation:** add measured, bounded recovery to the existing standalone
x86 DAWindower architecture first. Keep its confirmed cursor presentation fix.
An optional Direct3D presenter needs evidence that this approach fails recovery
or performance acceptance. No rendering code or shipped DLL changed in this
investigation.

## 1. Cause: verified defect, incident evidence still needed

The user clarified that the freeze occurs with legacy `dawnd.dll` across Dark
Ages clients. `DDERR_SURFACELOST` is reported evidence;
the failing call and affected surface have not been captured here during UAC.

The legacy DLL has a specific recovery hole in its primary/flip/complex
conversion path: it creates a separate real primary surface, presents into it,
ignores that presentation HRESULT, and provides no restoration/recreation path
for that private surface. The game cannot restore a surface it never receives.
This explains how game rendering/logic can continue while a window remains
frozen. **It is the leading incident mechanism, not yet a runtime-confirmed
attribution.** Its virtual front/source may also be lost. Confirm that the
affected instance uses this path: plain windowed caps `0x200` do not enter that
conversion branch and require tracing game-owned surfaces/GDI presentation.

The claim that the client never handles loss is too broad. The inspected installed
EXE uses v1 `IDirectDraw`/`IDirectDrawSurface` and contains:

| Installed EXE VA | Verified behavior |
| --- | --- |
| `0x44a0cb`–`0x44a0e6` | Calls primary `IsLost()` at vtable offset `0x60`, compares with `0x887601c2`, and calls `Restore()` at offset `0x6c`; aborts that update if restoration fails. |
| `0x44a24e` | Calls `BltFast()`; this branch does not examine its HRESULT before returning. |
| `0x44b997`–`0x44b9ba` | Calls surface `Unlock()`; on `DDERR_SURFACELOST`, calls `Restore()` and jumps back to `Unlock()` without checking restoration's result or bounding retries. |

The last path could spin while restoration is unavailable if `Unlock()` keeps
returning loss. That requires a failing-thread trace; static code alone does not
prove a hang. Findings apply to this EXE fingerprint, not every client version.

Fresh PE inspection and MSVC disassembly confirm the existing Dawnd analysis:

| Legacy DLL RVA | Resource lifecycle / error behavior |
| --- | --- |
| `0x171e` | Hooks `DirectDrawCreate`, creates a clipper, and patches native object vtable entries. |
| `0x150c` | Converts caps `0x218` primary/flip/complex into a 640×480 RGB565 offscreen surface; uses caps `0x40`, without explicitly forcing system memory. Separately creates real primary and attaches clipper. Other surfaces retain game-driven creation with forced RGB565 format. |
| `0x14b8` | Patches `BltFast`; saves original result, invokes private presentation for virtual front, then returns original blit's result. Presentation failure is hidden. |
| `0x13d0` | Real primary `Blt(DDBLT_WAIT)` stretches virtual front into window screen rectangle; return value is discarded. No loss check, restore, or recreation follows. |
| `0x19da` | Resize/repaint invoke the same presentation path. Activation is routed specially, but does not repair surfaces. No explicit display/session recovery was found. |
| Attach/detach | Initialization patches EXE imports during process attach. No project-owned detach cleanup for retained primary/clipper or hook restoration was found; game-owned objects retain native release behavior. |

Current DAWindower already avoids the legacy real desktop primary. Its
[creation path](../src/graphics/draw.cpp) uses RGB565 system-memory offscreens
under `DDSCL_NORMAL`; its [GDI path](../src/graphics/gdi.cpp) retains a logical
DIB canvas. Prior retail traces show windowed GDI `BitBlt` frames and cursor
readbacks, so tracing only primary `BltFast` would miss the observed path.

Current [surface wrapper](../src/graphics/surface.cpp) still forwards failed
`Blt`, `BltFast`, `Lock`, and `GetDC`. `IsLost()` is forwarded. `Restore()` only
runs on a caller's request, resets the synchronization generation, and restores
the separately emulated backbuffer. There is no automatic recovery coordinator
or native surface recreation. COM destruction releases native surfaces,
attachments, clipper, and owner; window destruction untracks the HWND and DIB/DC
cleanup follows `WindowState` lifetime.

Current [presentation](../src/window/window.cpp) invalidates on size/display
changes. Queued presentation clears its pending flag before attempting paint;
failure leaves the retained pixels but has no scheduled recovery retry. Further
draws/paint can retry. No dedicated activation/session-resume recovery exists.
This is a robustness gap; the same reported freeze has not been reproduced with
DAWindower.

Needed incident evidence: before/during/after one secure-desktop transition,
record per-surface identity/role/caps, source and destination HRESULTs,
`IsLost`/`Restore` results, graphics-thread heartbeat, and presentation activity.
Check the private real primary separately from the game-visible primary. If
surfaces are healthy, inspect GDI/DC failures, message dispatch, blocked threads,
and changed hook destinations/module ownership. Surface loss invalidates backing
memory; it is not itself proof that a COM pointer or hook address changed.
[Microsoft documents surface loss](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-idirectdrawsurface7-islost).

## 2. Wrapper findings

| Project | Verified architecture and recovery lesson |
| --- | --- |
| DDrawCompat | Native DirectX 1–7/GDI compatibility hooks, including driver-interface hooks; no DirectDraw-to-D3D9 API conversion. See [project README](https://github.com/narzoul/DDrawCompat). |
| DxWrapper | `Dd7to9` converts DirectDraw/legacy Direct3D to D3D9; `D3d9to9Ex` is a separate option. See [configuration](https://github.com/elishacloud/dxwrapper/wiki/Configuration). |
| dgVoodoo2 | Implements legacy APIs on D3D11/12. Useful presenter/virtual-resource reference; its internal recovery implementation was not inspected. Official redistribution terms restrict general launcher/framework bundling. See [author documentation](https://www.dege.freeweb.hu/dgVoodoo2/ReadmeGeneral/). |

DDrawCompat's inspected local revision is
`2c9a07fdf9308e2b0b117886a7e363b149ee1bc7`; live upstream source was also checked.
`PrimarySurfaceImpl::Restore` repairs the real primary before the virtual one;
`RealPrimarySurface::restore` checks fullscreen availability, restores, then
releases/recreates presentation resources and reapplies gamma.
[Real-primary source](https://github.com/narzoul/DDrawCompat/blob/master/DDrawCompat/DDraw/RealPrimarySurface.cpp)
shows explicit presentation-resource recovery rather than relying on game code.

`AltTabFix=keepvidmem` prevents selected video-memory loss around activation.
Its implementation temporarily adjusts DirectDraw internal invalid flags;
reactivation restores the real primary and reconnects GDI resources.
`noactivateapp` also bypasses DirectDraw activation handling and optionally
application notification. Default is `off`. These activation workarounds do not
guarantee recovery from UAC, device removal, or actual lost pixel content, and
depend on native DirectDraw internals. Do not transplant their internal-structure
patching or suppress application messages by default.
[Configuration](https://github.com/narzoul/DDrawCompat/wiki/Configuration#alttabfix),
[activation implementation](https://github.com/narzoul/DDrawCompat/blob/master/DDrawCompat/DDraw/DirectDraw.cpp).

DxWrapper maintains logical surface state separately from D3D9 resources.
`IsLost` distinguishes eligible video-memory surfaces; `Restore` maps device
availability, restores implicit children through their root, and attempts reset
when appropriate. Emulated surface memory and `LostDeviceBackup` support
repopulation. [Surface source](https://github.com/elishacloud/dxwrapper/blob/master/ddraw/IDirectDrawSurfaceX.cpp).
Its reset path releases dependent resources, observes the creation thread, and
falls back to releasing/recreating a failed device; presentation also handles
reset readiness. [Device source](https://github.com/elishacloud/dxwrapper/blob/master/ddraw/IDirectDrawX.cpp).
The reviewed web snapshot is upstream `master`, not a locally pinned checkout;
pin a revision before any reuse. DxWrapper has multiple component licenses;
its own custom-code license is permissive with attribution/origin conditions.
[License manifest](https://github.com/elishacloud/dxwrapper/blob/master/License.txt).

D3D9Ex reduces ordinary display-transition resource loss but still requires
`PresentEx`/`CheckDeviceState`, mode-change/occlusion handling, reset, and device
recreation for removal. Its `TestCooperativeLevel` always returns success.
[Microsoft 9Ex guidance](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9ex-checkdevicestate),
[device behavior](https://learn.microsoft.com/en-us/windows/win32/direct3d9/dx9lh).
D3D11 likewise requires rebuilding device/swapchain/resources after removal/reset,
including errors from `Present` or `ResizeBuffers`.
[Microsoft recovery guidance](https://learn.microsoft.com/en-us/windows/uwp/gaming/handling-device-lost-scenarios).
A modern presenter cannot restore lost native sprite/source pixels by itself.

## 3. Best implementation approach

Three options, in order:

1. **Keep current native offscreens + GDI canvas; add recovery.** Smallest scope,
   preserves the confirmed cursor fix, avoids a private desktop primary, and
   keeps source/cursor reads in logical RGB565 pixels. Recommended first.
2. **Optional D3D11 presentation only.** If measured GDI scaling/presentation
   remains unreliable or costly, upload completed canvas frames to one texture
   and point-sample into a normal window. Reuse the same recovery ownership,
   transforms, cursor transaction, and CPU canvas; retain GDI fallback. D3D9Ex
   is a possible comparison backend, not an automatic robustness guarantee.
3. **Full general wrapper or broader surface emulation.** Greater interface/GDI
   coverage, code/dependency size, and compatibility risk. Justified only if
   native offscreen recovery or required client API coverage proves insufficient.

Recovery must belong to each DirectDraw owner, with healthy/unavailable/recovering
states. On actual loss, inspect all surfaces involved in that operation, restore
the owning chain/root plus independently created surfaces as needed, and retry
only a safe operation once. Further attempts use a bounded, low-rate schedule
while unavailable; no spin or foreground-only gate. Activation, unlock/session,
display, paint, and new graphics calls are hints, not proof of loss. Background
clients must recover without receiving foreground focus.

**Restore is not pixel restoration.** It reallocates memory without reloading
bitmaps. [Microsoft Restore contract](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-idirectdrawsurface7-restore).
Use the retained canvas to repopulate the virtual primary, invalidate stale
synchronization generations, and preserve color keys/attachments. Test whether
system-memory sprite/frame/cursor surfaces retain content before adding backups.
If they can lose content, retain authoritative CPU data or implement a proven
rebuild path; copying every surface every frame is not acceptable.

Never recreate backing resources while a caller holds `Lock` memory or `GetDC`.
Keep wrapper COM identity stable, preserve release ordering, and handle cached
DC lifetimes explicitly. Do not blindly retry `Unlock`, `ReleaseDC`, or a flip
that already mutated state. Separate a completed logical operation from a failed
display attempt: retain and retry its presentation rather than misleading the
game into repeating completed writes. Genuine failed surface operations still
need truthful HRESULTs. Recovery repaint must use the existing coalesced cursor
presentation boundary, not expose the cursor-erasure intermediate frame.

## 4. Compatibility and loading requirements

Fresh inspection: both bundled original binaries are PE32/i386, 27,911 bytes,
SHA256 `6772726967951c472f7bae53aeb7d70ba2e38605af5f814f9b130c6f04b8e0d9`.
Sole export is `DAWnd_v3`, ordinal 1, RVA `0x5020`, in zero-initialized,
non-executable `.bss`. It is data, not an initializer function. Historical
Talos/Echo invoke remote `LoadLibraryA` and do not call this export; modern Talos
was not available for inspection. See [existing integration evidence](integration.md).

Current Release `DAWindower.dll` is already standalone x86, 166,400 bytes, with
three callable exports: `DAWindowerInitialize`, `DAWindowerShutdown`, and
`DAWindowerGetStats`. Runtime imports are Windows system libraries; native
DirectDraw is loaded by absolute system path. No Talos/Zeus dependency exists.

Loading alone does not initialize it. Explicit initialization must finish before
the first graphics creation; the separate `ddraw.dll` proxy initializes on its
factory call. Renaming current DLL to `dawnd.dll` also makes its name-based
conflict guard reject itself. It is therefore **not a LoadLibrary-only drop-in**.
Keep a documented explicit-init contract for third-party launchers. A true legacy
loading adapter needs a separately verified bootstrap and startup-order contract;
do not assume an arbitrary initialization worker wins that race. Preserve
`DAWnd_v3` as a data marker only if compatibility testing establishes a consumer
needs it, never as an invented function ABI.

Only one graphics/window/input compatibility owner should be active. Dawnd's
shared-vtable writes, DDrawCompat's API/driver/GDI hooks, proxy DLL selection, and
Talos injection can conflict by load order. Current DAWindower rejects Dawnd and
unexpected IAT replacements; it bypasses an adjacent wrapper when explicitly
loading native system DirectDraw. Wrapper combinations need explicit routing and
tests, not stacking by filename. DDrawCompat itself excludes other hooks/wrappers
from supported combinations. Its [Dark Ages monitor issue](https://github.com/narzoul/DDrawCompat/issues/599)
also makes multimonitor comparison mandatory; it is not evidence of a D3D9 cause.

Keep existing mouse transform, 4:3 sizing, RGB565 colors, color keys, and cursor
pipeline. Fractional nearest-neighbor scaling retains colors but produces unequal
pixel-block widths; uniform blocks require integer scale. Keep the narrowly named
singleton shim optional in the future standalone contract so launchers can own
multiclient policy. Preserve existing behavior until that choice is validated.
Recovery must not alter networking, gameplay, automation, or host activation
semantics. Current v1-only COM coverage needs testing against other client builds.

## 5. Minimal implementation and validation plan

1. Add opt-in event/failure diagnostics with separate capture/present/recovery
   counters, loss epochs, stable surface IDs, and source/destination status.
   Avoid frame-by-frame file logging. Current Debug logging is capped at 2,048
   lines; startup chatter must not exhaust failure evidence. Release currently
   disables logging. Record client/DLL hashes, load route, Windows/GPU/topology.
2. Create failing synthetic loss cases for destination and source loss,
   temporary restore failure, multiple lost surfaces, lost pixel contents,
   outstanding locks/cached DCs, and a failed queued presenter. Include repeated
   transitions and a background client that never gains focus.
3. Implement bounded recovery without changing renderer/backend, then run all
   existing cursor/GDI/resize/input contracts through both loading routes.
4. Capture a legacy-Dawnd failure on an isolated client with boundary tracing;
   compare the same sequence with DAWindower. If restoring only the private
   real primary resumes legacy presentation while game-visible surfaces remain
   healthy, the suspected ownership hole is confirmed. If not, trace the first
   failing source/call or blocked thread instead.
5. Validate UAC accept/cancel, Windows lock/unlock, resolution/refresh/DPI changes,
   monitor disconnect/reconnect, cross-monitor/adapter moves, Alt+Tab, and
   minimize/restore. Verify advancing frames in **every** client, cursor
   backgrounds/colors, accurate clicks, and unchanged background execution.
   User-driven desktop transitions are needed; none were triggered here.
6. Benchmark actual retail clients at 1, 10, and 20 instances with matched scenes,
   window sizes, focus, GPU, and transition sequence. Include all-visible and
   minimized/background sets, warmup, sustained rendering, and repeated recovery.
   Measure aggregate CPU/GPU, private bytes, graphics memory, GDI handles,
   frame-time percentiles, copy/upload counts/bytes, and return-to-live-frame time
   per client. Check for monotonic leaks and recovery storms. Set numerical
   acceptance budgets from the baseline before selecting another presenter.

One 640×480 RGB565 frame is 614,400 bytes. At 30 updates/s, one full transfer
per frame across 20 clients is 368.64 MB/s of payload, excluding read/write bus
traffic, desktop scaling, and additional copies. This is arithmetic, not a
measured workload. Reuse storage, coalesce completed frame/cursor updates, avoid
unchanged/hidden-window presentation, and keep locks per process/owner. Do not
scan every surface or create extra full-frame backups in the healthy path.

Verification performed now: existing Debug and Release each passed all seven
CTest cases (14/14 total, 6.37 s and 6.09 s). Their `Restore()` check operates on
an ordinary system-memory surface; it never induces loss. No real UAC/unlock
recovery or sustained 10–20-client performance result is established. No game
or Excalibur process was launched, stopped, restarted, or injected during this
investigation. Live module enumeration returned no usable module data, so it
cannot establish the active clients' hook stacks.

Read-only evidence: fresh Dawnd disassembly (`../research/dawnd-recovery-disasm.txt`),
fresh installed-client primary path (`../research/recovery-reference/retail-primary.txt`)
and unlock path (`../research/recovery-reference/retail-unlock.txt`).
Installed EXE SHA256 remains
`b168856a22a5315d2eb0fc942ca1cb6ef38e81035a445bc2cc9710d3d1a5715f`.
These fingerprints identify inspected artifacts; they do not authenticate their
distribution provenance.
