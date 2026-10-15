#include "icon.h"

namespace daw {
namespace {
BOOL CALLBACK firstIcon(HMODULE module, LPCWSTR, LPWSTR name, LONG_PTR parameter) {
    // Like dawnd's ExtractIconA(..., 0), use the first executable icon group.
    // LoadIcon shares the already-loaded module's resource: Windows owns its
    // lifetime, so repeated windows need no private HICONs or companion asset.
    *reinterpret_cast<HICON*>(parameter) = LoadIconW(module, name);
    return FALSE;
}
} // namespace

HICON smallWindowIcon(HWND window) {
    auto icon = reinterpret_cast<HICON>(SendMessageW(window, WM_GETICON, ICON_SMALL, 0));
    if (icon || !IsWindow(window))
        return icon;
    icon = reinterpret_cast<HICON>(GetClassLongPtrW(window, GCLP_HICONSM));
    if (icon)
        return icon;
    icon = reinterpret_cast<HICON>(SendMessageW(window, WM_GETICON, ICON_BIG, 0));
    return icon || !IsWindow(window)
               ? icon
               : reinterpret_cast<HICON>(GetClassLongPtrW(window, GCLP_HICON));
}

void applyGameIcon(HWND window) {
    if (smallWindowIcon(window) || !IsWindow(window))
        return; // Preserve launcher-supplied window icons and host class icons.
    const auto module = reinterpret_cast<HMODULE>(GetClassLongPtrW(window, GCLP_HMODULE));
    if (!module)
        return;
    HICON icon = nullptr;
    EnumResourceNamesW(module, RT_GROUP_ICON, firstIcon, reinterpret_cast<LONG_PTR>(&icon));
    if (!icon)
        return; // Missing icon resources must never prevent game startup.
    SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
    if (IsWindow(window))
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
}
} // namespace daw
