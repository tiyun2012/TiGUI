#include "Core/Log.h"
#include "Platform/Win32Window.h"
#include "Renderer/DX12Renderer.h"

// ============================================================================
// Application entry point
//
// Current architecture:
//
//                 Application
//                      |
//          +-----------+-----------+
//          |                       |
//         Core                 Platform
//          |                       |
//       Logger                Win32Window
//          |                       |
//          +-----------+-----------+
//                      |
//                   Renderer
//                      |
//                 DX12Renderer
//                      |
//                  DirectX 12
//
// There is intentionally no UI system yet.
// ============================================================================

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE,
    LPSTR,
    int nCmdShow)
{
    // ========================================================================
    // Initialize logging first.
    //
    // This means subsequent initialization failures have a place to report
    // themselves.
    // ========================================================================

    if (!Logger::Initialize())
    {
        // File logging may not be available, but the debugger sink still
        // attempts to receive this message.
        Logger::Error(
            "Logger initialization failed");
    }

    LOG_INFO(
        "============================================================");

    LOG_INFO(
        "MyUI application starting - Milestone 1 Extended");

    LOG_INFO(
        "============================================================");

    // ========================================================================
    // Create the native Win32 window.
    // ========================================================================

    Win32Window window;

    if (!window.Create(
            hInstance,
            nCmdShow))
    {
        LOG_ERROR(
            "Application startup failed: Win32 window creation");

        Logger::Shutdown();

        return -1;
    }

    // ========================================================================
    // Initialize DirectX 12.
    // ========================================================================

    DX12Renderer renderer;

    if (!renderer.Initialize(
            window.GetHwnd(),
            window.GetWidth(),
            window.GetHeight()))
    {
        LOG_ERROR(
            "Application startup failed: DX12 renderer initialization");

        renderer.Shutdown();

        Logger::Shutdown();

        return -1;
    }

    LOG_INFO(
        "Application initialization complete");

    MSG msg{};

    // ========================================================================
    // Main loop
    //
    // PeekMessage() allows the application to continue rendering when Windows
    // has no message waiting.
    // ========================================================================

    while (msg.message != WM_QUIT)
    {
        // Process all currently waiting Windows messages.
        while (PeekMessage(
                   &msg,
                   nullptr,
                   0,
                   0,
                   PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Begin recording this frame.
        if (!renderer.BeginFrame())
        {
            LOG_ERROR(
                "BeginFrame failed; stopping application loop");

            break;
        }

        // ====================================================================
        // Milestone 1 has exactly one rendering operation:
        //
        // clear the swap-chain back buffer.
        //
        // Later milestones will insert our DrawList/UI rendering here.
        // ====================================================================

        renderer.Clear({
            0.08f,
            0.09f,
            0.12f,
            1.0f
        });

        // Finish the frame and present it.
        renderer.EndFrame();
    }

    LOG_INFO(
        "Application main loop ended");

    // Wait for GPU work and release D3D12 resources.
    renderer.Shutdown();

    LOG_INFO(
        "MyUI application shutting down");

    LOG_INFO(
        "============================================================");

    Logger::Shutdown();

    return 0;
}