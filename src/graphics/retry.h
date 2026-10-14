#pragma once
#include <windows.h>

namespace daw {
// TickCount64 avoids wraparound. No sleeps or loops on a game thread.
struct Retry {
    ULONGLONG next = 0;
    unsigned delay = 100;

    bool due(ULONGLONG now) const noexcept {
        return now >= next;
    }

    void failed(ULONGLONG now) noexcept {
        next = now + delay;
        delay = delay < 1000 ? delay * 2 : 2000;
    }

    void reset() noexcept {
        next = 0;
        delay = 100;
    }
};
} // namespace daw
