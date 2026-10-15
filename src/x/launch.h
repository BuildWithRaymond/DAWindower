#pragma once
#include <windows.h>
#include <string>

namespace daw::x {
enum class LaunchStage {
    paths,
    createProcess,
    allocate,
    writePath,
    createLoader,
    waitLoader,
    loadLibrary,
    resume,
    complete
};

struct LaunchResult {
    DWORD error = ERROR_SUCCESS;
    LaunchStage stage = LaunchStage::complete;
    DWORD processId = 0;
};

// Synchronous transaction, called only on a short-lived UI-action worker.
// A failure terminates only the suspended child created by this transaction.
LaunchResult launchClient(const wchar_t* executable, const wchar_t* dll) noexcept;
bool currentClientPaths(std::wstring& executable, std::wstring& dll) noexcept;
const wchar_t* stageName(LaunchStage stage) noexcept;
} // namespace daw::x
