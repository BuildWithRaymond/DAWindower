#pragma once
#include <windows.h>

namespace daw {
// Loader-lock boundary: fixed storage, bounded PE reads, Kernel32 only.
// The gates do no runtime work until called by the EXE after LoadLibrary returns.
BOOL armBootstrap(HMODULE image) noexcept;
DWORD validateBootstrap(void* directDraw, void* createMutex) noexcept;
void* bootstrapTarget(unsigned index, void* fallback) noexcept;
DWORD disarmBootstrap() noexcept;
} // namespace daw
