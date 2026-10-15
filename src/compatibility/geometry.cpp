#include "geometry.h"
#include "graphics/gdi.h"

namespace daw {
namespace {
BOOL WINAPI clientMoveWindow(HWND window, int x, int y, int width, int height, BOOL repaint) {
    const auto entryError = GetLastError();
    const bool attached = tracksWindow(window);
    SetLastError(entryError);
    if (attached)
        return TRUE;
    return MoveWindow(window, x, y, width, height, repaint);
}

LONG WINAPI clientSetWindowLong(HWND window, int index, LONG value) {
    const auto entryError = GetLastError();
    if ((index == GWL_STYLE || index == GWL_EXSTYLE) && tracksWindow(window)) {
        const auto previous = GetWindowLongA(window, index);
        SetLastError(entryError);
        return previous;
    }
    SetLastError(entryError);
    return SetWindowLongA(window, index, value);
}

BOOL WINAPI clientShowWindow(HWND window, int command) {
    const auto entryError = GetLastError();
    if (tracksWindow(window)) {
        // Late startup "show normally" must not undo launcher fullscreen or a
        // user's maximize/minimize. Teardown hide and genuine minimize still
        // run. Restoring an iconic window uses its saved native placement.
        if (command == SW_SHOWNORMAL || command == SW_SHOWDEFAULT || command == SW_MAXIMIZE ||
            (command == SW_RESTORE && !IsIconic(window)))
            command = SW_SHOW;
        else if (command == SW_SHOWNOACTIVATE)
            command = SW_SHOWNA;
    }
    SetLastError(entryError);
    return ShowWindow(window, command);
}
} // namespace

DWORD installClientGeometryImports(HMODULE image, std::vector<ImportPatch>& patches) {
    struct Hook {
        const char* name;
        void* replacement;
    };

    const Hook hooks[] = {{"MoveWindow", reinterpret_cast<void*>(clientMoveWindow)},
                          {"ShowWindow", reinterpret_cast<void*>(clientShowWindow)},
                          {"SetWindowLongA", reinterpret_cast<void*>(clientSetWindowLong)}};
    const auto user = GetModuleHandleW(L"user32.dll");
    for (const auto& hook : hooks) {
        const auto result = patchImport(image,
                                        "user32.dll",
                                        hook.name,
                                        reinterpret_cast<void*>(GetProcAddress(user, hook.name)),
                                        hook.replacement,
                                        patches);
        if (result && result != ERROR_PROC_NOT_FOUND)
            return result;
    }
    return ERROR_SUCCESS;
}
} // namespace daw
