#pragma once
#include <windows.h>

namespace daw {
// Isolate the client's known singleton name by process; preserve other mutexes.
HANDLE WINAPI createClientMutex(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCSTR name);
} // namespace daw
