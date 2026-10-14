#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include "dawindower.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
unsigned sizingCalls = 0, clicks = 0;
int mouseX = -1, mouseY = -1;

void check(bool ok, const char* name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", name, GetLastError());
        std::exit(1);
    }
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_SIZING)
        ++sizingCalls;
    if (m == WM_MOUSEMOVE) {
        mouseX = GET_X_LPARAM(b);
        mouseY = GET_Y_LPARAM(b);
    }
    if (m == WM_LBUTTONDOWN)
        ++clicks;
    return DefWindowProcW(w, m, a, b);
}

void clientSize(HWND w, int width, int height) {
    RECT r{0, 0, width, height};
    check(AdjustWindowRectEx(
              &r, GetWindowLongW(w, GWL_STYLE), FALSE, GetWindowLongW(w, GWL_EXSTYLE)) != FALSE,
          "calculate requested size");
    check(SetWindowPos(w,
                       nullptr,
                       0,
                       0,
                       r.right - r.left,
                       r.bottom - r.top,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
          "apply size");
    RECT actual{};
    GetClientRect(w, &actual);
    if (actual.right != width || actual.bottom != height)
        std::fprintf(
            stderr, "requested=%dx%d actual=%ldx%ld\n", width, height, actual.right, actual.bottom);
    check(actual.right == width && actual.bottom == height,
          "every requested client pixel size accepted without snapping");
}
} // namespace

int main() {
    auto module = LoadLibraryA("DAWindower.dll");
    check(module != nullptr, "LoadLibrary-only setup");
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    WNDCLASSW c{};
    c.lpfnWndProc = proc;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"DAWSmoothResizeTest";
    check(RegisterClassW(&c) != 0, "register window");
    auto w = CreateWindowW(c.lpszClassName,
                           L"Smooth resize contract",
                           WS_POPUP,
                           30,
                           30,
                           640,
                           480,
                           nullptr,
                           nullptr,
                           c.hInstance,
                           nullptr);
    check(w != nullptr, "create window");
    IDirectDraw* draw = nullptr;
    check(SUCCEEDED(DirectDrawCreate(nullptr, &draw, nullptr)), "wrapped factory");
    check(SUCCEEDED(draw->SetCooperativeLevel(w, DDSCL_NORMAL)), "attach resizable window");
    for (int width = 640; width <= 672; ++width)
        clientSize(w, width, 503);
    for (int height = 480; height <= 512; ++height)
        clientSize(w, 701, height);
    clientSize(w, 481, 701);
    RECT original{};
    GetWindowRect(w, &original);
    SendMessageW(w, WM_ENTERSIZEMOVE, 0, 0);
    for (unsigned edge = WMSZ_LEFT; edge <= WMSZ_BOTTOMRIGHT; ++edge) {
        auto proposed = original;
        if (edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT)
            --proposed.left;
        if (edge == WMSZ_RIGHT || edge == WMSZ_TOPRIGHT || edge == WMSZ_BOTTOMRIGHT)
            ++proposed.right;
        if (edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT)
            --proposed.top;
        if (edge == WMSZ_BOTTOM || edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT)
            ++proposed.bottom;
        const auto requested = proposed;
        SendMessageW(w, WM_SIZING, edge, reinterpret_cast<LPARAM>(&proposed));
        check(EqualRect(&requested, &proposed) != FALSE,
              "one-pixel edge/corner drags remain unchanged");
    }
    SendMessageW(w, WM_EXITSIZEMOVE, 0, 0);
    check(sizingCalls == 8, "host receives every native sizing message");
    clientSize(w, 1001, 703);
    SendMessageW(w, WM_MOUSEMOVE, 0, MAKELPARAM(500, 351));
    check(mouseX >= 319 && mouseX <= 320 && mouseY >= 239 && mouseY <= 240,
          "arbitrary-size mouse center accurate");
    SendMessageW(w, WM_LBUTTONDOWN, 0, MAKELPARAM(-1, 351));
    check(clicks == 0, "outside client click excluded");
    SendMessageW(w, WM_LBUTTONDOWN, 0, MAKELPARAM(0, 351));
    check(clicks == 1, "left edge remains playable without letterboxing");
    SendMessageW(w, WM_LBUTTONDOWN, 0, MAKELPARAM(500, 351));
    check(clicks == 2, "game click delivered");
    draw->Release();
    DestroyWindow(w);
    check(stop && stop(nullptr) == 0, "clean lifetime");
    std::puts(
        "PASS: every pixel size, portrait/wide windows, native edge/corner drags, host messages and mouse mapping");
}
