#pragma once

#include "Module.h"
#include <dxgi1_6.h>
#include <array>

// Owns the core Direct3D 12 / DXGI objects.
// Add this module before modules that create D3D12 resources.
class D3D12Module: public Module
{
public:
    static constexpr UINT FRAME_COUNT = 2;
    ComPtr<ID3D12CommandAllocator> commandAllocators[FRAME_COUNT];
    ComPtr<ID3D12GraphicsCommandList> commandList;

    ComPtr<ID3D12Fence> fence;
    UINT64 fenceValues[FRAME_COUNT] = {};
    UINT64 nextFenceValue = 1;

    HANDLE fenceEvent = nullptr;
    explicit D3D12Module(HWND hWnd);
    D3D12Module() = default;
    ~D3D12Module() override = default;

    bool init() override;
    bool cleanUp() override;
    bool createCommandObjects();
    bool waitForFrame(UINT frameIndex);
    bool recordClearCommand(const FLOAT clearColor[4]);
    bool renderFrame(const FLOAT clearColor[4]);
    void render() override;

    ID3D12Device5* getDevice() const noexcept
    {
        return device.Get();
    }
    ID3D12CommandQueue* getCommandQueue() const
    {
        return commandQueue.Get();
    }

    IDXGISwapChain3* getSwapChain() const
    {
        return swapChain.Get();
    }

    ID3D12Resource* getCurrentBackBuffer() const
    {
        return backBuffers[currentBackBufferIndex].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE getCurrentRTV() const;

    UINT getCurrentBackBufferIndex() const
    {
        return currentBackBufferIndex;
    }

private:
    HWND hWnd = nullptr;
    bool createSwapChainAndRTVs();
    
    ComPtr<ID3D12Debug>       debugInterface;
    ComPtr<IDXGIFactory6>     factory;
    ComPtr<IDXGIAdapter4>     adapter;
    ComPtr<ID3D12Device5>     device;
    ComPtr<ID3D12InfoQueue>   infoQueue;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<IDXGISwapChain3> swapChain;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    std::array<ComPtr<ID3D12Resource>, FRAME_COUNT> backBuffers;
    UINT rtvDescriptorSize = 0;
    UINT currentBackBufferIndex = 0;
};
