// Development-only acceptance helper. Never distributed with DAWindower.dll.
// Mirrors the original launchers' x86 suspended-start / LoadLibraryA route.
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

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
                _wcsicmp(m.szModule, L"dawnd.dll") == 0)
                std::printf("MODULE pid=%lu path=%ls\n", pid, m.szExePath);
        } while (Module32NextW(list, &m));
    CloseHandle(list);
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc == 4 && std::strcmp(argv[1], "--launch") == 0) {
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
        RECT outer{0, 0, 960, 720};
        AdjustWindowRectEx(&outer, style, GetMenu(w) != nullptr, GetWindowLongW(w, GWL_EXSTYLE));
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
            outer = {0, 0, width, 703};
            AdjustWindowRectEx(
                &outer, style, GetMenu(w) != nullptr, GetWindowLongW(w, GWL_EXSTYLE));
            require(SetWindowPos(w,
                                 nullptr,
                                 0,
                                 0,
                                 outer.right - outer.left,
                                 outer.bottom - outer.top,
                                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                    "one-pixel retail resize");
            GetClientRect(w, &r);
            require(r.right == width && r.bottom == 703,
                    "retail keeps every requested pixel dimension");
        }
        for (int height = 703; height <= 719; ++height) {
            outer = {0, 0, 1016, height};
            AdjustWindowRectEx(
                &outer, style, GetMenu(w) != nullptr, GetWindowLongW(w, GWL_EXSTYLE));
            require(SetWindowPos(w,
                                 nullptr,
                                 0,
                                 0,
                                 outer.right - outer.left,
                                 outer.bottom - outer.top,
                                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE,
                    "one-pixel retail height resize");
            GetClientRect(w, &r);
            require(r.right == 1016 && r.bottom == height, "retail height independent from width");
        }
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
        "Usage: retail_loadlibrary_test --launch absolute-Darkages.exe absolute-DLL | --observe pid...\n");
    return 2;
}
