#include "graphics/surface.h"
#include "runtime.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>

namespace daw {
CreateDraw realCreateDraw = nullptr;
std::atomic<DWORD> devices{0}, surfaces{0}, windows{0}, frames{0}, failures{0}, showCalls{0},
    setCalls{0};

bool tracing() noexcept {
    return false;
}

void log(const wchar_t*, ...) noexcept {
}
} // namespace daw

static void check(bool value, const char* what) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

class FaultSurface final : public IDirectDrawSurface {
    ULONG refs = 1;

  public:
    std::vector<WORD> pixels = std::vector<WORD>(64, 0x07e0);
    bool lost = false, locked = false;
    unsigned restoreCalls = 0, failedRestores = 0, blitCalls = 0;
    DDCOLORKEY key{};

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override {
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++refs;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        auto n = --refs;
        if (!n)
            delete this;
        return n;
    }

    HRESULT STDMETHODCALLTYPE GetSurfaceDesc(DDSURFACEDESC* d) override {
        d->dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
        d->dwWidth = d->dwHeight = 8;
        d->lPitch = 16;
        d->ddpfPixelFormat = daw::rgb565();
        d->ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        return DD_OK;
    }

    HRESULT STDMETHODCALLTYPE Lock(RECT* r, DDSURFACEDESC* d, DWORD, HANDLE) override {
        if (lost)
            return DDERR_SURFACELOST;
        if (locked)
            return DDERR_SURFACEBUSY;
        GetSurfaceDesc(d);
        d->lpSurface = pixels.data() + (r ? r->top * 8 + r->left : 0);
        locked = true;
        return DD_OK;
    }

    HRESULT STDMETHODCALLTYPE Unlock(void*) override {
        locked = false;
        return lost ? DDERR_SURFACELOST : DD_OK;
    }

    HRESULT STDMETHODCALLTYPE IsLost() override {
        return lost ? DDERR_SURFACELOST : DD_OK;
    }

    HRESULT STDMETHODCALLTYPE Restore() override {
        ++restoreCalls;
        if (failedRestores) {
            --failedRestores;
            return DDERR_SURFACELOST;
        }
        if (lost) {
            std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(0));
            key = {};
        }
        lost = false;
        return DD_OK;
    }

    HRESULT STDMETHODCALLTYPE
    Blt(RECT*, IDirectDrawSurface* s, RECT*, DWORD f, DDBLTFX* fx) override {
        ++blitCalls;
        if (lost || (s && static_cast<FaultSurface*>(s)->lost))
            return DDERR_SURFACELOST;
        if (s)
            pixels = static_cast<FaultSurface*>(s)->pixels;
        else if (f & DDBLT_COLORFILL)
            std::fill(pixels.begin(), pixels.end(), static_cast<WORD>(fx->dwFillColor));
        return DD_OK;
    }

    HRESULT STDMETHODCALLTYPE BltFast(DWORD, DWORD, IDirectDrawSurface* s, RECT*, DWORD) override {
        return Blt(nullptr, s, nullptr, 0, nullptr);
    }

    HRESULT STDMETHODCALLTYPE SetColorKey(DWORD, DDCOLORKEY* k) override {
        key = k ? *k : DDCOLORKEY{};
        return DD_OK;
    }

    HRESULT STDMETHODCALLTYPE GetColorKey(DWORD, DDCOLORKEY* k) override {
        *k = key;
        return DD_OK;
    }

    HRESULT STDMETHODCALLTYPE AddAttachedSurface(IDirectDrawSurface*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE AddOverlayDirtyRect(RECT*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE BltBatch(DDBLTBATCH*, DWORD, DWORD) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE DeleteAttachedSurface(DWORD, IDirectDrawSurface*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE EnumAttachedSurfaces(void*, LPDDENUMSURFACESCALLBACK) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE EnumOverlayZOrders(DWORD, void*, LPDDENUMSURFACESCALLBACK) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE Flip(IDirectDrawSurface*, DWORD) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetAttachedSurface(DDSCAPS*, IDirectDrawSurface**) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetBltStatus(DWORD) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetCaps(DDSCAPS*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetClipper(IDirectDrawClipper**) override {
        return DDERR_NOCLIPPERATTACHED;
    }

    HRESULT STDMETHODCALLTYPE GetDC(HDC*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetFlipStatus(DWORD) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetOverlayPosition(LONG*, LONG*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetPalette(IDirectDrawPalette**) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE GetPixelFormat(DDPIXELFORMAT*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE Initialize(IDirectDraw*, DDSURFACEDESC*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE SetClipper(IDirectDrawClipper*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE SetOverlayPosition(LONG, LONG) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE SetPalette(IDirectDrawPalette*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE
    UpdateOverlay(RECT*, IDirectDrawSurface*, RECT*, DWORD, DDOVERLAYFX*) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE UpdateOverlayDisplay(DWORD) override {
        return DDERR_UNSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE UpdateOverlayZOrder(DWORD, IDirectDrawSurface*) override {
        return DDERR_UNSUPPORTED;
    }
};

int main() {
    IDirectDraw* native = nullptr;
    check(SUCCEEDED(DirectDrawCreate(nullptr, &native, nullptr)), "native test owner");
    auto owner = new daw::Draw(native);
    auto a = new FaultSurface, b = new FaultSurface;
    IDirectDrawSurface* dst = nullptr;
    IDirectDrawSurface* src = nullptr;
    check(SUCCEEDED(owner->wrap(a, DDSCAPS_OFFSCREENPLAIN, &dst)), "destination wrapper");
    check(SUCCEEDED(owner->wrap(b, DDSCAPS_OFFSCREENPLAIN, &src)), "source wrapper");
    DDSURFACEDESC d{};
    d.dwSize = sizeof(d);
    check(SUCCEEDED(src->Lock(nullptr, &d, 0, nullptr)), "write source pixels");
    static_cast<WORD*>(d.lpSurface)[7] = 0xf800;
    check(SUCCEEDED(src->Unlock(d.lpSurface)), "commit source pixels");
    DDCOLORKEY key{0x1234, 0x1234};
    check(SUCCEEDED(src->SetColorKey(DDCKEY_SRCBLT, &key)), "remember color key");
    a->lost = b->lost = true;
    check(SUCCEEDED(dst->Blt(nullptr, src, nullptr, 0, nullptr)),
          "recover source and destination loss automatically");
    check(a->pixels[7] == 0xf800 && a->pixels[0] == 0x07e0,
          "restore logical pixels, not blank allocation");
    check(b->key.dwColorSpaceLowValue == 0x1234, "reapply color key");
    void* identity = nullptr;
    check(SUCCEEDED(src->QueryInterface(IID_IUnknown, &identity)) && identity == src,
          "COM identity survives loss");
    static_cast<IUnknown*>(identity)->Release();
    check(SUCCEEDED(src->Lock(nullptr, &d, 0, nullptr)), "hold lock during loss");
    b->lost = true;
    const auto calls = b->restoreCalls;
    check(src->Restore() == DDERR_SURFACEBUSY && b->restoreCalls == calls,
          "never restore outstanding lock");
    // The lease ends even when native Unlock reports loss. Do not retry Unlock.
    check(src->Unlock(d.lpSurface) == DDERR_SURFACELOST, "truthful failed unlock");
    check(src->Unlock(d.lpSurface) == DDERR_NOTLOCKED,
          "ended lost lease cannot spin retail Unlock/Restore loop");
    b->failedRestores = 100;
    check(FAILED(src->Restore()), "temporary restore failure");
    const auto attempts = b->restoreCalls;
    for (unsigned i = 0; i < 1000; ++i)
        src->IsLost();
    check(b->restoreCalls == attempts, "unavailable surface uses bounded backoff");
    auto c = new FaultSurface, e = new FaultSurface;
    std::fill(e->pixels.begin(), e->pixels.end(), static_cast<WORD>(0x001f));
    IDirectDrawSurface* front = nullptr;
    IDirectDrawSurface* attached = nullptr;
    check(
        SUCCEEDED(owner->wrap(c, DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX, &front)),
        "flip root wrapper");
    check(SUCCEEDED(owner->wrap(e, DDSCAPS_BACKBUFFER, &attached)),
          "independent backbuffer wrapper");
    static_cast<daw::Surface*>(front)->setBack(static_cast<daw::Surface*>(attached));
    attached->AddRef();
    check(SUCCEEDED(front->Flip(nullptr, 0)), "completed logical flip");
    c->lost = e->lost = true;
    check(SUCCEEDED(front->Restore()), "restore whole emulated flip chain");
    check(c->pixels[0] == 0x07e0 && e->pixels[0] == 0x001f,
          "both sides retain distinct pixels after flip and loss");
    DDSCAPS caps{};
    caps.dwCaps = DDSCAPS_BACKBUFFER;
    IDirectDrawSurface* same = nullptr;
    check(SUCCEEDED(front->GetAttachedSurface(&caps, &same)) && same == attached,
          "attachment identity survives flip recovery");
    same->Release();
    attached->Release();
    front->Release();
    check(SUCCEEDED(owner->SetCooperativeLevel(nullptr, DDSCL_NORMAL)),
          "native recreation owner ready");
    auto f = new FaultSurface;
    IDirectDrawSurface* rebuilt = nullptr;
    check(SUCCEEDED(owner->wrap(f, DDSCAPS_OFFSCREENPLAIN, &rebuilt)), "recreation wrapper");
    check(SUCCEEDED(rebuilt->Lock(nullptr, &d, 0, nullptr)), "write pixels before recreation");
    static_cast<WORD*>(d.lpSurface)[3] = 0xf800;
    rebuilt->Unlock(d.lpSurface);
    rebuilt->SetColorKey(DDCKEY_SRCBLT, &key);
    f->lost = true;
    f->failedRestores = 3;
    check(FAILED(rebuilt->Restore()), "first failed restore remains bounded");
    Sleep(120);
    check(FAILED(rebuilt->Restore()), "second failed restore remains bounded");
    Sleep(220);
    check(SUCCEEDED(rebuilt->Restore()), "third failure recreates native backing");
    check(SUCCEEDED(rebuilt->Lock(nullptr, &d, DDLOCK_READONLY, nullptr)),
          "read recreated native backing");
    check(static_cast<WORD*>(d.lpSurface)[3] == 0xf800 &&
              static_cast<WORD*>(d.lpSurface)[0] == 0x07e0,
          "recreated native backing retains pixels");
    rebuilt->Unlock(d.lpSurface);
    DDCOLORKEY restoredKey{};
    check(SUCCEEDED(rebuilt->GetColorKey(DDCKEY_SRCBLT, &restoredKey)) &&
              restoredKey.dwColorSpaceLowValue == key.dwColorSpaceLowValue,
          "recreated native backing retains key");
    check(SUCCEEDED(rebuilt->QueryInterface(IID_IUnknown, &identity)) && identity == rebuilt,
          "recreated backing retains public identity");
    static_cast<IUnknown*>(identity)->Release();
    rebuilt->Release();
    src->Release();
    dst->Release();
    owner->Release();
    std::puts(
        "PASS: automatic loss recovery, retained pixels/key/identity, lock safety, bounded retries");
}
