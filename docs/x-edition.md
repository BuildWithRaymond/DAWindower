# DAWindower X

X adds window controls to DAWindower's existing renderer. It is an optional
edition, distributed as **one `DAWindowerX.dll`**, with Windows system dependencies
only. Use one edition per client. Both editions share the gradient caption renderer,
`Dark Ages · CharacterName` title and loading contract. Standard `DAWindower.dll`
has three ordinary window controls; X adds two client actions.

[![Actual DAWindower X client with a black gradient title bar, New client and native-size controls](images/x-native.png)](images/x-native.png)

Actual 7.41 main menu before login, with a **640×480** game area.
[Original captures and comparison](images/README.md).

## Controls

| Control | Action |
| --- | --- |
| Game icon | Open the native Windows system menu. The icon comes from the game executable; launcher-supplied icons take precedence. |
| + | Start another `Darkages.exe` from this client's game folder and inject the same X DLL before resuming startup. |
| 1:1 | Restore the window if maximized, then set the actual game client rectangle to 640×480. |
| − / □ / × | Windows minimize, maximize/restore and close commands. |
| Empty title bar | Native window move, double-click maximize and system menu behavior. |

The caption uses a restrained charcoal-to-black gradient, light semibold text,
the game icon, rounded hover states and descriptive dark tooltips on every button. There is no
visible side/bottom border. Invisible edge/corner hit regions keep resizing
available. The caption follows Windows high-contrast system colors. The two
extra actions also appear in the
Windows system menu (Alt+Space). Releasing outside a pressed button cancels it.
Title-bar clicks are consumed before the game's input-coordinate transform.

Both editions keep ordinary window resizing at native 4:3, with nearest-pixel
rounding. A launcher's bare-popup fullscreen selection owns monitor bounds and
suppresses X's caption/resize grips. Fullscreen fills the monitor, so non-4:3
monitors use independent horizontal/vertical scaling.

The New client action does not copy launcher command lines, credentials or
automatic-login state. It starts the same executable directly with its game
folder as working directory. One launch can be in flight per window. Waiting
for injection happens on a short-lived worker, never the game/UI thread. A failed
injection cleans up only its newly created suspended child. A red + and hover
text report the failed stage and Windows error; click again to retry.

Internal launch uses Unicode `CreateProcessW`/`LoadLibraryW` paths. Existing
launchers can still inject X with ordinary early **`LoadLibraryA` alone**, exactly
as described in the [integration guide](integration.md), substituting
`DAWindowerX.dll`. No additional exported call is required. The internal launch
transaction clones the running x86 executable and uses the same-bitness Windows
system-loader route tested by the development fixtures.

## Character title and client support

Both editions' character title uses bounded, read-only sampling once per second. It never
hooks networking or writes client state. Unknown clients, unavailable buffers
and invalid names display `Dark Ages`.

Initial layout support is limited to the inspected installed 7.41 executable:

```text
SHA256: b168856a22a5315d2eb0fc942ca1cb6ef38e81035a445bc2cc9710d3d1a5715f
PE timestamp: 0x56fb6f2d
SizeOfImage:  0x506000
Entry RVA:    0x22b2d9
Name RVA:     0x33d910 (13-byte buffer, at most 12 ASCII letters plus terminator)
Version RVA:  0x3505b8 (16-bit value 741)
```

The runtime checks layout identifiers, a name-address instruction reference and
the client version before accepting a name. These are compatibility guards,
not cryptographic executable attestation. Echo's 7.41 profile independently
references the same name buffer; that evidence does not establish successful
login/logout timing. Actual-client title acceptance must be recorded separately.
Do not add an offset for another client without identifying and testing it.

## Build and package

From the repository root in Developer PowerShell:

```powershell
cmake --preset windows-x86-x
cmake --build --preset x-release
ctest --preset x-release
cmake --install build/windows-x86-x --config Release
python tools/check_package.py dist/local-x --edition x
```

The package contains only `dist/local-x/DAWindowerX.dll`. Debug uses `x-debug`.
No-desktop checks use `x-unit-release` / `x-unit-debug`; they do not exercise
rendering, title-bar interaction or child injection. Both editions use the Windows
`DWMAPI.dll`, `UXTHEME.dll` and `MSIMG32.dll` components for their shared caption;
none is distributed.

## Code and validation

- [Shared caption](../src/window/caption.cpp): non-client painting, hit regions, UI lifetime,
  tooltip/menu actions and asynchronous launch ownership.
- [Shared name reader](../src/window/name.cpp): client layout guard, bounded copies and name validation.
- [Launch transaction](../src/x/launch.cpp): suspended child, injection, resume and failure cleanup.
- [Approved design](designs/2026-10-14-dawindower-x.md): scope and implementation sequence.

The existing rendering, recovery, cursor and resizing contracts run for both
editions, including caption and name-boundary contracts. X adds extra frame controls
and synthetic child-launch contracts. A
synthetic fixture proves injection ordering and UI wiring, not retail gameplay
or successful-login timing. The user confirmed login-name display, native reset,
New client and normal graphics/input on the initial X prototype. Later actual
checks confirmed retained title bars, smooth resizing, native-size startup and
all five corrected tooltips. Exact build scopes and remaining transition tests:
[current validation record](x-borderless-validation.md).
