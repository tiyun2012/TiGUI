#pragma once

#include <windows.h>

class Win32Window
{
public:
    bool Create(HINSTANCE hInstance, int nCmdShow);

    HWND GetHwnd() const { return hwnd_; }
    UINT GetWidth() const { return width_; }
    UINT GetHeight() const { return height_; }

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam);

private:
    HWND hwnd_ = nullptr;
    UINT width_ = 1280;
    UINT height_ = 720;
};