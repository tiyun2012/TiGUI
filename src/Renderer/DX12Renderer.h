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