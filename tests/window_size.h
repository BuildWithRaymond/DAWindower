#pragma once
#include <windows.h>

// Both editions own their non-client layout. Generic style-based frame metrics
// cannot describe either the borderless standard window or X's caption.
inline bool adjustForClient(HWND window, RECT& r) {
    RECT outer{}, client{};
    POINT origin{};
    if (!GetWindowRect(window, &outer) || !GetClientRect(window, &client) ||
        !ClientToScreen(window, &origin))
        return false;
    r.left -= origin.x - outer.left;
    r.top -= origin.y - outer.top;
    r.right += outer.right - origin.x - client.right;
    r.bottom += outer.bottom - origin.y - client.bottom;
    return true;
}

// Simulate a launcher/Windows action rather than a legacy client IAT call.
inline BOOL nativeShowWindow(HWND window, int command) {
    const auto show = reinterpret_cast<decltype(&ShowWindow)>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "ShowWindow"));
    return show(window, command);
}
