#include "frame.h"
#include <dwmapi.h>
#include <algorithm>

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

int resizeHit(HWND window, POINT screen) {
    RECT r{};
    if (!GetWindowRect(window, &r) || !PtInRect(&r, screen))
        return HTNOWHERE;
    if (!windowedFrame(window) || IsZoomed(window))
        return HTCLIENT;
    const auto grip = std::max(4, MulDiv(6, GetDpiForWindow(window), 96));
    const bool left = screen.x < r.left + grip, right = screen.x >= r.right - grip;
    const bool top = screen.y < r.top + grip, bottom = screen.y >= r.bottom - grip;
    if (top && left)
        return HTTOPLEFT;
    if (top && right)
        return HTTOPRIGHT;
    if (bottom && left)
        return HTBOTTOMLEFT;
    if (bottom && right)
        return HTBOTTOMRIGHT;
    if (left)
        return HTLEFT;
    if (right)
        return HTRIGHT;
    if (top)
        return HTTOP;
    if (bottom)
        return HTBOTTOM;
    return HTCLIENT;
}
} // namespace daw
