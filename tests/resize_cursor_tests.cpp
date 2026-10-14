#include <windows.h>
#include <ddraw.h>
#include "dawindower.h"
#include <cstdio>
#include <cstdlib>

namespace {
unsigned clientCursorCalls = 0;

void check(bool ok, const char* name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", name, GetLastError());
        std::exit(1);
    }
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_SETCURSOR) {
        if (LOWORD(b) == HTCLIENT)
            ++clientCursorCalls;
        // A software-cursor client owns the client-area shape. Frame cursor
        // handling must work even if the host also clears nonclient shapes.
        SetCursor(nullptr);
        return TRUE;
    }
    return DefWindowProcW(w, m, a, b);
}

int displayCount() {
    const auto incremented = ShowCursor(TRUE);
    ShowCursor(FALSE);
    return incremented - 1;
}

void setDisplayCount(int requested) {
    for (unsigned n = 0; n < 128; ++n) {
        const auto current = displayCount();
        if (current == requested)
            return;
        ShowCursor(current < requested);
    }
    check(false, "bounded test cursor-count setup");
}

void cursor(HWND w, int hit) {
    SendMessageW(w, WM_SETCURSOR, reinterpret_cast<WPARAM>(w), MAKELPARAM(hit, WM_MOUSEMOVE));
}

void hiddenClient(HWND w, int expected = -1) {
    const auto calls = clientCursorCalls;
    cursor(w, HTCLIENT);
    check(clientCursorCalls == calls + 1, "host still owns software cursor in client area");
    check(GetCursor() == nullptr, "client cursor shape remains hidden");
    check(displayCount() == expected, "exact game cursor count restored on client entry");
}
} // namespace

int main() {
    const auto initialCount = displayCount();
    const auto initialShape = GetCursor();
    auto module = LoadLibraryA("DAWindower.dll");
    check(module != nullptr, "LoadLibrary-only setup");
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    WNDCLASSW c{};
    c.lpfnWndProc = proc;
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"DAWResizeCursorTest";
    check(RegisterClassW(&c) != 0, "register cursor window");
    auto w = CreateWindowW(c.lpszClassName,
                           L"Resize cursor contract",
                           WS_POPUP,
                           40,
                           40,
                           640,
                           480,
                           nullptr,
                           nullptr,
                           c.hInstance,
                           nullptr);
    check(w != nullptr, "create cursor window");
    IDirectDraw* draw = nullptr;
    check(SUCCEEDED(DirectDrawCreate(nullptr, &draw, nullptr)), "wrapped factory");
    check(SUCCEEDED(draw->SetCooperativeLevel(w, DDSCL_NORMAL)), "attach frame");
    setDisplayCount(-1);
    SetCursor(nullptr);

    const struct {
        int hit;
        LPCWSTR shape;
    } edges[] = {{HTLEFT, IDC_SIZEWE},
                 {HTRIGHT, IDC_SIZEWE},
                 {HTTOP, IDC_SIZENS},
                 {HTBOTTOM, IDC_SIZENS},
                 {HTTOPLEFT, IDC_SIZENWSE},
                 {HTBOTTOMRIGHT, IDC_SIZENWSE},
                 {HTTOPRIGHT, IDC_SIZENESW},
                 {HTBOTTOMLEFT, IDC_SIZENESW}};

    for (const auto& edge : edges) {
        cursor(w, edge.hit);
        check(GetCursor() == LoadCursorW(nullptr, edge.shape),
              "correct native resize arrow on each edge/corner");
        check(displayCount() == 0, "resize arrow visible despite game hiding Windows cursor");
        hiddenClient(w);
    }
    for (unsigned i = 0; i < 1000; ++i)
        cursor(w, HTRIGHT);
    check(displayCount() == 0, "repeated border hover has no visibility-count drift");
    hiddenClient(w);
    setDisplayCount(-3);
    cursor(w, HTTOP);
    check(displayCount() == 0, "nested game hides still allow resize arrow");
    hiddenClient(w, -3);
    setDisplayCount(2);
    cursor(w, HTBOTTOM);
    check(displayCount() == 2, "already-visible game cursor count left intact");
    hiddenClient(w, 2);
    setDisplayCount(-1);
    cursor(w, HTLEFT);
    ShowCursor(FALSE); // A further application hide while over the frame.
    cursor(w, HTLEFT);
    check(displayCount() == 0, "frame accounts for additional application hide");
    hiddenClient(w, -2);
    setDisplayCount(-1);
    const UINT cleanup[] = {
        WM_NCMOUSELEAVE, WM_CANCELMODE, WM_ACTIVATEAPP, WM_ACTIVATE, WM_EXITSIZEMOVE};
    for (const auto message : cleanup) {
        cursor(w, HTBOTTOMRIGHT);
        SendMessageW(w, message, 0, 0);
        check(displayCount() == -1, "leave/cancel/deactivate/drag end restores game cursor count");
    }
    cursor(w, HTTOPRIGHT);
    SendMessageW(w, WM_ENTERSIZEMOVE, 0, 0);
    SendMessageW(w, WM_NCMOUSELEAVE, 0, 0);
    cursor(w, HTCLIENT);
    check(GetCursor() == LoadCursorW(nullptr, IDC_SIZENESW) && displayCount() == 0,
          "native arrow remains visible through captured resize drag");
    SendMessageW(w, WM_EXITSIZEMOVE, 0, 0);
    hiddenClient(w);
    cursor(w, HTRIGHT);
    draw->Release();
    DestroyWindow(w);
    check(displayCount() == -1, "window destruction releases frame visibility contribution");
    setDisplayCount(initialCount);
    SetCursor(initialShape);
    check(stop && stop(nullptr) == 0, "clean lifetime");
    std::puts(
        "PASS: all eight resize arrows, hidden/visible cursor counts, hover, drag, cancellation and cleanup");
}
