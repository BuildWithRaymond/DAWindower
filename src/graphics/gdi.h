#pragma once
#include <windows.h>
#include <memory>

namespace daw {
struct WindowState;
// Weak HWND registration redirects only tracked window DCs to logical pixels.
// Unknown DCs retain native BitBlt semantics; game networking/input is untouched.
HRESULT trackWindow(HWND window, const std::shared_ptr<WindowState>& state);
void untrackWindow(HWND window);
BOOL WINAPI windowBitBlt(HDC destination,
                         int x,
                         int y,
                         int width,
                         int height,
                         HDC source,
                         int sourceX,
                         int sourceY,
                         DWORD operation);
} // namespace daw
