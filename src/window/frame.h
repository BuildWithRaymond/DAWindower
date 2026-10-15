#pragma once
#include <windows.h>

namespace daw {
// The resize capability distinguishes our borderless window from a launcher's
// bare WS_POPUP fullscreen request. No launcher-specific export or setting.
bool windowedFrame(HWND window);
// Recognize a launcher's fullscreen bounds before adjusting window styles.
bool fullscreenBeforeAttach(HWND window);
// Hide native decorations while retaining resize and system-menu styles.
void removeVisibleFrame(HWND window);
} // namespace daw
