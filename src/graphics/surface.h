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
    HRESULT remember(RECT*);
    void rememberLocked(const DDSURFACEDESC&, const RECT&);
    HRESULT repopulate(IDirectDrawSurface*);
    HRESULT reapply(IDirectDrawSurface*);
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
    Surface(Draw*, IDirectDrawSurface*, DWORD);
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

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE AddAttachedSurface(IDirectDrawSurface*) override;
    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(RECT*) override;
    HRESULT STDMETHODCALLTYPE Blt(RECT*, IDirectDrawSurface*, RECT*, DWORD, DDBLTFX*) override;
    HRESULT STDMETHODCALLTYPE BltBatch(DDBLTBATCH*, DWORD, DWORD) override;
    HRESULT STDMETHODCALLTYPE BltFast(DWORD, DWORD, IDirectDrawSurface*, RECT*, DWORD) override;
    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD, IDirectDrawSurface*) override;
    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(void*, LPDDENUMSURFACESCALLBACK) override;
    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD, void*, LPDDENUMSURFACESCALLBACK) override;
    HRESULT STDMETHODCALLTYPE Flip(IDirectDrawSurface*, DWORD) override;
    HRESULT STDMETHODCALLTYPE GetAttachedSurface(DDSCAPS*, IDirectDrawSurface**) override;
    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD) override;
    HRESULT STDMETHODCALLTYPE GetCaps(DDSCAPS*) override;
    HRESULT STDMETHODCALLTYPE GetClipper(IDirectDrawClipper**) override;
    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD, DDCOLORKEY*) override;
    HRESULT STDMETHODCALLTYPE GetDC(HDC*) override;
    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD) override;
    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LONG*, LONG*) override;
    HRESULT STDMETHODCALLTYPE GetPalette(IDirectDrawPalette**) override;
    HRESULT STDMETHODCALLTYPE GetPixelFormat(DDPIXELFORMAT*) override;
    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(DDSURFACEDESC*) override;
    HRESULT STDMETHODCALLTYPE Initialize(IDirectDraw*, DDSURFACEDESC*) override;
    HRESULT STDMETHODCALLTYPE IsLost() override;
    HRESULT STDMETHODCALLTYPE Lock(RECT*, DDSURFACEDESC*, DWORD, HANDLE) override;
    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC) override;
    HRESULT STDMETHODCALLTYPE Restore() override;
    HRESULT STDMETHODCALLTYPE SetClipper(IDirectDrawClipper*) override;
    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD, DDCOLORKEY*) override;
    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG, LONG) override;
    HRESULT STDMETHODCALLTYPE SetPalette(IDirectDrawPalette*) override;
    HRESULT STDMETHODCALLTYPE Unlock(void*) override;
    HRESULT STDMETHODCALLTYPE
    UpdateOverlay(RECT*, IDirectDrawSurface*, RECT*, DWORD, DDOVERLAYFX*) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD) override;
    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD, IDirectDrawSurface*) override;
};
} // namespace daw
