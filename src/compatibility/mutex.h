#pragma once
#include <windows.h>

namespace daw {
HANDLE WINAPI createClientMutex(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);
}
