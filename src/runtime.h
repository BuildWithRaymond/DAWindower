#pragma once
#include <windows.h>
#include <ddraw.h>
#include <atomic>
#include "geometry.h"

namespace daw {
using CreateDraw = HRESULT(WINAPI*)(GUID*, IDirectDraw**, IUnknown*);
// Native factory and process-wide counters shared by the wrapper subsystems.
extern CreateDraw realCreateDraw;
extern std::atomic<DWORD> devices, surfaces, windows, frames, failures, showCalls, setCalls;
void log(const wchar_t* format, ...) noexcept;
bool tracing() noexcept;
HRESULT WINAPI createDraw(GUID* guid, IDirectDraw** output, IUnknown* outer);
// Outside loader lock: pin this module and resolve the system DirectDraw factory.
DWORD prepareRuntime();
} // namespace daw
