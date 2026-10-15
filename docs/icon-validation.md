# Game icons and native caption spacing

October 14, 2026; version 0.2.3 candidate. This does not replace the immutable
published 0.2.2 preview. Each edition remains a separate, single native x86 DLL.

## Changes and regression evidence

The inspected dawnd binary's RegisterClassA adapter calls
`ExtractIconA(classInstance, executablePath, 0)` and assigns the resulting icon
to the game window class. DAWindower had not supplied that missing game icon.

Both editions now load the first icon group from the window class's module
during normal window attachment, outside loader lock. Windows owns the shared
resource handle. Existing window/class icons and later launcher replacements
are preserved. The DLL needs no embedded artwork, companion icon or Shell32
import. X paints the same icon in its existing caption and exposes its system
menu hit target.

The first regression run failed because both WM_GETICON slots were empty. The
new [native icon contract](../tests/window_icon_tests.cpp) compares actual icon
pixels against ExtractIconA on an original two-icon synthetic EXE fixture. It
also checks host ownership, reentrant destruction and repeated-window USER
handle lifetime. No game artwork is checked into the test fixtures.

Standard's icon/title then exposed a separate frame-layout problem: returning
zero horizontal insets from WM_NCCALCSIZE removed the invisible native margins
Windows uses to position caption content. The composed desktop fixture measured
the attached icon at x=0. Standard now retains those invisible margins, includes
them in outer/client size conversion, and extends resize hit regions through
them. Windows continues to draw its caption and buttons. X's caption layout is
unchanged. A caption regression checks an inset icon, its native system-menu
target, title/activation updates and no thick visible side frame.

The approved actual-client caption had an eight-pixel icon inset on this
desktop. Windows retained its one-pixel visible edge; invisible resize margins
are outside that edge. A temporary custom-painting experiment was rejected
after actual-client inspection and is absent from the implementation.

## Candidate packages

Both installations passed the one-file PE32/i386, optional export ABI and
Windows-only import checks. Standard still has no UxTheme or Shell32 dependency;
X retains its existing Windows theme/gradient dependencies.

| Edition | Local package | Bytes | SHA256 |
| --- | --- | --- | --- |
| Standard | `dist/0.2.3-game-icons/DAWindower.dll` | 169,984 | `76631219ca22076bf5d6ef4917a66778377418901c5e4e97d1f7f597d34906e1` |
| X | `dist/0.2.3-x-game-icons/DAWindowerX.dll` | 208,384 | `bf520eb31b91fb2eafaf85c82cd02b910365643446d026e96eb27494b5a89c48` |

## Automated checks

The icon-restoration candidate initially passed the full standard 20/20 and X
22/22 suites in both Debug and Release. Standard subsequently gained the native
caption spacing contract, bringing its suite to 21 tests.

Final standard Release and Debug each passed 21/21. Final X Release and Debug
each passed 22/22. Full configuration results are recorded with the local
`caption-*-tests.log` evidence. Resize fixtures were corrected to
distinguish MINMAXINFO outer tracking dimensions from game client dimensions;
the continuous pixel sequence and launcher geometry assertions remain intact.
A final visible-edge grip regression failed against the pre-extension standard
DLL and passed in both final standard configurations.

## Actual-client evidence

The isolated Dark Ages 7.41 executable had SHA256
`b168856a22a5315d2eb0fc942ca1cb6ef38e81035a445bc2cc9710d3d1a5715f`.
The desktop ran Windows build 26120.6972.

Initial icon-restoration candidates launched standard PID 26324 and X PID 7380
through suspended creation, remote LoadLibraryA alone and resumed startup.
Both started with a 640×480 game area, zero initialization calls and no adjacent
proxy DLL. Small and large WM_GETICON handles from both actual windows rendered
the same 32×32 pixels as native `ExtractIconA(gameExecutable, 0)`: pixel SHA256
`5d65d3774b7ea87112318aa870353116c0cbb5e962dcf1bd7f522a1bbd872795`.
Both game window classes reported the actual EXE module at 0x00400000.

The native-frame correction was then loaded into a fresh actual standard
client, PID 8512, through the same LoadLibraryA-only route. Startup remained
exactly 640×480. A real desktop capture showed the padded game icon/title and
native Windows buttons. The user confirmed **“Spacing looks correct.”** That
review used SHA256
`2fa574ae5fb10ec94caa2495d54aa17f4ae9d72aff4c61d057b5592e87e46381`;
the final standard package additionally extends edge hit regions through the
restored invisible margins.

Final package hashes above were then tested in fresh actual clients: standard
PID 16032 / HWND 0x0091030c and X PID 28076 / HWND 0x009603fa. Both loaded with
remote LoadLibraryA alone, zero initialization calls, the selected edition DLL
and system DDRAW, and no adjacent proxy. Both passed intermediate width/height
pixel resizing, native launcher-style fullscreen, minimize/restore and return
to exactly 640?480. They were left at native game size. Logs are
`icon-final-standard-retail.log` and `icon-final-x-retail.log`.
Both final windows' small and large icon slots were compared again against
ExtractIconA; all four rendered pixel hashes matched the value recorded above.

These checks do not recertify UAC, physical monitor transitions, complete
launcher workflows or every Windows/DPI/theme combination on these binaries.
Earlier recovery and X control observations retain their original scope in the
[standalone](standalone-validation.md) and [X](x-borderless-validation.md) records.
The [README screenshots](images/README.md) retain their original capture hashes
and predate this icon/caption change.

Logs, rejected experiments, retail assets and test executables remain ignored
local artifacts. Only source, original synthetic icon fixtures and documentation
belong to the public repository.
