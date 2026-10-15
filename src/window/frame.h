#pragma once
#include <windows.h>

namespace daw {
// The resize capability distinguishes our borderless window from a launcher's
// bare WS_POPUP fullscreen request. No launcher-specific export or setting.
bool windowedFrame(HWND window);
bool fullscreenBeforeAttach(HWND window);
int nativeCaptionHeight(HWND window);
// Distances from the outer rectangle to the standard game client rectangle.
// Includes the caption and Windows' invisible side/bottom resize margins.
RECT nativeFrameInsets(HWND window);
void removeVisibleFrame(HWND window);
int resizeHit(HWND window, POINT screen);
} // namespace daw
