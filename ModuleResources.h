
#pragma once

#include "Module.h"

class D3D12Module;

class ModuleResources : public Module
{
public:
    ModuleResources(D3D12Module* d3d12Module);

    bool init() override;
    bool cleanUp() override;

    bool CreateUploadBuffer(
        const void* data,
        UINT64 bufferSize,
        ComPtr<ID3D12Resource>& buffer);

    bool CreateDefaultBuffer(
        const void* data,
        UINT64 bufferSize,
        ComPtr<ID3D12Resource>& buffer);

private:
    D3D12Module* d3d12Module = nullptr;
};
