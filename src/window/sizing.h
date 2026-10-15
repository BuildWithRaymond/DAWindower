#pragma once
#include <windows.h>

namespace daw {
void setSizingLimits(HWND window, MINMAXINFO& limits);
void constrainSizing(HWND window, UINT edge, RECT& proposed);
void constrainPosition(HWND window, WINDOWPOS& proposed);
void constrainInitialSize(HWND window);
} // namespace daw
