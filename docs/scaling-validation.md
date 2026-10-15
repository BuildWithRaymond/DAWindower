# RGB565 downscaling performance

The 0.2.3 candidate presents through a reusable 32-bit BGRX buffer in both
editions. The retained game/cursor canvas remains 640×480 RGB565. This removes
the reproduced GDI downscaling performance cliff without changing window sizing.

## Cause and correction

A 639×479 client is valid nearest-pixel 4:3 and remains at that size. However,
the previous `StretchDIBits` call combined RGB565 format conversion and shrinking.
An isolated GDI-only benchmark reproduced the high CPU cost, including at
636×477, which is exact 4:3. Aspect tolerance is therefore not the CPU cause.

[WindowState::paint](../src/window/window.cpp) now expands the completed frame
with SSE2 into one reusable BGRX buffer and passes that 32-bit image to the same
`StretchDIBits`/`COLORONCOLOR` presentation path. Bit replication matches Windows'
RGB565 conversion. The buffer adds 1.17 MiB per window and requires no additional
runtime DLL. Allocation failure follows the existing attachment error path.
The RGB565 canvas, GDI synchronization, cursor transaction boundary and recovery
scheduling are preserved. No native-size snap or sizing-hook change is included.

A simple `StretchBlt` substitution was rejected: it selected different source
pixels at odd dimensions in the comparison fixture. The 32-bit `StretchDIBits`
path retained exact colors and sampling at every tested size.

## Packaged DLL measurements

The synthetic fixture loads each actual packaged DLL by `LoadLibraryA` and
attaches through the wrapped DirectDraw factory. It writes a 640×480 frame
through the EXE's adapted `BitBlt`, dispatches presentation, and flushes GDI.
The production measurements use no research presentation replacement.

Each size has two 200-frame measured passes, with 30 warmup frames per pass.
CPU time is the `GetProcessTimes` difference divided by frame count; it includes
capture, conversion and presentation. The table reports mean milliseconds per
frame on the same local Windows x86 setup. Desktop load affects wall time;
these are CPU measurements, not an FPS guarantee.

| Client area | Previous standard | New standard | New X |
| --- | ---: | ---: | ---: |
| 640×480 | 2.34 | 1.60 | 1.45 |
| 639×479 | 25.78 | 2.46 | 2.38 |
| 636×477 | 24.34 | 2.46 | 2.27 |
| 641×481 | 2.38 | 1.60 | 1.60 |
| 800×600 | 2.42 | 1.91 | 2.03 |
| 960×720 | 2.54 | 1.37 | 2.03 |

The reproduced 639×479 case uses approximately 90% less process CPU per frame.
Each production fixture reports 2,760 frame writes and zero presentation failures.
This is synthetic performance evidence, not profiling of a live game session or
a guarantee of the same percentages on another CPU, driver or desktop setup.

| Package | Bytes | SHA256 |
| --- | ---: | --- |
| Previous standard, `dist/0.2.3-shared-caption/DAWindower.dll` | 178176 | `91805735a195914ce69653415821c8329e9ec50f79e9a246c7a70faeea12bd02` |
| Standard, `dist/0.2.3-fast-scaling/DAWindower.dll` | 178688 | `a2694f843310671a1d3e751247f3b2ba00f658364d082af0c8dafe78b59d0916` |
| X, `dist/0.2.3-x-fast-scaling/DAWindowerX.dll` | 209408 | `50b7af19f653beb6bf58ff1173eddc5b7b9c844cd01967b954fed1531eb0bb96` |

Both new packages pass the one-file PE32/i386, system-import and export checks.
Raw logs and the measurement fixture are ignored local development artifacts in
`runtime-tests/size-investigation`: `end-to-end-original.log`,
`production-standard.log`, `production-x.log`, `rendering-focused.log` and
`summary.json`.

## Regression evidence

The new `presentation_format_contract` first failed on the RGB565 desktop input.
After the correction, it compares every RGB565 color and Windows' exact output
sampling at the six sizes above. A second changed frame verifies that staging
does not leave stale pixels. The retained RGB565 canvas must remain unchanged.
See [the test](../tests/presentation_recovery_tests.cpp) and
[the full test map](testing.md).

All 22 standard and 23 X contracts pass in both Release and Debug: 90 checks in
total. Existing tests cover actual synthetic-window pixels, cached game DCs,
software-cursor composition, native resize cursors, transient presentation/queue
failure recovery, surface repair, window lifetime and both caption variants.
Formatting and documentation checks also pass. Raw suite logs are retained as
`runtime-tests/fast-scaling-*-tests.log`.

No existing game client or Excalibur process was restarted or modified. Retail
CPU profiling, prolonged multi-client load and additional hardware/DPI layouts
remain separate acceptance work.
