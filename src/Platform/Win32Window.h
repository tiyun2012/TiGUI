#pragma once

#include <windows.h>

// ============================================================================
// Win32Window
//
// Platform layer.
//
// Its job is only to deal with Windows-specific window creation and messages.
//
// Future UI code should NOT need to know about CreateWindowEx(), WNDCLASSEX,
// GWLP_USERDATA, etc.
//
// Later milestones can build a higher-level platform abstraction on top.
//
// Milestone 1 handles:
//
//     - window class registration
//     - HWND creation
//     - WM_DESTROY
//     - initial width/height
//
// Not yet implemented:
//
//     - resizing
//     - DPI
//     - mouse abstraction
//     - keyboard abstraction
//     - clipboard
//     - cursor management
//     - raw input
// ============================================================================
class Win32Window
{
public:
    bool Create(
        HINSTANCE hInstance,
        int nCmdShow);

    HWND GetHwnd() const
    {
        return hwnd_;
    }

    UINT GetWidth() const
    {
        return width_;
    }

    UINT GetHeight() const
    {
        return height_;
    }

    // Win32 requires a static callback-compatible window procedure.
    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam);

private:
    HWND hwnd_ = nullptr;

    // Desired initial client-area size.
    UINT width_ = 1280;
    UINT height_ = 720;
};