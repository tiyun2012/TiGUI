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