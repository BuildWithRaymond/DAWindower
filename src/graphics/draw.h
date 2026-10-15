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

    HRESULT wrap(IDirectDrawSurface* native, DWORD caps, IDirectDrawSurface** output);
    // COM declarations follow the DirectDraw v1 interface order.
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** output) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE Compact() override;
    HRESULT STDMETHODCALLTYPE CreateClipper(DWORD flags,
                                            IDirectDrawClipper** output,
                                            IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE CreatePalette(DWORD flags,
                                            PALETTEENTRY* entries,
                                            IDirectDrawPalette** output,
                                            IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE CreateSurface(DDSURFACEDESC* input,
                                            IDirectDrawSurface** output,
                                            IUnknown* outer) override;
    HRESULT STDMETHODCALLTYPE DuplicateSurface(IDirectDrawSurface* source,
                                               IDirectDrawSurface** output) override;
    HRESULT STDMETHODCALLTYPE EnumDisplayModes(DWORD flags,
                                               DDSURFACEDESC* filter,
                                               void* context,
                                               LPDDENUMMODESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE EnumSurfaces(DWORD flags,
                                           DDSURFACEDESC* filter,
                                           void* context,
                                           LPDDENUMSURFACESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE FlipToGDISurface() override;
    HRESULT STDMETHODCALLTYPE GetCaps(DDCAPS* hardware, DDCAPS* software) override;
    HRESULT STDMETHODCALLTYPE GetDisplayMode(DDSURFACEDESC* desc) override;
    HRESULT STDMETHODCALLTYPE GetFourCCCodes(DWORD* count, DWORD* codes) override;
    HRESULT STDMETHODCALLTYPE GetGDISurface(IDirectDrawSurface** output) override;
    HRESULT STDMETHODCALLTYPE GetMonitorFrequency(DWORD* value) override;
    HRESULT STDMETHODCALLTYPE GetScanLine(DWORD* value) override;
    HRESULT STDMETHODCALLTYPE GetVerticalBlankStatus(BOOL* value) override;
    HRESULT STDMETHODCALLTYPE Initialize(GUID* guid) override;
    HRESULT STDMETHODCALLTYPE RestoreDisplayMode() override;
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND window, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE SetDisplayMode(DWORD width, DWORD height, DWORD bits) override;
    HRESULT STDMETHODCALLTYPE WaitForVerticalBlank(DWORD flags, HANDLE event) override;
};

DDPIXELFORMAT rgb565();
} // namespace daw
