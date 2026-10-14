#include "sizing.h"

namespace daw {
void setSizingLimits(HWND window, MINMAXINFO& limits) {
    // Use native minimum tracking size and monitor work-area maximize bounds.
    // The legacy fullscreen client may supply limits before its window becomes
    // resizable; do not retain those or introduce an aspect-ratio size grid.
    DefWindowProcW(window, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&limits));
}
} // namespace daw
