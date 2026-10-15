#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include "dawindower.h"
#include "window_size.h"
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace {
unsigned sizingCalls = 0, clicks = 0;
int mouseX = -1, mouseY = -1;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", what, GetLastError());
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

bool ratio(int width, int height) {
    const auto error = 3LL * width - 4LL * height;
    return width > 0 && height > 0 && error >= -2 && error <= 2;
}

RECT clientSize(HWND w, int width, int height) {
    RECT outer{0, 0, width, height};
    check(adjustForClient(w, outer), "measure frame separately from game client");
    check(SetWindowPos(w,
                       nullptr,
                       0,
                       0,
                       outer.right - outer.left,
                       outer.bottom - outer.top,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
          "request client size");
    RECT actual{};
    GetClientRect(w, &actual);
    check(ratio(actual.right, actual.bottom), "client maintains nearest-pixel native 4:3");
    return actual;
}
} // namespace

int main() {
    auto module = LoadLibraryA(DAW_TEST_DLL_A);
    check(module != nullptr, "LoadLibrary-only setup");
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    WNDCLASSW c{};
    c.lpfnWndProc = proc;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"DAWAspectResizeTest";
    check(RegisterClassW(&c) != 0, "register window");
    auto w = CreateWindowW(c.lpszClassName,
                           L"Aspect resize contract",
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

    auto actual = clientSize(w, 704, 480);
    if (actual.right != 704 || actual.bottom != 528)
        std::fprintf(stderr, "Width-driven request actual=%ldx%ld\n", actual.right, actual.bottom);
    check(actual.right == 704 && actual.bottom == 528, "wider means taller without stretching");
    for (int width = 705; width <= 737; ++width) {
        const auto previous = actual;
        actual = clientSize(w, width, actual.bottom);
        check(actual.right == width && actual.bottom >= previous.bottom &&
                  actual.bottom <= previous.bottom + 1,
              "every width pixel accepted without four-pixel size steps");
    }
    for (int height = 600; height <= 632; ++height) {
        actual = clientSize(w, actual.right, height);
        check(actual.bottom == height, "every height pixel accepted while width follows");
    }
    actual = clientSize(w, 1280, 720);
    check(actual.right == 960 && actual.bottom == 720,
          "conflicting box fits a 4:3 client without bars");
    actual = clientSize(w, 481, 701);
    check(actual.right == 481 && actual.bottom == 361, "portrait box fits a 4:3 client");

    RECT original{};
    GetWindowRect(w, &original);
    const int frameWidth = original.right - original.left - actual.right;
    const int frameHeight = original.bottom - original.top - actual.bottom;
    SendMessageW(w, WM_ENTERSIZEMOVE, 0, 0);
    for (unsigned edge = WMSZ_LEFT; edge <= WMSZ_BOTTOMRIGHT; ++edge) {
        auto proposed = original;
        const bool left = edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT;
        const bool right = edge == WMSZ_RIGHT || edge == WMSZ_TOPRIGHT || edge == WMSZ_BOTTOMRIGHT;
        const bool top = edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT;
        const bool bottom =
            edge == WMSZ_BOTTOM || edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT;
        if (left)
            --proposed.left;
        if (right)
            ++proposed.right;
        if (top)
            --proposed.top;
        if (bottom)
            ++proposed.bottom;
        SendMessageW(w, WM_SIZING, edge, reinterpret_cast<LPARAM>(&proposed));
        check(ratio(proposed.right - proposed.left - frameWidth,
                    proposed.bottom - proposed.top - frameHeight),
              "all eight drag edges preserve client aspect ratio");
        check((!left || proposed.right == original.right) &&
                  (!right || proposed.left == original.left) &&
                  (!top || proposed.bottom == original.bottom) &&
                  (!bottom || proposed.top == original.top),
              "opposite drag edge/corner remains anchored");
    }
    SendMessageW(w, WM_EXITSIZEMOVE, 0, 0);
    check(sizingCalls == 8, "host continues receiving every sizing message");
    const unsigned corners[] = {WMSZ_TOPLEFT, WMSZ_TOPRIGHT, WMSZ_BOTTOMLEFT, WMSZ_BOTTOMRIGHT};
    for (const auto corner : corners) {
        for (int axis = 0; axis < 2; ++axis) {
            clientSize(w, 640, 480);
            RECT start{};
            GetWindowRect(w, &start);
            RECT previous{0, 0, 640, 480};
            SendMessageW(w, WM_ENTERSIZEMOVE, 0, 0);
            for (int tick = 1; tick <= 128; ++tick) {
                const bool outward = tick <= 64;
                const int step = outward ? tick : 128 - tick;
                auto proposed = start;
                const bool left = corner == WMSZ_TOPLEFT || corner == WMSZ_BOTTOMLEFT;
                const bool top = corner == WMSZ_TOPLEFT || corner == WMSZ_TOPRIGHT;
                if (axis == 0) {
                    if (left)
                        proposed.left -= step;
                    else
                        proposed.right += step;
                } else {
                    if (top)
                        proposed.top -= step;
                    else
                        proposed.bottom += step;
                }
                SendMessageW(w, WM_SIZING, corner, reinterpret_cast<LPARAM>(&proposed));
                SetWindowPos(w,
                             nullptr,
                             proposed.left,
                             proposed.top,
                             proposed.right - proposed.left,
                             proposed.bottom - proposed.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                RECT now{};
                GetClientRect(w, &now);
                check(ratio(now.right, now.bottom), "applied corner sequence keeps native aspect");
                const bool follows =
                    outward ? now.right >= previous.right && now.bottom >= previous.bottom
                            : now.right <= previous.right && now.bottom <= previous.bottom;
                if (!follows)
                    std::fprintf(stderr,
                                 "Corner %u axis %d step %d: %ldx%ld -> %ldx%ld\n",
                                 corner,
                                 axis,
                                 step,
                                 previous.right,
                                 previous.bottom,
                                 now.right,
                                 now.bottom);
                check(follows, "corner drag never reverses requested motion between mouse pixels");
                check(std::abs(now.right - previous.right) <= 2 &&
                          std::abs(now.bottom - previous.bottom) <= 2,
                      "corner sizing has no multi-pixel size breakpoint");
                previous = now;
            }
            SendMessageW(w, WM_EXITSIZEMOVE, 0, 0);
        }
    }
    MINMAXINFO limits{};
    SendMessageW(w, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&limits));
    // MINMAXINFO reports outer-window dimensions; the pixel sequence below
    // exercises client dimensions, independently of native caption/frame size.
    const int smallest = limits.ptMinTrackSize.x - frameWidth;
    const int largest = limits.ptMaxTrackSize.x - frameWidth;
    check(smallest > 0 && largest >= smallest, "native tracking bounds are ordered");
    clientSize(w, smallest, (smallest * 3 + 2) / 4);
    RECT start{};
    GetWindowRect(w, &start);
    SendMessageW(w, WM_ENTERSIZEMOVE, 0, 0);
    // Bounded even on unusually large virtual desktops; also check the endpoint.
    for (int width = smallest; width <= std::min(largest, smallest + 4096); ++width) {
        auto proposed = start;
        proposed.right = proposed.left + width + frameWidth;
        SendMessageW(w, WM_SIZING, WMSZ_RIGHT, reinterpret_cast<LPARAM>(&proposed));
        SetWindowPos(w,
                     nullptr,
                     proposed.left,
                     proposed.top,
                     proposed.right - proposed.left,
                     proposed.bottom - proposed.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        RECT now{};
        GetClientRect(w, &now);
        check(now.right == width && ratio(now.right, now.bottom),
              "every width pixel from minimum to maximum is available without preset sizes");
    }
    SendMessageW(w, WM_EXITSIZEMOVE, 0, 0);
    actual = clientSize(w, largest, (largest * 3 + 2) / 4);
    check(actual.right == largest, "largest native tracking size is reachable");
    actual = clientSize(w, 960, 720);
    actual = clientSize(w, 1001, actual.bottom);
    SendMessageW(w, WM_MOUSEMOVE, 0, MAKELPARAM(actual.right / 2, actual.bottom / 2));
    check(mouseX >= 319 && mouseX <= 320 && mouseY >= 239 && mouseY <= 240,
          "mouse center stays accurate at rounded aspect dimensions");
    SendMessageW(w, WM_LBUTTONDOWN, 0, MAKELPARAM(-1, actual.bottom / 2));
    check(clicks == 0, "outside-client click excluded");
    SendMessageW(w, WM_LBUTTONDOWN, 0, MAKELPARAM(0, actual.bottom / 2));
    check(clicks == 1, "client edge still receives mapped game input");
    draw->Release();
    DestroyWindow(w);
    check(stop && stop(nullptr) == 0, "clean lifetime");
    std::puts("Native aspect, continuous edge/corner sizing and mouse mapping passed.");
}
