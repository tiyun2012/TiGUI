#include "DX12Renderer.h"

#include <array>
#include <cassert>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace
{
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

        #if defined(_DEBUG)
        {
            ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
                debugController->EnableDebugLayer();
        }
        #endif

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
        Shutdown();
        return false;
    }
}

bool DX12Renderer::CreateDevice()
{
    HRESULT hr = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_));

    return SUCCEEDED(hr);
}

bool DX12Renderer::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    CheckHR(device_->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&commandQueue_)));

    CheckHR(device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&commandAllocator_)));

    CheckHR(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList_)));

    // A newly created command list starts in the recording state.
    CheckHR(commandList_->Close());

    return true;
}

bool DX12Renderer::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
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
        CheckHR(swapChain_->GetBuffer(
            i,
            IID_PPV_ARGS(&renderTargets_[i])));

        device_->CreateRenderTargetView(
            renderTargets_[i].Get(),
            nullptr,
            handle);

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
        CheckHR(commandAllocator_->Reset());
        CheckHR(commandList_->Reset(
            commandAllocator_.Get(),
            nullptr));

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource =
            renderTargets_[frameIndex_].Get();
        barrier.Transition.StateBefore =
            D3D12_RESOURCE_STATE_PRESENT;
        barrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_RENDER_TARGET;

        commandList_->ResourceBarrier(
            1,
            &barrier);

        D3D12_CPU_DESCRIPTOR_HANDLE rtv =
            rtvHeap_->GetCPUDescriptorHandleForHeapStart();

        rtv.ptr +=
            static_cast<SIZE_T>(frameIndex_) *
            rtvDescriptorSize_;

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

    commandList_->ClearRenderTargetView(
        rtv,
        color.data(),
        0,
        nullptr);
}
void DX12Renderer::EndFrame()
{
    assert(frameBegun_);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource =
        renderTargets_[frameIndex_].Get();
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_PRESENT;

    commandList_->ResourceBarrier(
        1,
        &barrier);

    CheckHR(commandList_->Close());

    ID3D12CommandList* lists[] = {
        commandList_.Get()
    };

    commandQueue_->ExecuteCommandLists(
        1,
        lists);

    CheckHR(swapChain_->Present(
        1,
        0));

    WaitForGpu();

    frameIndex_ =
        swapChain_->GetCurrentBackBufferIndex();

    frameBegun_ = false;
}

void DX12Renderer::WaitForGpu()
{
    ++fenceValue_;

    CheckHR(commandQueue_->Signal(
        fence_.Get(),
        fenceValue_));

    if (fence_->GetCompletedValue() < fenceValue_)
    {
        CheckHR(fence_->SetEventOnCompletion(
            fenceValue_,
            fenceEvent_));

        WaitForSingleObject(
            fenceEvent_,
            INFINITE);
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
