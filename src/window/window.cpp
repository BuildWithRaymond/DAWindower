#include "window.h"
#include "sizing.h"
#include "frame.h"
#include "icon.h"
#include "caption.h"
#include "runtime.h"
#include "graphics/gdi.h"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <emmintrin.h>
#include <wtsapi32.h>

namespace daw {
namespace {
constexpr UINT_PTR subclassId = 0x44415731;

struct Bitmap565 {
    BITMAPINFOHEADER header;
    DWORD masks[3];
};

thread_local bool painting = false;

void expandPresentation(const WORD* source, DWORD* destination) {
    static_assert((gameWidth * gameHeight) % 8 == 0);
    const auto zero = _mm_setzero_si128();
    auto expand = [](__m128i value) {
        const auto red = _mm_slli_epi32(_mm_and_si128(value, _mm_set1_epi32(0xf800)), 8);
        const auto green = _mm_slli_epi32(_mm_and_si128(value, _mm_set1_epi32(0x07e0)), 5);
        const auto blue = _mm_slli_epi32(_mm_and_si128(value, _mm_set1_epi32(0x001f)), 3);
        const auto bgr = _mm_or_si128(_mm_or_si128(red, green), blue);
        // Replicate high color bits into the low bits, matching GDI's RGB565
        // conversion exactly, including full-intensity white and cursor colors.
        return _mm_or_si128(
            bgr,
            _mm_or_si128(_mm_and_si128(_mm_srli_epi32(bgr, 5), _mm_set1_epi32(0x070007)),
                         _mm_and_si128(_mm_srli_epi32(bgr, 6), _mm_set1_epi32(0x000300))));
    };
    for (int offset = 0; offset < gameWidth * gameHeight; offset += 8) {
        const auto packed = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source + offset));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + offset),
                         expand(_mm_unpacklo_epi16(packed, zero)));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(destination + offset + 4),
                         expand(_mm_unpackhi_epi16(packed, zero)));
    }
}

// Native frame arrows temporarily borrow visibility from the game's hidden OS
// cursor. This ownership is separate from logical software-cursor composition.
struct FrameCursor {
    HCURSOR shape = nullptr;
    unsigned visibilityIncrements = 0;
    bool sizing = false;

    static HCURSOR resizeShape(int hit) {
        LPCWSTR resource = nullptr;
        switch (hit) {
            case HTLEFT:
            case HTRIGHT:
                resource = IDC_SIZEWE;
                break;
            case HTTOP:
            case HTBOTTOM:
                resource = IDC_SIZENS;
                break;
            case HTTOPLEFT:
            case HTBOTTOMRIGHT:
                resource = IDC_SIZENWSE;
                break;
            case HTTOPRIGHT:
            case HTBOTTOMLEFT:
                resource = IDC_SIZENESW;
                break;
            default:
                return nullptr;
        }
        return LoadCursorW(nullptr, resource);
    }

    void show(HWND window, HCURSOR cursor) {
        shape = cursor;
        SetCursor(shape);
        // Dark Ages hides the OS cursor for its software cursor. Contribute
        // only enough increments to show the frame arrow, and later undo only
        // our own calls. Never force the game's count to a guessed baseline.
        while (visibilityIncrements < 64) {
            const auto count = ShowCursor(TRUE);
            if (count > 0) {
                ShowCursor(FALSE);
                break;
            }
            ++visibilityIncrements;
            if (count == 0)
                break;
        }
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_NONCLIENT | TME_LEAVE, window, 0};
        TrackMouseEvent(&tracking);
    }

    void restore() {
        while (visibilityIncrements) {
            ShowCursor(FALSE);
            --visibilityIncrements;
        }
        shape = nullptr;
        sizing = false;
    }

    ~FrameCursor() {
        restore();
    }
};

struct WindowOwner {
    std::shared_ptr<WindowState> state;
    bool attaching = true;
    bool destroyed = false;
    FrameCursor cursor;
};

void traceWindow(HWND window, const wchar_t* reason) {
#ifdef _DEBUG
    if (!tracing())
        return;
    RECT client{}, outer{};
    GetClientRect(window, &client);
    GetWindowRect(window, &outer);
    auto dc = GetDC(window);
    RECT clip{};
    const auto clipType = GetClipBox(dc, &clip);
    POINT windowOrigin{}, viewportOrigin{};
    SIZE windowExtent{}, viewportExtent{};
    GetWindowOrgEx(dc, &windowOrigin);
    GetViewportOrgEx(dc, &viewportOrigin);
    GetWindowExtEx(dc, &windowExtent);
    GetViewportExtEx(dc, &viewportExtent);
    log(L"%s hwnd=%p root=%p classStyle=%08lx windowStyle=%08lx visible=%d iconic=%d client=%ldx%ld outer=%ld,%ld,%ld,%ld",
        reason,
        window,
        GetAncestor(window, GA_ROOT),
        GetClassLongW(window, GCL_STYLE),
        GetWindowLongW(window, GWL_STYLE),
        IsWindowVisible(window),
        IsIconic(window),
        client.right,
        client.bottom,
        outer.left,
        outer.top,
        outer.right,
        outer.bottom);
    log(L"Window DC mode=%d windowOrg=%ld,%ld viewportOrg=%ld,%ld windowExt=%ld,%ld viewportExt=%ld,%ld clip=%d:%ld,%ld,%ld,%ld",
        GetMapMode(dc),
        windowOrigin.x,
        windowOrigin.y,
        viewportOrigin.x,
        viewportOrigin.y,
        windowExtent.cx,
        windowExtent.cy,
        viewportExtent.cx,
        viewportExtent.cy,
        clipType,
        clip.left,
        clip.top,
        clip.right,
        clip.bottom);
    if (dc)
        ReleaseDC(window, dc);
#else
    (void)window;
    (void)reason;
#endif
}

bool isPress(UINT message) {
    return message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
           message == WM_XBUTTONDOWN || message == WM_LBUTTONDBLCLK ||
           message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDBLCLK ||
           message == WM_XBUTTONDBLCLK;
}

// Keep the host's window behavior running; intercept only compatibility work.
// Any DefSubclassProc call may synchronously destroy the window and its binding.
LRESULT CALLBACK subclass(HWND w, UINT m, WPARAM a, LPARAM b, UINT_PTR id, DWORD_PTR reference) {
    auto owner = reinterpret_cast<WindowOwner*>(reference);
    auto& state = *owner->state;
    if (m == WM_NCDESTROY) {
        owner->cursor.restore();
        if (state.recoveryTimer)
            KillTimer(w, state.recoveryTimer);
        WTSUnRegisterSessionNotification(w);
        state.hwnd.store(nullptr);
        state.presentationPending.store(false);
        untrackWindow(w);
        RemoveWindowSubclass(w, subclass, id);
        const auto result = DefSubclassProc(w, m, a, b);
        --windows;
        // SetWindowLong/SetWindowPos can synchronously destroy the window.
        // The attachment stack owns this binding until those calls return.
        if (owner->attaching)
            owner->destroyed = true;
        else
            delete owner;
        return result;
    }
    if (m == WM_NCCALCSIZE && b) {
        auto r = a ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(b)->rgrc[0] : reinterpret_cast<RECT*>(b);
        const auto requested = *r;
        auto retained = owner->state;
        DefSubclassProc(w, m, a, b);
        if (retained->hwnd.load() == w) {
            *r = requested;
        }
        return 0; // The shared caption subclass reserves its own strip.
    }
    if (m == WM_SETCURSOR && reinterpret_cast<HWND>(a) == w) {
        auto& cursor = owner->cursor;
        auto shape =
            cursor.sizing && cursor.shape ? cursor.shape : FrameCursor::resizeShape(LOWORD(b));
        if (!shape && LOWORD(b) != HTCLIENT && LOWORD(b) != HTNOWHERE)
            shape = LoadCursorW(nullptr, IDC_ARROW);
        if (shape) {
            cursor.show(w, shape);
            return TRUE;
        }
        cursor.restore(); // The host continues to own its client-area cursor.
    }
    if (m == WM_ENTERSIZEMOVE)
        owner->cursor.sizing = true;
    if (m == WM_EXITSIZEMOVE || m == WM_CANCELMODE || (m == WM_ACTIVATEAPP && !a) ||
        (m == WM_ACTIVATE && LOWORD(a) == WA_INACTIVE) ||
        (m == WM_NCMOUSELEAVE && !owner->cursor.sizing))
        owner->cursor.restore();
    if (m == state.presentationMessage && a == reinterpret_cast<WPARAM>(&state)) {
        if (state.presentationPending.exchange(false) && FAILED(state.present()))
            log(L"Queued presentation failed");
        return 0;
    }
    if (m == WM_TIMER && a == state.recoveryTimer && state.recoveryTimer) {
        state.serviceRecovery();
        return 0;
    }
    if (m == WM_DISPLAYCHANGE || m == WM_DEVICECHANGE || m == WM_WTSSESSION_CHANGE ||
        m == WM_POWERBROADCAST || (m == WM_ACTIVATEAPP && a) ||
        (m == WM_SIZE && a != SIZE_MINIMIZED))
        state.recoveryHint();
    if (m == WM_GETMINMAXINFO) {
        const bool attaching = owner->attaching;
        auto retained = owner->state;
        const auto result = DefSubclassProc(w, m, a, b);
        if (!attaching && retained->hwnd.load() == w) {
            setSizingLimits(w, *reinterpret_cast<MINMAXINFO*>(b));
        }
        return result;
    }
    if (m == WM_SIZING || m == WM_WINDOWPOSCHANGING) {
        const bool attaching = owner->attaching;
        auto retained = owner->state;
        const auto result = DefSubclassProc(w, m, a, b);
        if (!attaching && retained->hwnd.load() == w && b) {
            if (m == WM_SIZING)
                constrainSizing(w, static_cast<UINT>(a), *reinterpret_cast<RECT*>(b));
            else
                constrainPosition(w, *reinterpret_cast<WINDOWPOS*>(b));
        }
        return m == WM_SIZING ? TRUE : result;
    }
    if (m == WM_ERASEBKGND && state.frameAvailable())
        return 1;
    if (m == WM_PAINT) {
        // Keep host paint callbacks running. If they draw a new frame, wait
        // for its queued presentation so cursor writes can finish first.
        // Otherwise repaint retained content immediately. The host may destroy HWND.
        auto retained = owner->state;
        const auto result = DefSubclassProc(w, m, a, b);
        if (retained->hwnd.load() == w && retained->frameAvailable() &&
            !retained->presentationPending.load()) {
            retained->present();
        }
        return result;
    }
    if (m == WM_PRINTCLIENT) {
        RECT r{};
        GetClientRect(w, &r);
        if (!state.paint(reinterpret_cast<HDC>(a), r.right, r.bottom))
            ++failures;
        return 0;
    }
    if (m == WM_SIZE || m == WM_DISPLAYCHANGE) {
        traceWindow(w, m == WM_SIZE ? L"WM_SIZE" : L"WM_DISPLAYCHANGE");
        InvalidateRect(w, nullptr, FALSE);
    }
    if (m == WM_DPICHANGED) {
        // This message is delivered only for a DPI-aware window. Never change the
        // process DPI context after its windows have been created.
        const auto r = reinterpret_cast<const RECT*>(b);
        SetWindowPos(w,
                     nullptr,
                     r->left,
                     r->top,
                     r->right - r->left,
                     r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        InvalidateRect(w, nullptr, FALSE);
        return 0;
    }
    if (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) {
        RECT r{};
        GetClientRect(w, &r);
        POINT p{GET_X_LPARAM(b), GET_Y_LPARAM(b)};
        const bool wheel = m == WM_MOUSEWHEEL || m == WM_MOUSEHWHEEL;
        if (wheel)
            ScreenToClient(w, &p);
        const auto mapped = toGame(viewport(r.right, r.bottom), p.x, p.y);
        if (isPress(m) && !mapped.inside)
            return m == WM_XBUTTONDOWN || m == WM_XBUTTONDBLCLK ? TRUE : 0;
        p = {mapped.x, mapped.y};
        if (wheel)
            ClientToScreen(w, &p); // WM_MOUSEWHEEL retains its screen-coordinate ABI.
        b = MAKELPARAM(static_cast<SHORT>(p.x), static_cast<SHORT>(p.y));
    }
    return DefSubclassProc(w, m, a, b);
}
} // namespace

HRESULT attachWindow(const std::shared_ptr<WindowState>& state, HWND w) {
    if (!state->pixels || !state->canvas || !state->presentationPixels)
        return E_OUTOFMEMORY;
    if (!state->presentationMessage)
        return E_FAIL;
    if (!IsWindow(w) || GetWindowThreadProcessId(w, nullptr) != GetCurrentThreadId())
        return DDERR_INVALIDPARAMS;
    if (GetWindowLongW(w, GWL_STYLE) & WS_CHILD)
        return DDERR_INVALIDPARAMS;
    DWORD_PTR existing = 0;
    if (GetWindowSubclass(w, subclass, subclassId, &existing))
        return reinterpret_cast<WindowOwner*>(existing)->state.get() == state.get()
                   ? DD_OK
                   : DDERR_HWNDALREADYSET;
    auto owner = new (std::nothrow) WindowOwner{state};
    if (!owner)
        return E_OUTOFMEMORY;
    if (!SetWindowSubclass(w, subclass, subclassId, reinterpret_cast<DWORD_PTR>(owner))) {
        delete owner;
        return E_FAIL;
    }
    state->hwnd.store(w);
    ++windows;
    auto fail = [owner, w, &state] {
        if (!owner->destroyed) {
            RemoveWindowSubclass(w, subclass, subclassId);
            state->hwnd.store(nullptr);
            --windows;
        }
        delete owner;
        return E_FAIL;
    };
    RECT client{};
    GetClientRect(w, &client);
    auto style = GetWindowLongW(w, GWL_STYLE);
    const bool fullscreen = fullscreenBeforeAttach(w);
    const auto newStyle = (style & ~WS_CAPTION) | WS_POPUP | WS_THICKFRAME | WS_SYSMENU |
                          WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    RECT outer{0,
               0,
               client.right > 0 ? client.right : gameWidth,
               client.bottom > 0 ? client.bottom : gameHeight};
    // Both editions use the shared caption without native frame margins.
    // A launcher that already maximized a bare popup owns its bounds and style.
    SetLastError(0);
    const auto oldStyle = fullscreen ? style : SetWindowLongW(w, GWL_STYLE, newStyle);
    if (owner->destroyed || (!oldStyle && GetLastError()))
        return fail();
    if (!SetWindowPos(w,
                      nullptr,
                      0,
                      0,
                      outer.right - outer.left,
                      outer.bottom - outer.top,
                      SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED |
                          (fullscreen ? SWP_NOSIZE : 0))) {
        if (!owner->destroyed)
            SetWindowLongW(w, GWL_STYLE, style);
        return fail();
    }
    if (owner->destroyed)
        return fail();
    removeVisibleFrame(w);
    if (FAILED(trackWindow(w, state))) {
        SetWindowLongW(w, GWL_STYLE, style);
        return fail();
    }
    state->recoveryTimer = SetTimer(w, reinterpret_cast<UINT_PTR>(state.get()), 100, nullptr);
    if (!state->recoveryTimer) {
        untrackWindow(w);
        SetWindowLongW(w, GWL_STYLE, style);
        return fail();
    }
    // The desktop watchdog also works when session notification registration is
    // unavailable, and detects secure desktop changes for background windows.
    WTSRegisterSessionNotification(w, NOTIFY_FOR_THIS_SESSION);
    if (!attachCaption(w))
        log(L"Caption attachment failed");
    if (owner->destroyed)
        return fail();
    constrainInitialSize(w);
    if (owner->destroyed)
        return fail();
    applyGameIcon(w);
    if (owner->destroyed)
        return fail();
    owner->attaching = false;
    traceWindow(w, L"Attached");
    return DD_OK;
}

WindowState::WindowState() {
    presentationPixels.reset(new (std::nothrow) DWORD[gameWidth * gameHeight]);
    presentationMessage = RegisterWindowMessageW(L"DAWindower.Present.2026");
    Bitmap565 info{{sizeof(BITMAPINFOHEADER),
                    gameWidth,
                    -gameHeight,
                    1,
                    16,
                    BI_BITFIELDS,
                    gameWidth * gameHeight * 2,
                    0,
                    0,
                    0,
                    0},
                   {0xf800, 0x07e0, 0x001f}};
    void* data = nullptr;
    bitmap = CreateDIBSection(
        nullptr, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &data, nullptr, 0);
    canvas = CreateCompatibleDC(nullptr);
    if (bitmap && canvas)
        previousBitmap = SelectObject(canvas, bitmap);
    if (previousBitmap && previousBitmap != HGDI_ERROR) {
        pixels = static_cast<WORD*>(data);
        std::memset(pixels, 0, gameWidth * gameHeight * 2);
    }
}

WindowState::~WindowState() {
    if (canvas && previousBitmap && previousBitmap != HGDI_ERROR)
        SelectObject(canvas, previousBitmap);
    if (bitmap)
        DeleteObject(bitmap);
    if (canvas)
        DeleteDC(canvas);
}

bool WindowState::frameAvailable() {
    AcquireSRWLockShared(&lock);
    const auto available = hasFrame;
    ReleaseSRWLockShared(&lock);
    return available;
}

HRESULT WindowState::capture(IDirectDrawSurface* surface, DWORD* capturedGeneration) {
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    auto result = surface->Lock(nullptr, &desc, DDLOCK_READONLY | DDLOCK_WAIT, nullptr);
    if (FAILED(result))
        return result; // Caller restores lost content and retries; never spin here.
    if (desc.dwWidth != gameWidth || desc.dwHeight != gameHeight ||
        desc.ddpfPixelFormat.dwRGBBitCount != 16 || desc.lPitch < gameWidth * 2) {
        surface->Unlock(desc.lpSurface);
        return DDERR_INVALIDPIXELFORMAT;
    }
    AcquireSRWLockExclusive(&lock);
    GdiFlush();
    for (int y = 0; y < gameHeight; ++y)
        std::memcpy(pixels + y * gameWidth,
                    static_cast<const BYTE*>(desc.lpSurface) + y * desc.lPitch,
                    gameWidth * 2);
    hasFrame = true;
    *capturedGeneration = ++generation;
    ReleaseSRWLockExclusive(&lock);
    result = surface->Unlock(desc.lpSurface);
    if (SUCCEEDED(result))
        ++frames;
    return result;
}

HRESULT WindowState::copyTo(IDirectDrawSurface* surface, DWORD& copiedGeneration) {
    DDSURFACEDESC desc{};
    desc.dwSize = sizeof(desc);
    auto result = surface->Lock(nullptr, &desc, DDLOCK_WRITEONLY | DDLOCK_WAIT, nullptr);
    if (FAILED(result))
        return result;
    if (desc.dwWidth != gameWidth || desc.dwHeight != gameHeight || desc.lPitch < gameWidth * 2 ||
        desc.ddpfPixelFormat.dwRGBBitCount != 16) {
        surface->Unlock(desc.lpSurface);
        return DDERR_INVALIDPIXELFORMAT;
    }
    AcquireSRWLockShared(&lock);
    for (int y = 0; y < gameHeight; ++y)
        std::memcpy(static_cast<BYTE*>(desc.lpSurface) + y * desc.lPitch,
                    pixels + y * gameWidth,
                    gameWidth * 2);
    const auto copied = generation.load();
    ReleaseSRWLockShared(&lock);
    result = surface->Unlock(desc.lpSurface);
    if (SUCCEEDED(result))
        copiedGeneration = copied;
    return result;
}

bool WindowState::paint(HDC dc, int width, int height) {
    if (width <= 0 || height <= 0)
        return true;
    if (!dc || !presentationPixels)
        return false;
    if (painting)
        return true;
    painting = true;
    RECT client{0, 0, width, height};
    const auto v = viewport(width, height);
    BITMAPINFO dibInfo{};
    dibInfo.bmiHeader = {sizeof(BITMAPINFOHEADER),
                         gameWidth,
                         -gameHeight,
                         1,
                         32,
                         BI_RGB,
                         gameWidth * gameHeight * 4,
                         0,
                         0,
                         0,
                         0};
    const auto previous = SetStretchBltMode(dc, COLORONCOLOR);
    AcquireSRWLockShared(&lock);
    bool ok = true;
    if (hasFrame) {
        // Convert the completed frame at presentation. Scaling RGB565 directly
        // can take GDI's expensive downscale path even one pixel below native.
        expandPresentation(pixels, presentationPixels.get());
        const auto lines = StretchDIBits(dc,
                                         v.x,
                                         v.y,
                                         v.width,
                                         v.height,
                                         0,
                                         0,
                                         gameWidth,
                                         gameHeight,
                                         presentationPixels.get(),
                                         &dibInfo,
                                         DIB_RGB_COLORS,
                                         SRCCOPY);
        ok = lines != 0 && lines != GDI_ERROR;
#ifdef _DEBUG
        if (tracing() && (width != tracedWidth || height != tracedHeight || !ok)) {
            tracedWidth = width;
            tracedHeight = height;
            RECT clip{};
            const auto clipType = GetClipBox(dc, &clip);
            log(L"Paint dc=%p dcWindow=%p size=%dx%d viewport=%d,%d,%d,%d clip=%d:%ld,%ld,%ld,%ld lines=%d generation=%lu",
                dc,
                WindowFromDC(dc),
                width,
                height,
                v.x,
                v.y,
                v.width,
                v.height,
                clipType,
                clip.left,
                clip.top,
                clip.right,
                clip.bottom,
                lines,
                generation.load());
        }
#endif
    } else
        FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    ReleaseSRWLockShared(&lock);
    if (previous)
        SetStretchBltMode(dc, previous);
    painting = false;
    return ok;
}

HRESULT WindowState::requestPresentation() {
    const auto w = hwnd.load();
    if (!w || presentationPending.exchange(true))
        return DD_OK;
    if (PostMessageW(w, presentationMessage, reinterpret_cast<WPARAM>(this), 0))
        return DD_OK;
    const auto error = GetLastError();
    presentationPending.store(false);
    ++failures;
    retryPresentation.store(true);
    log(L"Queue presentation failed: %lu", error);
    return HRESULT_FROM_WIN32(error);
}

HRESULT WindowState::present() {
    const auto w = hwnd.load();
#ifdef _DEBUG
    static std::atomic<unsigned> reports{0};
    if (tracing() && ++reports <= 8)
        traceWindow(w, L"Present");
#endif
    if (!w)
        return DD_OK;
    const auto now = GetTickCount64();
    if (retryPresentation.load() && !presentationRetry.due(now))
        return S_FALSE;
    if (!desktopAvailable || IsIconic(w) || !IsWindowVisible(w)) {
        if (retryPresentation.load())
            presentationRetry.failed(now);
        return S_FALSE;
    }
    RECT r{};
    if (!GetClientRect(w, &r)) {
        retryPresentation.store(true);
        presentationRetry.failed(now);
        return E_FAIL;
    }
    auto dc = GetDC(w);
    const bool ok = paint(dc, r.right, r.bottom);
    if (dc)
        ReleaseDC(w, dc);
    if (!ok) {
        ++failures;
        log(L"GDI presentation failed: %lu", GetLastError());
        retryPresentation.store(true);
        presentationRetry.failed(now);
    } else {
        retryPresentation.store(false);
        presentationRetry.reset();
    }
    return ok ? DD_OK : E_FAIL;
}

void WindowState::recoveryHint() {
    retryPresentation.store(true);
    presentationRetry.reset();
    requestPresentation();
}

void WindowState::serviceRecovery() {
    if (auto binding = recovery.lock())
        binding->service();
    const auto now = GetTickCount64();
    if (now >= desktopCheck) {
        desktopCheck = now + 1000;
        auto input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
        wchar_t inputName[128]{}, windowName[128]{};
        DWORD length = 0;
        const auto thread = GetWindowThreadProcessId(hwnd.load(), nullptr);
        const bool available =
            input &&
            GetUserObjectInformationW(input, UOI_NAME, inputName, sizeof(inputName), &length) &&
            GetUserObjectInformationW(
                GetThreadDesktop(thread), UOI_NAME, windowName, sizeof(windowName), &length) &&
            std::wcscmp(inputName, windowName) == 0;
        if (input)
            CloseDesktop(input);
        if (available != desktopAvailable) {
            desktopAvailable = available;
            recoveryHint();
        }
    }
    // Queue to the existing cursor transaction boundary. A retry must never
    // paint the cursor-erasure intermediate frame or race pending game writes.
    if (retryPresentation.load() && presentationRetry.due(now) && !presentationPending.load())
        requestPresentation();
}
} // namespace daw
