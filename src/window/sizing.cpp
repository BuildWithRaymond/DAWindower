#include "sizing.h"
#include "aspect.h"
#include "frame.h"
#include "caption.h"
#include <algorithm>
#include <cstdint>

namespace daw {
namespace {
PixelSize frameSize(HWND window) {
    return {0, frameCaptionHeight(window)};
}

struct Bounds {
    PixelSize minimum, maximum;
};

Bounds bounds(PixelSize frame) {
    MINMAXINFO native{};
    // Windows normally prepopulates this structure before sending the message;
    // DefWindowProc alone does not fill a freshly zero-initialized structure.
    native.ptMinTrackSize = {GetSystemMetrics(SM_CXMINTRACK), GetSystemMetrics(SM_CYMINTRACK)};
    native.ptMaxTrackSize = {GetSystemMetrics(SM_CXMAXTRACK), GetSystemMetrics(SM_CYMAXTRACK)};
    PixelSize minimum{std::max(1L, native.ptMinTrackSize.x - frame.width),
                      std::max(1L, native.ptMinTrackSize.y - frame.height)};
    minimum = aspectSize(minimum,
                         std::int64_t(minimum.width) * 3 >= std::int64_t(minimum.height) * 4
                             ? AspectDriver::width
                             : AspectDriver::height);
    auto maximum =
        aspectSize({std::max<LONG>(minimum.width, native.ptMaxTrackSize.x - frame.width),
                    std::max<LONG>(minimum.height, native.ptMaxTrackSize.y - frame.height)},
                   AspectDriver::fit);
    return {minimum, maximum};
}

PixelSize bounded(PixelSize frame, PixelSize requested, AspectDriver driver) {
    const auto size = aspectSize(requested, driver);
    const auto limits = bounds(frame);
    if (size.width < limits.minimum.width || size.height < limits.minimum.height)
        return limits.minimum;
    if (size.width > limits.maximum.width || size.height > limits.maximum.height)
        return limits.maximum;
    return size;
}
} // namespace

void setSizingLimits(HWND window, MINMAXINFO& limits) {
    if (!windowedFrame(window)) {
        MONITORINFO monitor{sizeof(monitor)};
        if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
            limits.ptMaxPosition = {0, 0};
            limits.ptMaxSize = {monitor.rcMonitor.right - monitor.rcMonitor.left,
                                monitor.rcMonitor.bottom - monitor.rcMonitor.top};
            limits.ptMaxTrackSize.x = std::max(limits.ptMaxTrackSize.x, limits.ptMaxSize.x);
            limits.ptMaxTrackSize.y = std::max(limits.ptMaxTrackSize.y, limits.ptMaxSize.y);
        }
        return;
    }
    const auto frame = frameSize(window);
    const auto track = bounds(frame);
    limits.ptMinTrackSize = {track.minimum.width + frame.width,
                             track.minimum.height + frame.height};
    limits.ptMaxTrackSize = {track.maximum.width + frame.width,
                             track.maximum.height + frame.height};
    MONITORINFO monitor{sizeof(monitor)};
    if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor)) {
        const auto& work = monitor.rcWork;
        const auto client = aspectSize({std::max(1L, work.right - work.left - frame.width),
                                        std::max(1L, work.bottom - work.top - frame.height)},
                                       AspectDriver::fit);
        limits.ptMaxSize = {client.width + frame.width, client.height + frame.height};
        limits.ptMaxPosition = {
            work.left - monitor.rcMonitor.left + (work.right - work.left - limits.ptMaxSize.x) / 2,
            work.top - monitor.rcMonitor.top + (work.bottom - work.top - limits.ptMaxSize.y) / 2};
    }
}

void constrainSizing(HWND window, UINT edge, RECT& proposed) {
    if (!windowedFrame(window))
        return;
    const auto frame = frameSize(window);
    PixelSize requested{std::max(1L, proposed.right - proposed.left - frame.width),
                        std::max(1L, proposed.bottom - proposed.top - frame.height)};
    const bool left = edge == WMSZ_LEFT || edge == WMSZ_TOPLEFT || edge == WMSZ_BOTTOMLEFT;
    const bool top = edge == WMSZ_TOP || edge == WMSZ_TOPLEFT || edge == WMSZ_TOPRIGHT;
    auto driver = AspectDriver::corner;
    if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM)
        driver = AspectDriver::height;
    else if (edge == WMSZ_LEFT || edge == WMSZ_RIGHT)
        driver = AspectDriver::width;
    const auto size = bounded(frame, requested, driver);
    if (left)
        proposed.left = proposed.right - size.width - frame.width;
    else
        proposed.right = proposed.left + size.width + frame.width;
    if (top)
        proposed.top = proposed.bottom - size.height - frame.height;
    else
        proposed.bottom = proposed.top + size.height + frame.height;
}

void constrainPosition(HWND window, WINDOWPOS& proposed) {
    if ((proposed.flags & SWP_NOSIZE) || IsIconic(window) || !windowedFrame(window))
        return;
    const auto frame = frameSize(window);
    if (proposed.cx <= frame.width || proposed.cy <= frame.height)
        return;
    RECT current{};
    GetClientRect(window, &current);
    PixelSize requested{proposed.cx - frame.width, proposed.cy - frame.height};
    const bool width = requested.width != current.right,
               height = requested.height != current.bottom;
    const auto driver = width && !height   ? AspectDriver::width
                        : height && !width ? AspectDriver::height
                                           : AspectDriver::fit;
    const auto size = bounded(frame, requested, driver);
    proposed.cx = size.width + frame.width;
    proposed.cy = size.height + frame.height;
}

void constrainInitialSize(HWND window) {
    if (!windowedFrame(window))
        return;
    RECT client{}, outer{};
    if (!GetClientRect(window, &client) || !GetWindowRect(window, &outer) ||
        nativeAspect({client.right, client.bottom}))
        return;
    const auto size = aspectSize({client.right, client.bottom}, AspectDriver::fit);
    if (size.width && size.height)
        SetWindowPos(window,
                     nullptr,
                     0,
                     0,
                     size.width + outer.right - outer.left - client.right,
                     size.height + outer.bottom - outer.top - client.bottom,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}
} // namespace daw
