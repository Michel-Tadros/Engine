#pragma once

#include "Module.h"
#include <dxgi1_6.h>

// Owns the core Direct3D 12 / DXGI objects.
// Add this module before modules that create D3D12 resources.
class D3D12Module: public Module
{
public:
    D3D12Module() = default;
    ~D3D12Module() override = default;

    bool init() override;
    bool cleanUp() override;

    ID3D12Device5* getDevice() const noexcept
    {
        return device.Get();
    }

private:
    ComPtr<ID3D12Debug>       debugInterface;
    ComPtr<IDXGIFactory6>     factory;
    ComPtr<IDXGIAdapter4>     adapter;
    ComPtr<ID3D12Device5>     device;
    ComPtr<ID3D12InfoQueue>   infoQueue;
};
