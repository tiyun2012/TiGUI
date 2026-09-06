#pragma once


#include <array>
#include <cstdint>
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

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
    static constexpr UINT FrameCount = 2;

    HWND hwnd_ = nullptr;
    UINT width_ = 0;
    UINT height_ = 0;

    Microsoft::WRL::ComPtr<IDXGIFactory7> factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    UINT rtvDescriptorSize_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets_[FrameCount];

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    UINT64 fenceValue_ = 0;

    UINT frameIndex_ = 0;
    bool frameBegun_ = false;
};
