# Launcher integration: one DLL, LoadLibraryA only

Distribute **DAWindower.dll**, native x86. Replace the old dawnd resource or DLL
path in your launcher with this file. There is no Talos/Zeus dependency, proxy,
external configuration, companion DLL or supporting executable.

## Standard launch sequence

1. Create the 32-bit Dark Ages client suspended, as existing dawnd integrations do.
2. Write the absolute, null-terminated DLL path into the target process.
3. Start the usual remote thread at `LoadLibraryA` with that path as its argument.
4. Wait for completion and verify the returned module handle is nonzero.
5. Free the remote path buffer, close the injection thread handle, then resume
   the client's main thread.

**Do not call `DAWindowerInitialize`.** The first executable `DirectDrawCreate`
call completes initialization itself before creating any wrapped graphics object.
If initialization fails, that graphics call returns an error. The existing
`CreateMutexA` singleton check is adapted earlier by a separate Kernel32-only gate.

As with dawnd, finish injection before game startup. Late injection cannot replace
objects already created. Verify remote API addresses for the target architecture;
do not blindly reuse arbitrary local addresses across different-bitness processes.
The acceptance helper mirrors the historical x86 launchers' actual sequence and
records module paths and the injection thread's result. It is development code,
not a runtime requirement or distributable component.

A launcher with a fixed `dawnd.dll` path can rename this DLL to that name; the
conflict guard permits its own module. Named-load regression covers this case.
Preferred integration uses the delivered `DAWindower.dll` name. Do not also inject
original dawnd or stack other renderer/window/input wrappers. Remove optional
historical proxy routing from your integration before selecting this DLL.

## Why loading alone works

Original dawnd patches EXE imports inside process attach. Its only `DAWnd_v3`
export is zero-initialized data, not an initializer function. Historical
[Talos](https://github.com/adambraly/Talos/blob/3992da2d22c3bff3ae09bff06d52df3ecc76f177/Forms/MainForm.cs)
and [Echo](https://github.com/Sichii/Echo/blob/36d0eeb0709b2e1bd2ba6fa494294fc5b88b72ff/External.cs)
inject with remote LoadLibraryA; no call to that marker is needed. DAWindower
reproduces this observable early-load contract without inventing a marker ABI.

DllMain installs only a fixed, allocation-free IAT gate using bounded PE reads
and atomic Kernel32 pointer swaps. Full DLL loading, COM/GDI/window setup,
application locking and logging happen on the first normal graphics call outside
loader lock. The factory gate stays installed until the other imports are ready;
its final replacement is atomic. A racing initialization worker is unnecessary.
See [DLL initialization guidance](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices).

The contract targets the inspected executable's named v1 `DirectDrawCreate`
import. Dynamic factory resolution, COM activation, calls originating inside
another DLL's DllMain and other DirectDraw interface generations are not validated.
An injector must not race running startup or reuse the DLL as a late renderer swap.

## Borderless windows and launcher fullscreen

Both editions use the shared gradient caption and reserve no side or bottom
frame margins. Standard has three ordinary window controls; X adds New client
and native-size reset. Both keep invisible resize grips and native 4:3 windowed
sizing. The normal Windows system menu remains available with Alt+Space.

For launcher-controlled borderless fullscreen, a bare `WS_POPUP` without
`WS_THICKFRAME` selects external geometry ownership. A maximized or monitor-sized
bare popup already present at attachment is preserved. Applying the bare style
after attachment also suspends windowed aspect constraints and the shared caption.
Native maximize then uses monitor bounds; an explicit monitor rectangle is
respected. The renderer fills that rectangle and maps mouse coordinates with it.
This desktop-window mode does not expose exclusive DirectDraw fullscreen.

Only Dark Ages' EXE import slots for MoveWindow, ShowWindow and SetWindowLongA are
adapted. Late client geometry/style resets cannot undo the selected placement.
GWL_USERDATA, untracked windows, teardown hide, minimize and restore-from-minimize
keep their native paths. Direct USER32 operations from an external launcher or
its injected helper are outside the EXE adapters. No launcher initialization call,
configuration file or special launcher identity is introduced.

This contract is tested with synthetic launchers in both completed orderings.
A bare popup that has not yet been maximized or assigned monitor bounds before
attachment is indistinguishable from the game's original window. That partially
completed pre-attach transition is not a universal intent signal. Launchers can
apply their completed fullscreen style/bounds after the resizable window appears.
Acceptance of each launcher's workflow must be recorded separately; these
tests do not certify every launcher or injection pattern.

## Game window icon

Both editions restore the first icon embedded in the game window class's module,
matching dawnd's `ExtractIconA(..., 0)` selection. Restoration happens during
normal window attachment, outside loader lock. Existing per-window and class
icons are preserved; later launcher `WM_SETICON` changes retain native behavior.

Both editions draw the icon in their shared caption; the icon opens the native
system menu. Both expose small and large window icons
for Windows and launchers. No icon is embedded in or distributed alongside the
DLL: the existing game executable supplies its artwork. A missing resource is
nonfatal.

The implementation loads a shared resource from the already-loaded class module.
Windows owns that handle, so repeated windows do not allocate private icons or
add a Shell32 dependency to the DLL. See the
[icon helper](../src/window/icon.cpp), Microsoft's
[LoadIcon ownership guidance](https://learn.microsoft.com/en-us/windows/win32/menurc/using-icons),
and the [native ExtractIconA selection contract](https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-extracticona).

## Optional exports

The stable x86 exports use `DWORD WINAPI(void*)`:

| Export | Contract |
| --- | --- |
| DAWindowerInitialize | Optional explicit initialization, null argument, idempotent; never required by the launcher |
| DAWindowerShutdown | Optional controlled hook removal; ERROR_BUSY while objects/windows exist; quiesce graphics first |
| DAWindowerGetStats | Optional size-initialized DAWindowerStats buffer; does not initialize rendering |

Success returns zero. The module pins itself once initialized, so hook code cannot
be unmapped while in use. Normal integration keeps it loaded until process exit;
no shutdown call is needed. These exports are useful for development diagnostics,
not steps in the standard injection sequence.

Only this DLL is installed by CMake. Headers, source tests and documentation stay
in the source repository. Review [loading/recovery evidence](standalone-validation.md)
and [current resize validation](smooth-resize-validation.md) before wider rollout;
a confirmed client fingerprint is not universal compatibility.
