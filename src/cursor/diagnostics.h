#pragma once
#include <windows.h>

namespace daw {
// Debug import observers preserve the native return value and last-error state.
int WINAPI observeShowCursor(BOOL show);
HCURSOR WINAPI observeSetCursor(HCURSOR cursor);
} // namespace daw
