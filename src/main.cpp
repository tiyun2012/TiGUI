#include "DX12Renderer.h"
#include "Win32Window.h"

// -----------------------------------------------------------------------------
// Application entry point.
//
// Milestone 1 deliberately keeps the application loop simple:
//
//     Win32 window -> DX12 renderer -> BeginFrame -> Clear -> EndFrame
//
// There are no widgets, layout objects, docking nodes, tabs, or UI draw lists
// yet. Those belong to later milestones.
// -----------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    Win32Window window;
    if (!window.Create(hInstance, nCmdShow))
        return -1;

    DX12Renderer renderer;
    if (!renderer.Initialize(window.GetHwnd(), window.GetWidth(), window.GetHeight()))
        return -1;

    MSG msg{};

    // Use PeekMessage so rendering continues even when Windows has no pending
    // messages for the application.
    while (msg.message != WM_QUIT)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!renderer.BeginFrame())
            break;

        // Milestone 1's only GPU drawing operation.
        renderer.Clear({0.08f, 0.09f, 0.12f, 1.0f});

        renderer.EndFrame();
    }

    renderer.Shutdown();
    return 0;
}
