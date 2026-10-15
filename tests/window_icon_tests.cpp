#include <windows.h>
#include <shellapi.h>
#include <ddraw.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>

namespace {
HWND destroyOnIconQuery = nullptr;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", what, GetLastError());
        std::exit(1);
    }
}

LRESULT CALLBACK proc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_GETICON && w == destroyOnIconQuery) {
        destroyOnIconQuery = nullptr;
        DestroyWindow(w);
        return 0;
    }
    return DefWindowProcW(w, m, a, b);
}

HWND create(const wchar_t* name) {
    auto w = CreateWindowW(name,
                           L"Game",
                           WS_POPUP,
                           40,
                           40,
                           640,
                           480,
                           nullptr,
                           nullptr,
                           GetModuleHandleW(nullptr),
                           nullptr);
    check(w != nullptr, "create icon fixture window");
    return w;
}

IDirectDraw* attach(HWND w) {
    IDirectDraw* draw = nullptr;
    check(SUCCEEDED(DirectDrawCreate(nullptr, &draw, nullptr)), "LoadLibrary-only factory");
    check(SUCCEEDED(draw->SetCooperativeLevel(w, DDSCL_NORMAL)), "attach icon fixture");
    return draw;
}

HICON windowIcon(HWND w, WPARAM size) {
    return reinterpret_cast<HICON>(SendMessageW(w, WM_GETICON, size, 0));
}

COLORREF iconColor(HICON icon) {
    auto dc = CreateCompatibleDC(nullptr);
    auto bitmap = CreateBitmap(32, 32, 1, 32, nullptr);
    auto previous = SelectObject(dc, bitmap);
    check(DrawIconEx(dc, 0, 0, icon, 32, 32, 0, nullptr, DI_NORMAL) != FALSE,
          "icon is valid and drawable");
    auto color = GetPixel(dc, 16, 16);
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    return color;
}

#ifndef DAW_X
int captionIconLeft(HWND w, COLORREF expected) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        DwmFlush();
        RECT visible{};
        check(SUCCEEDED(
                  DwmGetWindowAttribute(w, DWMWA_EXTENDED_FRAME_BOUNDS, &visible, sizeof(visible))),
              "visible caption bounds");
        POINT client{};
        ClientToScreen(w, &client);
        auto screen = GetDC(nullptr);
        const auto height = std::max(1L, client.y - visible.top);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), 64, -height, 1, 32, BI_RGB};
        DWORD* pixels = nullptr;
        auto bitmap = CreateDIBSection(
            screen, &info, DIB_RGB_COLORS, reinterpret_cast<void**>(&pixels), nullptr, 0);
        auto memory = CreateCompatibleDC(screen);
        auto previous = SelectObject(memory, bitmap);
        check(bitmap && memory && pixels && previous && previous != HGDI_ERROR,
              "allocate bounded caption snapshot");
        check(BitBlt(memory, 0, 0, 64, height, screen, visible.left, visible.top, SRCCOPY) != FALSE,
              "capture caption once per compositor attempt");
        GdiFlush();
        const DWORD color =
            (GetRValue(expected) << 16) | (GetGValue(expected) << 8) | GetBValue(expected);
        int first = 1000;
        for (int y = 4; y < height; ++y)
            for (int x = 0; x < 64; ++x)
                if ((pixels[y * 64 + x] & 0xffffff) == color && x < first)
                    first = x;
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
        if (first != 1000)
            return first;
        Sleep(20); // Bound the compositor wait; never spin on the desktop.
    }
    check(false, "synthetic caption icon appears on the desktop");
    return 0;
}

void captionSpacing(HWND w, HICON icon, COLORREF expected) {
    SetWindowLongW(w, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    SendMessageW(w, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    SendMessageW(w, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    RECT native{0, 0, 640, 480};
    AdjustWindowRectExForDpi(&native, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForWindow(w));
    check(SetWindowPos(w,
                       HWND_TOPMOST,
                       40,
                       40,
                       native.right - native.left,
                       native.bottom - native.top,
                       SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED) != FALSE,
          "show native caption baseline");
    const auto original = captionIconLeft(w, expected);
    auto draw = attach(w);
    const auto attached = captionIconLeft(w, expected);
    std::printf("Caption icon left margin: native=%d attached=%d\n", original, attached);
    check(original > 0 && attached >= MulDiv(6, GetDpiForWindow(w), 96),
          "standard caption keeps native icon padding after frame removal");
    RECT outer{}, client{};
    POINT origin{};
    check(SUCCEEDED(DwmGetWindowAttribute(w, DWMWA_EXTENDED_FRAME_BOUNDS, &outer, sizeof(outer))),
          "native visible frame bounds");
    GetClientRect(w, &client);
    ClientToScreen(w, &origin);
    check(origin.x - outer.left >= 0 && origin.x - outer.left <= 1 &&
              outer.right - origin.x - client.right >= 0 &&
              outer.right - origin.x - client.right <= 1,
          "native caption padding adds no thick visible side frame");
    const auto dpi = GetDpiForWindow(w);
    const auto hit = SendMessageW(
        w,
        WM_NCHITTEST,
        0,
        MAKELPARAM(outer.left + attached + MulDiv(8, dpi, 96), outer.top + MulDiv(15, dpi, 96)));
    check(hit == HTSYSMENU, "padded standard icon retains its native system-menu hit target");
    SetWindowTextW(w, L"Updated game title");
    SendMessageW(w, WM_NCACTIVATE, TRUE, 0);
    check(captionIconLeft(w, expected) == attached,
          "native caption updates and activation preserve icon spacing");
    draw->Release();
    DestroyWindow(w);
}
#endif

#ifdef DAW_X
void paintedIcon(HWND w, COLORREF expected) {
    RECT r{};
    GetWindowRect(w, &r);
    auto dc = CreateCompatibleDC(nullptr);
    auto bitmap = CreateBitmap(r.right - r.left, r.bottom - r.top, 1, 32, nullptr);
    auto previous = SelectObject(dc, bitmap);
    SendMessageW(w, WM_PRINT, reinterpret_cast<WPARAM>(dc), PRF_NONCLIENT);
    const auto dpi = GetDpiForWindow(w);
    const int x = MulDiv(20, dpi, 96), y = MulDiv(18, dpi, 96);
    check(GetPixel(dc, x, y) == expected, "X paints the actual game icon in its caption");
    const auto hit = SendMessageW(w, WM_NCHITTEST, 0, MAKELPARAM(r.left + x, r.top + y));
    check(hit == HTSYSMENU, "X caption icon exposes the native system menu");
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
}
#endif
} // namespace

int main(int argc, char** argv) {
    check(LoadLibraryA(DAW_TEST_DLL_A) != nullptr, "load standalone edition without initializer");
    const auto instance = GetModuleHandleW(nullptr);
    char path[32768]{};
    check(GetModuleFileNameA(instance, path, sizeof(path)) != 0, "fixture executable path");
    auto legacy = ExtractIconA(instance, path, 0);
    check(reinterpret_cast<UINT_PTR>(legacy) > 1, "legacy ExtractIconA finds first EXE icon");
    const auto expected = iconColor(legacy);
    check(expected == RGB(19, 155, 208), "first icon group has the synthetic primary color");
    DestroyIcon(legacy);
    WNDCLASSEXW cls{sizeof(cls)};
    cls.lpfnWndProc = proc;
    cls.hInstance = instance;
    cls.lpszClassName = L"DAWIconContract";
    check(RegisterClassExW(&cls) != 0, "register class without an icon");
#ifndef DAW_X
    if (argc > 1 && std::strcmp(argv[1], "--caption") == 0) {
        auto icon = LoadIconW(instance, MAKEINTRESOURCEW(101));
        captionSpacing(create(cls.lpszClassName), icon, expected);
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif
    auto unrelated = create(cls.lpszClassName);
    auto w = create(cls.lpszClassName);
    check(!windowIcon(w, ICON_BIG) && !windowIcon(w, ICON_SMALL), "fixture starts without icons");
    auto draw = attach(w);
    auto largeIcon = windowIcon(w, ICON_BIG), smallIcon = windowIcon(w, ICON_SMALL);
    check(largeIcon && smallIcon, "both window icon sizes are supplied automatically");
    check(iconColor(largeIcon) == expected && iconColor(smallIcon) == expected,
          "window icons match the game's first ExtractIconA icon");
    check(!windowIcon(unrelated, ICON_BIG) && !GetClassLongPtrW(w, GCLP_HICON),
          "unrelated windows and the shared class are untouched");
#ifdef DAW_X
    paintedIcon(w, expected);
#endif
    draw->Release();
    check(iconColor(smallIcon) == expected, "icon survives DirectDraw release while window lives");
    auto custom = LoadIconW(nullptr, IDI_WARNING);
    SendMessageW(w, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(custom));
    check(windowIcon(w, ICON_SMALL) == custom, "later launcher icon replacement survives");
    DestroyWindow(w);
    DestroyWindow(unrelated);

    w = create(cls.lpszClassName);
    SendMessageW(w, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(custom));
    SendMessageW(w, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(custom));
    draw = attach(w);
    check(windowIcon(w, ICON_BIG) == custom && windowIcon(w, ICON_SMALL) == custom,
          "preexisting launcher icons are preserved");
    draw->Release();
    DestroyWindow(w);

    cls.lpszClassName = L"DAWClassIconContract";
    cls.hIcon = cls.hIconSm = custom;
    check(RegisterClassExW(&cls) != 0, "register host class icons");
    w = create(cls.lpszClassName);
    draw = attach(w);
    check(GetClassLongPtrW(w, GCLP_HICON) == reinterpret_cast<ULONG_PTR>(custom) &&
              !windowIcon(w, ICON_BIG) && !windowIcon(w, ICON_SMALL),
          "host class icons are preserved without per-window overrides");
    draw->Release();
    DestroyWindow(w);

    w = create(L"DAWIconContract");
    check(SUCCEEDED(DirectDrawCreate(nullptr, &draw, nullptr)), "destruction fixture factory");
    destroyOnIconQuery = w;
    check(FAILED(draw->SetCooperativeLevel(w, DDSCL_NORMAL)) && !IsWindow(w),
          "host destruction during icon query unwinds attachment safely");
    draw->Release();

    // Shared executable resources are cached once rather than leaked per window.
    const auto before = GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS);
    for (int i = 0; i < 20; ++i) {
        w = create(L"DAWIconContract");
        draw = attach(w);
        check(iconColor(windowIcon(w, ICON_BIG)) == expected, "repeated icon attachment");
        draw->Release();
        DestroyWindow(w);
    }
    check(GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS) <= before,
          "repeated attachment does not leak USER icon handles");
    std::puts("PASS: first EXE icon, native/X presentation, launcher ownership and lifetime");
}
