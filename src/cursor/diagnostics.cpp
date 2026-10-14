#include "diagnostics.h"
#include "runtime.h"

namespace daw {
int WINAPI observeShowCursor(BOOL show) {
    const auto count = ShowCursor(show);
    const auto error = GetLastError();
    if (++showCalls <= 32)
        log(L"ShowCursor(%d) -> %d, thread=%lu", show, count, GetCurrentThreadId());
    SetLastError(error);
    return count;
}

HCURSOR WINAPI observeSetCursor(HCURSOR cursor) {
    const auto call = ++setCalls;
    const auto previous = SetCursor(cursor);
    const auto error = GetLastError();
    if (call <= 32)
        log(L"SetCursor(%p) -> %p, thread=%lu", cursor, previous, GetCurrentThreadId());
    SetLastError(error);
    return previous;
}
} // namespace daw
