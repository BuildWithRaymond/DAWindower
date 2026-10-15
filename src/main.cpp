#include "runtime.h"
#include "compatibility/imports.h"
#include "compatibility/bootstrap.h"
#include "compatibility/mutex.h"
#include "compatibility/geometry.h"
#include "cursor/diagnostics.h"
#include "graphics/gdi.h"
#include "dawindower.h"
#include <cstdarg>
#include <cwchar>
#include <mutex>
#include <new>
#include <utility>

// Runtime setup is synchronous at the first factory call. DllMain below is a
// separate loader-lock boundary and must remain limited to the tiny import gates.
namespace {
HMODULE instance = nullptr;
HMODULE systemDraw = nullptr;
std::recursive_mutex initialization;
std::vector<daw::ImportPatch> patches;
bool prepared = false, initialized = false, diagnostics = false;
#ifdef _DEBUG
HANDLE diagnosticFile = INVALID_HANDLE_VALUE;
SRWLOCK diagnosticLock = SRWLOCK_INIT;
unsigned diagnosticLines = 0;
#endif
DWORD installCompatibilityImports() {
    const auto kernel = GetModuleHandleW(L"kernel32.dll");
    const auto nativeMutex = reinterpret_cast<void*>(GetProcAddress(kernel, "CreateMutexA"));
    const auto mutexResult =
        daw::patchImport(GetModuleHandleW(nullptr),
                         "kernel32.dll",
                         "CreateMutexA",
                         daw::bootstrapTarget(daw::mutexBootstrapGate, nativeMutex),
                         reinterpret_cast<void*>(daw::createClientMutex),
                         patches);
    if (!mutexResult)
        patches.back().original = nativeMutex;
    if (mutexResult && mutexResult != ERROR_PROC_NOT_FOUND)
        return mutexResult;
    const auto gdi = GetModuleHandleW(L"gdi32.dll");
    const auto bltResult = daw::patchImport(GetModuleHandleW(nullptr),
                                            "gdi32.dll",
                                            "BitBlt",
                                            reinterpret_cast<void*>(GetProcAddress(gdi, "BitBlt")),
                                            reinterpret_cast<void*>(daw::windowBitBlt),
                                            patches);
    if (bltResult && bltResult != ERROR_PROC_NOT_FOUND)
        return bltResult;
    const auto geometryResult =
        daw::installClientGeometryImports(GetModuleHandleW(nullptr), patches);
    if (geometryResult)
        return geometryResult;
#ifdef _DEBUG
    if (!diagnostics)
        return ERROR_SUCCESS;
    const auto user = GetModuleHandleW(L"user32.dll");
    for (const auto& hook : {std::pair<const char*, void*>{
                                 "ShowCursor", reinterpret_cast<void*>(daw::observeShowCursor)},
                             std::pair<const char*, void*>{
                                 "SetCursor", reinterpret_cast<void*>(daw::observeSetCursor)}}) {
        const auto result =
            daw::patchImport(GetModuleHandleW(nullptr),
                             "user32.dll",
                             hook.first,
                             reinterpret_cast<void*>(GetProcAddress(user, hook.first)),
                             hook.second,
                             patches);
        if (result && result != ERROR_PROC_NOT_FOUND)
            return result;
    }
#endif
    return ERROR_SUCCESS;
}
} // namespace

namespace daw {
CreateDraw realCreateDraw = nullptr;
std::atomic<DWORD> devices{0}, surfaces{0}, windows{0}, frames{0}, failures{0}, showCalls{0},
    setCalls{0};

bool tracing() noexcept {
#ifdef _DEBUG
    return diagnostics;
#else
    return false;
#endif
}

void log(const wchar_t* format, ...) noexcept {
#ifdef _DEBUG
    if (!diagnostics)
        return;
    const auto error = GetLastError();
    AcquireSRWLockExclusive(&diagnosticLock);
    if (diagnosticLines++ >= 2048) {
        ReleaseSRWLockExclusive(&diagnosticLock);
        SetLastError(error);
        return;
    }
    wchar_t buffer[1024]{};
    const auto prefix =
        swprintf_s(buffer, L"DAWindower [%lu/%llu]: ", GetCurrentThreadId(), GetTickCount64());
    va_list arguments;
    va_start(arguments, format);
    _vsnwprintf_s(buffer + prefix, 1024 - prefix, _TRUNCATE, format, arguments);
    va_end(arguments);
    OutputDebugStringW(buffer);
    OutputDebugStringW(L"\n");
    if (diagnosticFile != INVALID_HANDLE_VALUE) {
        char utf8[4096]{};
        const auto length =
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, utf8, 4095, nullptr, nullptr);
        if (length > 0) {
            utf8[length - 1] = '\n';
            DWORD written = 0;
            WriteFile(diagnosticFile, utf8, static_cast<DWORD>(length), &written, nullptr);
        }
    }
    ReleaseSRWLockExclusive(&diagnosticLock);
    SetLastError(error);
#else
    (void)format;
#endif
}

DWORD prepareRuntime() {
    std::lock_guard<std::recursive_mutex> lock(initialization);
    if (prepared)
        return ERROR_SUCCESS;
    const auto legacy = GetModuleHandleW(L"dawnd.dll");
    if (legacy && legacy != instance)
        return ERROR_NOT_SUPPORTED;
    wchar_t path[MAX_PATH]{};
#ifdef _DEBUG
    wchar_t diagnosticSetting[8]{};
    diagnostics = GetEnvironmentVariableW(L"DAW_DIAGNOSTICS", diagnosticSetting, 8) != 0 &&
                  diagnosticSetting[0] == L'1';
    if (diagnostics && diagnosticFile == INVALID_HANDLE_VALUE) {
        wchar_t logPath[MAX_PATH]{};
        const auto length = GetModuleFileNameW(instance, logPath, MAX_PATH);
        if (!length || length >= MAX_PATH)
            return ERROR_FILENAME_EXCED_RANGE;
        auto separator = std::wcsrchr(logPath, L'\\');
        if (!separator)
            return ERROR_FILENAME_EXCED_RANGE;
        auto logName = separator + 1;
        const auto available = MAX_PATH - static_cast<size_t>(logName - logPath);
        if (swprintf_s(logName, available, L"DAWindower-%lu.log", GetCurrentProcessId()) < 0)
            return ERROR_FILENAME_EXCED_RANGE;
        diagnosticFile = CreateFileW(logPath,
                                     FILE_APPEND_DATA,
                                     FILE_SHARE_READ,
                                     nullptr,
                                     CREATE_ALWAYS,
                                     FILE_ATTRIBUTE_NORMAL,
                                     nullptr);
        if (diagnosticFile == INVALID_HANDLE_VALUE)
            return GetLastError();
        log(L"Diagnostic build %S, compiled %S %S, pid=%lu",
            DAW_VERSION,
            __DATE__,
            __TIME__,
            GetCurrentProcessId());
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(instance, modulePath, MAX_PATH);
        log(L"Loaded compatibility module: %s", modulePath);
    }
#endif
    // DLLs stay mapped until process exit: no live hook can jump into freed code,
    // even if a launcher releases its LoadLibrary reference too soon.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(instance),
                            &pinned))
        return GetLastError();
    const auto systemLength = GetSystemDirectoryW(path, MAX_PATH);
    if (!systemLength || systemLength + 11 >= MAX_PATH)
        return ERROR_FILENAME_EXCED_RANGE;
    wcscat_s(path, L"\\ddraw.dll");
    systemDraw = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!systemDraw)
        return GetLastError();
    realCreateDraw = reinterpret_cast<CreateDraw>(GetProcAddress(systemDraw, "DirectDrawCreate"));
    if (!realCreateDraw) {
        FreeLibrary(systemDraw);
        systemDraw = nullptr;
        return ERROR_PROC_NOT_FOUND;
    }
    prepared = true;
    log(L"Native DirectDraw loaded from %s; scaling=full client, nearest neighbor", path);
    return ERROR_SUCCESS;
}
} // namespace daw

extern "C" DWORD WINAPI DAWindowerInitialize(void* reserved) {
    if (reserved)
        return ERROR_INVALID_PARAMETER;
    std::lock_guard<std::recursive_mutex> lock(initialization);
    if (initialized)
        return ERROR_SUCCESS;
    const auto preparedResult = daw::prepareRuntime();
    if (preparedResult)
        return preparedResult;
    DWORD result = ERROR_SUCCESS;
    try {
        patches.reserve(12);
        result = daw::validateBootstrap(reinterpret_cast<void*>(daw::realCreateDraw),
                                        reinterpret_cast<void*>(GetProcAddress(
                                            GetModuleHandleW(L"kernel32.dll"), "CreateMutexA")));
        if (result)
            return result;
        result = installCompatibilityImports();
        // Replace the factory gate last, atomically. No interval exposes the
        // native factory while other graphics hooks are being initialized.
        if (!result)
            result =
                daw::patchImport(GetModuleHandleW(nullptr),
                                 "ddraw.dll",
                                 "DirectDrawCreate",
                                 daw::bootstrapTarget(daw::directDrawBootstrapGate,
                                                      reinterpret_cast<void*>(daw::realCreateDraw)),
                                 reinterpret_cast<void*>(daw::createDraw),
                                 patches);
        if (!result)
            patches.back().original = reinterpret_cast<void*>(daw::realCreateDraw);
    } catch (const std::bad_alloc&) {
        result = ERROR_NOT_ENOUGH_MEMORY;
    }
    if (result) {
        const auto rollback = daw::restoreImports(patches);
        daw::log(L"Hook installation failed: %lu; rollback=%lu", result, rollback);
        return result;
    }
    initialized = true;
    return ERROR_SUCCESS;
}

extern "C" DWORD WINAPI DAWindowerShutdown(void* reserved) {
    if (reserved)
        return ERROR_INVALID_PARAMETER;
    std::lock_guard<std::recursive_mutex> lock(initialization);
    if (daw::devices.load() || daw::surfaces.load() || daw::windows.load())
        return ERROR_BUSY;
    auto result = daw::restoreImports(patches);
    if (!result)
        result = daw::disarmBootstrap();
    if (!result)
        initialized = false;
    return result;
}

extern "C" DWORD WINAPI DAWindowerGetStats(void* buffer) {
    auto stats = static_cast<DAWindowerStats*>(buffer);
    if (!stats || stats->size != sizeof(*stats))
        return ERROR_INVALID_PARAMETER;
    *stats = {sizeof(*stats),
              1,
              daw::devices.load(),
              daw::surfaces.load(),
              daw::windows.load(),
              daw::frames.load(),
              daw::failures.load(),
              daw::showCalls.load(),
              daw::setCalls.load()};
    return ERROR_SUCCESS;
}

extern "C" HRESULT WINAPI DAWindowerProxyCreate(GUID* guid, IDirectDraw** output, IUnknown* outer) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    std::lock_guard<std::recursive_mutex> lock(initialization);
    auto result = daw::prepareRuntime();
    if (!result && !initialized) {
        try {
            patches.reserve(12);
            result = installCompatibilityImports();
        } catch (const std::bad_alloc&) {
            result = ERROR_NOT_ENOUGH_MEMORY;
        }
        if (result)
            daw::restoreImports(patches);
        else
            initialized = true;
    }
    return result ? HRESULT_FROM_WIN32(result) : daw::createDraw(guid, output, outer);
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, void*) {
    // The statically linked CRT and thread_local guard need thread notifications.
    if (reason == DLL_PROCESS_ATTACH) {
        instance = module;
#ifndef DAW_PROXY
        // Only an allocation-free import gate here. Actual initialization runs
        // synchronously on the first EXE graphics call, after loader lock exits.
        return daw::armBootstrap(GetModuleHandleW(nullptr));
#endif
    }
    if (reason == DLL_PROCESS_DETACH)
        daw::disarmBootstrap();
#ifdef _DEBUG
    // Kernel handle cleanup only; logging and initialization occur outside DllMain.
    if (reason == DLL_PROCESS_DETACH && diagnosticFile != INVALID_HANDLE_VALUE)
        CloseHandle(diagnosticFile);
#endif
    return TRUE;
}
