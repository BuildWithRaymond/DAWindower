#pragma once
#include <ddraw.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "window/window.h"

namespace daw {
class Surface;

// One public v1 device wrapper. Surfaces retain this owner; registry/primary
// entries are weak and disappear before a surface's final deletion. The registry
// is keyed by public wrapper pointers, never by replaceable native backing.
class Draw final : public IDirectDraw {
    std::atomic<ULONG> references_{1};
    IDirectDraw* native_;

  public:
    // Guard registry, primary and mutable surface state with this owner mutex.
    // Timer recovery uses try_lock so it cannot stall the game's rendering calls.
    std::recursive_mutex mutex;
    std::unordered_map<IDirectDrawSurface*, Surface*> registry;
    Surface* primary = nullptr; // Weak; access and final Release use mutex.
    bool primaryUsesScreenCoordinates = false;
    std::shared_ptr<WindowState> window;
    std::shared_ptr<RecoveryBinding> recovery;
    bool recoveryPending = false;
    void recoverSurfaces();
    explicit Draw(IDirectDraw* native);
    ~Draw();
    Surface* find(IDirectDrawSurface* surface);

    IDirectDraw* native() const {
        return native_;
    }

    HRESULT wrap(IDirectDrawSurface*, DWORD caps, IDirectDrawSurface**);
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE Compact() override;
    HRESULT STDMETHODCALLTYPE CreateClipper(DWORD, IDirectDrawClipper**, IUnknown*) override;
    HRESULT STDMETHODCALLTYPE CreatePalette(DWORD,
                                            PALETTEENTRY*,
                                            IDirectDrawPalette**,
                                            IUnknown*) override;
    HRESULT STDMETHODCALLTYPE CreateSurface(DDSURFACEDESC*,
                                            IDirectDrawSurface**,
                                            IUnknown*) override;
    HRESULT STDMETHODCALLTYPE DuplicateSurface(IDirectDrawSurface*, IDirectDrawSurface**) override;
    HRESULT STDMETHODCALLTYPE EnumDisplayModes(DWORD,
                                               DDSURFACEDESC*,
                                               void*,
                                               LPDDENUMMODESCALLBACK) override;
    HRESULT STDMETHODCALLTYPE EnumSurfaces(DWORD,
                                           DDSURFACEDESC*,
                                           void*,
                                           LPDDENUMSURFACESCALLBACK) override;
    HRESULT STDMETHODCALLTYPE FlipToGDISurface() override;
    HRESULT STDMETHODCALLTYPE GetCaps(DDCAPS*, DDCAPS*) override;
    HRESULT STDMETHODCALLTYPE GetDisplayMode(DDSURFACEDESC*) override;
    HRESULT STDMETHODCALLTYPE GetFourCCCodes(DWORD*, DWORD*) override;
    HRESULT STDMETHODCALLTYPE GetGDISurface(IDirectDrawSurface**) override;
    HRESULT STDMETHODCALLTYPE GetMonitorFrequency(DWORD*) override;
    HRESULT STDMETHODCALLTYPE GetScanLine(DWORD*) override;
    HRESULT STDMETHODCALLTYPE GetVerticalBlankStatus(BOOL*) override;
    HRESULT STDMETHODCALLTYPE Initialize(GUID*) override;
    HRESULT STDMETHODCALLTYPE RestoreDisplayMode() override;
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND, DWORD) override;
    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD, DWORD, DWORD) override;
    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD, HANDLE) override;
};

DDPIXELFORMAT rgb565();
} // namespace daw
