#pragma once
#include <windows.h>
#include <d2d1.h>
#include <string>
#pragma comment(lib, "d2d1.lib")

class Overlay {
public:
    HWND      hwnd       = nullptr;
    ID2D1Factory*           factory  = nullptr;
    ID2D1HwndRenderTarget*  target   = nullptr;
    ID2D1SolidColorBrush*   brush    = nullptr;

    bool Init(HWND gameWindow);
    void BeginDraw();
    void DrawBox(float x, float y, float w, float h, D2D1_COLOR_F color);
    void EndDraw();
    void Shutdown();
};
