# DAWindower X

Approved layout, October 14, 2026:

```text
DAWindower X · CharacterName       [+] [1:1]   [−] [□] [×]
```

X is an optional edition built from the same rendering and recovery sources as
standard DAWindower. Its runtime package contains only `DAWindowerX.dll`.
Standard builds retain their existing window frame and package name. Neither
edition needs a launcher-specific initializer, a proxy DLL or configuration.

## Window controls

Draw a flat, near-black non-client frame with light text, compact vector icons,
subtle hover states and a red close hover. Keep the game client rectangle separate
from the title bar so rendering, software-cursor composition and mouse transforms
continue to use the existing geometry. Retain Windows resizing, moving, system
menu, minimize, maximize, restore and close commands.

The two extra actions also appear in the Windows system menu for keyboard access.
Hover text describes the compact buttons. A press activates only when released
over the same button; cancellation, deactivation and capture loss cancel it.
Windows high-contrast mode uses system colors. Changes in DPI/theme/activation
refresh the frame without changing process-wide DPI awareness.

The native-size action restores a maximized/minimized window before setting its
actual game client rectangle to exactly 640×480 pixels. Frame dimensions are
measured separately. It does not change another client's size or position.

## Character name

Use bounded, read-only name sampling for explicitly identified client builds.
Start with the installed 7.41 client, whose name-buffer address also appears in
Echo's client profiles. Verify executable identity before reading that address;
do not guess offsets for unknown clients. Validate length, termination and name
characters. Poll at most once per second, outside loader lock, and update window
text only when changed. Unknown builds or unavailable names retain the title
`Dark Ages — DAWindower X`. No networking, packet interception, credentials,
automatic login or game-memory writes are part of this feature.

Name sampling and login/logout behavior require actual-client acceptance. Static
address evidence alone does not establish successful-login timing.

## New client

Start the current Dark Ages executable directly, with its game folder as working
directory and without inheriting launcher command-line credentials or handles.
Create it suspended, load this exact X DLL, verify loading succeeded, then resume
its main thread. A short-lived worker owns the wait so the existing client's UI
thread does not block. Prevent duplicate in-flight clicks; allow another launch
after completion. A failed, newly created suspended child is cleaned up rather
than resumed without the DLL. Never affect any pre-existing process.

Use Unicode paths for the internal launch operation. Ordinary external
`LoadLibraryA`-only injection remains part of both editions' loading contract.
Report launch failures visibly with their Windows error and stage.

## Implementation and verification

1. Add a failing LoadLibrary-only window test against the standard DLL, proving
   the X frame/actions are absent before implementing them.
2. Isolate X frame, name-reader and client-launch code under `src/x/`; enable it
   only through an explicit X build option/preset.
3. Test hit regions, cancelled clicks, exact native size, frame/client cursor
   ownership, independent windows, high-contrast colors and title fallback.
4. Test a real suspended child/injection transaction using a synthetic executable
   fixture, including failure cleanup and paths containing spaces.
5. Run the existing rendering/recovery suite for both editions, Debug and Release,
   and verify each installation contains exactly its one native x86 DLL.
6. Launch isolated actual Dark Ages clients with X. Ask the user to verify frame
   appearance, login-name timing, resizing, extra-client launch and recovery.
   Record precisely what was observed; synthetic tests do not certify gameplay.

No release or broad compatibility claim precedes actual-client acceptance.
