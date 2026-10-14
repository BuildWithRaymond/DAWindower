# Standalone DLL acceptance — 2026-10-14

This is the version 0.2.0 loading/recovery record. Later free-resize and frame
cursor changes are recorded in [resize validation](smooth-resize-validation.md).
Raw files named below are ignored local development artifacts; see
[testing](testing.md) for reproduction steps.

## Deliverable

`dist/standalone/DAWindower.dll`, version 0.2.0, **177,152 bytes (173 KiB)**,
PE32/i386, static CRT. SHA256:

`49a2fd66fd53ee56ae1c8bbffe7b8d479a7af58f28741a8eed8204c1d42d22a9`

CMake installation produced one file. PE imports are exclusively `KERNEL32.dll`,
`USER32.dll`, `GDI32.dll`, `COMCTL32.dll` and `WTSAPI32.dll`. Native DirectDraw is
loaded from the Windows system directory. No external runtime, configuration,
launcher-specific library, proxy or supporting executable is distributed.
Source development helpers are not runtime dependencies.

## Actual client loading

The isolated retail EXE SHA256 is
`b168856a22a5315d2eb0fc942ca1cb6ef38e81035a445bc2cc9710d3d1a5715f`.
It was copied with normal game assets, excluding adjacent ddraw/dawnd and
DAWindower configuration/support files. No existing client was injected or stopped.
No Excalibur operation occurred.

The development helper used the same historical launcher sequence:
`CreateProcess(CREATE_SUSPENDED)` → remote `LoadLibraryA` → wait and verify
nonzero module handle → `ResumeThread`. **Zero initialization-export calls.**
No Talos, Zeus or existing launcher was involved.

| Process | DLL tested | Result |
| --- | --- | --- |
| 31484 | Recovery candidate, SHA256 `84604e3978a92512ddd133ddd76651e7dcfd1c1539b269a798f7e8dbd2f17fe4` | LoadLibraryA-only success; window immediately resizable to 960×720 |
| 26268 | Same recovery candidate | Second simultaneous actual client, same loading/resizing result |
| 12096 | Exact final distributable, SHA256 above | LoadLibraryA-only success; final package path verified in loaded modules; 960×720 client area |

All enumerated graphics modules were the injected DAWindower DLL and Windows
system DirectDraw, with no adjacent proxy or original dawnd. Three isolated
clients can coexist. The user confirmed the initial two clients “both opened
fine.” After the requested interruption checks, the user reported “works on ours,
not on dawnd.dll.” This is user-observed recovery evidence on the candidate;
it is not an instrumented, event-by-event certification of the final build.

The helper's WM_PRINTCLIENT retail captures were black even though the user saw
working clients. Their hashes are **not** evidence of frozen or advancing retail
frames. Automated retail frame-time/recovery-time measurement is therefore not
established. Native computer-use capture was unavailable because its pipe was
not running. Visual retail confirmation came from the user.

Logs: first load (`runtime-tests/retail-loadlibrary-1.log`),
second load (`runtime-tests/retail-loadlibrary-2.log`),
final distributable load (`runtime-tests/retail-loadlibrary-final.log`).
Runtime logs are local development artifacts and ignored by Git.

## Automated verification

Release: **11/11 passed**. Debug: **11/11 passed**. These exercise real native
DirectDraw offscreens and GDI output alongside deliberately injected failures.

| Contract | Coverage |
| --- | --- |
| geometry | Aspect fit and mouse-coordinate boundaries |
| surface recovery | Source/destination loss; distinct front/back retained pixels; color keys; attachment/public COM identity; active lock deferral; ended lost Unlock lease; temporary restoration failure/backoff; native backing recreation and repopulation |
| presentation recovery | Three transient StretchDIBits failures; timer recovery without new game writes/focus; failed PostMessage; retained visible pixels; minimize/restore; timer destruction |
| DirectDraw | Factory interception, caps, Lock/GetDC, Flip, lifetimes and controlled shutdown |
| LoadLibrary only | Factory interception with no initialization call |
| dormant unload | Pre-initialization shutdown removes gates; unload leaves a native factory |
| renamed loading | Self-renamed dawnd.dll no longer rejects itself |
| retail-style rendering, explicit and load-only | Windowed coordinate translation, RGB565/color keys, resizing and mouse mapping |
| GDI rendering, explicit and load-only | Actual synthetic window pixels, native cached surface DCs, nearest-neighbor scale and coalesced cursor composition |

A further **20/20 simultaneous synthetic clients** passed using LoadLibraryA only,
in 6.866 seconds. This checks isolation and startup under concurrency; it is not
a sustained 20-retail-client CPU/GPU benchmark.

Test-first regressions failed before the relevant fixes: LoadLibrary-only startup,
automatic source/destination restoration and distinct flip-chain pixels. The
presentation test initially failed inside the restricted sandbox because
OpenInputDesktop was denied; it passed with normal desktop access. Build output
completed without compiler warnings after correcting the test's WORD literal.

Logs: Release (`runtime-tests/standalone-release-tests.log`),
Debug (`runtime-tests/standalone-debug-tests.log`),
20-client synthetic result (`runtime-tests/standalone-multiclient.json`),
PE inspection (`runtime-tests/standalone-release-pe.json`).

## Acceptance status and remaining limits

| Requirement | Status |
| --- | --- |
| One distributable DLL | Verified directory contents and PE dependencies |
| LoadLibraryA alone, no initializer | Verified on three actual clients, including final DLL |
| Immediate resize | Verified actual 960×720 client area and resizable style |
| Multiple clients | Three actual clients coexist; 20 synthetic clients pass |
| Cursor fix and accurate scaling/input | Automated regressions pass; original 0.1.3 user confirmation retained; no cursor pipeline redesign |
| Interruption recovery | User reports candidate working while dawnd fails; simulated source/destination/presentation failure cases pass |
| Independence from Talos/Zeus | Verified standalone native injection helper and system-only imports |
| No additional DLL/configuration/executable | Verified installation contains only DAWindower.dll |

Exact UAC accept/cancel, lock/unlock, resolution, refresh, physical monitor
disconnect/reconnect and cross-adapter transitions were not individually logged
by the user. That hardware/event matrix and sustained retail performance testing
remain open. Final code changes after the manual candidate test preserve the GDI
cursor/presentation path and strengthen flip/backing/lease handling; final loading
and all synthetic contracts were rechecked on the distributable.

**Specific recovery limitation:** native Lock memory and cached GetDC handles
cannot be replaced while leased to the client. Recovery defers until the lease
ends. If native loss destroys pixels written inside an unfinished lease, the DLL
has no authoritative copy of those new writes. An indefinitely retained invalid
native DC needs client cooperation; it cannot be repaired by silently handing
the caller a different HDC. Completed snapshots and the logical GDI canvas are
preserved. Windows timers also require a functioning game message pump.

Initialization uses the inspected EXE's named v1 DirectDrawCreate import. Late
injection, dynamic factory resolution, newer COM interfaces and factory calls
inside another DLL's DllMain are outside the verified contract. No universal
renderer/wrapper stacking compatibility is claimed.
