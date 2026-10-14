# Testing and acceptance

Different checks answer different questions. A successful compile is not proof
of LoadLibrary-only retail startup, and a simulated loss is not a physical monitor test.

## Automated test map

The default build defines 13 contracts. `unit` requires no desktop; `desktop`
uses native Windows graphics/window behavior. Desktop cases are serialized even
when CTest is invoked with parallel execution.

| Contract | Source | What it protects |
| --- | --- | --- |
| `geometry` | [geometry_tests.cpp](../tests/geometry_tests.cpp) | Full-client viewport, independent scaling and input boundaries. |
| `surface_recovery_contract` | [recovery_tests.cpp](../tests/recovery_tests.cpp) | Lost source/destination, retained pixels, keys, COM/attachment identity, leases and bounded repair/recreation. |
| `presentation_recovery_contract` | [presentation_recovery_tests.cpp](../tests/presentation_recovery_tests.cpp) | Failed GDI/queue operations; timer-only retained-frame recovery and minimize/restore. |
| `directdraw_contract` | [ddraw_harness.cpp](../tools/ddraw_harness.cpp) | Wrapped factory, caps, drawing, lifetimes and shutdown. |
| `loadlibrary_only_contract` | Same harness | Factory interception with no initializer call. |
| `dormant_unload_contract` | Same harness | Pre-initialization gate removal and native factory after unload. |
| `renamed_loadlibrary_contract` | Same harness | DLL renamed to dawnd.dll can load itself. |
| `retail_render_contract` | [retail_render_tests.cpp](../tests/retail_render_tests.cpp) | Synthetic retail-style DirectDraw calls, RGB565/color keys, resize and input. |
| `retail_render_loadlibrary_contract` | Same render test | The same path through LoadLibrary-only loading. |
| `gdi_render_contract` | [gdi_render_tests.cpp](../tests/gdi_render_tests.cpp) | Native cached DCs, actual window pixels, scaling and coalesced cursor composition. |
| `gdi_render_loadlibrary_contract` | Same GDI test | The same GDI path through LoadLibrary-only loading. |
| `smooth_resize_contract` | [resize_tests.cpp](../tests/resize_tests.cpp) | One-pixel size steps, portrait/wide windows, host messages and mouse mapping. |
| `resize_cursor_contract` | [resize_cursor_tests.cpp](../tests/resize_cursor_tests.cpp) | Eight arrow shapes, visibility count balance, repeated hover, drag and cleanup. |

The test called `retail_render_contract` is synthetic. Only the separate helper
below launches an actual Dark Ages client.

Run the full suite with `ctest --preset release` and `ctest --preset debug` on an
interactive Windows desktop. Fault injection makes otherwise difficult surface
and GDI failures reproducible. Synthetic tests use no game assets or credentials.

## CI coverage

[The Windows workflow](../.github/workflows/ci.yml) builds Debug and Release,
checks C++ formatting and document links, runs the unit-labeled tests, and checks
that installation produces a single system-dependent x86 DLL with the expected
optional exports. The Release candidate is retained as a workflow artifact.

Hosted runners are not used to certify interactive desktop transitions or retail
rendering. All native test binaries are built, but the `desktop` group is run
locally on an interactive desktop. A CI artifact is a build candidate, not an
automatically published or retail-accepted release.

On 2026-10-14, the [initial public workflow run](https://github.com/BuildWithRaymond/DAWindower/actions/runs/38647995540)
passed all three jobs: formatting/documentation, x86 Release and x86 Debug.
This verifies hosted builds, unit tests and package checks. The full 13-contract
desktop suite was verified separately on an interactive local desktop; it was
not executed by the hosted workflow.

## Actual-client loading check

Use a separately prepared Dark Ages copy with its normal assets and without
adjacent legacy/proxy compatibility DLLs. Do not copy copyrighted game assets into
this source repository. Use an absolute path to a separately installed candidate.

```powershell
./build/windows-x86/Release/retail_loadlibrary_test.exe --launch `
  'C:\Tests\DarkAges\Darkages.exe' `
  'C:\Tests\DAWindower\DAWindower.dll'
```

The [helper source](../tools/retail_loadlibrary_test.cpp) restricts launches to
`Darkages.exe`, creates it suspended, injects by remote `LoadLibraryA`, checks the
module result, resumes startup and verifies native resizing. It calls no
initialization export. It also checks every intermediate pixel dimension in a
short width/height sequence and records graphics module paths.

**The launched client stays open.** The helper creates local bitmap/log evidence
when invoked from a test folder; none is a runtime distribution dependency.
It never launches or restarts Excalibur. Do not use existing production clients
as injection targets.

WM_PRINTCLIENT captures were black on the inspected retail client even when the
user saw working graphics. Those hashes do not prove frame progression or a freeze.
Confirm actual-client visuals and input manually and record that observation separately.

## Manual acceptance matrix

For each runtime candidate, record DLL SHA256, client SHA256, Windows build,
GPU/monitor arrangement, DPI and the exact injection sequence. Verify two isolated
clients together; each row must cover the foreground and background client.

| Scenario | Observe and record |
| --- | --- |
| Initial LoadLibrary-only startup | No initializer call; immediate resize; normal graphics and game behavior. |
| Edge/corner resizing | Smooth arbitrary sizes, full image, correct mouse targets and visible native arrows. |
| Game cursor motion | No erased-background flash, black square or extra OS cursor inside the game. |
| Alt+Tab and minimize/restore | Retained frame returns, then normal animation/input continue. |
| UAC accept and cancel | Both clients resume drawing after returning from secure desktop. |
| Windows lock/unlock | Foreground and background clients resume without manual graphics reset. |
| Resolution and refresh change | Correct new window presentation/input, with continued game drawing. |
| Monitor disconnect/reconnect | Recovery after physical topology change; record which monitor/adapter changed. |
| Sustained multiple clients | CPU, private memory, GDI handles and recovery time do not grow without bound. |

Mark a scenario **passed**, **failed**, or **not tested** and describe the event.
Do not turn a general “works” response into an event-by-event certification.
Hardware operations are performed by the tester; they are not triggered by CTest.

## Existing acceptance records

- [0.2.2 resize/cursor record](smooth-resize-validation.md): exact package hash,
  actual loading, one-pixel resizing and user confirmation.
- [0.2.0 loading/recovery record](standalone-validation.md): multiclient startup,
  fault injection, user interruption observations and remaining limits.
- [Historical 0.1.x validation](validation.md): prototype paths and earlier cursor work.

Raw runtime logs, captures, generated packages and research checkouts are local
development artifacts, excluded from the repository. Their paths in historical
records identify where evidence was collected; source readers should use the
recorded summaries and reproduce the relevant checks.
