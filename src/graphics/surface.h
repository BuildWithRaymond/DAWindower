#pragma once
#include "draw.h"
#include "retry.h"
#include <vector>

namespace daw {
// Keep client-visible COM identity stable while native_ may be restored/replaced.
// Mutable backing, attachments, snapshots and leases use owner_->mutex.
class Surface final : public IDirectDrawSurface {
    std::atomic<ULONG> references_{1};
    Draw* owner_;
    IDirectDrawSurface* native_;
    DWORD caps_;
    Surface* back_ = nullptr; // Owned attachment reference.
    unsigned locks_ = 0;
    HDC dc_ = nullptr;
    IDirectDrawClipper* windowClipper_ = nullptr;
    DWORD nativeGeneration_ = 0;

    // CPU pixels are the recovery authority. Duplicates share this store and
    // revisions even if a backing replacement splits their native allocations.
    struct PixelStore {
        DWORD width = 0, height = 0;
        std::vector<WORD> pixels;
        ULONGLONG revision = 0;
        bool splitBacking = false; // Duplicates may share pixels across replacement.
    };

    std::shared_ptr<PixelStore> store_;
    ULONGLONG nativeRevision_ = 0;
    DDSURFACEDESC description_{};

    // A caller's native Lock pointer remains borrowed until Unlock. Never replace
    // backing or dereference lost native memory while an outstanding lease exists.
    struct Lease {
        DDSURFACEDESC memory;
        RECT area;
        DWORD flags;
    };

    std::vector<Lease> leases_;
    DDCOLORKEY keys_[4]{};
    DWORD keyFlags_[4]{};
    bool keySet_[4]{};
    bool lost_ = false;
    Retry retry_;
    unsigned restoreFailures_ = 0;
    HRESULT initialStatus_ = DD_OK;
    IDirectDrawClipper* offscreenClipper_ = nullptr;
    HRESULT remember(RECT* changed);
    void rememberLocked(const DDSURFACEDESC& memory, const RECT& area);
    HRESULT repopulate(IDirectDrawSurface* target);
    HRESULT reapply(IDirectDrawSurface* target);
    void markLost();
    HRESULT repairLost(Surface* source = nullptr);
    HRESULT available();
    HRESULT syncFromWindow();
    HRESULT present();

    template <class F, class... A> HRESULT forward(F method, A... args) {
        std::lock_guard<std::recursive_mutex> lock(owner_->mutex);
        return (native_->*method)(args...);
    }

  public:
    Surface(Draw* owner, IDirectDrawSurface* native, DWORD caps);
    ~Surface();

    IDirectDrawSurface* native() const {
        return native_;
    }

    HRESULT initialStatus() const {
        return initialStatus_;
    }

    bool needsRecovery() const {
        return lost_;
    }

    bool recoveryDue(ULONGLONG now) const {
        return lost_ && retry_.due(now);
    }

    HRESULT recover(); // Owner mutex held; bounded and safe with active leases.

    void sharePixels(Surface* source) {
        store_ = source->store_;
    }

    void setBack(Surface* back) {
        back_ = back;
    }

    // COM declarations follow the DirectDrawSurface v1 interface order.
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** output) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE AddAttachedSurface(IDirectDrawSurface* attachment) override;
    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(RECT* dirty) override;
    HRESULT STDMETHODCALLTYPE Blt(RECT* destination,
                                  IDirectDrawSurface* source,
                                  RECT* sourceRect,
                                  DWORD flags,
                                  DDBLTFX* effects) override;
    HRESULT STDMETHODCALLTYPE BltBatch(DDBLTBATCH* batch, DWORD count, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE
    BltFast(DWORD x, DWORD y, IDirectDrawSurface* source, RECT* rect, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD flags,
                                                    IDirectDrawSurface* attachment) override;
    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(void* context,
                                                   LPDDENUMSURFACESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD flags,
                                                 void* context,
                                                 LPDDENUMSURFACESCALLBACK callback) override;
    HRESULT STDMETHODCALLTYPE Flip(IDirectDrawSurface* target, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetAttachedSurface(DDSCAPS* caps,
                                                 IDirectDrawSurface** output) override;
    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetCaps(DDSCAPS* output) override;
    HRESULT STDMETHODCALLTYPE GetClipper(IDirectDrawClipper** output) override;
    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD flags, DDCOLORKEY* output) override;
    HRESULT STDMETHODCALLTYPE GetDC(HDC* output) override;
    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LONG* x, LONG* y) override;
    HRESULT STDMETHODCALLTYPE GetPalette(IDirectDrawPalette** output) override;
    HRESULT STDMETHODCALLTYPE GetPixelFormat(DDPIXELFORMAT* output) override;
    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(DDSURFACEDESC* output) override;
    HRESULT STDMETHODCALLTYPE Initialize(IDirectDraw* draw, DDSURFACEDESC* description) override;
    HRESULT STDMETHODCALLTYPE IsLost() override;
    HRESULT STDMETHODCALLTYPE Lock(RECT* rect,
                                   DDSURFACEDESC* output,
                                   DWORD flags,
                                   HANDLE event) override;
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC dc) override;
    HRESULT STDMETHODCALLTYPE Restore() override;
    HRESULT STDMETHODCALLTYPE SetClipper(IDirectDrawClipper* clipper) override;
    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD flags, DDCOLORKEY* key) override;
    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG x, LONG y) override;
    HRESULT STDMETHODCALLTYPE SetPalette(IDirectDrawPalette* palette) override;
    HRESULT STDMETHODCALLTYPE Unlock(void* address) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlay(RECT* sourceRect,
                                            IDirectDrawSurface* destination,
                                            RECT* destinationRect,
                                            DWORD flags,
                                            DDOVERLAYFX* effects) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD flags,
                                                  IDirectDrawSurface* reference) override;
};
} // namespace daw
