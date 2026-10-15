#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include <commctrl.h>
#include <dwmapi.h>
#include "dawindower.h"
#include "window_size.h"
#include "compatibility/imports.h"
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <string>

namespace {
bool ansiHost = false, observeTitle = false;
unsigned setTextCalls = 0, titlePaints = 0;
std::wstring paintedTitle;
decltype(&DrawTextW) nativeDrawText = nullptr;
enum class HostTitle { ordinary, reject, rewrite, destroyOnRead, destroyOnSet };
HostTitle hostTitle = HostTitle::ordinary;
HWND controlledWindow = nullptr;

int WINAPI drawText(HDC dc, LPCWSTR text, int count, LPRECT rectangle, UINT format) {
    if (observeTitle && (format & DT_END_ELLIPSIS)) {
        ++titlePaints;
        paintedTitle.assign(text, count < 0 ? std::wcslen(text) : static_cast<size_t>(count));
    }
    return nativeDrawText(dc, text, count, rectangle, format);
}

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
    if (m == WM_SETTEXT) {
        ++setTextCalls;
        if (w == controlledWindow) {
            if (hostTitle == HostTitle::reject)
                return FALSE;
            if (hostTitle == HostTitle::destroyOnSet) {
                DestroyWindow(w);
                return FALSE;
            }
            if (hostTitle == HostTitle::rewrite)
                return ansiHost ? DefWindowProcA(
                                      w, m, a, reinterpret_cast<LPARAM>("Host accepted title"))
                                : DefWindowProcW(
                                      w, m, a, reinterpret_cast<LPARAM>(L"Host accepted title"));
        }
    }
    if (w == controlledWindow && m == WM_GETTEXT && hostTitle == HostTitle::destroyOnRead) {
        hostTitle = HostTitle::ordinary;
        DestroyWindow(w);
        return 0;
    }
    if (m == WM_SETCURSOR) {
        SetCursor(nullptr); // Host owns its software cursor inside the client.
        return TRUE;
    }
    return ansiHost ? DefWindowProcA(w, m, a, b) : DefWindowProcW(w, m, a, b);
}

HWND createWindow(int index = 0) {
    const auto instance = GetModuleHandleW(nullptr);
    return ansiHost ? CreateWindowA("DAWindowerFrameContract",
                                    "Original client title",
                                    WS_POPUP,
                                    40 + index * 60,
                                    40 + index * 60,
                                    640,
                                    480,
                                    nullptr,
                                    nullptr,
                                    instance,
                                    nullptr)
                    : CreateWindowW(L"DAWindowerFrameContract",
                                    L"Original client title",
                                    WS_POPUP,
                                    40 + index * 60,
                                    40 + index * 60,
                                    640,
                                    480,
                                    nullptr,
                                    nullptr,
                                    instance,
                                    nullptr);
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

void pump(DWORD duration = 100) {
    const auto until = GetTickCount64() + duration;
    do {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            DispatchMessageW(&message);
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    } while (GetTickCount64() < until);
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

void captionText(HWND window, const wchar_t* expected) {
    wchar_t actual[256]{};
    GetWindowTextW(window, actual, 256);
    check(std::wcscmp(actual, expected) == 0, "native window retains the host's accepted title");
    titlePaints = 0;
    paintedTitle.clear();
    observeTitle = true;
    framePixel(window, {});
    observeTitle = false;
    check(titlePaints == 1 && paintedTitle == expected,
          "painted caption matches the host's accepted window title");
}

void externalTitle(HWND window, bool ansi) {
    wchar_t executable[32768]{}, command[33000]{};
    check(GetModuleFileNameW(nullptr, executable, 32768) != 0, "title setter fixture path");
    swprintf_s(command,
               L"\"%s\" --external-title %llu %s",
               executable,
               static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(window)),
               ansi ? L"a" : L"w");
    STARTUPINFOW startup{sizeof(startup)};
    PROCESS_INFORMATION child{};
    check(CreateProcessW(executable,
                         command,
                         nullptr,
                         nullptr,
                         FALSE,
                         CREATE_NO_WINDOW,
                         nullptr,
                         nullptr,
                         &startup,
                         &child) != FALSE,
          "start separate-process title setter");
    CloseHandle(child.hThread);
    const auto previousMessages = setTextCalls;
    titlePaints = 0;
    paintedTitle.clear();
    observeTitle = true;
    const auto deadline = GetTickCount64() + 10000;
    while (WaitForSingleObject(child.hProcess, 0) != WAIT_OBJECT_0 && GetTickCount64() < deadline)
        pump();
    DWORD code = STILL_ACTIVE;
    GetExitCodeProcess(child.hProcess, &code);
    CloseHandle(child.hProcess);
    check(code == 0, "external title setter completed successfully");
    const auto expected = ansi ? L"External ANSI title" : L"External Unicode title";
    const auto repaintDeadline = GetTickCount64() + 2500;
    while (paintedTitle != expected && GetTickCount64() < repaintDeadline)
        pump();
    observeTitle = false;
    std::printf("External %s setter delivered %u WM_SETTEXT message(s).\n",
                ansi ? "ANSI" : "Unicode",
                setTextCalls - previousMessages);
    check(titlePaints > 0 && paintedTitle == expected,
          "external title automatically repaints even without WM_SETTEXT");
    captionText(window, expected);
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
    if (argc == 4 && std::strcmp(argv[1], "--external-title") == 0) {
        const auto window =
            reinterpret_cast<HWND>(static_cast<UINT_PTR>(std::strtoull(argv[2], nullptr, 10)));
        return (std::strcmp(argv[3], "a") == 0 ? SetWindowTextA(window, "External ANSI title")
                                               : SetWindowTextW(window, L"External Unicode title"))
                   ? 0
                   : 1;
    }
    ansiHost = argc > 2 && std::strcmp(argv[2], "--ansi") == 0;
    const char* dll = argc > 1 ? argv[1] : DAW_TEST_DLL_A;
    auto module = LoadLibraryA(dll);
    check(module != nullptr, "LoadLibrary-only initialization");
    nativeDrawText = reinterpret_cast<decltype(nativeDrawText)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "DrawTextW"));
    std::vector<daw::ImportPatch> patches;
    check(daw::patchImport(module,
                           "user32.dll",
                           "DrawTextW",
                           reinterpret_cast<void*>(nativeDrawText),
                           reinterpret_cast<void*>(drawText),
                           patches) == ERROR_SUCCESS,
          "observe actual caption text while retaining native drawing");
    WNDCLASSW c{};
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"DAWindowerFrameContract";
    c.lpfnWndProc = proc;
    if (ansiHost) {
        WNDCLASSA ansi{};
        ansi.hInstance = c.hInstance;
        ansi.lpszClassName = "DAWindowerFrameContract";
        ansi.lpfnWndProc = proc;
        check(RegisterClassA(&ansi) != 0, "register ANSI host fixture");
    } else
        check(RegisterClassW(&c) != 0, "register Unicode host fixture");
    HWND windows[2]{};
    IDirectDraw* draws[2]{};
    for (int i = 0; i < 2; ++i) {
        windows[i] = createWindow(i);
        check(windows[i] != nullptr, "create independent fixture window");
        check((IsWindowUnicode(windows[i]) != FALSE) != ansiHost,
              "fixture starts with its requested native encoding");
        const auto previousTitleWrites = setTextCalls;
        check(SUCCEEDED(DirectDrawCreate(nullptr, &draws[i], nullptr)), "factory hook");
        check(SUCCEEDED(draws[i]->SetCooperativeLevel(windows[i], DDSCL_NORMAL)), "attach frame");
        check(setTextCalls == previousTitleWrites,
              "caption attachment never replaces supplied window text");
        captionText(windows[i], L"Original client title");
    }
    TooltipSearch tooltip{windows[0]};
    EnumThreadWindows(GetCurrentThreadId(), findTooltip, reinterpret_cast<LPARAM>(&tooltip));
    check(tooltip.found != nullptr, "shared caption creates its own native tooltip control");
    check(SendMessageW(tooltip.found, TTM_GETTOOLCOUNT, 0, 0) == 1,
          "X tooltip has a registered tracking tool");
#ifdef DAW_X
    tooltipText(windows[0], tooltip.found, 4, L"Open another Dark Ages client");
    tooltipText(windows[0], tooltip.found, 3, L"Reset to native size (640 \u00d7 480)");
#endif
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
    check((GetWindowLongW(windows[0], GWL_STYLE) & WS_CAPTION) == 0,
          "shared frame owns caption layout without a native caption underneath");
    BOOL nativeRendering = TRUE;
    check(SUCCEEDED(DwmGetWindowAttribute(
              windows[0], DWMWA_NCRENDERING_ENABLED, &nativeRendering, sizeof(nativeRendering))) &&
              !nativeRendering,
          "Windows caption rendering is disabled beneath the shared gradient");
    check(SetWindowTextA(windows[0], "Dark Ages - Alpha") != FALSE, "ANSI title setter succeeds");
    captionText(windows[0], L"Dark Ages - Alpha");
    check(SetWindowTextW(windows[0], L"Dark Ages - Beta") != FALSE,
          "Unicode title setter succeeds");
    captionText(windows[0], L"Dark Ages - Beta");
    if (!ansiHost) {
        check(SetWindowTextW(windows[0], L"Dark Ages \u00b7 \u6f22\u5b57") != FALSE,
              "Unicode host accepts non-ASCII title");
        captionText(windows[0], L"Dark Ages \u00b7 \u6f22\u5b57");
    }
    check(SetWindowTextW(windows[0], L"Persistent host title") != FALSE,
          "set host-controlled title");
    const auto previousTitleWrites = setTextCalls;
    pump(1400);
    check(setTextCalls == previousTitleWrites, "idle caption never rewrites the host title");
    captionText(windows[0], L"Persistent host title");
    captionText(windows[1], L"Original client title");
    externalTitle(windows[0], true);
    externalTitle(windows[0], false);
    check(SetWindowTextW(windows[0], L"Persistent host title") != FALSE,
          "restore title before host rejection checks");
    controlledWindow = windows[0];
    hostTitle = HostTitle::reject;
    // USER32 can report successful delivery even when the host rejects the
    // request. Its accepted native text and the painted text are the contract.
    SetWindowTextA(windows[0], "Rejected title");
    captionText(windows[0], L"Persistent host title");
    hostTitle = HostTitle::rewrite;
    check(SetWindowTextW(windows[0], L"Requested title") != FALSE, "host rewrites requested title");
    captionText(windows[0], L"Host accepted title");
    hostTitle = HostTitle::ordinary;
    check(SetWindowTextW(windows[0], nullptr) != FALSE, "host clears title with null text");
    captionText(windows[0], L"");
    controlledWindow = nullptr;
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

#ifdef DAW_X
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
#else
    resize(windows[0], 901, 676);
    auto maximize = button(windows[0], 1);
    press(windows[0], maximize);
    check(GetCapture() == windows[0], "standard title button captures press");
    SendMessageW(windows[0], WM_CANCELMODE, 0, 0);
    check(GetCapture() != windows[0] && !IsZoomed(windows[0]),
          "cancelled standard click releases capture without maximizing");
    press(windows[0], maximize);
    POINT elsewhere{100, 100};
    ClientToScreen(windows[0], &elsewhere);
    release(windows[0], elsewhere);
    check(!IsZoomed(windows[0]), "releasing outside standard button cancels action");
    press(windows[0], maximize);
    release(windows[0], maximize);
    pump();
    check(IsZoomed(windows[0]) != FALSE, "standard maximize button works");
    maximize = button(windows[0], 1);
    press(windows[0], maximize);
    release(windows[0], maximize);
    pump();
    check(!IsZoomed(windows[0]), "standard maximize button restores window");
    const auto minimize = button(windows[0], 2);
    press(windows[0], minimize);
    release(windows[0], minimize);
    pump();
    check(IsIconic(windows[0]) != FALSE, "standard minimize button works");
    nativeShowWindow(windows[0], SW_RESTORE);
    pump();
    RECT restored{};
    GetClientRect(windows[0], &restored);
    check(!IsIconic(windows[0]) && restored.right == 901 && restored.bottom == 676,
          "standard restores exact game dimensions after minimize");
    const auto close = button(windows[1], 0);
    press(windows[1], close);
    release(windows[1], close);
    pump();
    check(!IsWindow(windows[1]) && IsWindow(windows[0]),
          "standard close button affects only its own client");
#endif

    const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 100; ++i)
        SendMessageW(windows[0], WM_NCPAINT, 1, 0);
    check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before,
          "repeated frame paint releases temporary GDI objects");

    auto menu = GetSystemMenu(windows[0], FALSE);
#ifdef DAW_X
    check(GetMenuItemCount(menu) >= 9, "extra actions available in Windows system menu");
#else
    check(GetMenuState(menu, 0xe810, MF_BYCOMMAND) == static_cast<UINT>(-1) &&
              GetMenuState(menu, 0xe820, MF_BYCOMMAND) == static_cast<UINT>(-1),
          "standard edition has no X-only client or native-size actions");
#endif
    for (int i = 0; i < 2; ++i) {
        DestroyWindow(windows[i]);
        draws[i]->Release();
    }
    for (const auto action : {HostTitle::destroyOnSet, HostTitle::destroyOnRead}) {
        auto window = createWindow();
        IDirectDraw* draw = nullptr;
        check(window && SUCCEEDED(DirectDrawCreate(nullptr, &draw, nullptr)) &&
                  SUCCEEDED(draw->SetCooperativeLevel(window, DDSCL_NORMAL)),
              "attach reentrant title fixture");
        controlledWindow = window;
        hostTitle = action;
        SetWindowTextW(window, L"Destroy during title update");
        check(!IsWindow(window), "host destruction during title update unwinds safely");
        controlledWindow = nullptr;
        hostTitle = HostTitle::ordinary;
        draw->Release();
    }
    controlledWindow = createWindow();
    hostTitle = HostTitle::destroyOnRead;
    IDirectDraw* draw = nullptr;
    check(controlledWindow && SUCCEEDED(DirectDrawCreate(nullptr, &draw, nullptr)),
          "create initial title-read destruction fixture");
    check(FAILED(draw->SetCooperativeLevel(controlledWindow, DDSCL_NORMAL)) &&
              !IsWindow(controlledWindow),
          "host destruction during initial title read unwinds safely");
    controlledWindow = nullptr;
    hostTitle = HostTitle::ordinary;
    draw->Release();
    check(daw::restoreImports(patches) == ERROR_SUCCESS, "restore caption text observer");
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    check(stop && stop(nullptr) == ERROR_SUCCESS, "shutdown after frame destruction");
    FreeLibrary(module);
    std::puts("Shared caption contract passed.");
}
