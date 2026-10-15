// Development-only acceptance helper. Never distributed with DAWindower.dll.
// Mirrors the original launchers' x86 suspended-start / LoadLibraryA route.
#include <windows.h>
#include <tlhelp32.h>
#include "../tests/window_size.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef DAW_X
constexpr int reviewLeft = 760;
#else
constexpr int reviewLeft = 40;
#endif

static void require(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s Win32=%lu\n", what, GetLastError());
        std::exit(1);
    }
}

struct WindowSearch {
    DWORD pid;
    HWND hwnd = nullptr;
};

static BOOL CALLBACK find(HWND w, LPARAM context) {
    auto p = reinterpret_cast<WindowSearch*>(context);
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == p->pid && !GetWindow(w, GW_OWNER) && IsWindowVisible(w)) {
        p->hwnd = w;
        return FALSE;
    }
    return TRUE;
}

static HWND window(DWORD pid) {
    WindowSearch result{pid};
    EnumWindows(find, reinterpret_cast<LPARAM>(&result));
    return result.hwnd;
}

static void fullscreenRoundTrip(HWND w) {
    const auto style = GetWindowLongA(w, GWL_STYLE);
    RECT normal{}, client{};
    require(GetWindowRect(w, &normal) && GetClientRect(w, &client),
            "save isolated client's windowed placement");
    MONITORINFO monitor{sizeof(monitor)};
    require(GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE,
            "read retail fullscreen monitor");
    SetWindowLongA(w, GWL_STYLE, WS_POPUP);
    ShowWindow(w, SW_MAXIMIZE);
    Sleep(300);
    RECT actual{};
    GetWindowRect(w, &actual);
    require(IsZoomed(w) && EqualRect(&actual, &monitor.rcMonitor),
            "actual client honors native launcher popup plus maximize");
    RECT fullClient{};
    POINT origin{};
    GetClientRect(w, &fullClient);
    ClientToScreen(w, &origin);
    require(origin.x == actual.left && origin.y == actual.top &&
                fullClient.right == actual.right - actual.left &&
                fullClient.bottom == actual.bottom - actual.top,
            "actual fullscreen has no caption, border or unused client margin");
    ShowWindow(w, SW_MINIMIZE);
    require(IsIconic(w) != FALSE, "actual fullscreen can minimize");
    ShowWindow(w, SW_RESTORE);
    GetWindowRect(w, &actual);
    require(IsZoomed(w) && EqualRect(&actual, &monitor.rcMonitor),
            "actual fullscreen survives minimize and restore");
    ShowWindow(w, SW_RESTORE);
    SetWindowLongA(w, GWL_STYLE, style);
    require(SetWindowPos(w,
                         nullptr,
                         normal.left,
                         normal.top,
                         normal.right - normal.left,
                         normal.bottom - normal.top,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) != FALSE,
            "return isolated client to its windowed mode");
    RECT restored{};
    GetClientRect(w, &restored);
    require(restored.right == client.right && restored.bottom == client.bottom,
            "actual client returns to exact windowed game dimensions");
    std::printf(
        "FULLSCREEN hwnd=%p monitor=%ldx%ld minimize_restore=passed windowed_restore=passed\n",
        w,
        fullClient.right,
        fullClient.bottom);
}

static unsigned snapshot(HWND w, DWORD pid) {
    RECT r{};
    GetClientRect(w, &r);
    if (!r.right || !r.bottom || r.right > 4096 || r.bottom > 4096)
        return 0;
    auto screen = GetDC(w), dc = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), r.right, -r.bottom, 1, 32, BI_RGB, 0, 0, 0, 0, 0};
    void* bits = nullptr;
    auto bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    auto old = SelectObject(dc, bitmap);
    DWORD_PTR ignored = 0;
    const bool ok = SendMessageTimeoutW(w,
                                        WM_PRINTCLIENT,
                                        reinterpret_cast<WPARAM>(dc),
                                        PRF_CLIENT,
                                        SMTO_ABORTIFHUNG,
                                        1000,
                                        &ignored) != 0;
    GdiFlush();
    unsigned hash = 2166136261u;
    const auto count = static_cast<size_t>(r.right) * r.bottom * 4;
    if (ok && bits) {
        for (size_t i = 0; i < count; ++i) {
            hash ^= static_cast<BYTE*>(bits)[i];
            hash *= 16777619u;
        }
        char path[128]{};
        sprintf_s(path, "retail-%lu.bmp", pid);
        auto file = CreateFileA(path,
                                GENERIC_WRITE,
                                FILE_SHARE_READ,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            BITMAPFILEHEADER head{
                0x4d42,
                static_cast<DWORD>(sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + count),
                0,
                0,
                sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)};
            DWORD bytes = 0;
            WriteFile(file, &head, sizeof(head), &bytes, nullptr);
            WriteFile(file, &info.bmiHeader, sizeof(info.bmiHeader), &bytes, nullptr);
            WriteFile(file, bits, static_cast<DWORD>(count), &bytes, nullptr);
            CloseHandle(file);
        }
    } else
        hash = 0;
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(w, screen);
    return hash;
}

static void modules(DWORD pid) {
    auto list = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    require(list != INVALID_HANDLE_VALUE, "enumerate retail modules");
    MODULEENTRY32W m{};
    m.dwSize = sizeof(m);
    if (Module32FirstW(list, &m))
        do {
            if (_wcsicmp(m.szModule, L"ddraw.dll") == 0 ||
                _wcsicmp(m.szModule, L"DAWindower.dll") == 0 ||
                _wcsicmp(m.szModule, L"DAWindowerX.dll") == 0 ||
                _wcsicmp(m.szModule, L"dawnd.dll") == 0)
                std::printf("MODULE pid=%lu path=%ls\n", pid, m.szExePath);
        } while (Module32NextW(list, &m));
    CloseHandle(list);
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    const bool nativeReview = argc == 4 && std::strcmp(argv[1], "--launch-native") == 0;
    if (argc == 4 && (std::strcmp(argv[1], "--launch") == 0 || nativeReview)) {
        const auto name = std::strrchr(argv[2], '\\');
        require(name && _stricmp(name + 1, "Darkages.exe") == 0,
                "only isolated Darkages.exe supported");
        char directory[MAX_PATH]{};
        strcpy_s(directory, argv[2]);
        *std::strrchr(directory, '\\') = 0;
        STARTUPINFOA start{};
        start.cb = sizeof(start);
        PROCESS_INFORMATION p{};
        require(CreateProcessA(argv[2],
                               nullptr,
                               nullptr,
                               nullptr,
                               FALSE,
                               CREATE_SUSPENDED,
                               nullptr,
                               directory,
                               &start,
                               &p) != FALSE,
                "create suspended isolated client");
        std::printf("CREATED pid=%lu\n", p.dwProcessId);
        std::fflush(stdout);
        const auto bytes = std::strlen(argv[3]) + 1;
        auto argument =
            VirtualAllocEx(p.hProcess, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        require(argument != nullptr, "remote DLL path allocation");
        SIZE_T written = 0;
        require(WriteProcessMemory(p.hProcess, argument, argv[3], bytes, &written) &&
                    written == bytes,
                "remote DLL path");
        auto loader = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryA"));
        auto thread = CreateRemoteThread(p.hProcess, nullptr, 0, loader, argument, 0, nullptr);
        require(thread != nullptr, "remote LoadLibraryA thread");
        require(WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0,
                "LoadLibraryA completes before resume");
        DWORD result = 0;
        require(GetExitCodeThread(thread, &result) && result != 0,
                "remote LoadLibraryA returns module");
        std::printf("LOADED pid=%lu module=%08lx route=LoadLibraryA initialization_calls=0\n",
                    p.dwProcessId,
                    result);
        CloseHandle(thread);
        VirtualFreeEx(p.hProcess, argument, 0, MEM_RELEASE);
        modules(p.dwProcessId);
        require(ResumeThread(p.hThread) != static_cast<DWORD>(-1), "resume client");
        CloseHandle(p.hThread);
        HWND w = nullptr;
        const auto until = GetTickCount64() + 20000;
        while (!(w = window(p.dwProcessId)) && GetTickCount64() < until &&
               WaitForSingleObject(p.hProcess, 100) == WAIT_TIMEOUT) {
        }
        require(w != nullptr, "retail window appears");
        Sleep(1500);
        const auto style = GetWindowLongW(w, GWL_STYLE);
        require((style & WS_THICKFRAME) != 0, "retail window immediately resizable");
        if (nativeReview) {
            RECT client{};
            GetClientRect(w, &client);
            require(client.right == 640 && client.bottom == 480,
                    "actual startup preserves native game dimensions without helper resizing");
            require(
                SetWindowPos(
                    w, nullptr, reviewLeft, 40, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) !=
                    FALSE,
                "place native review window without resizing it");
            const auto hash = snapshot(w, p.dwProcessId);
            std::printf("PASS_NATIVE pid=%lu hwnd=%p client=%ldx%ld style=%08lx snapshot=%08x\n",
                        p.dwProcessId,
                        w,
                        client.right,
                        client.bottom,
                        style,
                        hash);
            modules(p.dwProcessId);
            CloseHandle(p.hProcess);
            return hash ? 0 : 1;
        }
        RECT outer{0, 0, 960, 720};
        adjustForClient(w, outer);
        require(SetWindowPos(w,
                             nullptr,
                             40,
                             40,
                             outer.right - outer.left,
                             outer.bottom - outer.top,
                             SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                "resize actual client");
        Sleep(300);
        RECT r{};
        GetClientRect(w, &r);
        require(r.right == 960 && r.bottom == 720, "actual client area resized with 4:3 graphics");
        // Exercise every intermediate size on an actual client, not just
        // convenient 4:3 endpoints that hid the previous 4x3 size grid.
        for (int width = 1000; width <= 1016; ++width) {
            const int height = (width * 3 + 2) / 4;
            outer = {0, 0, width, height};
            adjustForClient(w, outer);
            require(SetWindowPos(w,
                                 nullptr,
                                 0,
                                 0,
                                 outer.right - outer.left,
                                 outer.bottom - outer.top,
                                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                    "one-pixel retail resize");
            GetClientRect(w, &r);
            require(r.right == width && r.bottom == height,
                    "retail keeps every width pixel at native aspect");
        }
        for (int height = 750; height <= 766; ++height) {
            const int width = (height * 4 + 1) / 3;
            outer = {0, 0, width, height};
            adjustForClient(w, outer);
            require(SetWindowPos(w,
                                 nullptr,
                                 0,
                                 0,
                                 outer.right - outer.left,
                                 outer.bottom - outer.top,
                                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                    "one-pixel retail height resize");
            GetClientRect(w, &r);
            require(r.right == width && r.bottom == height,
                    "retail keeps every height pixel at native aspect");
        }
        fullscreenRoundTrip(w);
        // Leave this helper's own new clients side by side for manual review.
        outer = {0, 0, 640, 480};
        require(adjustForClient(w, outer), "measure review client caption");
        require(SetWindowPos(w,
                             nullptr,
                             reviewLeft,
                             40,
                             outer.right - outer.left,
                             outer.bottom - outer.top,
                             SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                "place isolated review client");
        GetClientRect(w, &r);
        require(r.right == 640 && r.bottom == 480,
                "review client returns to exact native dimensions");
        auto hash = snapshot(w, p.dwProcessId);
        std::printf("PASS pid=%lu hwnd=%p client=%ldx%ld style=%08lx snapshot=%08x\n",
                    p.dwProcessId,
                    w,
                    r.right,
                    r.bottom,
                    style,
                    hash);
        modules(p.dwProcessId);
        CloseHandle(p.hProcess);
        return hash ? 0 : 1;
    }
    if (argc >= 3 && std::strcmp(argv[1], "--observe") == 0) {
        for (unsigned sample = 0; sample < 5; ++sample) {
            for (int i = 2; i < argc; ++i) {
                const auto pid = static_cast<DWORD>(std::strtoul(argv[i], nullptr, 10));
                auto w = window(pid);
                std::printf("FRAME sample=%u pid=%lu hwnd=%p hash=%08x iconic=%d\n",
                            sample,
                            pid,
                            w,
                            w ? snapshot(w, pid) : 0,
                            w ? IsIconic(w) : 0);
            }
            std::fflush(stdout);
            if (sample != 4)
                Sleep(2000);
        }
        return 0;
    }
    std::fprintf(
        stderr,
        "Usage: retail_loadlibrary_test --launch|--launch-native absolute-Darkages.exe absolute-DLL | --observe pid...\n");
    return 2;
}
