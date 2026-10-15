// Synthetic Darkages.exe fixture. It never calls LoadLibrary or an initializer:
// the launch transaction must have injected X before this entry point executes.
#include <windows.h>
#include <ddraw.h>
#include "dawindower.h"
#include <cwchar>
#include <windowsx.h>

bool clickNew(HWND w) {
    RECT outer{};
    GetWindowRect(w, &outer);
    POINT client{};
    ClientToScreen(w, &client);
    const int border = client.x - outer.left;
    const int width = MulDiv(36, GetDpiForWindow(w), 96);
    POINT p{outer.right - border - 4 * width - width / 2,
            outer.top + border + (client.y - outer.top - border) / 2};
    const auto screen = MAKELPARAM(p.x, p.y);
    const auto hit = SendMessageW(w, WM_NCHITTEST, 0, screen);
    SendMessageW(w, WM_NCLBUTTONDOWN, hit, screen);
    ScreenToClient(w, &p);
    SendMessageW(w, WM_LBUTTONUP, 0, MAKELPARAM(p.x, p.y));
    return hit == HTBORDER;
}

int main(int argc, char**) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const bool parent = argc > 1;
    auto module = parent ? LoadLibraryA("DAWindowerX.dll") : GetModuleHandleW(L"DAWindowerX.dll");
    if (!module)
        return 20;
    WNDCLASSW c{};
    c.hInstance = GetModuleHandleW(nullptr);
    c.lpszClassName = L"DAWindowerXLaunchFixture";
    c.lpfnWndProc = DefWindowProcW;
    if (!RegisterClassW(&c))
        return 21;
    auto window = CreateWindowW(c.lpszClassName,
                                L"Fixture",
                                WS_POPUP,
                                20,
                                20,
                                640,
                                480,
                                nullptr,
                                nullptr,
                                c.hInstance,
                                nullptr);
    IDirectDraw* draw = nullptr;
    if (!window || FAILED(DirectDrawCreate(nullptr, &draw, nullptr)) ||
        FAILED(draw->SetCooperativeLevel(window, DDSCL_NORMAL)))
        return 22;
    wchar_t title[256]{};
    GetWindowTextW(window, title, 256);
    if (std::wcscmp(title, L"Dark Ages") != 0)
        return 23;
    DAWindowerStats stats{sizeof(stats)};
    auto getStats = reinterpret_cast<DAWindowerEntry>(GetProcAddress(module, "DAWindowerGetStats"));
    if (!getStats || getStats(&stats) || stats.devices != 1 || stats.windows != 1)
        return 24;
    if (parent) {
        // Exercise actual title-bar worker wiring, including duplicate clicks.
        if (!clickNew(window) || !clickNew(window))
            return 27;
        const auto deadline = GetTickCount64() + 30000;
        while (GetFileAttributesW(L"x-fixture-result.bin") == INVALID_FILE_ATTRIBUTES &&
               GetTickCount64() < deadline) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                DispatchMessageW(&message);
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
        }
        if (GetFileAttributesW(L"x-fixture-result.bin") == INVALID_FILE_ATTRIBUTES)
            return 28;
    }
    auto file = CreateFileW(parent ? L"x-ui-parent-result.bin" : L"x-fixture-result.bin",
                            GENERIC_WRITE,
                            0,
                            nullptr,
                            CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL,
                            nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return 25;
    DWORD record[3] = {GetCurrentProcessId(), stats.devices, stats.windows}, written = 0;
    const bool ok =
        WriteFile(file, record, sizeof(record), &written, nullptr) && written == sizeof(record);
    CloseHandle(file);
    DestroyWindow(window);
    draw->Release();
    return ok ? 0 : 26;
}
