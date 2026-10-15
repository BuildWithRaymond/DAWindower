#include "frame.h"
#include <dwmapi.h>

namespace daw {
bool windowedFrame(HWND window) {
    return (GetWindowLongW(window, GWL_STYLE) & WS_THICKFRAME) != 0;
}

bool fullscreenBeforeAttach(HWND window) {
    const auto style = GetWindowLongW(window, GWL_STYLE);
    if (!(style & WS_POPUP) || (style & (WS_CAPTION | WS_THICKFRAME)))
        return false;
    if (style & WS_MAXIMIZE)
        return true;
    RECT outer{};
    MONITORINFO monitor{sizeof(monitor)};
    return GetWindowRect(window, &outer) &&
           GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor) &&
           EqualRect(&outer, &monitor.rcMonitor);
}

void removeVisibleFrame(HWND window) {
    const auto policy = DWMNCRP_DISABLED;
    DwmSetWindowAttribute(window, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
    // Windows 11's optional one-pixel outline. Older Windows ignores this value.
    const COLORREF noBorder = 0xfffffffe; // DWMWA_COLOR_NONE
    DwmSetWindowAttribute(window, static_cast<DWMWINDOWATTRIBUTE>(34), &noBorder, sizeof(noBorder));
}

} // namespace daw
