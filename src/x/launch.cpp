#include "launch.h"
#include <cwchar>
#include <new>

namespace daw::x {
namespace {
bool modulePath(HMODULE module, std::wstring& path) {
    path.resize(32768);
    const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) {
        path.clear();
        return false;
    }
    path.resize(length);
    return true;
}

bool gameExecutable(const wchar_t* path) noexcept {
    if (!path)
        return false;
    const auto slash = std::wcsrchr(path, L'\\');
    return slash && _wcsicmp(slash + 1, L"Darkages.exe") == 0;
}

struct Child {
    PROCESS_INFORMATION process{};
    HANDLE loader = nullptr;
    void* argument = nullptr;
    bool resumed = false;

    ~Child() {
        if (process.hProcess && !resumed) {
            // The child is ours and its main thread has never run. Do not
            // terminate or enumerate pre-existing clients on any failure path.
            TerminateProcess(process.hProcess, ERROR_DLL_INIT_FAILED);
            WaitForSingleObject(process.hProcess, 5000);
        }
        // On a timeout, terminate first: the loader may still reference path.
        if (argument)
            VirtualFreeEx(process.hProcess, argument, 0, MEM_RELEASE);
        if (loader)
            CloseHandle(loader);
        if (process.hThread)
            CloseHandle(process.hThread);
        if (process.hProcess)
            CloseHandle(process.hProcess);
    }
};
} // namespace

bool currentClientPaths(std::wstring& executable, std::wstring& dll) noexcept {
    try {
        HMODULE module = nullptr;
        return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  reinterpret_cast<LPCWSTR>(&currentClientPaths),
                                  &module) &&
               modulePath(nullptr, executable) && gameExecutable(executable.c_str()) &&
               modulePath(module, dll);
    } catch (const std::bad_alloc&) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return false;
    }
}

LaunchResult launchClient(const wchar_t* executable, const wchar_t* dll) noexcept {
    LaunchResult result;
    result.stage = LaunchStage::paths;
    Child child;
    auto fail = [&](DWORD error) {
        result.error = error ? error : ERROR_GEN_FAILURE;
        return result;
    };
    try {
        // This is a same-executable, same-bitness action, not a general injector.
        if (!gameExecutable(executable) || !dll || !*dll)
            return fail(ERROR_NOT_SUPPORTED);
        std::wstring directory(executable);
        directory.resize(directory.find_last_of(L'\\'));
        std::wstring command = L"\"" + std::wstring(executable) + L"\"";
        STARTUPINFOW startup{sizeof(startup)};
        result.stage = LaunchStage::createProcess;
        if (!CreateProcessW(executable,
                            command.data(),
                            nullptr,
                            nullptr,
                            FALSE,
                            CREATE_SUSPENDED,
                            nullptr,
                            directory.c_str(),
                            &startup,
                            &child.process))
            return fail(GetLastError());
        result.processId = child.process.dwProcessId;
        const auto bytes = (std::wcslen(dll) + 1) * sizeof(wchar_t);
        result.stage = LaunchStage::allocate;
        child.argument = VirtualAllocEx(
            child.process.hProcess, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!child.argument)
            return fail(GetLastError());
        result.stage = LaunchStage::writePath;
        SIZE_T written = 0;
        if (!WriteProcessMemory(child.process.hProcess, child.argument, dll, bytes, &written) ||
            written != bytes)
            return fail(GetLastError());
        result.stage = LaunchStage::createLoader;
        // Clone of the running x86 client: use the Windows system loader route
        // used by the existing launchers. W accepts Unicode game/DLL folders;
        // the injected DLL's LoadLibraryA-only contract remains unchanged.
        const auto load = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        if (!load)
            return fail(ERROR_PROC_NOT_FOUND);
        child.loader = CreateRemoteThread(
            child.process.hProcess, nullptr, 0, load, child.argument, 0, nullptr);
        if (!child.loader)
            return fail(GetLastError());
        result.stage = LaunchStage::waitLoader;
        const auto wait = WaitForSingleObject(child.loader, 15000);
        if (wait != WAIT_OBJECT_0)
            return fail(wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError());
        result.stage = LaunchStage::loadLibrary;
        DWORD module = 0;
        if (!GetExitCodeThread(child.loader, &module) || !module)
            return fail(ERROR_DLL_INIT_FAILED);
        result.stage = LaunchStage::resume;
        if (ResumeThread(child.process.hThread) == static_cast<DWORD>(-1))
            return fail(GetLastError());
        child.resumed = true;
        result.stage = LaunchStage::complete;
        return result;
    } catch (const std::bad_alloc&) {
        return fail(ERROR_NOT_ENOUGH_MEMORY);
    }
}

const wchar_t* stageName(LaunchStage stage) noexcept {
    switch (stage) {
        case LaunchStage::paths:
            return L"resolve game/DLL paths";
        case LaunchStage::createProcess:
            return L"create suspended client";
        case LaunchStage::allocate:
            return L"allocate DLL path";
        case LaunchStage::writePath:
            return L"write DLL path";
        case LaunchStage::createLoader:
            return L"start Windows loader";
        case LaunchStage::waitLoader:
            return L"wait for Windows loader";
        case LaunchStage::loadLibrary:
            return L"load DAWindower X";
        case LaunchStage::resume:
            return L"resume client";
        default:
            return L"complete";
    }
}
} // namespace daw::x
