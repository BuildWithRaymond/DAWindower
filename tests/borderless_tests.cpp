#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <dwmapi.h>
#include "dawindower.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", what, GetLastError());
        std::exit(1);
    }
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    return DefWindowProcW(w, m, a, b);
}

RECT outer(HWND w) {
    RECT r{};
    check(GetWindowRect(w, &r) != FALSE, "read outer bounds");
    return r;
}

void flushClient(HWND w, int caption = 0) {
    const auto r = outer(w);
    RECT c{};
    GetClientRect(w, &c);
    POINT p{};
    ClientToScreen(w, &p);
#ifndef DAW_X
    if (caption) {
        RECT visible{};
        check(SUCCEEDED(
                  DwmGetWindowAttribute(w, DWMWA_EXTENDED_FRAME_BOUNDS, &visible, sizeof(visible))),
              "native visible frame bounds");
        check(p.x - visible.left >= 0 && p.x - visible.left <= 1 &&
                  visible.right - p.x - c.right >= 0 && visible.right - p.x - c.right <= 1 &&
                  visible.bottom - p.y - c.bottom >= 0 && visible.bottom - p.y - c.bottom <= 1 &&
                  p.y == r.top + caption,
              "native caption keeps invisible margins without a thick visible frame");
        return;
    }
#endif
    check(p.x == r.left && p.y == r.top + caption && c.right == r.right - r.left &&
              c.bottom + caption == r.bottom - r.top,
          "no visible outer frame or unused client margin");
}

void launcherFullscreen(HWND w, const RECT& monitor, bool attached = false) {
    // Exercise a launcher fullscreen transition: replace the style, then maximize.
    // Also exercise explicit monitor bounds for borderless fullscreen.
    SetWindowLongW(w, GWL_STYLE, WS_POPUP);
    const auto show = reinterpret_cast<decltype(&ShowWindow)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "ShowWindow"));
    show(w, SW_MAXIMIZE); // Launcher uses native USER32, not Dark Ages' IAT slot.
    if (attached) {
        const auto maximized = outer(w);
        check(EqualRect(&maximized, &monitor) != FALSE,
              "launcher style plus maximize alone uses full monitor bounds");
    }
    check(SetWindowPos(w,
                       nullptr,
                       monitor.left,
                       monitor.top,
                       monitor.right - monitor.left,
                       monitor.bottom - monitor.top,
                       SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) != FALSE,
          "launcher sets monitor-sized popup");
}

void fullscreen(HWND w, const RECT& monitor) {
    check((GetWindowLongW(w, GWL_STYLE) & (WS_CAPTION | WS_THICKFRAME)) == 0,
          "launcher popup style survives attachment");
    check(IsZoomed(w) != FALSE, "launcher maximized state survives attachment");
    const auto r = outer(w);
    check(EqualRect(&r, &monitor) != FALSE,
          "launcher fullscreen is not shrunk to windowed dimensions");
    flushClient(w);
    const auto packed = MAKELPARAM(static_cast<SHORT>(r.left + 1), static_cast<SHORT>(r.top + 1));
    check(SendMessageW(w, WM_NCHITTEST, 0, packed) == HTCLIENT,
          "fullscreen edges belong to the game, not resize grips or X buttons");
}
} // namespace

int main(int argc, char** argv) {
    const bool before = argc > 1 && std::strcmp(argv[1], "--before") == 0;
    const bool after = argc > 1 && std::strcmp(argv[1], "--after") == 0;
    const bool gates = argc > 1 && std::strcmp(argv[1], "--gates") == 0;
    auto module = LoadLibraryA(DAW_TEST_DLL_A);
    check(module != nullptr, "LoadLibrary-only setup");
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    WNDCLASSW cls{};
    cls.lpfnWndProc = proc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"DAWBorderlessContract";
    check(RegisterClassW(&cls) != 0, "register fixture");
    auto w = CreateWindowW(cls.lpszClassName,
                           L"Game",
                           WS_POPUP,
                           40,
                           40,
                           640,
                           480,
                           nullptr,
                           nullptr,
                           cls.hInstance,
                           nullptr);
    check(w != nullptr, "create game window");
    MONITORINFO info{sizeof(info)};
    check(GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &info) != FALSE,
          "read monitor");
    if (before)
        launcherFullscreen(w, info.rcMonitor);
    IDirectDraw* draw = nullptr;
    check(SUCCEEDED(DirectDrawCreate(nullptr, &draw, nullptr)), "wrapped factory");
    check(SUCCEEDED(draw->SetCooperativeLevel(w, DDSCL_NORMAL)), "attach game");
    if (after)
        launcherFullscreen(w, info.rcMonitor, true);
    if (gates) {
        const auto selected = outer(w);
        check(MoveWindow(w, 0, 0, 320, 240, TRUE) != FALSE,
              "client MoveWindow reports successful adaptation");
        const auto moved = outer(w);
        check(EqualRect(&selected, &moved) != FALSE,
              "client MoveWindow cannot undo selected geometry");
        RECT client{};
        GetClientRect(w, &client);
        const auto move = reinterpret_cast<decltype(&MoveWindow)>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "MoveWindow"));
        check(move(w,
                   20,
                   20,
                   801 + selected.right - selected.left - client.right,
                   601 + selected.bottom - selected.top - client.bottom,
                   FALSE),
              "direct native launcher MoveWindow remains available");
        GetClientRect(w, &client);
        check(client.right == 801 && client.bottom == 601,
              "native launcher geometry bypasses client guard and keeps windowed aspect");
        const auto style = GetWindowLongA(w, GWL_STYLE);
        check(SetWindowLongA(w, GWL_STYLE, WS_POPUP) == style &&
                  GetWindowLongA(w, GWL_STYLE) == style,
              "client style writes preserve selected frame and return previous style");
        const auto extended = GetWindowLongA(w, GWL_EXSTYLE);
        SetWindowLongA(w, GWL_EXSTYLE, WS_EX_CLIENTEDGE);
        check(GetWindowLongA(w, GWL_EXSTYLE) == extended, "client cannot add an extended border");
        SetWindowLongA(w, GWL_USERDATA, 1234);
        check(GetWindowLongA(w, GWL_USERDATA) == 1234,
              "client userdata writes keep native behavior");
        auto other = CreateWindowW(cls.lpszClassName,
                                   L"Untracked",
                                   WS_POPUP,
                                   5,
                                   5,
                                   80,
                                   60,
                                   nullptr,
                                   nullptr,
                                   cls.hInstance,
                                   nullptr);
        check(other != nullptr && MoveWindow(other, 20, 20, 90, 70, FALSE),
              "untracked window geometry passes through");
        const auto unrelated = outer(other);
        check(unrelated.left == 20 && unrelated.top == 20 && unrelated.right - unrelated.left == 90,
              "untracked windows retain native MoveWindow semantics");
        DestroyWindow(other);
        launcherFullscreen(w, info.rcMonitor, true);
        ShowWindow(w, SW_SHOWNORMAL);
        fullscreen(w, info.rcMonitor);
        ShowWindow(w, SW_MINIMIZE);
        check(IsIconic(w) != FALSE, "client minimize remains available");
        ShowWindow(w, SW_RESTORE);
        fullscreen(w, info.rcMonitor);
        ShowWindow(w, SW_HIDE);
        check(!IsWindowVisible(w), "client hide remains available for normal teardown");
        ShowWindow(w, SW_SHOW);
        check(IsWindowVisible(w) != FALSE, "client show remains available");
    } else if (before || after) {
        fullscreen(w, info.rcMonitor);
        // Repeated display/activation/resize notifications must not undo the
        // launcher's mode or apply the ordinary 4:3 tracking constraint.
        SendMessageW(w,
                     WM_DISPLAYCHANGE,
                     32,
                     MAKELPARAM(info.rcMonitor.right - info.rcMonitor.left,
                                info.rcMonitor.bottom - info.rcMonitor.top));
        SendMessageW(w, WM_ACTIVATEAPP, TRUE, 0);
        fullscreen(w, info.rcMonitor);
        MoveWindow(w, 0, 0, 640, 480, TRUE);
        ShowWindow(w, SW_SHOWNORMAL);
        fullscreen(w, info.rcMonitor);
    } else {
        check((GetWindowLongW(w, GWL_STYLE) & WS_THICKFRAME) != 0,
              "ordinary borderless window remains resizable");
#ifdef DAW_X
        const int caption = MulDiv(36, GetDpiForWindow(w), 96);
#else
        check((GetWindowLongW(w, GWL_STYLE) & WS_CAPTION) == WS_CAPTION,
              "normal edition retains its native Windows title bar");
        RECT native{};
        AdjustWindowRectExForDpi(&native,
                                 GetWindowLongW(w, GWL_STYLE),
                                 FALSE,
                                 GetWindowLongW(w, GWL_EXSTYLE),
                                 GetDpiForWindow(w));
        const int caption = -native.top;
#endif
        flushClient(w, caption);
        const auto r = outer(w);
        RECT beforeClient{};
        GetClientRect(w, &beforeClient);
        const int frameWidth = r.right - r.left - beforeClient.right;
        const int frameHeight = r.bottom - r.top - beforeClient.bottom;
        check(SendMessageW(w, WM_NCHITTEST, 0, MAKELPARAM(r.left + 100, r.top + caption / 2)) ==
                  HTCAPTION,
              "both editions retain a draggable title bar");
        const int hits[] = {
            HTTOPLEFT, HTTOP, HTTOPRIGHT, HTLEFT, HTRIGHT, HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT};
        const POINT points[] = {{r.left + 1, r.top + 1},
                                {(r.left + r.right) / 2, r.top + 1},
                                {r.right - 1, r.top + 1},
                                {r.left + 1, (r.top + r.bottom) / 2},
                                {r.right - 1, (r.top + r.bottom) / 2},
                                {r.left + 1, r.bottom - 1},
                                {(r.left + r.right) / 2, r.bottom - 1},
                                {r.right - 1, r.bottom - 1}};
        for (unsigned i = 0; i < 8; ++i)
            check(SendMessageW(w, WM_NCHITTEST, 0, MAKELPARAM(points[i].x, points[i].y)) == hits[i],
                  "invisible edges retain all eight native resize hit regions");
#ifndef DAW_X
        POINT clientOrigin{};
        ClientToScreen(w, &clientOrigin);
        const POINT visibleEdges[] = {
            {clientOrigin.x + 1, clientOrigin.y + beforeClient.bottom / 2},
            {clientOrigin.x + beforeClient.right - 1, clientOrigin.y + beforeClient.bottom / 2},
            {clientOrigin.x + beforeClient.right / 2, clientOrigin.y + beforeClient.bottom - 1}};
        const int visibleHits[] = {HTLEFT, HTRIGHT, HTBOTTOM};
        for (unsigned i = 0; i < 3; ++i)
            check(SendMessageW(
                      w, WM_NCHITTEST, 0, MAKELPARAM(visibleEdges[i].x, visibleEdges[i].y)) ==
                      visibleHits[i],
                  "resize grips reach the visible game edges beyond invisible native margins");
#endif
        SetWindowPos(w,
                     nullptr,
                     0,
                     0,
                     801 + frameWidth,
                     480 + frameHeight,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        RECT c{};
        GetClientRect(w, &c);
        check(c.right == 801 && c.bottom == 601,
              "borderless window keeps continuous native aspect sizing");
    }
    draw->Release();
    DestroyWindow(w);
    check(stop && stop(nullptr) == 0, "clean lifetime");
    std::puts("Borderless window/fullscreen contract passed.");
}
