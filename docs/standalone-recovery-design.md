# Standalone initialization and rendering recovery — 2026-10-14

This continues the October 14 assessment under the user's single-DLL contract.
The distributable is `DAWindower.dll`, native x86, with a static CRT and Windows
system imports. No configuration, proxy, launcher-specific API or worker is needed.

## Initialization boundary

The original dawnd DLL (SHA256
`6772726967951c472f7bae53aeb7d70ba2e38605af5f814f9b130c6f04b8e0d9`)
patches executable imports during process attach. Its sole `DAWnd_v3` export
is data, not a callable initializer. Historical launchers create the client
suspended, inject through a remote `LoadLibraryA` thread, wait, then resume.

An unsynchronized initialization worker cannot guarantee first-object interception.
Doing complete renderer setup under loader lock violates Windows initialization
guidance. Instead, process attach installs two fixed, allocation-free import gates:
`DirectDrawCreate` and the earlier `CreateMutexA` singleton adapter. It uses bounded
reads of the loaded PE image, `VirtualProtect` and atomic pointer exchange. It
does not load DLLs, allocate containers, call User32/GDI/COM, wait, or start threads.
The graphics gate synchronously completes full initialization on the EXE's first
factory call. The factory gate remains in place until all other hooks are ready;
its final replacement is atomic. Initialization failures return errors instead
of creating an unwrapped object. Existing explicit exports remain optional.

Injection must finish before the EXE's singleton/graphics calls. As with original
dawnd, arbitrary late injection cannot retrofit existing COM objects. The gate
covers the inspected EXE's named v1 factory import, not arbitrary plugins using
`GetProcAddress`, delayed imports, COM activation or newer factory interfaces.

Microsoft guidance: [DLL best practices](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices).

## Recovery ownership

Native RGB565 system-memory offscreens remain the renderer. The ordinary virtual
primary uses the existing retained GDI canvas; flip chains retain both buffers.
Other surfaces retain CPU pixels at
creation, successful blits, completed writable locks and released DC leases.
Dirty blit rectangles limit readback; writable locks copy from their existing
pointer. This adds memory and copy cost, requiring retail performance measurement.
No healthy-path registry scan or per-frame recovery worker is added.

Actual loss triggers a per-surface retry schedule (100 ms, 200 ms, 400 ms, 800 ms,
1600 ms, then at most one attempt every 2 seconds). Safe operations retry once;
Unlock, ReleaseDC and completed Flip are never blindly repeated. Restore uploads
retained pixels and reapplies keys/clipper. Repeated restoration failures permit
native backing replacement, preserving the public wrapper and attachment identities.
Outstanding caller locks/DCs defer restoration/recreation. A weak window binding
protects owner lifetime and uses a nonblocking attempt to acquire its mutex.
An ended lost Lock lease returns NOTLOCKED on a repeated Unlock, breaking the
retail client's unbounded Unlock/Restore loop without retrying an ended operation.

Per-window timers service recovery, with a cap of four candidate surfaces per
tick. Failed GDI presentation and queueing retain pixels and retry at the existing
coalesced cursor boundary. Display/device/session/power/restore messages are hints;
a one-second input-desktop check detects secure-desktop transitions in background
clients without relying on focus. Windows timers require the game's message pump
to continue running. Window destruction removes timers and session registration.

Microsoft contract: [Restore does not reload bitmaps](https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-idirectdrawsurface7-restore).

## Verification gates

1. Fail then pass LoadLibrary-only and synthetic source/destination loss cases.
2. Exercise queued-present failure without subsequent game writes or focus.
3. Preserve cursor composition, resize, input, COM and teardown regressions.
4. Launch an isolated actual client through remote LoadLibraryA alone, before resume.
5. Verify several retail clients and exact module paths; never touch existing clients.
6. Record manual UAC/session/topology tests separately from simulated failures.
7. Inspect final PE imports and produce a directory containing one distributable DLL.
