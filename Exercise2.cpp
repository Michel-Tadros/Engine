#include "Globals.h"

#include "Exercise2.h"
#include "D3D12Module.h"
#include "ModuleResources.h"
#include "ReadData.h"

#include <exception>

Exercise2::Exercise2(
    D3D12Module* d3d12,
    ModuleResources* resources)
    : d3d12(d3d12), resources(resources)
{
}

bool Exercise2::init()
{
    if (!d3d12 || !resources)
        return false;

    if (!createVertexBuffer())
        return false;

    if (!createRootSignature())
        return false;

    if (!createPipelineState())
        return false;

    return true;
}

bool Exercise2::createVertexBuffer()
{
    const UINT64 bufferSize = sizeof(vertices);

    if (!resources->CreateDefaultBuffer(
        vertices,
        bufferSize,
        vertexBuffer))
    {
        return false;
    }

    vertexBufferView.BufferLocation =
        vertexBuffer->GetGPUVirtualAddress();

    vertexBufferView.SizeInBytes =
        static_cast<UINT>(bufferSize);

    vertexBufferView.StrideInBytes =
        sizeof(Vertex);

    return true;
}

bool Exercise2::createRootSignature()
{
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

    rootSignatureDesc.Init(
        0,
        nullptr,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signatureBlob,
        &errorBlob
    );

    if (FAILED(hr))
        return false;

    hr = d3d12->getDevice()->CreateRootSignature(
        0,
        signatureBlob->GetBufferPointer(),
        signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)
    );

    return SUCCEEDED(hr);
}

bool Exercise2::createPipelineState()
{
    std::vector<uint8_t> dataVS;
    std::vector<uint8_t> dataPS;

    try
    {
        dataVS = DX::ReadData(L"Exercise2VS.cso");
        dataPS = DX::ReadData(L"Exercise2PS.cso");
    }
    catch (const std::exception&)
    {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        {
            "MY_POSITION",
            0,
            DXGI_FORMAT_R32G32B32_FLOAT,
            0,
            0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            0
        }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

    psoDesc.pRootSignature = rootSignature.Get();

    psoDesc.VS =
    {
        dataVS.data(),
        dataVS.size()
    };

    psoDesc.PS =
    {
        dataPS.data(),
        dataPS.size()
    };

    psoDesc.InputLayout =
    {
        inputLayout,
        static_cast<UINT>(_countof(inputLayout))
    };

    psoDesc.PrimitiveTopologyType =
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.SampleMask = 0xffffffff;

    psoDesc.RasterizerState =
        CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);

    psoDesc.BlendState =
        CD3DX12_BLEND_DESC(D3D12_DEFAULT);

    psoDesc.DepthStencilState =
        CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    HRESULT hr = d3d12->getDevice()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&pipelineState)
    );

    return SUCCEEDED(hr);
}

bool Exercise2::renderFrame()
{
    IDXGISwapChain3* swapChain = d3d12->getSwapChain();

    if (!swapChain)
        return false;

    const UINT frameIndex =
        swapChain->GetCurrentBackBufferIndex();

    if (!d3d12->waitForFrame(frameIndex))
        return false;

    ID3D12CommandAllocator* allocator =
        d3d12->commandAllocators[frameIndex].Get();

    ID3D12GraphicsCommandList* commandList =
        d3d12->commandList.Get();

    HRESULT hr = allocator->Reset();

    if (FAILED(hr))
        return false;

    hr = commandList->Reset(
        allocator,
        pipelineState.Get()
    );

    if (FAILED(hr))
        return false;

    ID3D12Resource* backBuffer =
        d3d12->getBackBuffer(frameIndex);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv =
        d3d12->getRTV(frameIndex);

    // PRESENT -> RENDER_TARGET
    auto barrierToRT = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET
    );

    commandList->ResourceBarrier(1, &barrierToRT);

    // Set and clear the render target.
    commandList->OMSetRenderTargets(
        1,
        &rtv,
        FALSE,
        nullptr
    );

    const FLOAT clearColor[] =
    {
        0.2f, 0.2f, 0.2f, 1.0f
    };

    commandList->ClearRenderTargetView(
        rtv,
        clearColor,
        0,
        nullptr
    );

    // Set the root signature.
    commandList->SetGraphicsRootSignature(
        rootSignature.Get()
    );

    // Set the vertex buffer.
    commandList->IASetVertexBuffers(
        0,
        1,
        &vertexBufferView
    );

    // Get the swap-chain dimensions.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

    hr = swapChain->GetDesc1(&swapChainDesc);

    if (FAILED(hr))
        return false;

    const FLOAT width =
        static_cast<FLOAT>(swapChainDesc.Width);

    const FLOAT height =
        static_cast<FLOAT>(swapChainDesc.Height);

    D3D12_VIEWPORT viewport =
    {
        0.0f,
        0.0f,
        width,
        height,
        0.0f,
        1.0f
    };

    D3D12_RECT scissor =
    {
        0,
        0,
        static_cast<LONG>(width),
        static_cast<LONG>(height)
    };

    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    // Set triangle topology.
    commandList->IASetPrimitiveTopology(
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST
    );

    // Draw the three vertices.
    commandList->DrawInstanced(3, 1, 0, 0);

    // RENDER_TARGET -> PRESENT
    auto barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT
    );

    commandList->ResourceBarrier(1, &barrierToPresent);

    hr = commandList->Close();

    if (FAILED(hr))
        return false;

    ID3D12CommandList* commandLists[] =
    {
        commandList
    };

    d3d12->getCommandQueue()->ExecuteCommandLists(
        1,
        commandLists
    );

    // Present the completed frame.
    hr = swapChain->Present(1, 0);

    if (FAILED(hr))
        return false;

    // Signal a unique fence value for this submission.
    const UINT64 fenceValue = d3d12->nextFenceValue++;

    hr = d3d12->getCommandQueue()->Signal(
        d3d12->fence.Get(),
        fenceValue
    );

    if (FAILED(hr))
        return false;

    d3d12->fenceValues[frameIndex] = fenceValue;

    return true;
}

void Exercise2::render()
{
    if (!renderFrame())
    {
        // Handle rendering failure.
    }
}

bool Exercise2::cleanUp()
{
    if (d3d12 && !d3d12->Flush())
        return false;

    pipelineState.Reset();
    rootSignature.Reset();
    vertexBuffer.Reset();

    return true;
}