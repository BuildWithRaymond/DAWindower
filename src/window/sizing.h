#pragma once
#include <windows.h>

namespace daw {
// UI-thread constraints keep the game client at 4:3, excluding its caption strip.
void setSizingLimits(HWND window, MINMAXINFO& limits);
void constrainSizing(HWND window, UINT edge, RECT& proposed);
void constrainPosition(HWND window, WINDOWPOS& proposed);
void constrainInitialSize(HWND window);
} // namespace daw
