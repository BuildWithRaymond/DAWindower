#include "window/window.h"
#include "runtime.h"
#include "compatibility/imports.h"
#include <cstdio>
#include <cstdlib>
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

int main() {
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
