#pragma once

#include <windows.h>

// -----------------------------------------------------------------------------
// Win32Window
//
// Tiny wrapper around the native Win32 HWND. Its only responsibility in
// Milestone 1 is creating the OS window that will host our future renderer.
// -----------------------------------------------------------------------------
class Win32Window
{
public:
    bool Create(HINSTANCE hInstance, int nCmdShow);

    HWND GetHwnd() const { return hwnd_; }
    UINT GetWidth() const { return width_; }
    UINT GetHeight() const { return height_; }

    // Windows calls this function whenever the HWND receives a message.
    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam);

private:
    HWND hwnd_ = nullptr;

    // Initial client-area size. Resize handling is intentionally deferred.
    UINT width_ = 1280;
    UINT height_ = 720;
};
