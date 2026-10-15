#include "dawindower.h"
#include "window_size.h"
#include <ddraw.h>
#include <windowsx.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int mouseX = -1, mouseY = -1;

void check(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", message, GetLastError());
        std::exit(1);
    }
}

void success(HRESULT value, const char* message) {
    if (FAILED(value)) {
        std::fprintf(stderr, "FAIL: %s (HRESULT=%08lx)\n", message, value);
        std::exit(1);
    }
}

LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_MOUSEMOVE) {
        mouseX = GET_X_LPARAM(lparam);
        mouseY = GET_Y_LPARAM(lparam);
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void fill(IDirectDrawSurface* surface, DWORD color) {
    DDBLTFX effects{};
    effects.dwSize = sizeof(effects);
    effects.dwFillColor = color;
    success(surface->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &effects),
            "fill RGB565 surface");
}

void resize(HWND window, int width, int height) {
    RECT outer{0, 0, width, height};
    check(adjustForClient(window, outer), "compute client size");
    check(SetWindowPos(window,
                       nullptr,
                       100,
                       80,
                       outer.right - outer.left,
                       outer.bottom - outer.top,
                       SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
          "resize retail-style window");
}

struct Snapshot {
    HWND window;
    HDC screen, memory;
    HBITMAP bitmap;
    HGDIOBJ previous;

    Snapshot(HWND window, int width, int height)
        : window(window), screen(GetDC(window)), memory(CreateCompatibleDC(screen)),
          bitmap(CreateCompatibleBitmap(screen, width, height)),
          previous(SelectObject(memory, bitmap)) {
        check(screen && memory && bitmap && previous, "create window snapshot");
        refresh();
    }

    ~Snapshot() {
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(window, screen);
    }

    void refresh() {
        SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory), PRF_CLIENT);
    }

    COLORREF pixel(int x, int y) {
        return GetPixel(memory, x, y);
    }
};
} // namespace

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    bool proxy = false, cursorBackgroundOnly = false, loadOnly = false;
    for (int index = 1; index < argc; ++index) {
        proxy |= std::strcmp(argv[index], "--proxy") == 0;
        cursorBackgroundOnly |= std::strcmp(argv[index], "--cursor-background") == 0;
        loadOnly |= std::strcmp(argv[index], "--load-only") == 0;
    }
    auto module = LoadLibraryW(proxy ? L"ddraw.dll" : DAW_TEST_DLL_W);
    check(module != nullptr, "load compatibility component");
    auto init = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerInitialize"));
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    check(stop && (proxy || init), "find lifecycle exports");
    if (!proxy && !loadOnly)
        check(init(nullptr) == ERROR_SUCCESS, "initialize EXE import hook");
    WNDCLASSW cls{};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"DAWindowerRetailRenderTest";
    check(RegisterClassW(&cls) != 0, "register test window");
    auto window = CreateWindowW(cls.lpszClassName,
                                L"Retail rendering regression",
                                WS_OVERLAPPEDWINDOW,
                                100,
                                80,
                                660,
                                520,
                                nullptr,
                                nullptr,
                                cls.hInstance,
                                nullptr);
    check(window != nullptr, "create test window");
    IDirectDraw* draw = nullptr;
    success(DirectDrawCreate(nullptr, &draw, nullptr), "create DirectDraw v1");
    success(draw->SetCooperativeLevel(window, DDSCL_NORMAL), "retail windowed cooperative level");
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    IDirectDrawSurface* primary = nullptr;
    success(draw->CreateSurface(&desc, &primary, nullptr), "retail primary without flip chain");
    IDirectDrawClipper* clipper = nullptr;
    success(draw->CreateClipper(0, &clipper, nullptr), "create HWND clipper");
    success(clipper->SetHWnd(0, window), "bind clipper");
    success(primary->SetClipper(clipper), "attach primary clipper");
    clipper->Release();
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.dwWidth = 640;
    desc.dwHeight = 480;
    IDirectDrawSurface* frame = nullptr;
    success(draw->CreateSurface(&desc, &frame, nullptr), "create fixed retail frame");
    fill(frame, 0x07e0);
    resize(window, 1280, 960);
    POINT origin{};
    check(ClientToScreen(window, &origin) != FALSE, "find primary screen origin");
    success(primary->BltFast(static_cast<DWORD>(origin.x),
                             static_cast<DWORD>(origin.y),
                             frame,
                             nullptr,
                             DDBLTFAST_WAIT),
            "present fixed frame at screen origin");
    if (!cursorBackgroundOnly) {
        Snapshot image(window, 1280, 960);
        check(image.pixel(1050, 850) == RGB(0, 255, 0),
              "windowed fixed frame fills resized game viewport");
    }
    // Saving a cursor background must read logical frame pixels, including the
    // lower-right portion that the old windowed BltFast left black after resize.
    desc.dwWidth = 32;
    desc.dwHeight = 32;
    IDirectDrawSurface* background = nullptr;
    success(draw->CreateSurface(&desc, &background, nullptr), "create saved cursor background");
    RECT cursorArea{310, 230, 342, 262};
    success(background->BltFast(0, 0, primary, &cursorArea, DDBLTFAST_WAIT),
            "save cursor background from primary");
    HDC backgroundDC = nullptr;
    success(background->GetDC(&backgroundDC), "inspect saved cursor background");
    const auto backgroundPixel = GetPixel(backgroundDC, 30, 30);
    success(background->ReleaseDC(backgroundDC), "release saved background DC");
    check(backgroundPixel == RGB(0, 255, 0),
          "saved cursor background contains game pixels instead of black");
    // The retail client sets key zero and draws sprites with flags 0x11.
    desc.dwWidth = 32;
    desc.dwHeight = 32;
    IDirectDrawSurface* cursor = nullptr;
    success(draw->CreateSurface(&desc, &cursor, nullptr), "create software cursor sprite");
    fill(cursor, 0);
    HDC cursorDC = nullptr;
    success(cursor->GetDC(&cursorDC), "draw software cursor glyph");
    RECT glyph{8, 8, 24, 24};
    FillRect(cursorDC, &glyph, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
    success(cursor->ReleaseDC(cursorDC), "release cursor DC");
    DDCOLORKEY transparent{0, 0};
    success(cursor->SetColorKey(DDCKEY_SRCBLT, &transparent), "set retail cursor transparency");
    success(background->BltFast(0, 0, cursor, nullptr, DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY),
            "compose cursor over its saved background");
    success(primary->BltFast(static_cast<DWORD>(origin.x + 310),
                             static_cast<DWORD>(origin.y + 230),
                             background,
                             nullptr,
                             DDBLTFAST_WAIT),
            "present composed software cursor");
    success(primary->BltFast(static_cast<DWORD>(origin.x + 310),
                             static_cast<DWORD>(origin.y + 230),
                             cursor,
                             nullptr,
                             DDBLTFAST_WAIT | DDBLTFAST_SRCCOLORKEY),
            "draw keyed cursor on virtual primary");
    {
        Snapshot image(window, 1280, 960);
        check(image.pixel(620, 460) == RGB(0, 255, 0), "cursor black border remains transparent");
        check(image.pixel(640, 480) == RGB(255, 255, 255), "cursor glyph follows frame scale");
    }
    resize(window, 1280, 720);
    origin = {};
    check(ClientToScreen(window, &origin) != FALSE, "find resized primary origin");
    fill(frame, 0x001f);
    success(primary->BltFast(static_cast<DWORD>(origin.x),
                             static_cast<DWORD>(origin.y),
                             frame,
                             nullptr,
                             DDBLTFAST_WAIT),
            "present after fitting conflicting resize dimensions");
    {
        RECT client{};
        GetClientRect(window, &client);
        check(client.right == 960 && client.bottom == 720, "requested wide box fits native aspect");
        Snapshot image(window, 960, 720);
        check(image.pixel(900, 650) == RGB(0, 0, 255), "fixed frame fills fitted client");
        check(image.pixel(10, 360) == RGB(0, 0, 255), "game image fills former letterbox area");
    }
    SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(480, 360));
    check(mouseX == 320 && mouseY == 240, "mouse coordinates match resized frame");
    background->Release();
    cursor->Release();
    frame->Release();
    primary->Release();
    draw->Release();
    DestroyWindow(window);
    check(stop(nullptr) == ERROR_SUCCESS, "shutdown after retail-style surface cleanup");
    std::puts("PASS: windowed fixed frame scaling, software cursor transparency and input");
}
