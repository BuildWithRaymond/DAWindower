# Window title ownership

Both editions display the existing title supplied by the game or launcher. Remove
the character-name memory reader, executable offsets, decoder tests and game-memory
name polling from the source and build. Do not synthesize or replace the native title.

Read `GetWindowTextW` when attaching the caption. On `WM_SETTEXT`, call
`DefSubclassProc` first, then read the accepted text through `GetWindowTextW` and
redraw. This handles ANSI/Unicode conversion, title rejection or rewriting by the
host, and empty titles. Keep the frame alive across reentrant host callbacks and
stop processing if the host destroys its window during either call.

External `SetWindowTextA/W` can change the stored window title without delivering
`WM_SETTEXT` to its owner. Also refresh through `GetWindowTextW` before painting
and once per second, repainting only when the accepted text changes. This check
uses only Windows window text and never executable addresses or game-state buffers.

Retain X's worker-completion handling separately from title updates. Its timer is
created before starting a worker and removed on completion, thread-start failure
or window destruction. It is separate from the window-title refresh timer.

Observe the actual `DrawTextW` call in both DLL editions while retaining native
drawing. Test ANSI and Unicode host classes, both setter encodings, non-ASCII
Unicode text, initial title preservation, independent windows, no delayed title
overwrites, rejected/rewritten/empty text, reentrant destruction, and setters in a
separate process that must trigger an automatic repaint. Existing
window controls, rendering, recovery and X launch contracts remain required.

## Verification

The initial ANSI/Unicode regressions failed because caption attachment replaced
the supplied title. A separate-process setter then demonstrated zero incoming
`WM_SETTEXT` messages and failed against the message-only implementation. The
final implementation automatically repaints those external title changes.

All 22 standard and 23 X contracts pass in both Release and Debug: 90 final checks
on the interactive desktop. Formatting, documentation links and whitespace checks
also pass. Logs are retained locally as `runtime-tests/window-title-*-tests.log`.

Release packages are `dist/0.2.3-host-titles/DAWindower.dll` and
`dist/0.2.3-x-host-titles/DAWindowerX.dll`. Both pass one-file, x86, system-import and
export-ABI checks. All four built DLLs import `GetWindowTextW` and no longer import
`ReadProcessMemory` or `SetWindowTextW`. These are synthetic-window results;
actual-client login-title timing was not retested.
