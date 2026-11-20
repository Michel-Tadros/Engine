#pragma once

#include "Module.h"

class D3D12Module;
class ModuleResources;

class Exercise2 : public Module
{
public:
    Exercise2(D3D12Module* d3d12, ModuleResources* resources);

    bool init() override;
    bool renderFrame();
    void render() override;
    bool cleanUp() override;

private:
    struct Vertex
    {
        float x, y, z;
    };

    bool createVertexBuffer();
    bool createRootSignature();
    bool createPipelineState();

    D3D12Module* d3d12 = nullptr;
    ModuleResources* resources = nullptr;

    Vertex vertices[3] =
    {
        { -1.0f, -1.0f, 0.0f },
        {  0.0f,  1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f }
    };

    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;
};