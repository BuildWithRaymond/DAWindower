#include "caption.h"
#ifdef DAW_X
#include "x/launch.h"
#endif
#include "runtime.h"
#include "window/icon.h"
#include <commctrl.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <algorithm>
#include <cwchar>
#include <new>
#include <atomic>
#include <memory>
#include <process.h>

namespace daw {
namespace {
#ifdef DAW_X
using x::currentClientPaths;
using x::launchClient;
using x::LaunchResult;
using x::stageName;
constexpr UINT_PTR frameId = 0x44415831;
constexpr UINT nativeCommand = 0xe820, newCommand = 0xe810;
constexpr int firstButton = 0;
#else
constexpr UINT_PTR frameId = 0x44414331;
constexpr int firstButton = 2;
#endif
// Button values index layout and tooltip slots; the base edition skips X actions.
enum class Button { none = -1, newClient, native, minimize, maximize, close };
constexpr int buttonSlots = static_cast<int>(Button::close) + 1;
constexpr int buttonCount = buttonSlots - firstButton;

#ifdef DAW_X
struct LaunchStatus {
    std::atomic<bool> complete{false};
    LaunchResult result;
};

struct LaunchWork {
    std::wstring executable, dll;
    std::shared_ptr<LaunchStatus> status;
};

unsigned __stdcall launchWorker(void* parameter) {
    std::unique_ptr<LaunchWork> work(static_cast<LaunchWork*>(parameter));
    work->status->result = launchClient(work->executable.c_str(), work->dll.c_str());
    work->status->complete.store(true, std::memory_order_release);
    return 0;
}
#endif

struct Layout {
    RECT outer{}, client{}, caption{}, buttons[buttonSlots]{};
};

Layout layout(HWND w) {
    RECT screen{}, client{};
    GetWindowRect(w, &screen);
    GetClientRect(w, &client);
    POINT origin{};
    ClientToScreen(w, &origin);
    OffsetRect(&client, origin.x - screen.left, origin.y - screen.top);
    Layout l;
    l.outer = {0, 0, screen.right - screen.left, screen.bottom - screen.top};
    l.client = client;
    const auto border = std::max<LONG>(0, client.left);
    l.caption = {border, border, l.outer.right - border, client.top};
    // Very narrow windows keep every action reachable without imposing a new
    // minimum size or a size breakpoint on the game viewport.
    const int width =
        std::min<int>(MulDiv(36, GetDpiForWindow(w), 96),
                      std::max<LONG>(1, (l.caption.right - l.caption.left) / buttonCount));
    for (int i = firstButton; i < buttonSlots; ++i)
        l.buttons[i] = {l.caption.right - (buttonSlots - i) * width,
                        l.caption.top,
                        l.caption.right - (buttonSlots - i - 1) * width,
                        l.caption.bottom};
    return l;
}

RECT iconBounds(HWND window, const Layout& l) {
    const auto dpi = GetDpiForWindow(window);
    const int width = GetSystemMetricsForDpi(SM_CXSMICON, dpi);
    const int height = GetSystemMetricsForDpi(SM_CYSMICON, dpi);
    const auto left = l.caption.left + MulDiv(12, dpi, 96);
    const auto top = l.caption.top + (l.caption.bottom - l.caption.top - height) / 2;
    if (left + width + MulDiv(8, dpi, 96) > l.buttons[firstButton].left)
        return {};
    return {left, top, left + width, top + height};
}

POINT localPoint(HWND w, LPARAM packed, bool clientCoordinates = false) {
    POINT p{GET_X_LPARAM(packed), GET_Y_LPARAM(packed)};
    if (clientCoordinates)
        ClientToScreen(w, &p);
    RECT r{};
    GetWindowRect(w, &r);
    p.x -= r.left;
    p.y -= r.top;
    return p;
}

Button buttonAt(const Layout& l, POINT p) {
    for (int i = firstButton; i < buttonSlots; ++i)
        if (PtInRect(&l.buttons[i], p))
            return static_cast<Button>(i);
    return Button::none;
}

bool nativeSize(HWND w, int width = 640, int height = 480) {
    if (IsZoomed(w) || IsIconic(w))
        ShowWindow(w, SW_RESTORE);
    RECT r{}, c{};
    if (!IsWindow(w) || !GetWindowRect(w, &r) || !GetClientRect(w, &c))
        return false;
    return SetWindowPos(w,
                        nullptr,
                        0,
                        0,
                        width + r.right - r.left - c.right,
                        height + r.bottom - r.top - c.bottom,
                        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

class Frame {
  public:
    HWND window = nullptr;
    unsigned calls = 0;
    bool destroyed = false;
    bool refreshing = false;
    Button hot = Button::none, pressed = Button::none;
    HFONT font = nullptr;
    HWND tooltip = nullptr;
    UINT_PTR hoverTimer = 0;
    UINT_PTR titleTimer = 0;
#ifdef DAW_X
    UINT_PTR launchTimer = 0;
    std::shared_ptr<LaunchStatus> launch;
    wchar_t launchError[256]{};
#endif
    wchar_t title[256]{};

    ~Frame() {
        if (tooltip)
            DestroyWindow(tooltip);
        if (font)
            DeleteObject(font);
    }

    void refreshStyle() {
        if (refreshing || destroyed)
            return;
        refreshing = true;
        removeVisibleFrame(window);
        SetWindowTheme(window, L"", L"");
        if (font)
            DeleteObject(font);
        font = CreateFontW(-MulDiv(13, GetDpiForWindow(window), 96),
                           0,
                           0,
                           0,
                           FW_SEMIBOLD,
                           FALSE,
                           FALSE,
                           FALSE,
                           DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY,
                           DEFAULT_PITCH,
                           L"Segoe UI");
        if (tooltip)
            SendMessageW(tooltip, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
        refreshing = false;
    }

    bool refreshTitle(bool* changed = nullptr) {
        wchar_t next[256]{};
        // Let USER32 perform text conversion and report the host's accepted
        // title rather than interpreting a WM_SETTEXT payload ourselves.
        GetWindowTextW(window, next, 256);
        if (destroyed)
            return false;
        const bool different = std::wcscmp(title, next) != 0;
        if (different)
            wcscpy_s(title, next);
        if (changed)
            *changed = different;
        return true;
    }

#ifdef DAW_X
    void updateLaunch() {
        if (launch && launch->complete.load(std::memory_order_acquire)) {
            KillTimer(window, launchTimer);
            launchTimer = 0;
            const auto result = launch->result;
            launch.reset();
            if (result.error) {
                swprintf_s(launchError,
                           L"New client failed: %s (Windows %lu). Click to retry.",
                           stageName(result.stage),
                           result.error);
                MessageBeep(MB_ICONWARNING);
            }
            EnableMenuItem(GetSystemMenu(window, FALSE), newCommand, MF_BYCOMMAND | MF_ENABLED);
            paint();
        }
    }
#endif

    void paint(HDC supplied = nullptr) {
        if (destroyed || IsIconic(window) || !windowedFrame(window))
            return;
        if (!refreshTitle())
            return;
        const auto icon = smallWindowIcon(window);
        if (destroyed)
            return; // A host WM_GETICON callback may destroy this HWND.
        const auto l = layout(window);
        HDC dc = supplied ? supplied : GetWindowDC(window);
        if (!dc)
            return;
        const auto saved = SaveDC(dc);
        if (!saved) {
            if (!supplied)
                ReleaseDC(window, dc);
            return;
        }
        ExcludeClipRect(dc, l.client.left, l.client.top, l.client.right, l.client.bottom);
        HIGHCONTRASTW contrast{sizeof(contrast)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        const bool accessible = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
        const auto background = accessible ? GetSysColor(COLOR_WINDOW) : RGB(18, 18, 18);
        const auto foreground = accessible                    ? GetSysColor(COLOR_WINDOWTEXT)
                                : GetActiveWindow() == window ? RGB(238, 238, 238)
                                                              : RGB(160, 160, 160);
        SetDCBrushColor(dc, background);
        FillRect(dc, &l.outer, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        // Only the small caption strip is double buffered; never allocate a
        // second game-sized bitmap just to redraw a hovered title button.
        auto memory = CreateCompatibleDC(dc);
        auto bitmap = CreateCompatibleBitmap(
            dc, std::max<LONG>(1, l.outer.right), std::max<LONG>(1, l.caption.bottom));
        auto oldBitmap = memory && bitmap ? SelectObject(memory, bitmap) : nullptr;
        HDC caption = oldBitmap && oldBitmap != HGDI_ERROR ? memory : dc;
        const auto oldFont = SelectObject(caption, font ? font : GetStockObject(DEFAULT_GUI_FONT));
        SetBkMode(caption, TRANSPARENT);
        SetTextColor(caption, foreground);
        SetDCBrushColor(caption, background);
        RECT top{0, 0, l.outer.right, l.caption.bottom};
        FillRect(caption, &top, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        if (!accessible) {
            TRIVERTEX vertices[] = {{0, 0, 36 << 8, 37 << 8, 41 << 8, 0xff00},
                                    {top.right, top.bottom, 10 << 8, 11 << 8, 13 << 8, 0xff00}};
            GRADIENT_RECT gradient{0, 1};
            GradientFill(caption, vertices, 2, &gradient, 1, GRADIENT_FILL_RECT_V);
        }
        RECT text = l.caption;
        text.left += MulDiv(12, GetDpiForWindow(window), 96);
        text.right = l.buttons[firstButton].left - MulDiv(8, GetDpiForWindow(window), 96);
        const auto bounds = iconBounds(window, l);
        if (icon && !IsRectEmpty(&bounds)) {
            DrawIconEx(caption,
                       bounds.left,
                       bounds.top,
                       icon,
                       bounds.right - bounds.left,
                       bounds.bottom - bounds.top,
                       0,
                       nullptr,
                       DI_NORMAL);
            text.left = bounds.right + MulDiv(8, GetDpiForWindow(window), 96);
        }
        if (text.right > text.left)
            DrawTextW(caption,
                      title,
                      -1,
                      &text,
                      DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
        for (int i = firstButton; i < buttonSlots; ++i) {
            const auto b = static_cast<Button>(i);
            const auto& r = l.buttons[i];
            if (hot == b) {
                const auto color = accessible           ? GetSysColor(COLOR_HIGHLIGHT)
                                   : b == Button::close ? RGB(185, 45, 50)
                                   : pressed == b       ? RGB(62, 62, 62)
                                                        : RGB(43, 43, 43);
                SetDCBrushColor(caption, color);
                auto hover = r;
                const auto inset = std::max(2, MulDiv(3, GetDpiForWindow(window), 96));
                InflateRect(&hover, -inset, -inset);
                const auto brush = SelectObject(caption, GetStockObject(DC_BRUSH));
                const auto pen = SelectObject(caption, GetStockObject(NULL_PEN));
                const auto radius = MulDiv(8, GetDpiForWindow(window), 96);
                RoundRect(
                    caption, hover.left, hover.top, hover.right, hover.bottom, radius, radius);
                SelectObject(caption, pen);
                SelectObject(caption, brush);
            }
            const auto ink = accessible && hot == b ? GetSysColor(COLOR_HIGHLIGHTTEXT) : foreground;
            SetTextColor(caption, ink);
            SetDCPenColor(caption, ink);
#ifdef DAW_X
            if (b == Button::newClient && !accessible)
                SetDCPenColor(caption,
                              launch           ? RGB(100, 100, 100)
                              : launchError[0] ? RGB(235, 110, 110)
                                               : ink);
#endif
            auto oldPen = SelectObject(caption, GetStockObject(DC_PEN));
            auto oldBrush = SelectObject(caption, GetStockObject(NULL_BRUSH));
            const int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
            const int s = std::max(3, MulDiv(4, GetDpiForWindow(window), 96));
            auto line = [&](int x1, int y1, int x2, int y2) {
                MoveToEx(caption, x1, y1, nullptr);
                LineTo(caption, x2, y2);
            };
            switch (b) {
                case Button::newClient:
                    line(cx - s, cy, cx + s + 1, cy);
                    line(cx, cy - s, cx, cy + s + 1);
                    break;
                case Button::native: {
                    RECT label = r;
                    DrawTextW(caption, L"1:1", -1, &label, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    break;
                }
                case Button::minimize:
                    line(cx - s, cy + s, cx + s + 1, cy + s);
                    break;
                case Button::maximize:
                    if (IsZoomed(window)) {
                        line(cx - s + 2, cy - s - 2, cx + s + 3, cy - s - 2);
                        line(cx + s + 2, cy - s - 2, cx + s + 2, cy + s - 1);
                    }
                    Rectangle(caption, cx - s, cy - s, cx + s + 1, cy + s + 1);
                    break;
                case Button::close:
                    line(cx - s, cy - s, cx + s + 1, cy + s + 1);
                    line(cx + s, cy - s, cx - s - 1, cy + s + 1);
                    break;
                default:
                    break;
            }
            SelectObject(caption, oldBrush);
            SelectObject(caption, oldPen);
        }
        SelectObject(caption, oldFont);
        if (caption == memory)
            BitBlt(dc, 0, 0, top.right, top.bottom, caption, 0, 0, SRCCOPY);
        if (oldBitmap && oldBitmap != HGDI_ERROR)
            SelectObject(memory, oldBitmap);
        if (bitmap)
            DeleteObject(bitmap);
        if (memory)
            DeleteDC(memory);
        if (saved)
            RestoreDC(dc, saved);
        if (!supplied)
            ReleaseDC(window, dc);
    }

    void hideTip() {
        if (hoverTimer)
            KillTimer(window, hoverTimer);
        hoverTimer = 0;
        if (tooltip) {
            TOOLINFOW tool{TTTOOLINFOW_V2_SIZE};
            tool.hwnd = window;
            tool.uId = 1;
            SendMessageW(tooltip, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&tool));
        }
    }

    void hover(Button b) {
        if (hot == b)
            return;
        hideTip();
        hot = b;
        if (hot != Button::none && pressed == Button::none)
            hoverTimer = SetTimer(window, frameId + 1, 500, nullptr);
        paint();
    }

    void showTip() {
        if (hoverTimer)
            KillTimer(window, hoverTimer);
        hoverTimer = 0;
        if (!tooltip || hot == Button::none)
            return;
        const wchar_t* labels[] = {
#ifdef DAW_X
            launch           ? L"Starting client\u2026"
            : launchError[0] ? launchError
                             : L"Open another Dark Ages client",
            L"Reset to native size (640 \u00d7 480)",
#else
            L"",
            L"",
#endif
            L"Minimize",
            IsZoomed(window) ? L"Restore" : L"Maximize",
            L"Close"};
        TOOLINFOW tool{TTTOOLINFOW_V2_SIZE};
        tool.hwnd = window;
        tool.uId = 1;
        tool.lpszText = const_cast<wchar_t*>(labels[static_cast<int>(hot)]);
        SendMessageW(tooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tool));
        POINT p{};
        GetCursorPos(&p);
        SendMessageW(tooltip, TTM_TRACKPOSITION, 0, MAKELPARAM(p.x + 12, p.y + 20));
        SendMessageW(tooltip, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&tool));
    }

    void cancel() {
        pressed = Button::none;
        hideTip();
        if (GetCapture() == window)
            ReleaseCapture();
        hover(Button::none);
    }

    void activate(Button b) {
        switch (b) {
#ifdef DAW_X
            case Button::newClient:
                startLaunch();
                break;
            case Button::native:
                nativeSize(window);
                break;
#endif
            case Button::minimize:
                PostMessageW(window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
                break;
            case Button::maximize:
                PostMessageW(window, WM_SYSCOMMAND, IsZoomed(window) ? SC_RESTORE : SC_MAXIMIZE, 0);
                break;
            case Button::close:
                PostMessageW(window, WM_SYSCOMMAND, SC_CLOSE, 0);
                break;
            default:
                break;
        }
    }

#ifdef DAW_X
    void startLaunch() {
        if (launch)
            return;
        launchError[0] = 0;
        try {
            auto work = std::make_unique<LaunchWork>();
            if (!currentClientPaths(work->executable, work->dll)) {
                wcscpy_s(launchError,
                         L"New client unavailable: this executable is not Darkages.exe.");
                paint();
                return;
            }
            launch = std::make_shared<LaunchStatus>();
            work->status = launch;
            launchTimer = SetTimer(window, frameId + 3, 1000, nullptr);
            if (!launchTimer) {
                launch.reset();
                wcscpy_s(launchError,
                         L"New client failed: could not create completion timer. Click to retry.");
                paint();
                return;
            }
            const auto thread = _beginthreadex(nullptr, 0, launchWorker, work.get(), 0, nullptr);
            if (!thread) {
                launch.reset();
                wcscpy_s(launchError,
                         L"New client failed: could not create launch worker. Click to retry.");
            } else {
                work.release(); // Worker owns paths and status independently of this HWND.
                CloseHandle(reinterpret_cast<HANDLE>(thread));
                EnableMenuItem(GetSystemMenu(window, FALSE), newCommand, MF_BYCOMMAND | MF_GRAYED);
            }
        } catch (const std::bad_alloc&) {
            launch.reset();
            wcscpy_s(launchError, L"New client failed: insufficient memory. Click to retry.");
        }
        if (!launch && launchTimer) {
            KillTimer(window, launchTimer);
            launchTimer = 0;
        }
        paint();
    }
#endif
};

// Nested DefSubclassProc/GetWindowText/SetCapture calls can destroy the HWND.
// Keep the frame object alive until every callback already on its stack returns.
struct Call {
    Frame* frame;

    explicit Call(Frame* f) : frame(f) {
        ++frame->calls;
    }

    ~Call() {
        if (!--frame->calls && frame->destroyed)
            delete frame;
    }
};

LRESULT CALLBACK subclass(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR reference) {
    auto frame = reinterpret_cast<Frame*>(reference);
    Call keep(frame);
    if (message == WM_NCDESTROY) {
        frame->destroyed = true;
        frame->hideTip();
        if (frame->titleTimer)
            KillTimer(window, frame->titleTimer);
#ifdef DAW_X
        if (frame->launchTimer)
            KillTimer(window, frame->launchTimer);
#endif
        RemoveWindowSubclass(window, subclass, id);
        return DefSubclassProc(window, message, wParam, lParam);
    }
    if (message == WM_NCCALCSIZE && lParam) {
        auto rectangle = wParam ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0]
                                : reinterpret_cast<RECT*>(lParam);
        rectangle->top = std::min(rectangle->bottom, rectangle->top + frameCaptionHeight(window));
        return 0; // Caption only: game pixels reach both sides and bottom.
    }
    if (message == WM_NCHITTEST) {
        const auto captionLayout = layout(window);
        const auto p = localPoint(window, lParam);
        if (windowedFrame(window) && !IsZoomed(window)) {
            const auto grip = std::max(4, MulDiv(6, GetDpiForWindow(window), 96));
            const bool left = p.x >= 0 && p.x < grip;
            const bool right =
                p.x < captionLayout.outer.right && p.x >= captionLayout.outer.right - grip;
            const bool top = p.y >= 0 && p.y < grip;
            const bool bottom =
                p.y < captionLayout.outer.bottom && p.y >= captionLayout.outer.bottom - grip;
            if (top && left)
                return HTTOPLEFT;
            if (top && right)
                return HTTOPRIGHT;
            if (bottom && left)
                return HTBOTTOMLEFT;
            if (bottom && right)
                return HTBOTTOMRIGHT;
            if (left)
                return HTLEFT;
            if (right)
                return HTRIGHT;
            if (top)
                return HTTOP;
            if (bottom)
                return HTBOTTOM;
        }
        const auto button = buttonAt(captionLayout, p);
        if (button != Button::none) {
            if (button == Button::minimize)
                return HTMINBUTTON;
            if (button == Button::maximize)
                return HTMAXBUTTON;
            if (button == Button::close)
                return HTCLOSE;
            return HTBORDER;
        }
        if (PtInRect(&captionLayout.caption, p)) {
            const auto icon = smallWindowIcon(window);
            if (frame->destroyed)
                return HTNOWHERE;
            const auto bounds = iconBounds(window, captionLayout);
            return icon && PtInRect(&bounds, p) ? HTSYSMENU : HTCAPTION;
        }
        if (PtInRect(&captionLayout.client, p))
            return HTCLIENT;
    }
    if (message == WM_NCPAINT) {
        frame->paint();
        return 0;
    }
    if (message == WM_NCACTIVATE) {
        frame->paint();
        return TRUE;
    }
    if (message == WM_NCMOUSEMOVE) {
        frame->hover(buttonAt(layout(window), localPoint(window, lParam)));
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_NONCLIENT | TME_LEAVE, window, 0};
        TrackMouseEvent(&tracking);
        if (frame->hot != Button::none)
            return 0;
    }
    if (message == WM_NCMOUSELEAVE && frame->pressed == Button::none)
        frame->hover(Button::none);
    if (message == WM_NCLBUTTONDOWN || message == WM_NCLBUTTONDBLCLK) {
        const auto button = buttonAt(layout(window), localPoint(window, lParam));
        if (button != Button::none) {
            frame->hideTip();
            frame->pressed = button;
            frame->hot = button;
            SetCapture(window);
            frame->paint();
            return 0;
        }
    }
    if (message == WM_MOUSEMOVE && frame->pressed != Button::none) {
        frame->hover(buttonAt(layout(window), localPoint(window, lParam, true)));
        return 0; // Captured title-bar drags never reach the game's mouse transform.
    }
    if (message == WM_LBUTTONUP && frame->pressed != Button::none) {
        const auto button = frame->pressed;
        const bool activate = buttonAt(layout(window), localPoint(window, lParam, true)) == button;
        frame->cancel();
        if (!frame->destroyed && activate)
            frame->activate(button);
        return 0;
    }
    if (message == WM_CANCELMODE || message == WM_CAPTURECHANGED ||
        (message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE) ||
        (message == WM_ACTIVATEAPP && !wParam))
        frame->cancel();
#ifdef DAW_X
    if (message == WM_SYSCOMMAND &&
        ((wParam & 0xfff0) == nativeCommand || (wParam & 0xfff0) == newCommand)) {
        frame->activate((wParam & 0xfff0) == nativeCommand ? Button::native : Button::newClient);
        return 0;
    }
#endif
    if (message == WM_TIMER && wParam == frame->hoverTimer && frame->hoverTimer) {
        frame->showTip();
        return 0;
    }
    if (message == WM_TIMER && wParam == frame->titleTimer && frame->titleTimer) {
        // External SetWindowText can update USER32's stored title without
        // delivering WM_SETTEXT to this process. Read only window text here.
        bool changed = false;
        if (frame->refreshTitle(&changed) && changed)
            frame->paint();
        return 0;
    }
#ifdef DAW_X
    if (message == WM_TIMER && wParam == frame->launchTimer && frame->launchTimer) {
        frame->updateLaunch();
        return 0;
    }
#endif
    const auto result = DefSubclassProc(window, message, wParam, lParam);
    if (!frame->destroyed) {
        if (message == WM_SETTEXT && !frame->refreshTitle())
            return result;
        if (message == WM_DPICHANGED || message == WM_THEMECHANGED || message == WM_SETTINGCHANGE ||
            message == WM_DWMCOMPOSITIONCHANGED)
            frame->refreshStyle();
        if (message == WM_PRINT && (lParam & PRF_NONCLIENT))
            frame->paint(reinterpret_cast<HDC>(wParam));
        else if (message == WM_SETTEXT || message == WM_WINDOWPOSCHANGED ||
                 message == WM_ACTIVATE || message == WM_DPICHANGED || message == WM_THEMECHANGED ||
                 message == WM_SETTINGCHANGE || message == WM_SETICON ||
                 message == WM_DWMCOMPOSITIONCHANGED)
            frame->paint();
    }
    return result;
}
} // namespace

bool attachCaption(HWND window) {
    DWORD_PTR existing = 0;
    if (GetWindowSubclass(window, subclass, frameId, &existing))
        return true;
    auto f = new (std::nothrow) Frame;
    if (!f)
        return false;
    f->window = window;
    if (!SetWindowSubclass(window, subclass, frameId, reinterpret_cast<DWORD_PTR>(f))) {
        delete f;
        return false;
    }
    Call keep(f);
    if (!f->refreshTitle())
        return false;
    RECT original{};
    GetClientRect(window, &original);
    f->refreshStyle();
    SetWindowPos(window,
                 nullptr,
                 0,
                 0,
                 0,
                 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (f->destroyed)
        return false;
    if (windowedFrame(window) && !IsZoomed(window))
        nativeSize(window, original.right, original.bottom);
    if (f->destroyed)
        return false;
#ifdef DAW_X
    auto menu = GetSystemMenu(window, FALSE);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, newCommand, L"New &client");
    AppendMenuW(menu, MF_STRING, nativeCommand, L"&Native dimensions (640 \u00d7 480)");
#endif
    f->tooltip = CreateWindowExW(WS_EX_TOPMOST,
                                 TOOLTIPS_CLASSW,
                                 nullptr,
                                 WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 window,
                                 nullptr,
                                 GetModuleHandleW(nullptr),
                                 nullptr);
    if (f->tooltip) {
        SetWindowTheme(f->tooltip, L"", L"");
        SendMessageW(f->tooltip, TTM_SETTIPBKCOLOR, RGB(24, 25, 29), 0);
        SendMessageW(f->tooltip, TTM_SETTIPTEXTCOLOR, RGB(235, 235, 238), 0);
        SendMessageW(f->tooltip, WM_SETFONT, reinterpret_cast<WPARAM>(f->font), FALSE);
        // Dark Ages does not activate common-controls v6. Its v5 tooltip rejects
        // sizeof(TOOLINFOW), which includes the v6-only reserved field. V2 works
        // with either host activation context; no manifest/companion is needed.
        TOOLINFOW tool{TTTOOLINFOW_V2_SIZE};
        tool.uFlags = TTF_TRACK | TTF_ABSOLUTE;
        tool.hwnd = window;
        tool.uId = 1;
        tool.lpszText = const_cast<wchar_t*>(L"");
        if (!SendMessageW(f->tooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool))) {
            log(L"Caption tooltip registration failed: %lu", GetLastError());
            DestroyWindow(f->tooltip);
            f->tooltip = nullptr;
        }
    }
    f->paint();
    if (!f->destroyed)
        f->titleTimer = SetTimer(window, frameId + 2, 1000, nullptr);
    return !f->destroyed;
}
} // namespace daw
