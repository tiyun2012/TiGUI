#include "Win32Window.h"

#include "Core/Log.h"

namespace
{
    constexpr wchar_t kWindowClassName[] =
        L"MyUI_Milestone1_Window";

    constexpr wchar_t kWindowTitle[] =
        L"MyUI - Milestone 1";

    // ========================================================================
    // RegisterWindowClass
    //
    // Before Windows can create an HWND from our class, the class has to be
    // registered with the operating system.
    // ========================================================================
    bool RegisterWindowClass(
        HINSTANCE hInstance)
    {
        WNDCLASSEXW wc{};

        wc.cbSize =
            sizeof(WNDCLASSEXW);

        wc.style =
            CS_HREDRAW | CS_VREDRAW;

        // Windows calls this function whenever the window receives a message.
        wc.lpfnWndProc =
            Win32Window::WindowProc;

        wc.hInstance =
            hInstance;

        wc.hCursor =
            LoadCursorW(
                nullptr,
                IDC_ARROW);

        // DirectX will render the client area.
        wc.hbrBackground = nullptr;

        wc.lpszClassName =
            kWindowClassName;

        if (RegisterClassExW(&wc) == 0)
        {
            const DWORD error =
                GetLastError();

            // The class may already exist during development.
            if (error == ERROR_CLASS_ALREADY_EXISTS)
            {
                return true;
            }

            LOG_ERROR(
                "RegisterClassExW failed. Win32 error={}",
                error);

            return false;
        }

        return true;
    }
}

bool Win32Window::Create(
    HINSTANCE hInstance,
    int nCmdShow)
{
    LOG_INFO(
        "Creating Win32 application window");

    if (!RegisterWindowClass(hInstance))
    {
        return false;
    }

    // ========================================================================
    // The requested width/height represent the CLIENT area.
    //
    // WS_OVERLAPPEDWINDOW adds a title bar and borders, so the total window
    // rectangle is larger than the client area.
    //
    // AdjustWindowRect() calculates the required outer dimensions.
    // ========================================================================
    RECT rect{
        0,
        0,
        static_cast<LONG>(width_),
        static_cast<LONG>(height_)
    };

    AdjustWindowRect(
        &rect,
        WS_OVERLAPPEDWINDOW,
        FALSE);

    // ========================================================================
    // Create the native HWND.
    //
    // The final "this" parameter is passed to WM_NCCREATE.
    // WindowProc uses that pointer to associate the HWND with this C++ object.
    // ========================================================================
    hwnd_ =
        CreateWindowExW(
            0,
            kWindowClassName,
            kWindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            rect.right - rect.left,
            rect.bottom - rect.top,
            nullptr,
            nullptr,
            hInstance,
            this);

    if (!hwnd_)
    {
        LOG_ERROR(
            "CreateWindowExW failed. Win32 error={}",
            GetLastError());

        return false;
    }

    ShowWindow(
        hwnd_,
        nCmdShow);

    UpdateWindow(hwnd_);

    LOG_INFO(
        "Win32 window created: {}x{}",
        width_,
        height_);

    return true;
}

LRESULT CALLBACK Win32Window::WindowProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    // Retrieve our C++ object associated with this HWND.
    auto* self =
        reinterpret_cast<Win32Window*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

    // ========================================================================
    // WM_NCCREATE
    //
    // This is part of the native window creation process.
    //
    // CREATESTRUCT contains the lpParam value from CreateWindowExW().
    // We passed "this", so we can recover our C++ object here.
    // ========================================================================
    if (msg == WM_NCCREATE)
    {
        auto* createStruct =
            reinterpret_cast<CREATESTRUCTW*>(
                lParam);

        self =
            static_cast<Win32Window*>(
                createStruct->lpCreateParams);

        SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(self));

        self->hwnd_ = hwnd;
    }

    switch (msg)
    {
        case WM_DESTROY:
        {
            LOG_INFO(
                "Win32 window destroyed");

            // Causes the application message loop to terminate.
            PostQuitMessage(0);

            return 0;
        }

        default:
            break;
    }

    // Any message we do not process is handled by Windows.
    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam);
}