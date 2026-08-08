#include "overlay.h"

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcW(h, m, w, l);
}

bool Overlay::Init(HWND gameWindow) {
    RECT r;
    GetWindowRect(gameWindow, &r);
    int w = r.right - r.left, h = r.bottom - r.top;

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc   = WndProc;
    wc.lpszClassName = L"XRayOverlay";
    wc.hInstance     = GetModuleHandleW(nullptr);
    RegisterClassExW(&wc);

    hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
        L"XRayOverlay", L"",
        WS_POPUP,
        r.left, r.top, w, h,
        nullptr, nullptr, wc.hInstance, nullptr
    );
    if (!hwnd) return false;

    // invisible to OBS, Discord, any screen capture
    SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
    SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
    ShowWindow(hwnd, SW_SHOW);

    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory);
    auto size  = D2D1::SizeU(w, h);
    auto props = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    factory->CreateHwndRenderTarget(
        props,
        D2D1::HwndRenderTargetProperties(hwnd, size),
        &target);
    target->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &brush);
    return true;
}

void Overlay::BeginDraw() {
    target->BeginDraw();
    target->Clear(D2D1::ColorF(0, 0));
}

void Overlay::DrawBox(float x, float y, float w, float h, D2D1_COLOR_F color) {
    brush->SetColor(color);
    target->DrawRectangle(D2D1::RectF(x, y, x+w, y+h), brush, 2.f);
}

void Overlay::EndDraw() {
    target->EndDraw();
}

void Overlay::Shutdown() {
    if (brush)   brush->Release();
    if (target)  target->Release();
    if (factory) factory->Release();
    if (hwnd)    DestroyWindow(hwnd);
}
