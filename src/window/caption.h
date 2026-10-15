#pragma once
#include <windows.h>
#include "window/frame.h"

namespace daw {
// UI-thread only, after the ordinary renderer has attached its window.
bool attachCaption(HWND window);

inline int frameCaptionHeight(HWND window) {
    if (!windowedFrame(window))
        return 0;
    const auto dpi = GetDpiForWindow(window);
    return MulDiv(36, dpi ? dpi : 96, 96);
}
} // namespace daw
