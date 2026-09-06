#include "DX12Renderer.h"

#include <cassert>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
    // Turn a failed HRESULT into an exception so the initialization sequence
    // remains easy to read. The top-level Initialize() catches the failure.
    void CheckHR(HRESULT hr)
    {
        if (FAILED(hr))
            throw std::runtime_error("DirectX 12 call failed.");
    }
}

bool DX12Renderer::Initialize(HWND hwnd, UINT width, UINT height)
{
    try
    {
        hwnd_ = hwnd;
        width_ = width;
        height_ = height;

        // Enable D3D12 validation in Debug builds. This should be enabled
        // before creating the device so validation covers device creation.
#if defined(_DEBUG)
        {
            ComPtr<ID3D12Debug> debugController;

            if (SUCCEEDED(D3D12GetDebugInterface(
                    IID_PPV_ARGS(&debugController))))
            {
                debugController->EnableDebugLayer();
            }
        }
#endif

        // DXGI owns graphics infrastructure such as adapters and swap chains.
        CheckHR(CreateDXGIFactory2(
            0,
            IID_PPV_ARGS(&factory_)));

        if (!CreateDevice())
            return false;

        if (!CreateCommandObjects())
            return false;

        if (!CreateSwapChain(hwnd_, width_, height_))
            return false;

        if (!CreateRTVHeap())
            return false;

        if (!CreateRenderTargets())
            return false;

        if (!CreateSyncObjects())
            return false;

        return true;
    }
    catch (...)
    {
        // Release anything already created before reporting failure.
        Shutdown();
        return false;
    }
}

bool DX12Renderer::CreateDevice()
{
    // nullptr selects the default hardware adapter. Feature level 11_0 is
    // enough for this first clear/present renderer.
    HRESULT hr = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_));

    return SUCCEEDED(hr);
}

bool DX12Renderer::CreateCommandObjects()
{
    // A DIRECT queue can execute the normal graphics command types.
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    CheckHR(device_->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&commandQueue_)));

    // The allocator supplies memory used while recording a command list.
    CheckHR(device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator_)));

    // Create the graphics command list associated with that allocator.
    CheckHR(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList_)));

    // Newly-created command lists are already open. Close here so every frame
    // follows the same Reset -> record -> Close pattern.
    CheckHR(commandList_->Close());

    return true;
}

bool DX12Renderer::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
    // Describe the two images that will alternate as the displayed back buffer.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;

    CheckHR(factory_->CreateSwapChainForHwnd(
        commandQueue_.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1));

    // We will manage fullscreen explicitly in a later window layer.
    CheckHR(factory_->MakeWindowAssociation(
        hwnd,
        DXGI_MWA_NO_ALT_ENTER));

    CheckHR(swapChain1.As(&swapChain_));

    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    return true;
}

bool DX12Renderer::CreateRTVHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = FrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    CheckHR(device_->CreateDescriptorHeap(
        &heapDesc,
        IID_PPV_ARGS(&rtvHeap_)));

    // Descriptor size is implementation-dependent and must be queried.
    rtvDescriptorSize_ =
        device_->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return true;
}

bool DX12Renderer::CreateRenderTargets()
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FrameCount; ++i)
    {
        // Get the actual swap-chain resource for this buffer.
        CheckHR(swapChain_->GetBuffer(
            i,
            IID_PPV_ARGS(&renderTargets_[i])));

        // Create an RTV descriptor that points to that resource.
        device_->CreateRenderTargetView(
            renderTargets_[i].Get(),
            nullptr,
            handle);

        // Move to the next descriptor slot.
        handle.ptr += rtvDescriptorSize_;
    }

    return true;
}

bool DX12Renderer::CreateSyncObjects()
{
    CheckHR(device_->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence_)));

    fenceValue_ = 0;

    // Windows event used to put the CPU thread to sleep until the GPU reaches
    // a requested fence value.
    fenceEvent_ = CreateEventW(
        nullptr,
        FALSE,
        FALSE,
        nullptr);

    return fenceEvent_ != nullptr;
}

bool DX12Renderer::BeginFrame()
{
    if (!commandAllocator_ || !commandList_)
        return false;

    try
    {
        // Milestone 1 waits for the GPU at the end of every frame. Therefore it
        // is safe to reuse this single allocator and command list.
        CheckHR(commandAllocator_->Reset());
        CheckHR(commandList_->Reset(
            commandAllocator_.Get(),
            nullptr));

        // Transition the current back buffer so it can be rendered into.
        //
        //     PRESENT -> RENDER_TARGET
        //
        // D3D12 requires this state change to be explicit.
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = renderTargets_[frameIndex_].Get();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

        commandList_->ResourceBarrier(1, &barrier);

        // Locate the RTV belonging to the current frame.
        D3D12_CPU_DESCRIPTOR_HANDLE rtv =
            rtvHeap_->GetCPUDescriptorHandleForHeapStart();

        rtv.ptr +=
            static_cast<SIZE_T>(frameIndex_) *
            rtvDescriptorSize_;

        // Tell the output-merger stage which render target receives writes.
        commandList_->OMSetRenderTargets(
            1,
            &rtv,
            FALSE,
            nullptr);

        frameBegun_ = true;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void DX12Renderer::Clear(const std::array<float, 4>& color)
{
    assert(frameBegun_);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    rtv.ptr +=
        static_cast<SIZE_T>(frameIndex_) *
        rtvDescriptorSize_;

    // std::array is contiguous, so data() provides the float[4] expected by
    // the D3D12 API.
    commandList_->ClearRenderTargetView(
        rtv,
        color.data(),
        0,
        nullptr);
}

void DX12Renderer::EndFrame()
{
    assert(frameBegun_);

    // The back buffer must be returned to PRESENT before Present() is called.
    //
    //     RENDER_TARGET -> PRESENT
    //
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets_[frameIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    commandList_->ResourceBarrier(1, &barrier);

    // Stop recording and submit the command list to the GPU.
    CheckHR(commandList_->Close());

    ID3D12CommandList* lists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, lists);

    // Ask the swap chain to display the completed back buffer.
    CheckHR(swapChain_->Present(1, 0));

    // Simple Milestone 1 synchronization. This is intentionally conservative.
    WaitForGpu();

    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    frameBegun_ = false;
}

void DX12Renderer::WaitForGpu()
{
    // Give this submission a new fence value.
    ++fenceValue_;

    // The GPU will signal the fence when it reaches this point in the queue.
    CheckHR(commandQueue_->Signal(
        fence_.Get(),
        fenceValue_));

    if (fence_->GetCompletedValue() < fenceValue_)
    {
        CheckHR(fence_->SetEventOnCompletion(
            fenceValue_,
            fenceEvent_));

        // Block the CPU until the GPU has completed the submitted work.
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void DX12Renderer::Shutdown()
{
    if (commandQueue_ && fence_)
    {
        try
        {
            WaitForGpu();
        }
        catch (...)
        {
            // Ignore shutdown synchronization errors while leaving the app.
        }
    }

    if (fenceEvent_)
    {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    for (auto& renderTarget : renderTargets_)
        renderTarget.Reset();

    swapChain_.Reset();
    rtvHeap_.Reset();
    commandList_.Reset();
    commandAllocator_.Reset();
    commandQueue_.Reset();
    fence_.Reset();
    device_.Reset();
    factory_.Reset();

    frameBegun_ = false;
}
