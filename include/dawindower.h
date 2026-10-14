#pragma once
#include <windows.h>

// Optional diagnostics/lifecycle exports. Ordinary early LoadLibraryA injection
// initializes automatically on the executable's first DirectDrawCreate call.
using DAWindowerEntry = DWORD(WINAPI*)(void*);

struct DAWindowerStats {
    DWORD size;
    DWORD version;
    DWORD devices;
    DWORD surfaces;
    DWORD windows;
    DWORD frames;
    DWORD presentationFailures;
    DWORD showCursorCalls;
    DWORD setCursorCalls;
};

extern "C" DWORD WINAPI DAWindowerInitialize(void* reserved);
extern "C" DWORD WINAPI DAWindowerShutdown(void* reserved);
extern "C" DWORD WINAPI DAWindowerGetStats(void* stats);
