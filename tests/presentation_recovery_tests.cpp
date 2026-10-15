#include "window/window.h"
#include "runtime.h"
#include "compatibility/imports.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace daw {
CreateDraw realCreateDraw = nullptr;
std::atomic<DWORD> devices{0}, surfaces{0}, windows{0}, frames{0}, failures{0}, showCalls{0},
    setCalls{0};

bool tracing() noexcept {
    return false;
}

void log(const wchar_t*, ...) noexcept {
}

void RecoveryBinding::service() {
}
} // namespace daw

static unsigned rejected = 0, attempts = 0;
static WORD presentedBitCount = 0;
static bool failQueue = false;
static decltype(&StretchDIBits) nativeStretch;
static decltype(&PostMessageW) nativePost;

static int WINAPI stretch(HDC dc,
                          int x,
                          int y,
                          int w,
                          int h,
                          int sx,
                          int sy,
                          int sw,
                          int sh,
                          const void* bits,
                          const BITMAPINFO* info,
                          UINT use,
                          DWORD rop) {
    ++attempts;
    presentedBitCount = info->bmiHeader.biBitCount;
    if (rejected) {
        --rejected;
        return GDI_ERROR;
    }
    return nativeStretch(dc, x, y, w, h, sx, sy, sw, sh, bits, info, use, rop);
}

static BOOL WINAPI post(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (failQueue && m >= 0xc000) {
        SetLastError(ERROR_NOT_ENOUGH_QUOTA);
        return FALSE;
    }
    return nativePost(w, m, a, b);
}

static void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

static void pump(DWORD duration) {
    const auto until = GetTickCount64() + duration;
    do {
        MSG m{};
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    } while (GetTickCount64() < until);
}

static void verifyPresentationFormat(daw::WindowState& state) {
    constexpr int outputWidth = 960, outputHeight = 720;
    BITMAPINFO outputInfo{};
    outputInfo.bmiHeader = {sizeof(BITMAPINFOHEADER),
                            outputWidth,
                            -outputHeight,
                            1,
                            32,
                            BI_RGB,
                            outputWidth * outputHeight * 4,
                            0,
                            0,
                            0,
                            0};
    void* actualPixels = nullptr;
    void* expectedPixels = nullptr;
    auto actualBitmap =
        CreateDIBSection(nullptr, &outputInfo, DIB_RGB_COLORS, &actualPixels, nullptr, 0);
    auto expectedBitmap =
        CreateDIBSection(nullptr, &outputInfo, DIB_RGB_COLORS, &expectedPixels, nullptr, 0);
    auto actualDC = CreateCompatibleDC(nullptr);
    auto expectedDC = CreateCompatibleDC(nullptr);
    check(actualBitmap && expectedBitmap && actualDC && expectedDC,
          "allocate presentation comparison images");
    auto oldActual = SelectObject(actualDC, actualBitmap);
    auto oldExpected = SelectObject(expectedDC, expectedBitmap);
    SetStretchBltMode(expectedDC, COLORONCOLOR);

    struct {
        BITMAPINFOHEADER header;
        DWORD masks[3];
    } nativeInfo{
        {sizeof(BITMAPINFOHEADER), 640, -480, 1, 16, BI_BITFIELDS, 640 * 480 * 2, 0, 0, 0, 0},
        {0xf800, 0x07e0, 0x001f}};

    const int sizes[][2] = {{640, 480}, {639, 479}, {636, 477}, {641, 481}, {800, 600}, {960, 720}};
    std::vector<WORD> source(640 * 480);
    // Each frame covers all 65,536 RGB565 colors. Change content without a
    // resize so a retained presentation buffer cannot silently become stale.
    for (int frame = 0; frame < 2; ++frame) {
        GdiFlush();
        for (unsigned i = 0; i < source.size(); ++i)
            source[i] = static_cast<WORD>(i * 211 + frame * 9973);
        std::memcpy(state.pixels, source.data(), source.size() * sizeof(WORD));
        state.hasFrame = true;
        ++state.generation;
        for (const auto& size : sizes) {
            check(nativeStretch(expectedDC,
                                0,
                                0,
                                size[0],
                                size[1],
                                0,
                                0,
                                640,
                                480,
                                source.data(),
                                reinterpret_cast<const BITMAPINFO*>(&nativeInfo),
                                DIB_RGB_COLORS,
                                SRCCOPY) != 0,
                  "render native RGB565 reference");
            check(state.paint(actualDC, size[0], size[1]), "present completed frame");
            check(presentedBitCount == 32,
                  "desktop scaling uses 32-bit pixels instead of slow RGB565 downscaling");
            check(GdiFlush() != FALSE, "finish comparison images");
            auto actual = static_cast<const DWORD*>(actualPixels);
            auto expected = static_cast<const DWORD*>(expectedPixels);
            for (int y = 0; y < size[1]; ++y) {
                for (int x = 0; x < size[0]; ++x) {
                    const int index = y * outputWidth + x;
                    check(
                        (actual[index] & 0xffffff) == (expected[index] & 0xffffff),
                        "32-bit presentation preserves RGB565 colors and nearest-neighbor sampling");
                }
            }
            check(std::memcmp(state.pixels, source.data(), source.size() * sizeof(WORD)) == 0,
                  "presentation preserves game and cursor RGB565 pixels");
        }
    }
    SelectObject(actualDC, oldActual);
    SelectObject(expectedDC, oldExpected);
    DeleteObject(actualBitmap);
    DeleteObject(expectedBitmap);
    DeleteDC(actualDC);
    DeleteDC(expectedDC);
}

int main(int argc, char** argv) {
    WNDCLASSW c{};
    c.lpfnWndProc = DefWindowProcW;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"DAWRecoveryTest";
    check(RegisterClassW(&c) != 0, "register recovery window");
    auto w = CreateWindowW(c.lpszClassName,
                           L"Presentation recovery test",
                           WS_OVERLAPPEDWINDOW,
                           40,
                           40,
                           640,
                           480,
                           nullptr,
                           nullptr,
                           c.hInstance,
                           nullptr);
    auto state = std::make_shared<daw::WindowState>();
    check(SUCCEEDED(daw::attachWindow(state, w)), "attach recovery window");
    std::fill(state->pixels, state->pixels + 640 * 480, static_cast<WORD>(0x07e0));
    state->hasFrame = true;
    ShowWindow(w, SW_SHOWNOACTIVATE);
    pump(100);
    nativeStretch = reinterpret_cast<decltype(nativeStretch)>(
        GetProcAddress(GetModuleHandleW(L"gdi32.dll"), "StretchDIBits"));
    nativePost = reinterpret_cast<decltype(nativePost)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "PostMessageW"));
    std::vector<daw::ImportPatch> patches;
    check(daw::patchImport(c.hInstance,
                           "gdi32.dll",
                           "StretchDIBits",
                           reinterpret_cast<void*>(nativeStretch),
                           reinterpret_cast<void*>(stretch),
                           patches) == 0,
          "inject transient display failure");
    check(daw::patchImport(c.hInstance,
                           "user32.dll",
                           "PostMessageW",
                           reinterpret_cast<void*>(nativePost),
                           reinterpret_cast<void*>(post),
                           patches) == 0,
          "inject transient queue failure");
    if (argc > 1 && std::strcmp(argv[1], "--format") == 0) {
        verifyPresentationFormat(*state);
        DestroyWindow(w);
        state.reset();
        check(daw::restoreImports(patches) == 0, "restore presentation observer");
        check(daw::windows.load() == 0, "presentation buffer ends with window state");
        std::puts(
            "PASS: 32-bit presentation, all RGB565 colors, six sizes, updated frames and logical canvas preservation");
        return 0;
    }
    rejected = 3;
    state->requestPresentation();
    pump(40);
    std::printf(
        "first attempts=%u remaining=%u desktop=%d\n", attempts, rejected, state->desktopAvailable);
    check(attempts > 0 && attempts <= 2, "bounded initial failed queued frame");
    // No more frames, focus or activation events. Timer alone must recover.
    pump(1600);
    std::printf("present attempts=%u remaining=%u desktop=%d pending=%d retry=%d\n",
                attempts,
                rejected,
                state->desktopAvailable,
                state->presentationPending.load(),
                state->retryPresentation.load());
    check(rejected == 0 && attempts >= 4 && attempts <= 6 && !state->retryPresentation.load(),
          "bounded retries resume retained frame without new game writes");
    auto dc = GetDC(w);
    check(GetPixel(dc, 20, 20) == RGB(0, 255, 0), "recovered window has retained pixels");
    ReleaseDC(w, dc);
    const auto previous = attempts;
    failQueue = true;
    check(FAILED(state->requestPresentation()), "initial queue failure observed");
    pump(200);
    failQueue = false;
    pump(1500);
    check(attempts > previous, "failed PostMessage does not strand retained frame");
    ShowWindow(w, SW_MINIMIZE);
    state->requestPresentation();
    pump(200);
    ShowWindow(w, SW_SHOWNOACTIVATE);
    pump(200);
    dc = GetDC(w);
    check(GetPixel(dc, 20, 20) == RGB(0, 255, 0), "minimize restore repaints retained frame");
    ReleaseDC(w, dc);
    DestroyWindow(w);
    state.reset();
    check(daw::restoreImports(patches) == 0, "restore fault hooks");
    check(daw::windows.load() == 0, "timer lifetime ends with window");
    std::puts(
        "PASS: failed present/queue recovery without new frames or focus, backoff, minimize/restore, timer cleanup");
}
