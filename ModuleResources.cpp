
#include "Globals.h"
#include "ModuleResources.h"
#include "D3D12Module.h"

ModuleResources::ModuleResources(D3D12Module* d3d12Module)
    : d3d12Module(d3d12Module)
{
}

bool ModuleResources::init()
{
    return d3d12Module != nullptr &&
        d3d12Module->getDevice() != nullptr;
}

bool ModuleResources::cleanUp()
{
    return true;
}

// --------------------------------------------------
// CreateUploadBuffer
// --------------------------------------------------

bool ModuleResources::CreateUploadBuffer(
    const void* data,
    UINT64 bufferSize,
    ComPtr<ID3D12Resource>& buffer)
{
    if (data == nullptr || bufferSize == 0)
        return false;

    ID3D12Device5* device = d3d12Module->getDevice();

    // Describe the buffer.
    auto bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    // UPLOAD heap properties.
    auto heapProps =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Create the resource.
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&buffer));

    if (FAILED(hr))
        return false;

    // Map the buffer.
    BYTE* mappedData = nullptr;

    CD3DX12_RANGE readRange(0, 0);

    hr = buffer->Map(
        0,
        &readRange,
        reinterpret_cast<void**>(&mappedData));

    if (FAILED(hr))
        return false;

    // Copy CPU data into the upload buffer.
    memcpy(
        mappedData,
        data,
        static_cast<size_t>(bufferSize));

    // Unmap the buffer.
    buffer->Unmap(0, nullptr);

    return true;
}

// --------------------------------------------------
// CreateDefaultBuffer
// --------------------------------------------------

bool ModuleResources::CreateDefaultBuffer(
    const void* data,
    UINT64 bufferSize,
    ComPtr<ID3D12Resource>& buffer)
{
    if (data == nullptr || bufferSize == 0)
        return false;

    ID3D12Device5* device = d3d12Module->getDevice();

    // 1. Create the final DEFAULT buffer.
    auto bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

    auto defaultHeap =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

    HRESULT hr = device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&buffer));

    if (FAILED(hr))
        return false;

    // 2. Create and fill the temporary UPLOAD buffer.
    ComPtr<ID3D12Resource> stagingBuffer;

    if (!CreateUploadBuffer(
        data,
        bufferSize,
        stagingBuffer))
    {
        return false;
    }

    // 3. Wait until the command allocator is safe to reuse.
    if (!d3d12Module->Flush())
        return false;

    ID3D12CommandAllocator* allocator =
        d3d12Module->getCommandAllocator(0);

    ID3D12GraphicsCommandList* commandList =
        d3d12Module->getCommandList();

    // 4. Reset allocator and command list.
    hr = allocator->Reset();

    if (FAILED(hr))
        return false;

    hr = commandList->Reset(allocator, nullptr);

    if (FAILED(hr))
        return false;

    // 5. Copy staging buffer into DEFAULT buffer.
    commandList->CopyResource(
        buffer.Get(),
        stagingBuffer.Get());

    // 6. Close the command list.
    hr = commandList->Close();

    if (FAILED(hr))
        return false;

    // 7. Execute the copy command.
    ID3D12CommandList* commandLists[] =
    {
        commandList
    };

    d3d12Module->getCommandQueue()->ExecuteCommandLists(
        1,
        commandLists);

    // 8. Flush the queue.
    // The copy must finish before this method returns.
    if (!d3d12Module->Flush())
        return false;

    return true;
}
