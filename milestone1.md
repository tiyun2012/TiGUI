Below is the complete matched Milestone 1 Extended version. You can replace your current D:\TIGUI contents with this structure.

One important correction from the previous version: because the source files now use includes such as "Core/Log.h", CMakeLists.txt must add src as an include directory.

1. Directory structure
D:\TIGUI
│
├── CMakeLists.txt
├── make.bat
├── README.md
│
├── logs\
│   └── MyUI.log              ← created automatically
│
└── src\
    │
    ├── main.cpp
    │
    ├── Core\
    │   ├── Log.h
    │   └── Log.cpp
    │
    ├── Platform\
    │   ├── Win32Window.h
    │   └── Win32Window.cpp
    │
    └── Renderer\
        ├── DX12Renderer.h
        └── DX12Renderer.cpp
2. src/Core/Log.h
#pragma once

#include <format>
#include <string_view>
#include <utility>

// ============================================================================
// LogLevel
//
// Three basic levels are enough for Milestone 1.
//
// Logger belongs to Core rather than Platform or Renderer because every
// subsystem should be able to use logging:
//
//     Core
//       ↑
//       ├── Platform
//       ├── Renderer
//       └── future UI
//
// Later we can add categories, filtering, console sinks, asynchronous logging,
// profiling output, and a GUI Output Log panel.
// ============================================================================
enum class LogLevel
{
    Info,
    Warning,
    Error
};

// ============================================================================
// Logger
//
// Current output destinations:
//
//     1. logs/MyUI.log
//     2. Visual Studio Output window through OutputDebugStringW
//
// We intentionally do NOT create a visual Output Log panel yet.
// That will be part of the future UI framework.
// ============================================================================
class Logger
{
public:
    // Create/open the log file.
    //
    // The default path is relative to the application's working directory:
    //
    //     logs/MyUI.log
    //
    static bool Initialize(
        std::string_view filePath = "logs/MyUI.log");

    // Flush and close the log file.
    static void Shutdown();

    // Write an already formatted message.
    static void Write(
        LogLevel level,
        std::string_view message);

    // ------------------------------------------------------------------------
    // Formatted helpers.
    //
    // Example:
    //
    //     Logger::Info("Window size: {}x{}", width, height);
    //
    //     Logger::Error(
    //         "CreateDevice failed. HRESULT=0x{:08X}",
    //         hr);
    // ------------------------------------------------------------------------

    template <typename... Args>
    static void Info(
        std::format_string<Args...> format,
        Args&&... args)
    {
        Write(
            LogLevel::Info,
            std::format(
                format,
                std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Warning(
        std::format_string<Args...> format,
        Args&&... args)
    {
        Write(
            LogLevel::Warning,
            std::format(
                format,
                std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Error(
        std::format_string<Args...> format,
        Args&&... args)
    {
        Write(
            LogLevel::Error,
            std::format(
                format,
                std::forward<Args>(args)...));
    }

private:
    static std::string MakeTimestamp();

    static const char* ToString(
        LogLevel level);
};

// ============================================================================
// Convenience macros
//
// These keep subsystem code clean:
//
//     LOG_INFO("Renderer initialized");
//     LOG_WARNING("Something unusual happened");
//     LOG_ERROR("Could not create device");
// ============================================================================
#define LOG_INFO(...)    Logger::Info(__VA_ARGS__)
#define LOG_WARNING(...) Logger::Warning(__VA_ARGS__)
#define LOG_ERROR(...)   Logger::Error(__VA_ARGS__)
3. src/Core/Log.cpp
#include "Log.h"

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace
{
    // ------------------------------------------------------------------------
    // Logger state.
    //
    // Kept private to this .cpp file so no other part of the application can
    // directly manipulate the logger's internal file/mutex state.
    // ------------------------------------------------------------------------
    std::ofstream gLogFile;

    std::mutex gLogMutex;

    bool gInitialized = false;

    // ------------------------------------------------------------------------
    // Convert UTF-8/std::string into UTF-16.
    //
    // OutputDebugStringW is a wide-character Windows API, so we convert our
    // UTF-8/std::string log message before sending it to Visual Studio.
    // ------------------------------------------------------------------------
    std::wstring ToWide(
        std::string_view text)
    {
        if (text.empty())
        {
            return {};
        }

        const int required =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0);

        if (required <= 0)
        {
            return L"<UTF-8 conversion failed>";
        }

        std::wstring result(
            required,
            L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            required);

        return result;
    }
}

bool Logger::Initialize(
    std::string_view filePath)
{
    std::scoped_lock lock(gLogMutex);

    // Close an existing file if Initialize() is called again.
    if (gLogFile.is_open())
    {
        gLogFile.close();
    }

    try
    {
        const std::filesystem::path path(filePath);

        // Create the parent directory automatically.
        //
        // Example:
        //
        //     logs/MyUI.log
        //
        // creates:
        //
        //     logs\
        //
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(
                path.parent_path());
        }

        // Append instead of overwriting previous runs.
        gLogFile.open(
            path,
            std::ios::out | std::ios::app);

        if (!gLogFile.is_open())
        {
            gInitialized = false;
            return false;
        }

        gInitialized = true;

        return true;
    }
    catch (...)
    {
        gInitialized = false;
        return false;
    }
}

void Logger::Shutdown()
{
    std::scoped_lock lock(gLogMutex);

    if (gLogFile.is_open())
    {
        gLogFile.flush();
        gLogFile.close();
    }

    gInitialized = false;
}

void Logger::Write(
    LogLevel level,
    std::string_view message)
{
    std::scoped_lock lock(gLogMutex);

    const std::string line =
        std::format(
            "[{}] [{}] {}",
            MakeTimestamp(),
            ToString(level),
            message);

    // ========================================================================
    // Sink 1: File
    //
    // The full message is appended to:
    //
    //     logs/MyUI.log
    // ========================================================================
    if (gInitialized &&
        gLogFile.is_open())
    {
        gLogFile << line << '\n';
        gLogFile.flush();
    }

    // ========================================================================
    // Sink 2: Visual Studio debugger
    //
    // This appears in:
    //
    //     Debug
    //       -> Windows
    //          -> Output
    //
    // when running under Visual Studio.
    // ========================================================================
    OutputDebugStringW(
        (ToWide(line) + L"\n").c_str());
}

std::string Logger::MakeTimestamp()
{
    const auto now =
        std::chrono::system_clock::now();

    const auto time =
        std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

    // Thread-safe Windows version of localtime().
    localtime_s(
        &localTime,
        &time);

    // Add milliseconds so events occurring within the same second can still
    // be distinguished.
    const auto milliseconds =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                now.time_since_epoch()) %
        1000;

    std::ostringstream stream;

    stream
        << std::put_time(
               &localTime,
               "%Y-%m-%d %H:%M:%S")
        << '.'
        << std::setfill('0')
        << std::setw(3)
        << milliseconds.count();

    return stream.str();
}

const char* Logger::ToString(
    LogLevel level)
{
    switch (level)
    {
        case LogLevel::Info:
            return "INFO ";

        case LogLevel::Warning:
            return "WARN ";

        case LogLevel::Error:
            return "ERROR";
    }

    return "?????";
}
4. src/Platform/Win32Window.h
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
5. src/Platform/Win32Window.cpp
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
6. src/Renderer/DX12Renderer.h
#pragma once

#include <array>

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// ============================================================================
// DX12Renderer
//
// Minimal DirectX 12 renderer.
//
// Current responsibility:
//
//     initialize D3D12
//     create swap chain
//     create render targets
//     record clear
//     present
//     synchronize CPU/GPU
//
// There is deliberately NO UI code here.
//
// Not implemented yet:
//
//     - vertex buffers
//     - index buffers
//     - shaders
//     - pipeline states
//     - textures
//     - Draw()
//     - DrawList
//     - UI rendering
//     - render graph
// ============================================================================

class DX12Renderer
{
public:
    bool Initialize(
        HWND hwnd,
        UINT width,
        UINT height);

    void Shutdown();

    bool BeginFrame();

    void Clear(
        const std::array<float, 4>& color);

    void EndFrame();

private:
    bool CreateDevice();

    bool CreateCommandObjects();

    bool CreateSwapChain(
        HWND hwnd,
        UINT width,
        UINT height);

    bool CreateRTVHeap();

    bool CreateRenderTargets();

    bool CreateSyncObjects();

    void WaitForGpu();

private:
    // Double buffering.
    static constexpr UINT FrameCount = 2;

    HWND hwnd_ = nullptr;

    UINT width_ = 0;
    UINT height_ = 0;

    // ========================================================================
    // DXGI
    // ========================================================================

    Microsoft::WRL::ComPtr<IDXGIFactory7>
        factory_;

    // ========================================================================
    // D3D12 device
    //
    // The device creates D3D12 resources and objects.
    // ========================================================================

    Microsoft::WRL::ComPtr<ID3D12Device>
        device_;

    // ========================================================================
    // Command infrastructure
    //
    // Queue:
    //     submits GPU commands
    //
    // Allocator:
    //     owns command-list recording memory
    //
    // Command list:
    //     records the actual GPU commands
    // ========================================================================

    Microsoft::WRL::ComPtr<ID3D12CommandQueue>
        commandQueue_;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
        commandAllocator_;

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>
        commandList_;

    // ========================================================================
    // Swap chain
    // ========================================================================

    Microsoft::WRL::ComPtr<IDXGISwapChain3>
        swapChain_;

    // ========================================================================
    // RTV descriptor heap
    //
    // Contains one render-target-view descriptor per back buffer.
    // ========================================================================

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
        rtvHeap_;

    UINT rtvDescriptorSize_ = 0;

    // Actual swap-chain images.
    Microsoft::WRL::ComPtr<ID3D12Resource>
        renderTargets_[FrameCount];

    // ========================================================================
    // GPU synchronization
    // ========================================================================

    Microsoft::WRL::ComPtr<ID3D12Fence>
        fence_;

    HANDLE fenceEvent_ = nullptr;

    UINT64 fenceValue_ = 0;

    // Current swap-chain back-buffer index.
    UINT frameIndex_ = 0;

    // Ensures Clear()/EndFrame() are only used after BeginFrame().
    bool frameBegun_ = false;
};
7. src/Renderer/DX12Renderer.cpp
#include "DX12Renderer.h"

#include "Core/Log.h"

#include <cassert>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
    // ========================================================================
    // CheckHR
    //
    // D3D12 reports errors through HRESULT.
    //
    // We log the operation and HRESULT, then throw so initialization code can
    // abort cleanly.
    // ========================================================================
    void CheckHR(
        HRESULT hr,
        const char* operation)
    {
        if (FAILED(hr))
        {
            LOG_ERROR(
                "{} failed. HRESULT=0x{:08X}",
                operation,
                static_cast<unsigned long>(hr));

            throw std::runtime_error(
                operation);
        }
    }
}

bool DX12Renderer::Initialize(
    HWND hwnd,
    UINT width,
    UINT height)
{
    LOG_INFO(
        "Initializing DirectX 12 renderer: {}x{}",
        width,
        height);

    try
    {
        hwnd_ = hwnd;
        width_ = width;
        height_ = height;

        // ====================================================================
        // D3D12 Debug Layer
        //
        // Only enabled in Debug builds.
        //
        // This is very useful while learning because D3D12 is explicit and
        // many mistakes otherwise result in difficult-to-understand behavior.
        // ====================================================================
#if defined(_DEBUG)

        {
            ComPtr<ID3D12Debug> debugController;

            if (SUCCEEDED(
                    D3D12GetDebugInterface(
                        IID_PPV_ARGS(
                            &debugController))))
            {
                debugController->EnableDebugLayer();

                LOG_INFO(
                    "D3D12 debug layer enabled");
            }
            else
            {
                LOG_WARNING(
                    "D3D12 debug layer is unavailable");
            }
        }

#endif

        // ====================================================================
        // DXGI Factory
        //
        // DXGI handles infrastructure such as the swap chain.
        // ====================================================================

        CheckHR(
            CreateDXGIFactory2(
                0,
                IID_PPV_ARGS(
                    &factory_)),
            "CreateDXGIFactory2");

        LOG_INFO(
            "DXGI factory created");

        // ====================================================================
        // Device
        // ====================================================================

        if (!CreateDevice())
        {
            return false;
        }

        // ====================================================================
        // Command queue + allocator + command list
        // ====================================================================

        if (!CreateCommandObjects())
        {
            return false;
        }

        // ====================================================================
        // Swap chain
        // ====================================================================

        if (!CreateSwapChain(
                hwnd_,
                width_,
                height_))
        {
            return false;
        }

        // ====================================================================
        // RTV descriptors
        // ====================================================================

        if (!CreateRTVHeap())
        {
            return false;
        }

        if (!CreateRenderTargets())
        {
            return false;
        }

        // ====================================================================
        // Fence
        // ====================================================================

        if (!CreateSyncObjects())
        {
            return false;
        }

        LOG_INFO(
            "DirectX 12 renderer initialized successfully");

        return true;
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR(
            "DX12 initialization exception: {}",
            exception.what());

        Shutdown();

        return false;
    }
    catch (...)
    {
        LOG_ERROR(
            "DX12 initialization failed with unknown exception");

        Shutdown();

        return false;
    }
}

bool DX12Renderer::CreateDevice()
{
    // nullptr:
    //     use the default hardware adapter.
    //
    // Feature level 11_0 is sufficient for this first renderer.

    const HRESULT hr =
        D3D12CreateDevice(
            nullptr,
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(
                &device_));

    if (FAILED(hr))
    {
        LOG_ERROR(
            "D3D12CreateDevice failed. HRESULT=0x{:08X}",
            static_cast<unsigned long>(hr));

        return false;
    }

    LOG_INFO(
        "D3D12 device created");

    return true;
}

bool DX12Renderer::CreateCommandObjects()
{
    // ========================================================================
    // Command queue
    //
    // DIRECT queue:
    //     graphics + compute + copy commands can be executed here.
    // ========================================================================

    D3D12_COMMAND_QUEUE_DESC queueDesc{};

    queueDesc.Type =
        D3D12_COMMAND_LIST_TYPE_DIRECT;

    CheckHR(
        device_->CreateCommandQueue(
            &queueDesc,
            IID_PPV_ARGS(
                &commandQueue_)),
        "CreateCommandQueue");

    LOG_INFO(
        "D3D12 command queue created");

    // ========================================================================
    // Command allocator
    //
    // Owns memory used while recording commands.
    // ========================================================================

    CheckHR(
        device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(
                &commandAllocator_)),
        "CreateCommandAllocator");

    LOG_INFO(
        "D3D12 command allocator created");

    // ========================================================================
    // Command list
    //
    // This object records GPU commands.
    // ========================================================================

    CheckHR(
        device_->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            commandAllocator_.Get(),
            nullptr,
            IID_PPV_ARGS(
                &commandList_)),
        "CreateCommandList");

    // Newly created command lists are open.
    //
    // We close it now because every frame follows:
    //
    //     Reset
    //       ↓
    //     Record
    //       ↓
    //     Close
    //       ↓
    //     Execute
    //
    CheckHR(
        commandList_->Close(),
        "ID3D12GraphicsCommandList::Close");

    LOG_INFO(
        "D3D12 command list created");

    return true;
}

bool DX12Renderer::CreateSwapChain(
    HWND hwnd,
    UINT width,
    UINT height)
{
    // ========================================================================
    // Swap-chain description
    //
    // Two buffers:
    //
    //     back buffer 0
    //     back buffer 1
    //
    // Format:
    //
    //     R8G8B8A8_UNORM
    //
    // Presentation:
    //
    //     FLIP_DISCARD
    // ========================================================================

    DXGI_SWAP_CHAIN_DESC1 desc{};

    desc.BufferCount =
        FrameCount;

    desc.Width =
        width;

    desc.Height =
        height;

    desc.Format =
        DXGI_FORMAT_R8G8B8A8_UNORM;

    desc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;

    desc.SwapEffect =
        DXGI_SWAP_EFFECT_FLIP_DISCARD;

    desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;

    CheckHR(
        factory_->CreateSwapChainForHwnd(
            commandQueue_.Get(),
            hwnd,
            &desc,
            nullptr,
            nullptr,
            &swapChain1),
        "CreateSwapChainForHwnd");

    // We will implement fullscreen behavior ourselves later.
    CheckHR(
        factory_->MakeWindowAssociation(
            hwnd,
            DXGI_MWA_NO_ALT_ENTER),
        "MakeWindowAssociation");

    // Upgrade interface.
    CheckHR(
        swapChain1.As(
            &swapChain_),
        "IDXGISwapChain1::As");

    frameIndex_ =
        swapChain_->GetCurrentBackBufferIndex();

    LOG_INFO(
        "Swap chain created: {} buffers",
        FrameCount);

    return true;
}

bool DX12Renderer::CreateRTVHeap()
{
    // ========================================================================
    // Create one RTV descriptor for each swap-chain buffer.
    // ========================================================================

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};

    heapDesc.NumDescriptors =
        FrameCount;

    heapDesc.Type =
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    heapDesc.Flags =
        D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    CheckHR(
        device_->CreateDescriptorHeap(
            &heapDesc,
            IID_PPV_ARGS(
                &rtvHeap_)),
        "CreateDescriptorHeap");

    // Descriptor size is hardware/API dependent.
    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    LOG_INFO(
        "RTV descriptor heap created. Descriptor size={}",
        rtvDescriptorSize_);

    return true;
}

bool DX12Renderer::CreateRenderTargets()
{
    // Start at descriptor 0.
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0;
         i < FrameCount;
         ++i)
    {
        // Retrieve the actual swap-chain resource.
        CheckHR(
            swapChain_->GetBuffer(
                i,
                IID_PPV_ARGS(
                    &renderTargets_[i])),
            "IDXGISwapChain::GetBuffer");

        // Create an RTV describing that resource.
        device_->CreateRenderTargetView(
            renderTargets_[i].Get(),
            nullptr,
            handle);

        // Move to the next descriptor.
        handle.ptr +=
            rtvDescriptorSize_;
    }

    LOG_INFO(
        "Created {} swap-chain render target views",
        FrameCount);

    return true;
}

bool DX12Renderer::CreateSyncObjects()
{
    // ========================================================================
    // Fence
    //
    // D3D12 is explicit about synchronization.
    // The CPU needs a way to ask:
    //
    //     "Has the GPU finished executing my previous work?"
    //
    // A fence provides that mechanism.
    // ========================================================================

    CheckHR(
        device_->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(
                &fence_)),
        "CreateFence");

    fenceValue_ = 0;

    // Windows event used when the CPU has to wait for a fence value.
    fenceEvent_ =
        CreateEventW(
            nullptr,
            FALSE,
            FALSE,
            nullptr);

    if (!fenceEvent_)
    {
        LOG_ERROR(
            "CreateEventW failed. Win32 error={}",
            GetLastError());

        return false;
    }

    LOG_INFO(
        "GPU fence synchronization created");

    return true;
}

bool DX12Renderer::BeginFrame()
{
    if (!commandAllocator_ ||
        !commandList_)
    {
        LOG_ERROR(
            "BeginFrame called before command objects were initialized");

        return false;
    }

    try
    {
        // ====================================================================
        // Reuse command memory.
        //
        // This is safe because EndFrame() currently waits until the GPU has
        // completed the frame.
        //
        // Later we can remove this CPU wait and introduce multiple allocators
        // / frames-in-flight.
        // ====================================================================

        CheckHR(
            commandAllocator_->Reset(),
            "ID3D12CommandAllocator::Reset");

        CheckHR(
            commandList_->Reset(
                commandAllocator_.Get(),
                nullptr),
            "ID3D12GraphicsCommandList::Reset");

        // ====================================================================
        // Resource transition:
        //
        //     PRESENT
        //        |
        //        v
        //     RENDER_TARGET
        //
        // D3D12 requires us to explicitly state how the resource's usage
        // changes.
        // ====================================================================

        D3D12_RESOURCE_BARRIER barrier{};

        barrier.Type =
            D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

        barrier.Transition.pResource =
            renderTargets_[frameIndex_].Get();

        barrier.Transition.StateBefore =
            D3D12_RESOURCE_STATE_PRESENT;

        barrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_RENDER_TARGET;

        commandList_->ResourceBarrier(
            1,
            &barrier);

        // ====================================================================
        // Find the RTV corresponding to frameIndex_.
        // ====================================================================

        D3D12_CPU_DESCRIPTOR_HANDLE rtv =
            rtvHeap_->GetCPUDescriptorHandleForHeapStart();

        rtv.ptr +=
            static_cast<SIZE_T>(
                frameIndex_) *
            rtvDescriptorSize_;

        // Tell D3D12 which render target the output-merger should write to.
        commandList_->OMSetRenderTargets(
            1,
            &rtv,
            FALSE,
            nullptr);

        frameBegun_ = true;

        return true;
    }
    catch (const std::exception& exception)
    {
        LOG_ERROR(
            "BeginFrame failed: {}",
            exception.what());

        return false;
    }
    catch (...)
    {
        LOG_ERROR(
            "BeginFrame failed with unknown exception");

        return false;
    }
}

void DX12Renderer::Clear(
    const std::array<float, 4>& color)
{
    // Clear is only legal between BeginFrame() and EndFrame().
    assert(frameBegun_);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    rtv.ptr +=
        static_cast<SIZE_T>(
            frameIndex_) *
        rtvDescriptorSize_;

    // std::array::data() gives us float* pointing to the four color values.
    commandList_->ClearRenderTargetView(
        rtv,
        color.data(),
        0,
        nullptr);
}

void DX12Renderer::EndFrame()
{
    assert(frameBegun_);

    // ========================================================================
    // Resource transition:
    //
    //     RENDER_TARGET
    //          |
    //          v
    //       PRESENT
    //
    // The swap chain expects the back buffer to be in PRESENT state.
    // ========================================================================

    D3D12_RESOURCE_BARRIER barrier{};

    barrier.Type =
        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

    barrier.Transition.pResource =
        renderTargets_[frameIndex_].Get();

    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_RENDER_TARGET;

    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PRESENT;

    commandList_->ResourceBarrier(
        1,
        &barrier);

    // Finish recording.
    CheckHR(
        commandList_->Close(),
        "ID3D12GraphicsCommandList::Close");

    ID3D12CommandList* lists[] = {
        commandList_.Get()
    };

    // Submit recorded commands to the GPU.
    commandQueue_->ExecuteCommandLists(
        1,
        lists);

    // Present the current back buffer.
    CheckHR(
        swapChain_->Present(
            1,
            0),
        "IDXGISwapChain::Present");

    // ========================================================================
    // Synchronize.
    //
    // For learning, Milestone 1 waits for the GPU every frame.
    //
    // This is intentionally NOT the final high-performance architecture.
    // Later we will use multiple frames in flight.
    // ========================================================================

    WaitForGpu();

    // Ask the swap chain which back buffer comes next.
    frameIndex_ =
        swapChain_->GetCurrentBackBufferIndex();

    frameBegun_ = false;
}

void DX12Renderer::WaitForGpu()
{
    // Advance fence value for this submission.
    ++fenceValue_;

    // GPU signals the fence after all preceding commands on this queue have
    // completed.
    CheckHR(
        commandQueue_->Signal(
            fence_.Get(),
            fenceValue_),
        "ID3D12CommandQueue::Signal");

    // Has the GPU already reached the requested value?
    if (fence_->GetCompletedValue() <
        fenceValue_)
    {
        // Ask Windows to signal fenceEvent_ once the GPU reaches this value.
        CheckHR(
            fence_->SetEventOnCompletion(
                fenceValue_,
                fenceEvent_),
            "ID3D12Fence::SetEventOnCompletion");

        // Sleep the CPU thread until the event is signaled.
        WaitForSingleObject(
            fenceEvent_,
            INFINITE);
    }
}

void DX12Renderer::Shutdown()
{
    LOG_INFO(
        "Shutting down DirectX 12 renderer");

    // Wait for any outstanding GPU work before releasing resources.
    if (commandQueue_ &&
        fence_ &&
        fenceEvent_)
    {
        try
        {
            WaitForGpu();
        }
        catch (...)
        {
            LOG_WARNING(
                "GPU synchronization during shutdown failed");
        }
    }

    if (fenceEvent_)
    {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    // Release render targets first.
    for (auto& renderTarget :
         renderTargets_)
    {
        renderTarget.Reset();
    }

    // Then higher-level DXGI/D3D12 objects.
    swapChain_.Reset();
    rtvHeap_.Reset();

    commandList_.Reset();
    commandAllocator_.Reset();
    commandQueue_.Reset();

    fence_.Reset();
    device_.Reset();
    factory_.Reset();

    frameBegun_ = false;

    LOG_INFO(
        "DirectX 12 renderer shutdown complete");
}
8. src/main.cpp
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
9. CMakeLists.txt

This is the important corrected version with:

target_include_directories(... src)
cmake_minimum_required(VERSION 3.25)

project(MyUI_Milestone1 LANGUAGES CXX)

# ============================================================================
# Source layout
#
#     Core
#         General application infrastructure.
#
#     Platform
#         Windows-specific functionality.
#
#     Renderer
#         DirectX 12-specific functionality.
#
#     main.cpp
#         Application entry point.
# ============================================================================

add_executable(
    MyUI_Milestone1
    WIN32

    src/main.cpp

    src/Core/Log.cpp
    src/Core/Log.h

    src/Platform/Win32Window.cpp
    src/Platform/Win32Window.h

    src/Renderer/DX12Renderer.cpp
    src/Renderer/DX12Renderer.h
)

# ============================================================================
# C++ standard
# ============================================================================

target_compile_features(
    MyUI_Milestone1
    PRIVATE
        cxx_std_20
)

# ============================================================================
# Include root
#
# This is important because our source code uses:
#
#     #include "Core/Log.h"
#     #include "Platform/Win32Window.h"
#     #include "Renderer/DX12Renderer.h"
#
# Adding "src" means those paths resolve correctly.
# ============================================================================

target_include_directories(
    MyUI_Milestone1
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================================
# Windows definitions
# ============================================================================

target_compile_definitions(
    MyUI_Milestone1
    PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
)

# ============================================================================
# DirectX 12 libraries
# ============================================================================

target_link_libraries(
    MyUI_Milestone1
    PRIVATE
        d3d12.lib
        dxgi.lib
)
10. make.bat
@echo off
setlocal EnableExtensions

rem ============================================================================
rem MyUI build script
rem ============================================================================
rem
rem This is the main developer entry point.
rem
rem Supported commands:
rem
rem     make.bat
rem     make.bat debug
rem     make.bat release
rem     make.bat clean
rem     make.bat rebuild
rem     make.bat rebuild release
rem     make.bat help
rem
rem The actual build is still handled by CMake + Visual Studio.
rem This script just gives us a stable, simple workflow.
rem ============================================================================

set "ROOT=%~dp0"
set "BUILD_DIR=%ROOT%build"

rem Expected Visual Studio generator from your current environment.
set "GENERATOR=Visual Studio 18 2026"

set "ARCH=x64"

rem Default settings.
set "CONFIG=Debug"
set "ACTION=build"

rem ============================================================================
rem Parse first parameter
rem ============================================================================

if /I "%~1"=="debug"   set "CONFIG=Debug"
if /I "%~1"=="release" set "CONFIG=Release"
if /I "%~1"=="clean"   set "ACTION=clean"
if /I "%~1"=="rebuild" set "ACTION=rebuild"
if /I "%~1"=="help"    goto :help
if /I "%~1"=="/?"      goto :help

rem ============================================================================
rem Parse optional second parameter
rem
rem Example:
rem
rem     make.bat rebuild release
rem ============================================================================

if /I "%~2"=="debug"   set "CONFIG=Debug"
if /I "%~2"=="release" set "CONFIG=Release"

echo.
echo ============================================================
echo MyUI - Milestone 1 Extended
echo ============================================================
echo Root      : %ROOT%
echo Build     : %BUILD_DIR%
echo Generator : %GENERATOR%
echo Platform  : %ARCH%
echo Config    : %CONFIG%
echo Action    : %ACTION%
echo ============================================================
echo.

rem ============================================================================
rem Configure
rem
rem We configure only when the build directory has no CMake cache.
rem ============================================================================

if not exist "%BUILD_DIR%\CMakeCache.txt" (
    echo [CMAKE] Configuring project...

    cmake -S "%ROOT%" ^
          -B "%BUILD_DIR%" ^
          -G "%GENERATOR%" ^
          -A %ARCH%

    if errorlevel 1 (
        echo.
        echo [ERROR] CMake configuration failed.
        exit /b 1
    )
)

rem ============================================================================
rem Clean
rem ============================================================================

if "%ACTION%"=="clean" (
    echo [BUILD] Cleaning %CONFIG%...

    cmake --build "%BUILD_DIR%" ^
          --config %CONFIG% ^
          --target clean

    if errorlevel 1 (
        echo.
        echo [ERROR] Clean failed.
        exit /b 1
    )

    echo.
    echo [DONE] Clean completed.
    exit /b 0
)

rem ============================================================================
rem Rebuild
rem ============================================================================

if "%ACTION%"=="rebuild" (
    echo [BUILD] Rebuilding %CONFIG%...

    cmake --build "%BUILD_DIR%" ^
          --config %CONFIG% ^
          --clean-first

    if errorlevel 1 (
        echo.
        echo [ERROR] Rebuild failed.
        exit /b 1
    )

    goto :done
)

rem ============================================================================
rem Normal build
rem ============================================================================

echo [BUILD] Building %CONFIG%...

cmake --build "%BUILD_DIR%" ^
      --config %CONFIG%

if errorlevel 1 (
    echo.
    echo [ERROR] Build failed.
    exit /b 1
)

:done

echo.
echo ============================================================
echo Build completed successfully.
echo ============================================================
echo Executable:
echo %BUILD_DIR%\%CONFIG%\MyUI_Milestone1.exe
echo.
echo Log:
echo %ROOT%logs\MyUI.log
echo.

exit /b 0

:help

echo.
echo MyUI build commands
echo.
echo   make.bat                  Build Debug
echo   make.bat debug            Build Debug
echo   make.bat release          Build Release
echo   make.bat clean            Clean Debug
echo   make.bat rebuild          Clean and build Debug
echo   make.bat rebuild release  Clean and build Release
echo   make.bat help             Show this help
echo.

exit /b 0
11. README.md
# MyUI — Milestone 1 Extended

Milestone 1 Extended establishes the low-level foundation of the future
3D application/UI framework.

The only addition to the original Milestone 1 is the Core logging system.

No actual UI exists yet.

## Directory structure

```text
MyUI/
|
├── CMakeLists.txt
├── make.bat
├── README.md
|
├── logs/
|   └── MyUI.log
|
└── src/
    |
    ├── main.cpp
    |
    ├── Core/
    |   ├── Log.h
    |   └── Log.cpp
    |
    ├── Platform/
    |   ├── Win32Window.h
    |   └── Win32Window.cpp
    |
    └── Renderer/
        ├── DX12Renderer.h
        └── DX12Renderer.cpp
Architecture
                    MyUI Application
                           |
          +----------------+----------------+
          |                                 |
         Core                            Platform
          |                                 |
       Logger                         Win32Window
          |                                 |
          +----------------+----------------+
                           |
                       Renderer
                           |
                      DX12Renderer
                           |
                       DirectX 12
Rendering pipeline
Win32 HWND
    |
    v
DXGI Factory
    |
    v
D3D12 Device
    |
    v
Command Queue
    |
    +-- Command Allocator
    |
    +-- Command List
    |
    v
Swap Chain
    |
    +-- Back Buffer 0
    +-- Back Buffer 1
    |
    v
RTV Heap
    |
    v
BeginFrame()
    |
    | PRESENT -> RENDER_TARGET
    v
Clear()
    |
    | RENDER_TARGET -> PRESENT
    v
EndFrame()
    |
    v
ExecuteCommandLists()
    |
    v
Present()
    |
    v
Fence Synchronization
Logger

Examples:

LOG_INFO("Renderer initialized");

LOG_WARNING("Something unusual happened");

LOG_ERROR("Could not create device");

Formatted logging:

LOG_INFO(
    "Window size: {}x{}",
    width,
    height);

LOG_ERROR(
    "CreateDevice failed. HRESULT=0x{:08X}",
    hr);
Log destinations
File
logs/MyUI.log

The directory is created automatically.

Visual Studio

Messages are also sent through:

OutputDebugStringW()

In Visual Studio:

Debug
  -> Windows
     -> Output
Build

Debug:

.\make.bat

or:

.\make.bat debug

Release:

.\make.bat release

Clean:

.\make.bat clean

Rebuild:

.\make.bat rebuild

Rebuild Release:

.\make.bat rebuild release

Help:

.\make.bat help
Manual CMake
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
Current limitations

Milestone 1 intentionally does NOT contain:

UI widgets
UI IDs
widget state
layout
DrawList
text rendering
docking
tabs
workspace
3D viewport
input abstraction
textures
shaders
vertex buffers
index buffers
render graph
GUI Output Log panel
Milestone 1 status

COMPLETE.

Foundation:

Win32
+
DirectX 12
+
GPU synchronization
+
basic logging
=
ready for the next milestone

---

## 12. Build from your current `D:\TIGUI`

After replacing the files:

```powershell
cd D:\TIGUI

Because your existing build directory already contains a valid CMake configuration, I recommend the first build as:

.\make.bat rebuild

Then:

.\build\Debug\MyUI_Milestone1.exe

You should get:

D:\TIGUI\
├── build\
│   └── Debug\
│       └── MyUI_Milestone1.exe
│
└── logs\
    └── MyUI.log

The important output should look approximately like:

[2026-09-07 ...] [INFO ] MyUI application starting - Milestone 1 Extended
[2026-09-07 ...] [INFO ] Creating Win32 application window
[2026-09-07 ...] [INFO ] Win32 window created: 1280x720
[2026-09-07 ...] [INFO ] Initializing DirectX 12 renderer: 1280x720
[2026-09-07 ...] [INFO ] D3D12 debug layer enabled
[2026-09-07 ...] [INFO ] DXGI factory created
[2026-09-07 ...] [INFO ] D3D12 device created
[2026-09-07 ...] [INFO ] D3D12 command queue created
[2026-09-07 ...] [INFO ] D3D12 command allocator created
[2026-09-07 ...] [INFO ] D3D12 command list created
[2026-09-07 ...] [INFO ] Swap chain created: 2 buffers
[2026-09-07 ...] [INFO ] RTV descriptor heap created
[2026-09-07 ...] [INFO ] Created 2 swap-chain render target views
[2026-09-07 ...] [INFO ] GPU fence synchronization created
[2026-09-07 ...] [INFO ] DirectX 12 renderer initialized successfully

At this point I would consider Milestone 1 Extended frozen.

The next milestone should introduce our first framework-specific layer:

DX12 Renderer
      ↓
DrawCommand
      ↓
DrawList
      ↓
draw one rectangle

That is where TIGUI starts becoming our own renderer/UI framework, instead of just a DirectX 12 initialization sample.
