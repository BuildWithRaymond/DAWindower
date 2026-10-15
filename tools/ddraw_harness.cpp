#include <windows.h>
#include <windowsx.h>
#include <ddraw.h>
#include "dawindower.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

static int mouseX, mouseY, presses;
static bool closeDuringStyleChange = false;

static void require(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s (Win32=%lu)\n", message, GetLastError());
        std::exit(1);
    }
}

static void hr(HRESULT value, const char* message) {
    if (FAILED(value)) {
        std::fprintf(stderr, "FAIL: %s (HRESULT=%08lx)\n", message, value);
        std::exit(1);
    }
}

static LRESULT CALLBACK windowProc(HWND w, UINT m, WPARAM a, LPARAM b) {
    if (m == WM_STYLECHANGED && closeDuringStyleChange)
        DestroyWindow(w);
    if (m == WM_MOUSEMOVE) {
        mouseX = GET_X_LPARAM(b);
        mouseY = GET_Y_LPARAM(b);
    }
    if (m == WM_LBUTTONDOWN)
        ++presses;
    if (m == WM_DESTROY)
        PostQuitMessage(0);
    return DefWindowProcW(w, m, a, b);
}

static void fill(IDirectDrawSurface* s, DWORD color) {
    DDBLTFX fx{};
    fx.dwSize = sizeof(fx);
    fx.dwFillColor = color;
    hr(s->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx), "color fill");
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    const bool diagnostic = argc > 1 && std::strstr(argv[1], "diagnostic") != nullptr;
    const bool proxy = argc > 1 && std::strstr(argv[1], "--proxy") != nullptr;
    const bool loadOnly = argc > 1 && std::strstr(argv[1], "--load-only") != nullptr;
    const bool renamed = argc > 1 && std::strstr(argv[1], "renamed") != nullptr;
    const bool unloadOnly = argc > 1 && std::strcmp(argv[1], "--dormant-unload") == 0;
    const bool test =
        loadOnly || proxy || diagnostic || (argc > 1 && std::strcmp(argv[1], "--self-test") == 0);
    auto module = LoadLibraryA(proxy ? "ddraw.dll" : renamed ? "dawnd.dll" : DAW_TEST_DLL_A);
    require(module != nullptr, "load DAWindower prototype");
    auto init = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerInitialize"));
    auto stop = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerShutdown"));
    auto stats = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerGetStats"));
    require((proxy || init) && stop && stats, "documented exports");
    if (unloadOnly) {
        require(stop(nullptr) == 0, "shutdown removes dormant bootstrap gates");
        require(FreeLibrary(module) != FALSE, "unload dormant DLL");
        IDirectDraw* native = nullptr;
        hr(DirectDrawCreate(nullptr, &native, nullptr), "native factory after dormant unload");
        IDirectDraw7* v7 = nullptr;
        hr(native->QueryInterface(IID_IDirectDraw7, reinterpret_cast<void**>(&v7)),
           "dormant gates do not dangle after unload");
        v7->Release();
        native->Release();
        std::puts("PASS: dormant shutdown/unload restores factory");
        return 0;
    }
    if (!proxy && !loadOnly) {
        require(init(nullptr) == ERROR_SUCCESS, "controlled initialization");
        require(init(nullptr) == ERROR_SUCCESS, "idempotent initialization");
    }
    WNDCLASSW cls{};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"DAWindowerHarness";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    require(RegisterClassW(&cls) != 0, "register harness window");
    auto window = CreateWindowW(cls.lpszClassName,
                                L"DAWindower DirectDraw harness",
                                WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                660,
                                520,
                                nullptr,
                                nullptr,
                                cls.hInstance,
                                nullptr);
    require(window != nullptr, "create harness window");
    IDirectDraw* draw = nullptr;
    hr(DirectDrawCreate(nullptr, &draw, nullptr), "DirectDrawCreate through EXE import hook");
    hr(draw->SetCooperativeLevel(window, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN),
       "virtual exclusive mode");
    hr(draw->SetDisplayMode(640, 480, 16), "virtual 640x480 RGB565");
    auto singleton = CreateMutexA(nullptr, FALSE, "Nexon.SingleInstance");
    const auto singletonError = GetLastError();
    require(singleton != nullptr && singletonError != ERROR_ALREADY_EXISTS,
            "client singleton is isolated per process");
    auto sameSingleton = CreateMutexA(nullptr, FALSE, "Nexon.SingleInstance");
    require(sameSingleton != nullptr && GetLastError() == ERROR_ALREADY_EXISTS,
            "same-process singleton semantics retained");
    CloseHandle(sameSingleton);
    CloseHandle(singleton);
    char unrelatedName[96]{};
    sprintf_s(unrelatedName, "DAWindower.Harness.%lu", GetCurrentProcessId());
    auto unrelated = CreateMutexA(nullptr, FALSE, unrelatedName);
    auto sameUnrelated = CreateMutexA(nullptr, FALSE, unrelatedName);
    require(unrelated && sameUnrelated && GetLastError() == ERROR_ALREADY_EXISTS,
            "unrelated mutex names unchanged");
    CloseHandle(sameUnrelated);
    CloseHandle(unrelated);
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    desc.dwBackBufferCount = 1;
    IDirectDrawSurface* primary = nullptr;
    hr(draw->CreateSurface(&desc, &primary, nullptr), "virtual flip primary");
    DDSCAPS caps{};
    caps.dwCaps = DDSCAPS_BACKBUFFER;
    IDirectDrawSurface* back = nullptr;
    hr(primary->GetAttachedSurface(&caps, &back), "attached backbuffer");
    fill(back, 0x07e0);
    hr(primary->Flip(nullptr, DDFLIP_WAIT), "present green frame");
    if (test) {
        DDSURFACEDESC mode{};
        mode.dwSize = sizeof(mode);
        hr(draw->GetDisplayMode(&mode), "logical display mode");
        require(mode.dwWidth == 640 && mode.dwHeight == 480 &&
                    mode.ddpfPixelFormat.dwRGBBitCount == 16,
                "logical dimensions and format");
        void* identity = nullptr;
        hr(primary->QueryInterface(IID_IUnknown, &identity), "surface identity");
        require(identity == primary, "IUnknown identity remains wrapped");
        static_cast<IUnknown*>(identity)->Release();
        require(primary->QueryInterface(IID_IDirectDrawSurface7, &identity) == E_NOINTERFACE &&
                    identity == nullptr,
                "unsupported interface cannot escape hooks");
        require(stop(nullptr) == ERROR_BUSY, "shutdown refuses live COM objects");
        DDSURFACEDESC memory{};
        memory.dwSize = sizeof(memory);
        hr(primary->Lock(nullptr, &memory, DDLOCK_WAIT, nullptr), "lock virtual primary");
        require(primary->Flip(nullptr, DDFLIP_WAIT) == DDERR_SURFACEBUSY,
                "flip respects outstanding lock");
        hr(primary->Unlock(memory.lpSurface), "unlock virtual primary");
        const auto cursor = SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        const auto count = ShowCursor(TRUE);
        require(ShowCursor(FALSE) == count - 1, "cursor counter changes exactly once per call");
        require(SetCursor(cursor) != nullptr, "cursor handle round trip");
        RECT r{0, 0, 960, 720};
        RECT previousOuter{}, previousClient{};
        GetWindowRect(window, &previousOuter);
        GetClientRect(window, &previousClient);
        r.right += previousOuter.right - previousOuter.left - previousClient.right;
        r.bottom += previousOuter.bottom - previousOuter.top - previousClient.bottom;
        SetWindowPos(window,
                     nullptr,
                     0,
                     0,
                     r.right - r.left,
                     r.bottom - r.top,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        RECT client{};
        GetClientRect(window, &client);
        require(client.right == 960 && client.bottom == 720,
                "native aspect client size remains unchanged");
        SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(480, 360));
        require(mouseX == 320 && mouseY == 240, "window input matches presentation scale");
        SendMessageW(window, WM_LBUTTONDOWN, 0, MAKELPARAM(-1, 360));
        require(presses == 0, "outside-image press suppressed");
        SendMessageW(window, WM_LBUTTONDOWN, 0, MAKELPARAM(480, 360));
        require(presses == 1, "game press delivered");
        auto screen = GetDC(window);
        auto dc = CreateCompatibleDC(screen);
        auto bitmap = CreateCompatibleBitmap(screen, 1280, 720);
        auto old = SelectObject(dc, bitmap);
        SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
        require(GetPixel(dc, 480, 360) == RGB(0, 255, 0), "RGB565 frame paints through GDI");
        require(GetPixel(dc, 10, 360) == RGB(0, 255, 0), "game image fills former letterbox area");
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
        ReleaseDC(window, screen);
        hr(primary->Restore(), "restore sysmem surface");
        fill(back, 0xf800);
        hr(primary->Flip(nullptr, DDFLIP_WAIT), "second frame after restore");
        // Dawnd intercepts BltFast and follows it with a real-primary Blt.
        fill(back, 0x001f);
        hr(primary->BltFast(0, 0, back, nullptr, DDBLTFAST_WAIT), "retail-style primary BltFast");
        auto verifyDC = GetDC(window);
        auto verifyMemory = CreateCompatibleDC(verifyDC);
        auto verifyBitmap = CreateCompatibleBitmap(verifyDC, 1280, 720);
        auto previousBitmap = SelectObject(verifyMemory, verifyBitmap);
        SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(verifyMemory), PRF_CLIENT);
        require(GetPixel(verifyMemory, 480, 360) == RGB(0, 0, 255),
                "primary Blt retains blue frame");
        HDC surfaceDC = nullptr;
        hr(primary->GetDC(&surfaceDC), "GDI drawing on virtual primary");
        require(SetPixel(surfaceDC, 320, 240, RGB(255, 255, 255)) != CLR_INVALID,
                "GDI text/pixel color conversion");
        require(primary->Flip(nullptr, DDFLIP_WAIT) == DDERR_SURFACEBUSY,
                "flip respects outstanding DC");
        hr(primary->ReleaseDC(surfaceDC), "ReleaseDC captures GDI update");
        SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(verifyMemory), PRF_CLIENT);
        require(GetPixel(verifyMemory, 480, 360) == RGB(255, 255, 255),
                "RGB565 GDI update reaches window");
        hr(draw->SetCooperativeLevel(window, DDSCL_NORMAL), "normal cooperative mode");
        IDirectDrawClipper* clipper = nullptr;
        hr(draw->CreateClipper(0, &clipper, nullptr), "windowed clipper");
        hr(clipper->SetHWnd(0, window), "clipper window");
        hr(primary->SetClipper(clipper), "clipper attaches to virtual primary");
        IDirectDrawClipper* retainedClipper = nullptr;
        hr(primary->GetClipper(&retainedClipper), "virtual primary clipper identity");
        require(retainedClipper == clipper, "primary returns original clipper");
        retainedClipper->Release();
        clipper->Release();
        RECT screenRect{};
        GetClientRect(window, &screenRect);
        POINT origin{};
        ClientToScreen(window, &origin);
        OffsetRect(&screenRect, origin.x, origin.y);
        fill(back, 0xf800);
        hr(primary->Blt(&screenRect, back, nullptr, DDBLT_WAIT, nullptr),
           "windowed primary uses screen coordinates");
        SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(verifyMemory), PRF_CLIENT);
        require(GetPixel(verifyMemory, 480, 360) == RGB(255, 0, 0),
                "windowed screen rectangle fills game frame");
        SelectObject(verifyMemory, previousBitmap);
        DeleteObject(verifyBitmap);
        DeleteDC(verifyMemory);
        ReleaseDC(window, verifyDC);
        DAWindowerStats values{};
        values.size = sizeof(values);
        require(stats(&values) == 0 && values.frames >= 2 && values.presentationFailures == 0,
                "capture and presentation counters");
        if (diagnostic)
            require(values.showCursorCalls == 2 && values.setCursorCalls == 2,
                    "Debug cursor hooks observe without changing semantics");
        DestroyWindow(window);
        fill(back, 0x07e0);
        hr(primary->Flip(nullptr, DDFLIP_WAIT), "window destruction before COM release");
        back->Release();
        primary->Release();
        draw->Release();
        values.size = sizeof(values);
        require(stats(&values) == 0, "read cleanup stats");
        require(values.devices == 0 && values.surfaces == 0 && values.windows == 0,
                "COM and window cleanup");
        auto closingWindow = CreateWindowW(cls.lpszClassName,
                                           L"Reentrant destruction test",
                                           WS_POPUP,
                                           0,
                                           0,
                                           640,
                                           480,
                                           nullptr,
                                           nullptr,
                                           cls.hInstance,
                                           nullptr);
        require(closingWindow != nullptr, "create reentrant test window");
        IDirectDraw* closingDraw = nullptr;
        hr(DirectDrawCreate(nullptr, &closingDraw, nullptr), "reentrant test device");
        closeDuringStyleChange = true;
        require(FAILED(closingDraw->SetCooperativeLevel(closingWindow, DDSCL_NORMAL)),
                "destroyed window rejects attachment");
        closeDuringStyleChange = false;
        require(!IsWindow(closingWindow), "test window destroyed during style callback");
        closingDraw->Release();
        values.size = sizeof(values);
        require(stats(&values) == 0 && values.windows == 0 && values.devices == 0,
                "reentrant attachment cleanup");
        require(stop(nullptr) == ERROR_SUCCESS, "shutdown restores imports");
        if (!proxy) {
            IDirectDraw* restored = nullptr;
            hr(DirectDrawCreate(nullptr, &restored, nullptr), "native factory after shutdown");
            IDirectDraw7* native7 = nullptr;
            hr(restored->QueryInterface(IID_IDirectDraw7, reinterpret_cast<void**>(&native7)),
               "restored import reaches native COM factory");
            native7->Release();
            restored->Release();
            values.size = sizeof(values);
            require(stats(&values) == 0 && values.devices == 0,
                    "native factory bypasses wrapper after shutdown");
        }
        std::puts("PASS: DirectDraw, GDI, input, COM lifetime and shutdown contracts");
        return 0;
    }
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    std::puts(
        "Resize, move across monitors, Alt+Tab, minimize/restore. Native cursor should stay steady. Close to exit.");
    SetTimer(window, 1, 16, nullptr);
    MSG msg{};
    DWORD frame = 0;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_TIMER) {
            fill(back, ((frame++ / 60) & 1) ? 0x07e0 : 0x001f);
            DDBLTFX fx{};
            fx.dwSize = sizeof(fx);
            fx.dwFillColor = 0xf800;
            LONG x = static_cast<LONG>(frame % 600);
            RECT box{x, 200, x + 40, 240};
            hr(back->Blt(&box, nullptr, nullptr, DDBLT_WAIT | DDBLT_COLORFILL, &fx),
               "moving marker");
            hr(primary->Flip(nullptr, DDFLIP_WAIT), "animated frame");
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    back->Release();
    primary->Release();
    draw->Release();
    stop(nullptr);
    return 0;
}
