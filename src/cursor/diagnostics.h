#pragma once
#include <windows.h>

namespace daw {
int WINAPI observeShowCursor(BOOL show);
HCURSOR WINAPI observeSetCursor(HCURSOR cursor);
} // namespace daw
