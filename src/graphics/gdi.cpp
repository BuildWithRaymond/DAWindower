#include "gdi.h"
#include "window/window.h"
#include "runtime.h"
#include <functional>
#include <mutex>
#include <new>
#include <unordered_map>

namespace daw {
namespace {
std::mutex registryLock;
using WindowRegistry = std::unordered_map<HWND, std::weak_ptr<WindowState>>;
std::unique_ptr<WindowRegistry> registry; // Allocate only during controlled window attachment.

std::shared_ptr<WindowState> findWindow(HWND window) {
    if (!window)
        return {};
    std::lock_guard<std::mutex> guard(registryLock);
    if (!registry)
        return {};
    const auto found = registry->find(window);
    return found == registry->end() ? nullptr : found->second.lock();
}

void acquire(WindowState* state) {
    if (!state)
        return;
    AcquireSRWLockExclusive(&state->lock);
}

void release(WindowState* state) {
    if (!state)
        return;
    ReleaseSRWLockExclusive(&state->lock);
}
} // namespace

HRESULT trackWindow(HWND window, const std::shared_ptr<WindowState>& state) {
    std::lock_guard<std::mutex> guard(registryLock);
    try {
        if (!registry)
            registry = std::make_unique<WindowRegistry>();
        (*registry)[window] = state;
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

void untrackWindow(HWND window) {
    std::lock_guard<std::mutex> guard(registryLock);
    if (registry)
        registry->erase(window);
}

bool tracksWindow(HWND window) {
    const auto state = findWindow(window);
    return state && state->hwnd.load() == window;
}

BOOL WINAPI windowBitBlt(HDC destination,
                         int x,
                         int y,
                         int width,
                         int height,
                         HDC source,
                         int sourceX,
                         int sourceY,
                         DWORD operation) {
    const auto entryError = GetLastError();
    auto target = findWindow(destination ? WindowFromDC(destination) : nullptr);
    auto input = findWindow(source ? WindowFromDC(source) : nullptr);
    if (input && !input->frameAvailable())
        input.reset();
    if ((!target && !input) || (target && GetMapMode(destination) != MM_TEXT) ||
        (input && GetMapMode(source) != MM_TEXT)) {
        SetLastError(entryError);
        return BitBlt(destination, x, y, width, height, source, sourceX, sourceY, operation);
    }
    // Cursor reads and writes address logical pixels, not an enlarged or
    // occluded desktop image. Different source/destination windows lock in order.
    auto first = target ? target.get() : input.get();
    WindowState* second = nullptr;
    if (target && input && target != input) {
        second = input.get();
        if (std::less<WindowState*>{}(second, first))
            std::swap(first, second);
    }
    // Even source-only GDI calls use the HDC. Serialize HDC access; direct
    // readers of the pixel memory can still take the WindowState shared lock.
    acquire(first);
    acquire(second);
    const auto targetDC = target ? target->canvas : destination;
    const auto sourceDC = input ? input->canvas : source;
    const auto saved = target ? SaveDC(targetDC) : 0;
    if (target && !saved) {
        release(second);
        release(first);
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
    if (target) {
        SetBkColor(targetDC, GetBkColor(destination));
        SetTextColor(targetDC, GetTextColor(destination));
        auto brush = GetCurrentObject(destination, OBJ_BRUSH);
        if (brush)
            SelectObject(targetDC, brush);
        POINT origin{};
        if (GetBrushOrgEx(destination, &origin))
            SetBrushOrgEx(targetDC, origin.x, origin.y, nullptr);
        // Preserve application clipping, but not the desktop visibility region.
        auto region = CreateRectRgn(0, 0, 0, 0);
        if (region) {
            if (GetClipRgn(destination, region) == 1)
                SelectClipRgn(targetDC, region);
            DeleteObject(region);
        }
    }
    SetLastError(entryError);
    auto result = BitBlt(targetDC, x, y, width, height, sourceDC, sourceX, sourceY, operation);
    auto error = GetLastError();
    if (!GdiFlush()) {
        result = FALSE;
        error = ERROR_GEN_FAILURE;
        ++failures;
    }
    if (target) {
        RestoreDC(targetDC, saved);
        if (result) {
            target->hasFrame = true;
            ++target->generation;
            ++frames;
        }
    }
    release(second);
    release(first);
#ifdef _DEBUG
    static std::atomic<unsigned> framesReported{0}, partialReported{0}, readsReported{0};
    const bool report =
        tracing() && (input                                        ? ++readsReported <= 128
                      : width == gameWidth && height == gameHeight ? ++framesReported <= 4
                                                                   : ++partialReported <= 128);
    if (report)
        log(L"GDI canvas BitBlt dstWindow=%p srcWindow=%p dst=%d,%d size=%dx%d src=%d,%d rop=%08lx result=%d error=%lu",
            target ? target->hwnd.load() : nullptr,
            input ? input->hwnd.load() : nullptr,
            x,
            y,
            width,
            height,
            sourceX,
            sourceY,
            operation,
            result,
            result ? 0 : error);
#endif
    if (result && target)
        target->requestPresentation();
    SetLastError(error);
    return result;
}
} // namespace daw
