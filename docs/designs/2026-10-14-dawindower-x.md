# DAWindower X

Historical design. Standard now shares X's caption renderer and caption-only
frame layout; see the [current architecture](../architecture.md). Current title
behavior is defined in [window title ownership](2026-10-15-window-title-ownership.md);
neither edition retains the character-memory reader described below.

Approved layout, October 14, 2026:

```text
Dark Ages · CharacterName          [+] [1:1]   [−] [□] [×]
```

X is an optional edition built from the same rendering and recovery sources as
standard DAWindower. Its runtime package contains only `DAWindowerX.dll`.
Standard builds retain their native Windows title bar and package name. Neither
edition needs a launcher-specific initializer, a proxy DLL or configuration.

## Window controls

Draw a charcoal-to-black gradient caption with semibold light text, compact vector icons,
rounded hover states, a red close hover and descriptive dark tooltips. There is
no visible side/bottom border. Keep the game client rectangle separate
from the title bar so rendering, software-cursor composition and mouse transforms
continue to use the existing geometry. Retain Windows resizing, moving, system
menu, minimize, maximize, restore and close commands.

The two extra actions also appear in the Windows system menu for keyboard access.
Hover text describes the compact buttons. A press activates only when released
over the same button; cancellation, deactivation and capture loss cancel it.
The caption follows Windows high-contrast system colors. Changes in DPI/theme/activation
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
`Dark Ages`. No networking, packet interception, credentials,
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

## Shared borderless sizing and launcher ownership

Subsequent user requests apply to both editions: preserve native 4:3 windowed
aspect while accepting every nearest-pixel width/height, without the old 4×3
size grid. Both retain title bars: native Windows in standard, custom in X.
X reserves no side/bottom frame insets. Standard retains invisible native frame
margins for correct Windows caption icon/title spacing. Corner motion follows a continuous
closest-size projection onto 4:3; it never switches between competing axis
drivers based on a size that the previous callback already changed.
Invisible edge hit regions retain native resizing and visible resize arrows.
Windowed maximize fits a 4:3 client inside the monitor work area. Standard keeps
the Windows system menu for Alt+Space move/size/minimize/close; no game input
gesture is repurposed for moving.

A launcher's explicit bare `WS_POPUP` fullscreen request owns its window style
and monitor bounds. Recognize a maximized or monitor-sized bare popup already
present at attachment; never restore or resize it. After attachment, removing
the resize capability selects external fullscreen, suspending 4:3 constraints
and X's caption. This mode fills the monitor, so non-4:3 monitors use independent
axis scaling. Windowed mode remains 4:3. No polling worker fights window styles.

Install scoped EXE-import adapters outside loader lock alongside BitBlt. For
tracked attached windows, client MoveWindow calls cannot reset geometry and
client SetWindowLongA style/extended-style writes preserve the selected style.
Other indices, especially the inspected client's GWL_USERDATA writes, pass
through. ShowWindow keeps hide/minimize/restore-from-minimize and ordinary show
behavior, while normal/maximize commands cannot undo an existing selected mode.
Untracked windows and direct USER32 calls from launchers/Windows/X retain native
semantics. Aspect enforcement stays in the shared HWND sizing path because user
dragging and launcher operations need not pass through the client's IAT.

Test both launcher-before-attach and launcher-after-attach ordering, plus late
client resets, unrelated windows, fullscreen client insets and keyboard/window
lifetime behavior. Record actual launcher runs separately before claiming
launcher-specific runtime acceptance. No release or broad compatibility claim
precedes actual-client acceptance.
