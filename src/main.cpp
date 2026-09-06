#include "DX12Renderer.h"
#include "Win32Window.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    Win32Window window;
    if (!window.Create(hInstance, nCmdShow))
        return -1;

    DX12Renderer renderer;
    if (!renderer.Initialize(window.GetHwnd(), window.GetWidth(), window.GetHeight()))
        return -1;

    MSG msg{};
    while (msg.message != WM_QUIT)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!renderer.BeginFrame())
            break;

        renderer.Clear({0.08f, 0.09f, 0.12f, 1.0f});
        renderer.EndFrame();
    }

    renderer.Shutdown();
    return 0;
}
