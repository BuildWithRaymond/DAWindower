#pragma once
#include <windows.h>
#include <ddraw.h>
#include <atomic>
#include "geometry.h"

namespace daw {
using CreateDraw = HRESULT(WINAPI*)(GUID*, IDirectDraw**, IUnknown*);
extern CreateDraw realCreateDraw;
extern std::atomic<DWORD> devices, surfaces, windows, frames, failures, showCalls, setCalls;
void log(const wchar_t* format, ...) noexcept;
bool tracing() noexcept;
HRESULT WINAPI createDraw(GUID*, IDirectDraw**, IUnknown*);
DWORD prepareRuntime();
} // namespace daw
