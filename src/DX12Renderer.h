#pragma once

#include <array>
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

// -----------------------------------------------------------------------------
// DX12Renderer
//
// The complete low-level graphics foundation for Milestone 1.
//
// Initialization creates:
//     DXGI factory
//     D3D12 device
//     direct command queue
//     command allocator
//     command list
//     swap chain
//     RTV descriptor heap
//     swap-chain render targets
//     fence + event
//
// The frame path is:
//     BeginFrame -> transition PRESENT to RENDER_TARGET
//                -> Clear
//     EndFrame   -> transition RENDER_TARGET to PRESENT
//                -> execute -> Present -> fence wait
// -----------------------------------------------------------------------------
class DX12Renderer
{
public:
    bool Initialize(HWND hwnd, UINT width, UINT height);
    void Shutdown();

    bool BeginFrame();
    void Clear(const std::array<float, 4>& color);
    void EndFrame();

private:
    bool CreateDevice();
    bool CreateCommandObjects();
    bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
    bool CreateRTVHeap();
    bool CreateRenderTargets();
    bool CreateSyncObjects();

    void WaitForGpu();

private:
    // Two swap-chain buffers = double buffering.
    static constexpr UINT FrameCount = 2;

    HWND hwnd_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;

    Microsoft::WRL::ComPtr<IDXGIFactory7> factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    // Commands are recorded into a list, then submitted through the queue.
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;

    // One RTV descriptor for each back buffer.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    UINT rtvDescriptorSize_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[FrameCount];

    // Explicit CPU/GPU synchronization for resource reuse.
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 fenceValue_ = 0;

    // Current swap-chain buffer index.
    UINT frameIndex_ = 0;

    // Guards frame-only operations such as Clear().
    bool frameBegun_ = false;
};
