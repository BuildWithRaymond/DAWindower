#include "surface.h"
#include "runtime.h"
#include <utility>
#include <cstring>
#include <algorithm>

namespace daw {
// Snapshot and recovery helpers. Backups contain completed writes; caller-owned
// Lock/DC leases are not replaceable resources and may defer repair.
Surface::Surface(Draw* owner, IDirectDrawSurface* native, DWORD caps)
    : owner_(owner), native_(native), caps_(caps) {
    description_.dwSize = sizeof(description_);
    initialStatus_ = native_->GetSurfaceDesc(&description_);
    if (SUCCEEDED(initialStatus_) &&
        (!(caps_ & DDSCAPS_PRIMARYSURFACE) || (caps_ & DDSCAPS_FLIP))) {
        store_ = std::make_shared<PixelStore>();
        store_->width = description_.dwWidth;
        store_->height = description_.dwHeight;
        if (!store_->width || !store_->height || store_->width > 16384 || store_->height > 16384)
            initialStatus_ = DDERR_INVALIDPARAMS;
        else {
            store_->pixels.resize(static_cast<size_t>(store_->width) * store_->height);
            initialStatus_ = remember(nullptr);
        }
    }
    owner_->AddRef();
    ++surfaces;
}

Surface::~Surface() {
    if (back_)
        back_->Release();
    if (windowClipper_)
        windowClipper_->Release();
    if (offscreenClipper_)
        offscreenClipper_->Release();
    native_->Release();
    --surfaces;
    owner_->Release();
}

void Surface::rememberLocked(const DDSURFACEDESC& memory, const RECT& area) {
    if (!store_ || !memory.lpSurface || memory.lPitch < static_cast<LONG>(store_->width * 2))
        return;
    const auto width = static_cast<size_t>(area.right - area.left);
    for (LONG y = area.top; y < area.bottom; ++y)
        std::memcpy(store_->pixels.data() + static_cast<size_t>(y) * store_->width + area.left,
                    static_cast<const BYTE*>(memory.lpSurface) +
                        static_cast<size_t>(y - area.top) * memory.lPitch,
                    width * 2);
    nativeRevision_ = ++store_->revision;
}

HRESULT Surface::remember(RECT* changed) {
    if (!store_)
        return DD_OK; // Virtual primary uses the existing retained canvas.
    RECT area{0, 0, static_cast<LONG>(store_->width), static_cast<LONG>(store_->height)};
    if (changed) {
        RECT intersection{};
        if (!IntersectRect(&intersection, &area, changed))
            return DD_OK;
        area = intersection;
    }
    DDSURFACEDESC memory{};
    memory.dwSize = sizeof(memory);
    const auto result = native_->Lock(&area, &memory, DDLOCK_READONLY, nullptr);
    if (FAILED(result))
        return result;
    rememberLocked(memory, area);
    return native_->Unlock(memory.lpSurface);
}

void Surface::markLost() {
    if (!lost_) {
        ++failures;
        retry_.reset();
        log(L"Surface lost wrapper=%p caps=%08lx", this, caps_);
    }
    lost_ = true;
    owner_->recoveryPending = true;
}

HRESULT Surface::reapply(IDirectDrawSurface* target) {
    for (unsigned i = 0; i < 4; ++i)
        if (keySet_[i]) {
            const auto result = target->SetColorKey(keyFlags_[i], &keys_[i]);
            if (FAILED(result))
                return result;
        }
    return offscreenClipper_ ? target->SetClipper(offscreenClipper_) : DD_OK;
}

HRESULT Surface::repopulate(IDirectDrawSurface* target) {
    if ((caps_ & DDSCAPS_PRIMARYSURFACE) && owner_->window && owner_->window->frameAvailable()) {
        nativeGeneration_ = 0;
        const auto result = owner_->window->copyTo(target, nativeGeneration_);
        if (SUCCEEDED(result) && store_ && store_->width == gameWidth &&
            store_->height == gameHeight) {
            AcquireSRWLockShared(&owner_->window->lock);
            std::memcpy(store_->pixels.data(), owner_->window->pixels, gameWidth * gameHeight * 2);
            ReleaseSRWLockShared(&owner_->window->lock);
            nativeRevision_ = ++store_->revision;
        }
        return result;
    }
    if (!store_) {
        if (!owner_->window || !owner_->window->frameAvailable())
            return DD_OK;
        nativeGeneration_ = 0;
        return owner_->window->copyTo(target, nativeGeneration_);
    }
    DDSURFACEDESC memory{};
    memory.dwSize = sizeof(memory);
    auto result = target->Lock(nullptr, &memory, DDLOCK_WRITEONLY, nullptr);
    if (FAILED(result))
        return result;
    if (memory.lPitch < static_cast<LONG>(store_->width * 2)) {
        target->Unlock(memory.lpSurface);
        return DDERR_INVALIDPARAMS;
    }
    for (DWORD y = 0; y < store_->height; ++y)
        std::memcpy(static_cast<BYTE*>(memory.lpSurface) + static_cast<size_t>(y) * memory.lPitch,
                    store_->pixels.data() + static_cast<size_t>(y) * store_->width,
                    store_->width * 2);
    const auto unlocked = target->Unlock(memory.lpSurface);
    if (SUCCEEDED(unlocked))
        nativeRevision_ = store_->revision;
    return unlocked;
}

HRESULT Surface::recover() {
    if (!lost_)
        return DD_OK;
    if (!retry_.due(GetTickCount64()))
        return DDERR_SURFACELOST;
    if (locks_ || dc_) {
        retry_.failed(GetTickCount64());
        return DDERR_SURFACEBUSY;
    }
    auto result = native_->Restore();
    if (SUCCEEDED(result))
        result = repopulate(native_);
    if (SUCCEEDED(result))
        result = reapply(native_);
    if (FAILED(result) && ++restoreFailures_ >= 3) {
        // Replace only native backing, never the public COM wrapper or an HDC
        // leased to the caller. Retained pixels and metadata are authoritative.
        auto desc = description_;
        desc.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS | DDSD_PIXELFORMAT;
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        IDirectDrawSurface* replacement = nullptr;
        auto rebuilt = owner_->native()->CreateSurface(&desc, &replacement, nullptr);
        if (SUCCEEDED(rebuilt))
            rebuilt = repopulate(replacement);
        if (SUCCEEDED(rebuilt))
            rebuilt = reapply(replacement);
        if (SUCCEEDED(rebuilt)) {
            if (store_)
                store_->splitBacking = true;
            auto previous = native_;
            native_ = replacement;
            previous->Release();
            result = DD_OK;
        } else if (replacement)
            replacement->Release();
    }
    if (SUCCEEDED(result)) {
        lost_ = false;
        retry_.reset();
        restoreFailures_ = 0;
        log(L"Surface recovered wrapper=%p", this);
        if (owner_->window)
            owner_->window->requestPresentation();
    } else
        retry_.failed(GetTickCount64());
    return result;
}

HRESULT Surface::available() {
    return lost_ ? recover() : DD_OK;
}

HRESULT Surface::repairLost(Surface* source) {
    bool detected = false;
    if (native_->IsLost() == DDERR_SURFACELOST) {
        markLost();
        detected = true;
    }
    if (source && source != this && source->native_->IsLost() == DDERR_SURFACELOST) {
        source->markLost();
        detected = true;
    }
    if (!detected)
        markLost();
    auto result = available();
    const auto sourceResult = source ? source->available() : DD_OK;
    return FAILED(result) ? result : sourceResult;
}

// COM lifetime: remove weak owner entries before releasing the final wrapper.
ULONG Surface::AddRef() {
    return ++references_;
}

ULONG Surface::Release() {
    ULONG remaining;
    {
        std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
        remaining = --references_;
        if (!remaining) {
            owner_->registry.erase(this);
            if (owner_->primary == this)
                owner_->primary = nullptr;
        }
    }
    if (!remaining)
        delete this;
    return remaining;
}

HRESULT Surface::QueryInterface(REFIID id, void** output) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    if (id != IID_IUnknown && id != IID_IDirectDrawSurface) {
        log(L"Unsupported surface interface requested");
        return E_NOINTERFACE;
    }
    *output = static_cast<IDirectDrawSurface*>(this);
    AddRef();
    return S_OK;
}

// Synchronize native backing with retained logical pixels before game access.
HRESULT Surface::syncFromWindow() {
    const auto ready = available();
    if (FAILED(ready))
        return ready;
    if (store_ && store_->splitBacking && nativeRevision_ != store_->revision) {
        const auto result = repopulate(native_);
        if (result == DDERR_SURFACELOST)
            markLost();
        if (FAILED(result))
            return result;
    }
    if (!(caps_ & DDSCAPS_PRIMARYSURFACE) || !owner_->window ||
        nativeGeneration_ == owner_->window->generation.load())
        return DD_OK;
    auto result = owner_->window->copyTo(native_, nativeGeneration_);
    if (result == DDERR_SURFACELOST) {
        markLost();
        result = available();
    }
    if (SUCCEEDED(result) && store_)
        result = remember(nullptr);
    return result;
}

HRESULT Surface::present() {
    if (!(caps_ & DDSCAPS_PRIMARYSURFACE) || !owner_->window)
        return DD_OK;
    auto result = owner_->window->capture(native_, &nativeGeneration_);
    if (FAILED(result)) {
        ++failures;
        log(L"Frame capture failed: %08lx", result);
    }
    // A failed display attempt must never ask the game to repeat completed
    // writes/Unlock/ReleaseDC/Flip. The window retries retained content.
    if (result == DDERR_SURFACELOST)
        markLost();
    owner_->window->requestPresentation();
    return DD_OK;
}

// Drawing adapters preserve completed-operation results. Only a failed native
// draw may be retried once after loss repair; display retries are queued separately.
HRESULT Surface::Blt(RECT* destination,
                     IDirectDrawSurface* source,
                     RECT* sourceRect,
                     DWORD flags,
                     DDBLTFX* effects) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    auto wrapped = source ? owner_->find(source) : nullptr;
    if (source && !wrapped)
        return DDERR_INVALIDOBJECT;
    auto synchronized = syncFromWindow();
    if (SUCCEEDED(synchronized) && wrapped)
        synchronized = wrapped->syncFromWindow();
    if (FAILED(synchronized))
        return synchronized;
    constexpr DWORD unsupported =
        DDBLT_ALPHADEST | DDBLT_ALPHADESTCONSTOVERRIDE | DDBLT_ALPHADESTNEG |
        DDBLT_ALPHADESTSURFACEOVERRIDE | DDBLT_ALPHAEDGEBLEND | DDBLT_ALPHASRC |
        DDBLT_ALPHASRCCONSTOVERRIDE | DDBLT_ALPHASRCNEG | DDBLT_ALPHASRCSURFACEOVERRIDE |
        DDBLT_ZBUFFER | DDBLT_ZBUFFERDESTCONSTOVERRIDE | DDBLT_ZBUFFERDESTOVERRIDE |
        DDBLT_ZBUFFERSRCCONSTOVERRIDE | DDBLT_ZBUFFERSRCOVERRIDE;
    if (flags & unsupported)
        return DDERR_UNSUPPORTED;
    RECT local{};
    if (destination && (caps_ & DDSCAPS_PRIMARYSURFACE) && owner_->primaryUsesScreenCoordinates) {
        const auto w = owner_->window->hwnd.load();
        RECT client{};
        POINT top{destination->left, destination->top},
            bottom{destination->right, destination->bottom};
        if (!GetClientRect(w, &client) || !ScreenToClient(w, &top) || !ScreenToClient(w, &bottom))
            return DDERR_INVALIDPARAMS;
        // Windowed DirectDraw addresses the desktop primary in screen pixels.
        // Rasterize that client rectangle into the virtual frame; scaling is
        // applied once, when the stored frame fills the real window.
        const Viewport view{0, 0, client.right, client.bottom};
        const auto first = toGame(view, top.x, top.y), last = toGame(view, bottom.x, bottom.y);
        local = {first.x, first.y, last.x, last.y};
        destination = &local;
    }
    auto result =
        native_->Blt(destination, wrapped ? wrapped->native_ : nullptr, sourceRect, flags, effects);
    if (result == DDERR_SURFACELOST && SUCCEEDED(repairLost(wrapped)))
        result = native_->Blt(
            destination, wrapped ? wrapped->native_ : nullptr, sourceRect, flags, effects);
    if (result == DDERR_SURFACELOST) {
        markLost();
        if (wrapped && wrapped->native_->IsLost() == DDERR_SURFACELOST)
            wrapped->markLost();
    }
    if (SUCCEEDED(result)) {
        const auto saved = remember(destination);
        if (saved == DDERR_SURFACELOST)
            markLost();
    }
#ifdef _DEBUG
    static std::atomic<unsigned> primaryReads{0}, primaryWrites{0};
    if (tracing() &&
        ((wrapped && (wrapped->caps_ & DDSCAPS_PRIMARYSURFACE) && ++primaryReads <= 128) ||
         ((caps_ & DDSCAPS_PRIMARYSURFACE) && ++primaryWrites <= 8)))
        log(L"Blt dst=%p caps=%08lx rect=%ld,%ld,%ld,%ld src=%p srcCaps=%08lx rect=%ld,%ld,%ld,%ld flags=%08lx result=%08lx",
            this,
            caps_,
            destination ? destination->left : -1,
            destination ? destination->top : -1,
            destination ? destination->right : -1,
            destination ? destination->bottom : -1,
            source,
            wrapped ? wrapped->caps_ : 0,
            sourceRect ? sourceRect->left : -1,
            sourceRect ? sourceRect->top : -1,
            sourceRect ? sourceRect->right : -1,
            sourceRect ? sourceRect->bottom : -1,
            flags,
            result);
#endif
    return FAILED(result) ? result : present();
}

HRESULT Surface::BltFast(DWORD x, DWORD y, IDirectDrawSurface* source, RECT* rect, DWORD flags) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    auto wrapped = owner_->find(source);
    if (!wrapped)
        return DDERR_INVALIDOBJECT;
    auto synchronized = syncFromWindow();
    if (SUCCEEDED(synchronized))
        synchronized = wrapped->syncFromWindow();
    if (FAILED(synchronized))
        return synchronized;
#ifdef _DEBUG
    static std::atomic<unsigned> primaryReports{0}, primaryReads{0}, keyedReports{0};
    const bool trace = tracing() && ((caps_ & DDSCAPS_PRIMARYSURFACE) ? ++primaryReports <= 16
                                     : (wrapped->caps_ & DDSCAPS_PRIMARYSURFACE)
                                         ? ++primaryReads <= 128
                                         : (flags & DDBLTFAST_SRCCOLORKEY) && ++keyedReports <= 16);
    if (trace)
        log(L"BltFast primary=%d screenCoords=%d dst=%ld,%ld src=%p rect=%ld,%ld,%ld,%ld flags=%08lx srcPrimary=%d",
            !!(caps_ & DDSCAPS_PRIMARYSURFACE),
            owner_->primaryUsesScreenCoordinates,
            static_cast<LONG>(x),
            static_cast<LONG>(y),
            source,
            rect ? rect->left : -1,
            rect ? rect->top : -1,
            rect ? rect->right : -1,
            rect ? rect->bottom : -1,
            flags,
            !!(wrapped->caps_ & DDSCAPS_PRIMARYSURFACE));
#endif
    if ((caps_ & DDSCAPS_PRIMARYSURFACE) && owner_->primaryUsesScreenCoordinates) {
        // BltFast copies surface pixels without stretching. The retail client
        // keeps its 640x480 frame after a resize and adds the screen origin to
        // these coordinates. Scale only when painting the completed frame.
        POINT destination{static_cast<LONG>(x), static_cast<LONG>(y)};
        if (!owner_->window || !ScreenToClient(owner_->window->hwnd.load(), &destination))
            return DDERR_INVALIDPARAMS;
        x = static_cast<DWORD>(destination.x);
        y = static_cast<DWORD>(destination.y);
    }
    auto result = native_->BltFast(x, y, wrapped->native_, rect, flags);
    if (result == DDERR_SURFACELOST && SUCCEEDED(repairLost(wrapped)))
        result = native_->BltFast(x, y, wrapped->native_, rect, flags);
    if (result == DDERR_SURFACELOST) {
        markLost();
        if (wrapped->native_->IsLost() == DDERR_SURFACELOST)
            wrapped->markLost();
    }
    if (SUCCEEDED(result)) {
        RECT changed{
            static_cast<LONG>(x),
            static_cast<LONG>(y),
            static_cast<LONG>(x) + (rect ? rect->right - rect->left
                                         : static_cast<LONG>(wrapped->description_.dwWidth)),
            static_cast<LONG>(y) + (rect ? rect->bottom - rect->top
                                         : static_cast<LONG>(wrapped->description_.dwHeight))};
        if (remember(&changed) == DDERR_SURFACELOST)
            markLost();
    }
    if (FAILED(result))
        log(L"BltFast failed: %08lx primary=%d translated=%ld,%ld flags=%08lx",
            result,
            !!(caps_ & DDSCAPS_PRIMARYSURFACE),
            static_cast<LONG>(x),
            static_cast<LONG>(y),
            flags);
    return FAILED(result) ? result : present();
}

HRESULT Surface::Flip(IDirectDrawSurface* target, DWORD) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (!back_)
        return DDERR_NOTFLIPPABLE;
    if (target && target != back_)
        return DDERR_INVALIDPARAMS;
    if (locks_ || dc_ || back_->locks_ || back_->dc_)
        return DDERR_SURFACEBUSY;
    auto ready = syncFromWindow();
    if (SUCCEEDED(ready))
        ready = back_->syncFromWindow();
    if (FAILED(ready))
        return ready;
    std::swap(native_, back_->native_);
    std::swap(store_, back_->store_);
    std::swap(nativeGeneration_, back_->nativeGeneration_);
    std::swap(nativeRevision_, back_->nativeRevision_);
    reapply(native_);
    back_->reapply(back_->native_);
    return present();
}

HRESULT Surface::GetAttachedSurface(DDSCAPS* caps, IDirectDrawSurface** output) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (!caps || !back_ || !(caps->dwCaps & DDSCAPS_BACKBUFFER))
        return DDERR_NOTFOUND;
    *output = back_;
    back_->AddRef();
    return DD_OK;
}

HRESULT Surface::GetCaps(DDSCAPS* output) {
    if (!output)
        return E_POINTER;
    output->dwCaps = caps_;
    return DD_OK;
}

HRESULT Surface::GetSurfaceDesc(DDSURFACEDESC* output) {
    auto result = forward(&IDirectDrawSurface::GetSurfaceDesc, output);
    if (SUCCEEDED(result)) {
        output->ddsCaps.dwCaps = caps_;
        output->dwBackBufferCount = back_ ? 1 : 0;
    }
    return result;
}

// Native caller leases: snapshot committed writes, then end exactly one lease.
HRESULT Surface::Lock(RECT* rect, DDSURFACEDESC* output, DWORD flags, HANDLE event) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    const auto synchronized = syncFromWindow();
    if (FAILED(synchronized))
        return synchronized;
    auto result = native_->Lock(rect, output, flags, event);
    if (result == DDERR_SURFACELOST && SUCCEEDED(repairLost()))
        result = native_->Lock(rect, output, flags, event);
    if (result == DDERR_SURFACELOST)
        markLost();
    if (SUCCEEDED(result)) {
        const auto area = rect ? *rect
                               : RECT{0,
                                      0,
                                      static_cast<LONG>(description_.dwWidth),
                                      static_cast<LONG>(description_.dwHeight)};
        try {
            leases_.push_back({*output, area, flags});
        } catch (const std::bad_alloc&) {
            native_->Unlock(output->lpSurface);
            return E_OUTOFMEMORY;
        }
        ++locks_;
    }
    return result;
}

HRESULT Surface::Unlock(void* address) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (!locks_)
        return DDERR_NOTLOCKED;
    auto lease = address ? std::find_if(leases_.begin(),
                                        leases_.end(),
                                        [address](const Lease& l) {
                                            return l.memory.lpSurface == address;
                                        })
                         : leases_.end() - 1;
    if (lease == leases_.end())
        return DDERR_NOTLOCKED;
    // Loss invalidates native Lock memory. Never dereference it after IsLost.
    if (!(lease->flags & DDLOCK_READONLY) && native_->IsLost() != DDERR_SURFACELOST)
        rememberLocked(lease->memory, lease->area);
    auto result = native_->Unlock(address);
    if (SUCCEEDED(result) || result == DDERR_SURFACELOST) {
        --locks_;
        leases_.erase(lease);
    }
    if (result == DDERR_SURFACELOST) {
        // Caller ended its lease. Lost memory is invalid; do not retry Unlock.
        markLost();
    }
    if (SUCCEEDED(result) && !locks_)
        result = present();
    return result;
}

HRESULT Surface::GetDC(HDC* output) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    const auto synchronized = syncFromWindow();
    if (FAILED(synchronized))
        return synchronized;
    auto result = native_->GetDC(output);
    if (SUCCEEDED(result))
        dc_ = *output;
    if (result == DDERR_SURFACELOST && SUCCEEDED(repairLost())) {
        result = native_->GetDC(output);
        if (SUCCEEDED(result))
            dc_ = *output;
    }
    if (result == DDERR_SURFACELOST)
        markLost();
#ifdef _DEBUG
    static std::atomic<unsigned> reports{0};
    if (tracing() && (caps_ & DDSCAPS_PRIMARYSURFACE) && ++reports <= 16)
        log(L"Primary GetDC surface=%p dc=%p result=%08lx generation=%lu",
            this,
            SUCCEEDED(result) ? *output : nullptr,
            result,
            nativeGeneration_);
#endif
    return result;
}

HRESULT Surface::ReleaseDC(HDC dc) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    auto result = native_->ReleaseDC(dc);
    if (SUCCEEDED(result)) {
        dc_ = nullptr;
        if (remember(nullptr) == DDERR_SURFACELOST)
            markLost();
        present();
    } else if (result == DDERR_SURFACELOST) {
        dc_ = nullptr;
        markLost();
    }
    return result;
}

HRESULT Surface::Restore() {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (locks_ || dc_)
        return DDERR_SURFACEBUSY;
    if (!lost_) {
        const auto status = native_->IsLost();
        if (status == DDERR_SURFACELOST)
            markLost();
        else if (FAILED(status))
            return status;
    }
    auto result = available();
    if (SUCCEEDED(result) && back_)
        result = back_->Restore();
    return result;
}

HRESULT Surface::EnumAttachedSurfaces(void* context, LPDDENUMSURFACESCALLBACK callback) {
    if (!callback)
        return DDERR_INVALIDPARAMS;
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (back_) {
        DDSURFACEDESC desc{};
        desc.dwSize = sizeof(desc);
        back_->GetSurfaceDesc(&desc);
        back_->AddRef();
        callback(back_, &desc, context);
    }
    return DD_OK; // DirectDraw enumeration hands an owned reference to the callback.
}

// Additional v1 API adapters. Unsupported operations fail explicitly instead of
// exposing unwrapped interfaces or inventing game behavior.
HRESULT Surface::AddAttachedSurface(IDirectDrawSurface*) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::DeleteAttachedSurface(DWORD, IDirectDrawSurface*) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::BltBatch(DDBLTBATCH*, DWORD, DWORD) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::AddOverlayDirtyRect(RECT*) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::EnumOverlayZOrders(DWORD, void*, LPDDENUMSURFACESCALLBACK) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::UpdateOverlay(RECT*, IDirectDrawSurface*, RECT*, DWORD, DDOVERLAYFX*) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::UpdateOverlayDisplay(DWORD) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::UpdateOverlayZOrder(DWORD, IDirectDrawSurface*) {
    return DDERR_UNSUPPORTED;
}

HRESULT Surface::Initialize(IDirectDraw*, DDSURFACEDESC*) {
    return DDERR_ALREADYINITIALIZED;
}

HRESULT Surface::GetBltStatus(DWORD flags) {
    return forward(&IDirectDrawSurface::GetBltStatus, flags);
}

HRESULT Surface::GetClipper(IDirectDrawClipper** output) {
    if (!(caps_ & DDSCAPS_PRIMARYSURFACE))
        return forward(&IDirectDrawSurface::GetClipper, output);
    if (!output)
        return E_POINTER;
    *output = nullptr;
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (!windowClipper_)
        return DDERR_NOCLIPPERATTACHED;
    *output = windowClipper_;
    windowClipper_->AddRef();
    return DD_OK;
}

HRESULT Surface::GetColorKey(DWORD flags, DDCOLORKEY* output) {
    return forward(&IDirectDrawSurface::GetColorKey, flags, output);
}

HRESULT Surface::GetFlipStatus(DWORD) {
    return back_ ? DD_OK : DDERR_NOTFLIPPABLE;
}

HRESULT Surface::GetOverlayPosition(LONG* x, LONG* y) {
    return forward(&IDirectDrawSurface::GetOverlayPosition, x, y);
}

HRESULT Surface::GetPalette(IDirectDrawPalette** output) {
    return forward(&IDirectDrawSurface::GetPalette, output);
}

HRESULT Surface::GetPixelFormat(DDPIXELFORMAT* output) {
    return forward(&IDirectDrawSurface::GetPixelFormat, output);
}

HRESULT Surface::IsLost() {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (!lost_) {
        const auto status = native_->IsLost();
        if (status == DDERR_SURFACELOST)
            markLost();
        else
            return status;
    }
    return SUCCEEDED(available()) ? DD_OK : DDERR_SURFACELOST;
}

HRESULT Surface::SetClipper(IDirectDrawClipper* clipper) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    if (!(caps_ & DDSCAPS_PRIMARYSURFACE)) {
        const auto result = native_->SetClipper(clipper);
        if (SUCCEEDED(result)) {
            if (clipper)
                clipper->AddRef();
            if (offscreenClipper_)
                offscreenClipper_->Release();
            offscreenClipper_ = clipper;
        }
        return result;
    }
    if (clipper) {
        HWND window = nullptr;
        if (FAILED(clipper->GetHWnd(&window)))
            return DDERR_UNSUPPORTED;
        if (!owner_->window || window != owner_->window->hwnd.load())
            return DDERR_INVALIDPARAMS;
        clipper->AddRef();
    }
    // Desktop HWND clipping belongs to the final window DC. Applying that
    // screen-space region to a 640x480 offscreen surface corrupts/blocks updates,
    // especially while hidden or minimized. Keep its COM attachment separately.
    if (windowClipper_)
        windowClipper_->Release();
    windowClipper_ = clipper;
    return DD_OK;
}

HRESULT Surface::SetColorKey(DWORD flags, DDCOLORKEY* key) {
    std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
    const auto result = forward(&IDirectDrawSurface::SetColorKey, flags, key);
    if (SUCCEEDED(result)) {
        const DWORD types[]{DDCKEY_DESTBLT, DDCKEY_DESTOVERLAY, DDCKEY_SRCBLT, DDCKEY_SRCOVERLAY};
        for (unsigned i = 0; i < 4; ++i)
            if (flags & types[i]) {
                keySet_[i] = key != nullptr;
                keyFlags_[i] = flags;
                if (key)
                    keys_[i] = *key;
            }
    }
    log(L"SetColorKey surface=%p flags=%08lx key=%08lx..%08lx result=%08lx",
        this,
        flags,
        key ? key->dwColorSpaceLowValue : 0,
        key ? key->dwColorSpaceHighValue : 0,
        result);
    return result;
}

HRESULT Surface::SetOverlayPosition(LONG x, LONG y) {
    return forward(&IDirectDrawSurface::SetOverlayPosition, x, y);
}

HRESULT Surface::SetPalette(IDirectDrawPalette* palette) {
    return forward(&IDirectDrawSurface::SetPalette, palette);
}
} // namespace daw
