#pragma once
#include <windows.h>

// Optional diagnostics/lifecycle exports. Ordinary early LoadLibraryA injection
// initializes automatically on the executable's first DirectDrawCreate call.
using DAWindowerEntry = DWORD(WINAPI*)(void*);

struct DAWindowerStats {
    DWORD size;    // Caller initializes to sizeof(DAWindowerStats).
    DWORD version; // Returned layout version; currently 1.
    // Current live wrapper/window counts.
    DWORD devices;
    DWORD surfaces;
    DWORD windows;
    // Cumulative process counters; shutdown does not reset them.
    DWORD frames;
    DWORD presentationFailures;
    // Cursor observers are installed only in diagnostic Debug builds.
    DWORD showCursorCalls;
    DWORD setCursorCalls;
};

// Lifecycle calls require reserved == nullptr and return Win32 error codes.
// Initialize is idempotent. Shutdown returns ERROR_BUSY while wrappers/windows live.
extern "C" DWORD WINAPI DAWindowerInitialize(void* reserved);
extern "C" DWORD WINAPI DAWindowerShutdown(void* reserved);
// stats points to DAWindowerStats with size initialized; returns ERROR_SUCCESS
// or ERROR_INVALID_PARAMETER. Each counter is an independent atomic sample.
extern "C" DWORD WINAPI DAWindowerGetStats(void* stats);
