#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <commctrl.h>
#include "dawindower.h"
#include "window_size.h"
#include <cstdio>
#include <cstdlib>
#include <cwchar>

namespace {
struct TooltipSearch {
    HWND owner, found = nullptr;
};

BOOL CALLBACK findTooltip(HWND window, LPARAM parameter) {
    auto& search = *reinterpret_cast<TooltipSearch*>(parameter);
    wchar_t name[64]{};
    GetClassNameW(window, name, 64);
    if (GetWindow(window, GW_OWNER) == search.owner && std::wcscmp(name, TOOLTIPS_CLASSW) == 0)
        search.found = window;
    return TRUE;
}

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", what, GetLastError());
        std::exit(1);
    }
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_SETCURSOR) {
        SetCursor(nullptr); // Host owns its software cursor inside the client.
        return TRUE;
    }
    return DefWindowProcW(w, m, a, b);
}

void resize(HWND w, int width, int height) {
    RECT outer{}, client{};
    GetWindowRect(w, &outer);
    GetClientRect(w, &client);
    check(SetWindowPos(w,
                       nullptr,
                       0,
                       0,
                       width + outer.right - outer.left - client.right,
                       height + outer.bottom - outer.top - client.bottom,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
          "set arbitrary game client size");
}

POINT button(HWND w, int fromRight) {
    RECT r{};
    GetWindowRect(w, &r);
    POINT client{};
    ClientToScreen(w, &client);
    const int border = client.x - r.left;
    const int width = MulDiv(36, GetDpiForWindow(w), 96);
    return {r.right - border - (fromRight * width + width / 2),
            r.top + border + (client.y - r.top - border) / 2};
}

void press(HWND w, POINT screen) {
    const auto packed = MAKELPARAM(static_cast<SHORT>(screen.x), static_cast<SHORT>(screen.y));
    const auto hit = SendMessageW(w, WM_NCHITTEST, 0, packed);
    SendMessageW(w, WM_NCLBUTTONDOWN, hit, packed);
}

void release(HWND w, POINT screen) {
    ScreenToClient(w, &screen);
    SendMessageW(
        w, WM_LBUTTONUP, 0, MAKELPARAM(static_cast<SHORT>(screen.x), static_cast<SHORT>(screen.y)));
}

COLORREF framePixel(HWND w, POINT screen) {
    RECT r{};
    GetWindowRect(w, &r);
    auto dc = GetWindowDC(w), memory = CreateCompatibleDC(dc);
    auto bitmap = CreateCompatibleBitmap(dc, r.right - r.left, r.bottom - r.top);
    auto old = SelectObject(memory, bitmap);
    SendMessageW(w, WM_PRINT, reinterpret_cast<WPARAM>(memory), PRF_NONCLIENT | PRF_CLIENT);
    const auto color = GetPixel(memory, screen.x - r.left, screen.y - r.top);
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(w, dc);
    return color;
}

int cursorCount() {
    const auto count = ShowCursor(TRUE);
    ShowCursor(FALSE);
    return count - 1;
}

void tooltipText(HWND window, HWND tooltip, int fromRight, const wchar_t* expected) {
    const auto p = button(window, fromRight);
    SendMessageW(window, WM_NCMOUSEMOVE, HTBORDER, MAKELPARAM(p.x, p.y));
    // This synthetic hover does not move the user's real pointer. Suppress its
    // unrelated physical leave; deliver the tested leave explicitly below.
    TRACKMOUSEEVENT cancel{sizeof(cancel), TME_CANCEL | TME_NONCLIENT | TME_LEAVE, window, 0};
    TrackMouseEvent(&cancel);
    MSG physicalLeave{};
    unsigned discarded = 0;
    while (PeekMessageW(&physicalLeave, window, WM_NCMOUSELEAVE, WM_NCMOUSELEAVE, PM_REMOVE))
        ++discarded;
    std::printf("Synthetic hover: removed %u physical-pointer leave message(s).\n", discarded);
    const auto deadline = GetTickCount64() + 2000;
    while (!IsWindowVisible(tooltip) && GetTickCount64() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            DispatchMessageW(&message);
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    check(IsWindowVisible(tooltip) != FALSE, "hover timer displays native tooltip");
    wchar_t text[512]{};
    TOOLINFOW tool{TTTOOLINFOW_V2_SIZE};
    tool.hwnd = window;
    tool.uId = 1;
    tool.lpszText = text;
    SendMessageW(tooltip, TTM_GETTEXTW, 512, reinterpret_cast<LPARAM>(&tool));
    check(std::wcscmp(text, expected) == 0, "native tooltip displays correct button description");
    SendMessageW(window, WM_NCMOUSELEAVE, 0, 0);
    check(!IsWindowVisible(tooltip), "leaving title bar dismisses tooltip");
}
} // namespace

int main(int argc, char** argv) {
    const char* dll = argc > 1 ? argv[1] : "DAWindower.dll";
    auto module = LoadLibraryA(dll);
    check(module != nullptr, "LoadLibrary-only initialization");
    WNDCLASSW c{};
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"DAWindowerXFrameContract";
    c.lpfnWndProc = proc;
    check(RegisterClassW(&c) != 0, "register fixture");
    HWND windows[2]{};
    IDirectDraw* draws[2]{};
    for (int i = 0; i < 2; ++i) {
        windows[i] = CreateWindowW(c.lpszClassName,
                                   L"Original client title",
                                   WS_POPUP,
                                   40 + i * 60,
                                   40 + i * 60,
                                   640,
                                   480,
                                   nullptr,
                                   nullptr,
                                   c.hInstance,
                                   nullptr);
        check(windows[i] != nullptr, "create independent fixture window");
        check(SUCCEEDED(DirectDrawCreate(nullptr, &draws[i], nullptr)), "factory hook");
        check(SUCCEEDED(draws[i]->SetCooperativeLevel(windows[i], DDSCL_NORMAL)), "attach frame");
    }
    TooltipSearch tooltip{windows[0]};
    EnumThreadWindows(GetCurrentThreadId(), findTooltip, reinterpret_cast<LPARAM>(&tooltip));
    check(tooltip.found != nullptr, "X registers and creates its own native tooltip control");
    check(SendMessageW(tooltip.found, TTM_GETTOOLCOUNT, 0, 0) == 1,
          "X tooltip has a registered tracking tool");
    tooltipText(windows[0], tooltip.found, 4, L"Open another Dark Ages client");
    tooltipText(windows[0], tooltip.found, 3, L"Reset to native size (640 \u00d7 480)");
    tooltipText(windows[0], tooltip.found, 2, L"Minimize");
    tooltipText(windows[0], tooltip.found, 1, L"Maximize");
    tooltipText(windows[0], tooltip.found, 0, L"Close");
    RECT outer{}, client{};
    GetWindowRect(windows[0], &outer);
    GetClientRect(windows[0], &client);
    POINT origin{};
    ClientToScreen(windows[0], &origin);
    check(origin.x == outer.left && origin.y + client.bottom == outer.bottom &&
              client.right == outer.right - outer.left,
          "game client reaches both sides and bottom without an outer border");
    wchar_t title[256]{};
    GetWindowTextW(windows[0], title, 256);
    check(std::wcscmp(title, L"Dark Ages") == 0,
          "X title safely falls back for an unknown executable");
    auto center = button(windows[0], 2);
    center.x -= 160;
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    if (contrast.dwFlags & HCF_HIGHCONTRASTON) {
        check(framePixel(windows[0], center) == GetSysColor(COLOR_WINDOW),
              "high-contrast caption follows system colors");
    } else {
        auto upper = center, lower = center;
        upper.y = outer.top + 1;
        lower.y = origin.y - 2;
        const auto light = framePixel(windows[0], upper);
        const auto dark = framePixel(windows[0], lower);
        check(GetRValue(light) > GetRValue(dark) + 10 && GetRValue(light) < 60 &&
                  GetGValue(light) < 60 && GetBValue(light) < 60,
              "caption has a restrained dark-to-black vertical gradient");
    }
    const int initialCursorCount = cursorCount();
    while (cursorCount() > -1)
        ShowCursor(FALSE);
    while (cursorCount() < -1)
        ShowCursor(TRUE);
    SendMessageW(windows[0],
                 WM_SETCURSOR,
                 reinterpret_cast<WPARAM>(windows[0]),
                 MAKELPARAM(HTCAPTION, WM_MOUSEMOVE));
    check(GetCursor() == LoadCursorW(nullptr, IDC_ARROW) && cursorCount() == 0,
          "normal title-bar pointer visible despite game's hidden OS cursor");
    SendMessageW(windows[0],
                 WM_SETCURSOR,
                 reinterpret_cast<WPARAM>(windows[0]),
                 MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    check(GetCursor() == nullptr && cursorCount() == -1,
          "game cursor and exact visibility count restored inside client");
    while (cursorCount() > initialCursorCount)
        ShowCursor(FALSE);
    while (cursorCount() < initialCursorCount)
        ShowCursor(TRUE);

    resize(windows[0], 901, 676);
    resize(windows[1], 733, 550);
    auto native = button(windows[0], 3);
    press(windows[0], native);
    check(GetCapture() == windows[0], "title button captures drag/release");
    POINT elsewhere{0, 0};
    ClientToScreen(windows[0], &elsewhere);
    elsewhere.x += 200;
    elsewhere.y += 200;
    release(windows[0], elsewhere);
    RECT r{};
    GetClientRect(windows[0], &r);
    check(r.right == 901 && r.bottom == 676, "release outside cancels native-size action");
    press(windows[0], native);
    release(windows[0], native);
    GetClientRect(windows[0], &r);
    check(r.right == 640 && r.bottom == 480, "native-size button restores exactly 640x480");
    GetClientRect(windows[1], &r);
    check(r.right == 733 && r.bottom == 550, "native-size action leaves other clients untouched");
    nativeShowWindow(windows[0], SW_MAXIMIZE);
    check(IsZoomed(windows[0]) != FALSE, "fixture maximized");
    native = button(windows[0], 3);
    press(windows[0], native);
    release(windows[0], native);
    GetClientRect(windows[0], &r);
    check(!IsZoomed(windows[0]) && r.right == 640 && r.bottom == 480,
          "native action restores maximized window before measuring exact client size");

    const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 100; ++i)
        SendMessageW(windows[0], WM_NCPAINT, 1, 0);
    check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before,
          "repeated frame paint releases temporary GDI objects");

    auto menu = GetSystemMenu(windows[0], FALSE);
    check(GetMenuItemCount(menu) >= 9, "extra actions available in Windows system menu");
    for (int i = 0; i < 2; ++i) {
        DestroyWindow(windows[i]);
        draws[i]->Release();
    }
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    check(stop && stop(nullptr) == ERROR_SUCCESS, "shutdown after frame destruction");
    FreeLibrary(module);
    std::puts("X frame contract passed.");
}
