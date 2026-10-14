#pragma once
#include <windows.h>
#include <ddraw.h>
#include <atomic>
#include <memory>
#include "geometry.h"
#include "graphics/retry.h"

namespace daw {
class Draw;

// Window timers may outlive their Draw. A weak binding plus lifetime lock allows
// them to skip work after destruction without owning the device or blocking it.
struct RecoveryBinding {
    SRWLOCK lifetime = SRWLOCK_INIT;
    Draw* draw = nullptr;
    void service();
};

struct WindowState {
    // Canvas pixel/HDC access is synchronized. UI presentation and recovery
    // scheduling belong to the HWND thread; game writes only queue presentation.
    SRWLOCK lock = SRWLOCK_INIT;
    std::atomic<HWND> hwnd{nullptr};
    HDC canvas = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previousBitmap = nullptr;
    WORD* pixels = nullptr;
    std::atomic<DWORD> generation{0};
    UINT presentationMessage = 0;
    std::atomic<bool> presentationPending{false};
    bool hasFrame = false;
    std::weak_ptr<RecoveryBinding> recovery;
    UINT_PTR recoveryTimer = 0;
    std::atomic<bool> retryPresentation{false};
    Retry presentationRetry;
    ULONGLONG desktopCheck = 0;
    bool desktopAvailable = true;
    void recoveryHint();
    void serviceRecovery();
#ifdef _DEBUG
    int tracedWidth = -1, tracedHeight = -1;
#endif
    WindowState();
    ~WindowState();
    bool frameAvailable();
    HRESULT capture(IDirectDrawSurface* surface, DWORD* capturedGeneration);
    HRESULT copyTo(IDirectDrawSurface* surface, DWORD& copiedGeneration);
    bool paint(HDC dc, int width, int height);
    HRESULT requestPresentation();
    HRESULT present();
};

HRESULT attachWindow(const std::shared_ptr<WindowState>& state, HWND window);
} // namespace daw
