#include "draw.h"
#include "surface.h"
#include "runtime.h"
#include <new>

namespace daw {
DDPIXELFORMAT rgb565() {
    DDPIXELFORMAT format{};
    format.dwSize = sizeof(format);
    format.dwFlags = DDPF_RGB;
    format.dwRGBBitCount = 16;
    format.dwRBitMask = 0xf800;
    format.dwGBitMask = 0x07e0;
    format.dwBBitMask = 0x001f;
    return format;
}

Draw::Draw(IDirectDraw* native) : native_(native), recovery(std::make_shared<RecoveryBinding>()) {
    recovery->draw = this;
    ++devices;
}

Draw::~Draw() {
    AcquireSRWLockExclusive(&recovery->lifetime);
    recovery->draw = nullptr;
    ReleaseSRWLockExclusive(&recovery->lifetime);
    native_->Release();
    --devices;
}

void RecoveryBinding::service() {
    if (!TryAcquireSRWLockShared(&lifetime))
        return;
    if (draw)
        draw->recoverSurfaces();
    ReleaseSRWLockShared(&lifetime);
}

void Draw::recoverSurfaces() {
    std::unique_lock<std::recursive_mutex> guard(mutex, std::try_to_lock);
    if (!guard.owns_lock() || !recoveryPending)
        return;
    recoveryPending = false;
    unsigned attempted = 0;
    const auto now = GetTickCount64();
    for (const auto& entry : registry) {
        auto s = entry.second;
        if (!s->needsRecovery())
            continue;
        if (s->recoveryDue(now) && attempted < 4) {
            ++attempted;
            s->recover();
        }
        if (s->needsRecovery())
            recoveryPending = true;
    }
}

Surface* Draw::find(IDirectDrawSurface* surface) {
    const auto found = registry.find(surface);
    return found != registry.end() ? found->second : nullptr;
}

HRESULT Draw::wrap(IDirectDrawSurface* native, DWORD caps, IDirectDrawSurface** output) {
    Surface* wrapper = nullptr;
    try {
        wrapper = new Surface(this, native, caps);
        if (FAILED(wrapper->initialStatus())) {
            auto result = wrapper->initialStatus();
            delete wrapper;
            return result;
        }
        registry.emplace(wrapper, wrapper);
    } catch (const std::bad_alloc&) {
        if (wrapper)
            delete wrapper;
        else
            native->Release();
        return E_OUTOFMEMORY;
    }
    *output = wrapper;
    return DD_OK;
}

HRESULT Draw::QueryInterface(REFIID id, void** output) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    if (id != IID_IUnknown && id != IID_IDirectDraw) {
        log(L"Unsupported DirectDraw interface requested");
        return E_NOINTERFACE;
    }
    *output = static_cast<IDirectDraw*>(this);
    AddRef();
    return S_OK;
}

ULONG Draw::AddRef() {
    return ++references_;
}

ULONG Draw::Release() {
    const auto remaining = --references_;
    if (!remaining)
        delete this;
    return remaining;
}

HRESULT Draw::CreateSurface(DDSURFACEDESC* input, IDirectDrawSurface** output, IUnknown* outer) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    if (!input || input->dwSize != sizeof(*input))
        return DDERR_INVALIDPARAMS;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    const auto caps = input->ddsCaps.dwCaps;
    log(L"CreateSurface caps=%08lx flags=%08lx size=%lux%lu",
        caps,
        input->dwFlags,
        input->dwWidth,
        input->dwHeight);
    const bool isPrimary = (caps & DDSCAPS_PRIMARYSURFACE) != 0;
    if (isPrimary && primary)
        return DDERR_PRIMARYSURFACEALREADYEXISTS;
    if (isPrimary && !window)
        return DDERR_NOCOOPERATIVELEVELSET;
    if (caps & (DDSCAPS_OVERLAY | DDSCAPS_ZBUFFER | DDSCAPS_MIPMAP | DDSCAPS_TEXTURE))
        return DDERR_UNSUPPORTED;
    if (isPrimary && (input->dwFlags & DDSD_BACKBUFFERCOUNT) && input->dwBackBufferCount > 1)
        return DDERR_UNSUPPORTED;
    auto desc = *input;
    if ((desc.dwFlags & DDSD_PIXELFORMAT) &&
        (desc.ddpfPixelFormat.dwFlags != DDPF_RGB || desc.ddpfPixelFormat.dwRGBBitCount != 16 ||
         desc.ddpfPixelFormat.dwRBitMask != 0xf800 || desc.ddpfPixelFormat.dwGBitMask != 0x07e0 ||
         desc.ddpfPixelFormat.dwBBitMask != 0x001f))
        return DDERR_INVALIDPIXELFORMAT;
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.dwBackBufferCount = 0;
    if (isPrimary) {
        desc.dwWidth = gameWidth;
        desc.dwHeight = gameHeight;
    }
    if (!desc.dwWidth || !desc.dwHeight)
        return DDERR_INVALIDPARAMS;
    desc.ddpfPixelFormat = rgb565();
    IDirectDrawSurface* nativeSurface = nullptr;
    auto result = native_->CreateSurface(&desc, &nativeSurface, nullptr);
    if (FAILED(result)) {
        log(L"CreateSurface failed: %08lx", result);
        return result;
    }
    result = wrap(nativeSurface, caps, output);
    if (FAILED(result))
        return result;
    auto surface = find(*output);
    if (isPrimary)
        primary = surface;
    if (isPrimary && (caps & DDSCAPS_FLIP)) {
        IDirectDrawSurface* nativeBack = nullptr;
        result = native_->CreateSurface(&desc, &nativeBack, nullptr);
        IDirectDrawSurface* back = nullptr;
        if (SUCCEEDED(result))
            result = wrap(nativeBack,
                          DDSCAPS_BACKBUFFER | DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY,
                          &back);
        if (FAILED(result)) {
            surface->Release();
            *output = nullptr;
            return result;
        }
        surface->setBack(find(back));
    }
    return DD_OK;
}

HRESULT Draw::DuplicateSurface(IDirectDrawSurface* source, IDirectDrawSurface** output) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto original = find(source);
    if (!original || original == primary)
        return DDERR_INVALIDPARAMS;
    IDirectDrawSurface* duplicate = nullptr;
    auto result = native_->DuplicateSurface(original->native(), &duplicate);
    if (SUCCEEDED(result))
        result = wrap(duplicate, DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY, output);
    if (SUCCEEDED(result))
        find(*output)->sharePixels(original);
    return result;
}

HRESULT Draw::SetCooperativeLevel(HWND w, DWORD flags) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    log(L"SetCooperativeLevel hwnd=%p flags=%08lx callerThread=%lu windowThread=%lu",
        w,
        flags,
        GetCurrentThreadId(),
        GetWindowThreadProcessId(w, nullptr));
    if (!w)
        return flags == DDSCL_NORMAL ? native_->SetCooperativeLevel(nullptr, DDSCL_NORMAL)
                                     : DDERR_INVALIDPARAMS;
    if (window && window->hwnd.load() != w)
        return DDERR_HWNDALREADYSET;
    if (GetWindowThreadProcessId(w, nullptr) != GetCurrentThreadId())
        return DDERR_INVALIDPARAMS;
    auto result = native_->SetCooperativeLevel(
        w, DDSCL_NORMAL | (flags & (DDSCL_MULTITHREADED | DDSCL_FPUPRESERVE | DDSCL_FPUSETUP)));
    if (FAILED(result))
        return result;
    try {
        if (!window)
            window = std::make_shared<WindowState>();
    } catch (const std::bad_alloc&) {
        return E_OUTOFMEMORY;
    }
    window->recovery = recovery;
    result = attachWindow(window, w);
    if (FAILED(result)) {
        window.reset();
        log(L"Window attachment failed: %08lx", result);
    } else
        primaryUsesScreenCoordinates = (flags & DDSCL_EXCLUSIVE) == 0;
    return result;
}

HRESULT Draw::SetDisplayMode(DWORD width, DWORD height, DWORD bits) {
    if (width != gameWidth || height != gameHeight || bits != 16) {
        log(L"Unsupported mode %lux%lu/%lu", width, height, bits);
        return DDERR_UNSUPPORTEDMODE;
    }
    return DD_OK;
}

HRESULT Draw::GetDisplayMode(DDSURFACEDESC* desc) {
    if (!desc || desc->dwSize != sizeof(*desc))
        return DDERR_INVALIDPARAMS;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto result = native_->GetDisplayMode(desc);
    if (FAILED(result))
        return result;
    desc->ddpfPixelFormat = rgb565();
    if (window) {
        desc->dwWidth = gameWidth;
        desc->dwHeight = gameHeight;
        desc->lPitch = gameWidth * 2;
    }
    return DD_OK;
}

HRESULT Draw::EnumDisplayModes(DWORD,
                               DDSURFACEDESC* filter,
                               void* context,
                               LPDDENUMMODESCALLBACK callback) {
    if (!callback)
        return DDERR_INVALIDPARAMS;
    DDSURFACEDESC mode{};
    mode.dwSize = sizeof(mode);
    mode.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_REFRESHRATE;
    mode.dwWidth = gameWidth;
    mode.dwHeight = gameHeight;
    mode.dwRefreshRate = 60;
    mode.ddpfPixelFormat = rgb565();
    if (filter &&
        (((filter->dwFlags & DDSD_WIDTH) && filter->dwWidth != gameWidth) ||
         ((filter->dwFlags & DDSD_HEIGHT) && filter->dwHeight != gameHeight) ||
         ((filter->dwFlags & DDSD_PIXELFORMAT) && filter->ddpfPixelFormat.dwRGBBitCount != 16)))
        return DD_OK;
    callback(&mode, context);
    return DD_OK;
}

HRESULT Draw::GetGDISurface(IDirectDrawSurface** output) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (!primary)
        return DDERR_NOTFOUND;
    *output = primary;
    primary->AddRef();
    return DD_OK;
}

HRESULT Draw::Compact() {
    return native_->Compact();
}

HRESULT Draw::CreateClipper(DWORD flags, IDirectDrawClipper** output, IUnknown* outer) {
    return native_->CreateClipper(flags, output, outer);
}

HRESULT Draw::CreatePalette(DWORD flags,
                            PALETTEENTRY* entries,
                            IDirectDrawPalette** output,
                            IUnknown* outer) {
    return native_->CreatePalette(flags, entries, output, outer);
}

HRESULT Draw::EnumSurfaces(DWORD, DDSURFACEDESC*, void*, LPDDENUMSURFACESCALLBACK) {
    return DDERR_UNSUPPORTED;
}

HRESULT Draw::FlipToGDISurface() {
    return DD_OK;
}

HRESULT Draw::GetCaps(DDCAPS* hardware, DDCAPS* software) {
    return native_->GetCaps(hardware, software);
}

HRESULT Draw::GetFourCCCodes(DWORD* count, DWORD* codes) {
    return native_->GetFourCCCodes(count, codes);
}

HRESULT Draw::GetMonitorFrequency(DWORD* value) {
    return native_->GetMonitorFrequency(value);
}

HRESULT Draw::GetScanLine(DWORD* value) {
    return native_->GetScanLine(value);
}

HRESULT Draw::GetVerticalBlankStatus(BOOL* value) {
    return native_->GetVerticalBlankStatus(value);
}

HRESULT Draw::Initialize(GUID* guid) {
    return native_->Initialize(guid);
}

HRESULT Draw::RestoreDisplayMode() {
    return DD_OK;
}

HRESULT Draw::WaitForVerticalBlank(DWORD flags, HANDLE event) {
    return native_->WaitForVerticalBlank(flags, event);
}

HRESULT WINAPI createDraw(GUID* guid, IDirectDraw** output, IUnknown* outer) {
    if (!output)
        return E_POINTER;
    *output = nullptr;
    if (outer)
        return CLASS_E_NOAGGREGATION;
    IDirectDraw* native = nullptr;
    auto result = realCreateDraw(guid, &native, nullptr);
    if (FAILED(result))
        return result;
    Draw* wrapper = nullptr;
    try {
        wrapper = new Draw(native);
    } catch (const std::bad_alloc&) {
        native->Release();
        return E_OUTOFMEMORY;
    }
    if (!wrapper) {
        native->Release();
        return E_OUTOFMEMORY;
    }
    *output = wrapper;
    return DD_OK;
}
} // namespace daw
