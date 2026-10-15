#pragma once
#include <windows.h>

namespace daw {
// Borrowed window/class handle. The caller must not destroy it.
HICON smallWindowIcon(HWND window);
// Supply the executable's first icon only when no window/class icon exists.
void applyGameIcon(HWND window);
} // namespace daw
