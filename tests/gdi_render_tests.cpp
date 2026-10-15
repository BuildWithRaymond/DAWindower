#include "dawindower.h"
#include "window_size.h"
#include <ddraw.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
HDC gameDC = nullptr;
unsigned paintCalls = 0;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", what, GetLastError());
        std::exit(1);
    }
}

void success(HRESULT result, const char* what) {
    if (FAILED(result)) {
        std::fprintf(stderr, "FAIL: %s (HRESULT=%08lx)\n", what, result);
        std::exit(1);
    }
}

void dispatchPresentation() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    GdiFlush();
}

LRESULT CALLBACK proc(HWND window, UINT message, WPARAM a, LPARAM b) {
    if (message == WM_PAINT && gameDC) {
        ++paintCalls;
        PAINTSTRUCT paint{};
        auto dc = BeginPaint(window, &paint);
        check(BitBlt(dc, 0, 0, 640, 480, gameDC, 0, 0, SRCCOPY) != FALSE,
              "game paint handler draws frame");
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcW(window, message, a, b);
}
} // namespace

int main(int argc, char** argv) {
    bool proxy = false, readbackOnly = false, loadOnly = false;
    for (int index = 1; index < argc; ++index) {
        proxy |= std::strcmp(argv[index], "--proxy") == 0;
        readbackOnly |= std::strcmp(argv[index], "--ddraw-readback") == 0;
        loadOnly |= std::strcmp(argv[index], "--load-only") == 0;
    }
    auto module = LoadLibraryW(proxy ? L"ddraw.dll" : DAW_TEST_DLL_W);
    check(module != nullptr, "load component");
    auto init = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerInitialize"));
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    check(stop && (proxy || init), "load lifecycle exports");
    if (!proxy && !loadOnly)
        check(init(nullptr) == 0, "initialize imports");
    WNDCLASSW cls{};
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpfnWndProc = proc;
    cls.lpszClassName = L"DAWindowerGdiRenderTest";
    check(RegisterClassW(&cls) != 0, "register window");
    auto window = CreateWindowW(cls.lpszClassName,
                                L"Windowed GDI rendering regression",
                                WS_OVERLAPPEDWINDOW,
                                100,
                                80,
                                660,
                                520,
                                nullptr,
                                nullptr,
                                cls.hInstance,
                                nullptr);
    check(window != nullptr, "create window");
    IDirectDraw* draw = nullptr;
    success(DirectDrawCreate(nullptr, &draw, nullptr), "create draw");
    success(draw->SetCooperativeLevel(window, DDSCL_NORMAL), "windowed renderer");
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    IDirectDrawSurface* primary = nullptr;
    success(draw->CreateSurface(&desc, &primary, nullptr), "unused windowed primary");
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.dwWidth = 640;
    desc.dwHeight = 480;
    IDirectDrawSurface* gameSurface = nullptr;
    success(draw->CreateSurface(&desc, &gameSurface, nullptr), "retail game render surface");
    success(gameSurface->GetDC(&gameDC), "retail DirectDraw game DC");
    auto green = CreateSolidBrush(RGB(0, 255, 0));
    RECT game{0, 0, 640, 480};
    FillRect(gameDC, &game, green);
    DeleteObject(green);
    // Retail may keep a DC acquired at its original size. Read the actual
    // presented window rather than relying only on WM_PRINTCLIENT snapshots.
    auto windowDC = GetDC(window);
    RECT base{}, baseClient{};
    GetWindowRect(window, &base);
    GetClientRect(window, &baseClient);
    const int frameWidth = base.right - base.left - baseClient.right;
    const int frameHeight = base.bottom - base.top - baseClient.bottom;
    SendMessageW(window, WM_ENTERSIZEMOVE, 0, 0);
    for (unsigned edge = WMSZ_LEFT; edge <= WMSZ_BOTTOMRIGHT; ++edge) {
        auto proposed = base;
        const bool left = edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT;
        const bool right = edge == WMSZ_RIGHT || edge == WMSZ_TOPRIGHT || edge == WMSZ_BOTTOMRIGHT;
        const bool top = edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT;
        const bool bottom =
            edge == WMSZ_BOTTOM || edge == WMSZ_BOTTOMLEFT || edge == WMSZ_BOTTOMRIGHT;
        if (left)
            proposed.left -= 160;
        if (right)
            proposed.right += 160;
        if (top)
            proposed.top -= 120;
        if (bottom)
            proposed.bottom += 120;
        SendMessageW(window, WM_SIZING, edge, reinterpret_cast<LPARAM>(&proposed));
        const auto width = proposed.right - proposed.left - frameWidth;
        const auto height = proposed.bottom - proposed.top - frameHeight;
        check(std::llabs(3LL * width - 4LL * height) <= 2,
              "native edge and corner sizes keep nearest-pixel aspect");
        check((!(left || right) || width == baseClient.right + 160) &&
                  (!(top || bottom) || height == baseClient.bottom + 120),
              "requested drag axis drives coupled dimensions");
        if (left)
            check(proposed.right == base.right, "left drag anchors opposite right edge");
        if (right)
            check(proposed.left == base.left, "right drag anchors opposite left edge");
        if (top)
            check(proposed.bottom == base.bottom, "top drag anchors opposite bottom edge");
        if (bottom)
            check(proposed.top == base.top, "bottom drag anchors opposite top edge");
    }
    SendMessageW(window, WM_EXITSIZEMOVE, 0, 0);
    auto dragged = base;
    dragged.top -= 120;
    dragged.right += 160;
    SendMessageW(window, WM_SIZING, WMSZ_TOPRIGHT, reinterpret_cast<LPARAM>(&dragged));
    check(SetWindowPos(window,
                       nullptr,
                       dragged.left,
                       dragged.top,
                       dragged.right - dragged.left,
                       dragged.bottom - dragged.top,
                       SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
          "apply upward corner resize");
    RECT grown{};
    GetClientRect(window, &grown);
    check(grown.right == baseClient.right + 160 && grown.bottom == baseClient.bottom + 120,
          "upward corner resize grows actual game client");
    RECT outer{0, 0, 1280, 960};
    adjustForClient(window, outer);
    check(SetWindowPos(window,
                       nullptr,
                       100,
                       80,
                       outer.right - outer.left,
                       outer.bottom - outer.top,
                       SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
          "resize window");
    ShowWindow(window, SW_SHOWNOACTIVATE);
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    dispatchPresentation();
    check(BitBlt(windowDC, 0, 0, 640, 480, gameDC, 0, 0, SRCCOPY) != FALSE,
          "retail GDI frame presentation");
    dispatchPresentation();
    const auto displayed = GetPixel(windowDC, 1050, 850);
    if (displayed != RGB(0, 255, 0))
        std::fprintf(stderr, "Actual window pixel = %08lx\n", displayed);
    check(displayed == RGB(0, 255, 0), "actual window output enlarges with a retained startup DC");
    auto snapshot = CreateCompatibleDC(windowDC);
    auto bitmap = CreateCompatibleBitmap(windowDC, 1280, 960);
    auto previous = SelectObject(snapshot, bitmap);
    SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(snapshot), PRF_CLIENT);
    if (!readbackOnly)
        check(GetPixel(snapshot, 1050, 850) == RGB(0, 255, 0),
              "GDI game frame scales across resized viewport");
    DDSURFACEDESC primaryPixels{};
    primaryPixels.dwSize = sizeof(primaryPixels);
    success(primary->Lock(nullptr, &primaryPixels, DDLOCK_READONLY | DDLOCK_WAIT, nullptr),
            "DirectDraw reads GDI primary frame");
    const auto pixel = *reinterpret_cast<const WORD*>(
        static_cast<const BYTE*>(primaryPixels.lpSurface) + 260 * primaryPixels.lPitch + 340 * 2);
    success(primary->Unlock(primaryPixels.lpSurface), "release synchronized primary read");
    check(pixel == 0x07e0, "DirectDraw cursor background sees GDI frame instead of black primary");
    desc.dwWidth = 32;
    desc.dwHeight = 32;
    IDirectDrawSurface* cursorSurface = nullptr;
    success(draw->CreateSurface(&desc, &cursorSurface, nullptr), "retail cursor save surface");
    HDC cursorDC = nullptr;
    success(cursorSurface->GetDC(&cursorDC), "retail cursor save DC");
    check(BitBlt(cursorDC, 0, 0, 32, 32, windowDC, 310, 230, SRCCOPY) != FALSE,
          "save logical cursor background");
    check(GetPixel(cursorDC, 30, 30) == RGB(0, 255, 0),
          "cursor readback uses logical frame instead of window pixels");
    RECT glyph{8, 8, 24, 24};
    FillRect(cursorDC, &glyph, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    check(BitBlt(windowDC, 310, 230, 32, 32, cursorDC, 0, 0, SRCCOPY) != FALSE,
          "present cursor with saved background");
    dispatchPresentation();
    SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(snapshot), PRF_CLIENT);
    check(GetPixel(snapshot, 620, 460) == RGB(0, 255, 0),
          "cursor border preserves game background");
    check(GetPixel(snapshot, 640, 480) == RGB(255, 255, 255),
          "GDI cursor follows presentation scaling");
    // A game frame clears the old cursor, then draws its replacement. The
    // desktop must retain the last completed frame until that transaction ends.
    check(BitBlt(windowDC, 0, 0, 640, 480, gameDC, 0, 0, SRCCOPY) != FALSE,
          "redraw beneath cursor");
    GdiFlush();
    check(GetPixel(windowDC, 640, 480) == RGB(255, 255, 255),
          "cursor erase intermediate frame is not presented");
    check(BitBlt(windowDC, 350, 230, 32, 32, cursorDC, 0, 0, SRCCOPY) != FALSE,
          "draw replacement cursor");
    dispatchPresentation();
    check(GetPixel(windowDC, 640, 480) == RGB(0, 255, 0),
          "completed frame restores old cursor background");
    check(GetPixel(windowDC, 720, 480) == RGB(255, 255, 255),
          "completed frame displays replacement cursor");
    InvalidateRect(window, nullptr, FALSE);
    SendMessageW(window, WM_PAINT, 0, 0);
    GdiFlush();
    check(GetPixel(windowDC, 720, 480) == RGB(255, 255, 255),
          "host paint does not expose cursor-erasure intermediate");
    check(BitBlt(windowDC, 350, 230, 32, 32, cursorDC, 0, 0, SRCCOPY) != FALSE,
          "finish cursor after host paint");
    dispatchPresentation();
    RECT sourcePixel{300, 100, 301, 101};
    FillRect(gameDC, &sourcePixel, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    check(BitBlt(windowDC, 0, 0, 640, 480, gameDC, 0, 0, SRCCOPY) != FALSE, "pixel-art frame");
    dispatchPresentation();
    check(GetPixel(windowDC, 600, 200) == RGB(255, 255, 255) &&
              GetPixel(windowDC, 601, 201) == RGB(255, 255, 255) &&
              GetPixel(windowDC, 599, 200) == RGB(0, 255, 0) &&
              GetPixel(windowDC, 602, 201) == RGB(0, 255, 0),
          "nearest neighbor makes exact two-pixel blocks");
    outer = {0, 0, 960, 960};
    adjustForClient(window, outer);
    check(SetWindowPos(window,
                       nullptr,
                       100,
                       0,
                       outer.right - outer.left,
                       outer.bottom - outer.top,
                       SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
          "resize to non-4:3 client");
    dispatchPresentation();
    RECT fitted{};
    GetClientRect(window, &fitted);
    check(fitted.right == 960 && fitted.bottom == 720,
          "programmatic resize fits native aspect inside conflicting dimensions");
    check(GetPixel(windowDC, 0, 0) == RGB(0, 255, 0) &&
              GetPixel(windowDC, 959, 719) == RGB(0, 255, 0) &&
              GetPixel(windowDC, 400, 50) == RGB(0, 255, 0),
          "actual game image fills client without black bars");
    check(GetPixel(windowDC, 450, 150) == RGB(255, 255, 255),
          "native-aspect fractional scaling preserves source pixel position");
    for (int y = 148; y != 155; ++y)
        for (int x = 448; x != 455; ++x) {
            const auto color = GetPixel(windowDC, x, y);
            check(color == RGB(0, 255, 0) || color == RGB(255, 255, 255),
                  "fractional scaling never interpolates pixel colors");
        }
    nativeShowWindow(window, SW_MAXIMIZE);
    dispatchPresentation();
    RECT maximized{};
    GetClientRect(window, &maximized);
    check(IsZoomed(window) != FALSE && maximized.right > 0 && maximized.bottom > 0,
          "native monitor maximize works");
    check(std::llabs(3LL * maximized.right - 4LL * maximized.bottom) <= 2,
          "maximized game client keeps native aspect ratio");
    check(GetPixel(windowDC, 0, 0) == RGB(0, 255, 0) &&
              GetPixel(windowDC, maximized.right - 1, maximized.bottom - 1) == RGB(0, 255, 0),
          "maximize fills client without bars");
    ShowWindow(window, SW_MINIMIZE);
    check(IsIconic(window) != FALSE, "minimize still works");
    nativeShowWindow(window, SW_RESTORE);
    dispatchPresentation();
    GetClientRect(window, &fitted);
    check(IsZoomed(window) != FALSE && fitted.right == maximized.right &&
              fitted.bottom == maximized.bottom,
          "restore from minimized maximized window retains maximized state");
    nativeShowWindow(window, SW_RESTORE);
    dispatchPresentation();
    GetClientRect(window, &fitted);
    check(fitted.right == 960 && fitted.bottom == 720,
          "restore retains native-aspect normal-window dimensions");
    RECT corners{};
    GetWindowRect(window, &corners);

    struct Corner {
        LONG x, y;
        LRESULT hit;
    };

    const Corner grabs[]{{corners.left + 2, corners.top + 2, HTTOPLEFT},
                         {corners.right - 3, corners.top + 2, HTTOPRIGHT},
                         {corners.left + 2, corners.bottom - 3, HTBOTTOMLEFT},
                         {corners.right - 3, corners.bottom - 3, HTBOTTOMRIGHT}};
    for (const auto& grab : grabs)
        check(SendMessageW(window,
                           WM_NCHITTEST,
                           0,
                           MAKELPARAM(static_cast<SHORT>(grab.x), static_cast<SHORT>(grab.y))) ==
                  grab.hit,
              "all four window corners are draggable");
    InvalidateRect(window, nullptr, FALSE);
    SendMessageW(window, WM_PAINT, 0, 0);
    check(paintCalls != 0, "host paint handler continues running");
    success(cursorSurface->ReleaseDC(cursorDC), "release retail cursor DC");
    cursorSurface->Release();
    SelectObject(snapshot, previous);
    DeleteObject(bitmap);
    DeleteDC(snapshot);
    ReleaseDC(window, windowDC);
    success(gameSurface->ReleaseDC(gameDC), "release retail game DC");
    gameDC = nullptr;
    gameSurface->Release();
    primary->Release();
    draw->Release();
    DestroyWindow(window);
    check(stop(nullptr) == 0, "GDI import and object cleanup");
    // Offscreen GDI operations must remain native after shutdown.
    auto dc = CreateCompatibleDC(nullptr);
    auto smallBitmap = CreateBitmap(8, 8, 1, 32, nullptr);
    auto old = SelectObject(dc, smallBitmap);
    check(BitBlt(dc, 0, 0, 8, 8, nullptr, 0, 0, BLACKNESS) != FALSE,
          "native pattern blit after shutdown");
    SelectObject(dc, old);
    DeleteObject(smallBitmap);
    DeleteDC(dc);
    std::puts(
        "PASS: full-client GDI output, native aspect, nearest neighbor, coalesced cursor and native DD DCs");
}
