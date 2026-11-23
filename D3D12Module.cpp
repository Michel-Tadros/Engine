#include "Globals.h"
#include "D3D12Module.h"
#include <cstdio>

D3D12Module::D3D12Module(HWND hWnd)
    : hWnd(hWnd)
{
}
bool D3D12Module::init()
{
    HRESULT hr = S_OK;

    // The debug layer must be enabled before creating the factory/device.
#if defined(_DEBUG)
    hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
    if (FAILED(hr))
        return false;

    debugInterface->EnableDebugLayer();
#endif

    // Create the DXGI factory. The debug flag gives additional DXGI diagnostics
    // in Debug configurations.
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    hr = CreateDXGIFactory2(
        factoryFlags,
        IID_PPV_ARGS(&factory));

    if (FAILED(hr))
        return false;

    //My NVIDIA graphic card does not support DirectX feature_level_12_0 but the intel integrated one does.
    //Replace DXGI_GPU_PREFERENCE_MINIMUM_POWER with DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE 
    hr = factory->EnumAdapterByGpuPreference(
        0,
        DXGI_GPU_PREFERENCE_MINIMUM_POWER,
        IID_PPV_ARGS(&adapter));

    if (FAILED(hr))
        return false;

    // Create the D3D12 device using the feature level required by the exercise.
    hr = D3D12CreateDevice(
        adapter.Get(),
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&device));

    if (FAILED(hr))
        return false;

#if defined(_DEBUG)
    // Obtain the device's debug message queue and break on serious diagnostics.
    hr = device.As(&infoQueue);
    if (FAILED(hr))
        return false;

    if (FAILED(infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE)))
        return false;

    if (FAILED(infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_ERROR, TRUE)))
        return false;

    if (FAILED(infoQueue->SetBreakOnSeverity(
            D3D12_MESSAGE_SEVERITY_WARNING, TRUE)))
        return false;
#endif
    if (!createSwapChainAndRTVs())
        return false;
    if (!createCommandObjects())
        return false;
    if (!createDepthStencilBuffer())
        return false;
    return true;
}

bool D3D12Module::cleanUp()
{
    // Release dependent interfaces/objects before their parent objects.
    infoQueue.Reset();
    device.Reset();
    adapter.Reset();
    factory.Reset();
    debugInterface.Reset();
    for (auto& backBuffer : backBuffers)
        backBuffer.Reset();

    rtvHeap.Reset();
    swapChain.Reset();
    commandQueue.Reset();

    if (fenceEvent)
    {
        CloseHandle(fenceEvent);
        fenceEvent = nullptr;
    }

    fence.Reset();
    commandList.Reset();

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        commandAllocators[i].Reset();
    }

    return true;
}

bool D3D12Module::createSwapChainAndRTVs()
{
    HRESULT hr;

    // --------------------------------------------------
    // 1. Create the command queue
    // --------------------------------------------------

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    hr = device->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&commandQueue));

    if (FAILED(hr))
        return false;


    // --------------------------------------------------
    // 2. Get the window dimensions
    // --------------------------------------------------

    RECT clientRect = {};

    if (!GetClientRect(hWnd, &clientRect))
        return false;

    UINT width = clientRect.right - clientRect.left;
    UINT height = clientRect.bottom - clientRect.top;

    if (width == 0 || height == 0)
        return false;


    // --------------------------------------------------
    // 3. Create the swap chain
    // --------------------------------------------------

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;

    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;

    swapChainDesc.BufferUsage =
        DXGI_USAGE_RENDER_TARGET_OUTPUT;

    swapChainDesc.BufferCount = FRAME_COUNT;

    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    ComPtr<IDXGISwapChain1> swapChain1;

    hr = factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hWnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1);

    if (FAILED(hr))
        return false;

    hr = swapChain1.As(&swapChain);

    if (FAILED(hr))
        return false;


    // Disable the default Alt+Enter fullscreen shortcut.
    factory->MakeWindowAssociation(
        hWnd,
        DXGI_MWA_NO_ALT_ENTER);


    // --------------------------------------------------
    // 4. Create the RTV descriptor heap
    // --------------------------------------------------

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};

    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;

    hr = device->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(&rtvHeap));

    if (FAILED(hr))
        return false;


    // --------------------------------------------------
    // 5. Get the RTV descriptor size
    // --------------------------------------------------

    rtvDescriptorSize =
        device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


    // --------------------------------------------------
    // 6. Create an RTV for each back buffer
    // --------------------------------------------------

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        hr = swapChain->GetBuffer(
            i,
            IID_PPV_ARGS(&backBuffers[i]));

        if (FAILED(hr))
            return false;

        device->CreateRenderTargetView(
            backBuffers[i].Get(),
            nullptr,
            rtvHandle);

        // Move to the next descriptor.
        rtvHandle.ptr += rtvDescriptorSize;
    }


    // --------------------------------------------------
    // 7. Get the current back buffer index
    // --------------------------------------------------

    currentBackBufferIndex =
        swapChain->GetCurrentBackBufferIndex();

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Module::getCurrentRTV() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap->GetCPUDescriptorHandleForHeapStart();

    handle.ptr +=
        currentBackBufferIndex * rtvDescriptorSize;

    return handle;
}

bool D3D12Module::createCommandObjects()
{
    HRESULT hr;

    // One allocator for each back buffer.
    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&commandAllocators[i]));

        if (FAILED(hr))
            return false;
    }

    // Create the graphics command list.
    // CreateCommandList creates it in the recording state.
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        commandAllocators[0].Get(),
        nullptr,
        IID_PPV_ARGS(&commandList));

    if (FAILED(hr))
        return false;

    // Close it; it will be reset when recording the first frame.
    hr = commandList->Close();

    if (FAILED(hr))
        return false;

    // Fence used to track GPU execution.
    hr = device->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence));

    if (FAILED(hr))
        return false;

    fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (fenceEvent == nullptr)
        return false;

    return true;
}

bool D3D12Module::waitForFrame(UINT frameIndex)
{
    const UINT64 fenceValue = fenceValues[frameIndex];

    if (fence->GetCompletedValue() < fenceValue)
    {
        HRESULT hr = fence->SetEventOnCompletion(
            fenceValue,
            fenceEvent);

        if (FAILED(hr))
            return false;

        WaitForSingleObject(fenceEvent, INFINITE);
    }

    return true;
}

bool D3D12Module::recordClearCommand(
    const FLOAT clearColor[4])
{
    const UINT frameIndex = swapChain->GetCurrentBackBufferIndex();

    // Ensure the GPU has finished using this frame's allocator.
    if (!waitForFrame(frameIndex))
        return false;

    // The allocator can now safely be reused.
    HRESULT hr = commandAllocators[frameIndex]->Reset();

    if (FAILED(hr))
        return false;

    // Reset the command list and begin recording.
    hr = commandList->Reset(
        commandAllocators[frameIndex].Get(),
        nullptr);

    if (FAILED(hr))
        return false;

    // Get the RTV handle for the current back buffer.
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap->GetCPUDescriptorHandleForHeapStart();

    rtvHandle.ptr +=
        static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize;

    // Record the clear command.
    commandList->ClearRenderTargetView(
        rtvHandle,
        clearColor,
        0,
        nullptr);

    // Finish recording.
    hr = commandList->Close();

    if (FAILED(hr))
        return false;

    // Submit the recorded commands to the GPU.
    ID3D12CommandList* lists[] = { commandList.Get() };

    commandQueue->ExecuteCommandLists(
        1,
        lists);

    // Signal the fence so we know when this allocator is safe to reuse.
    const UINT64 nextFenceValue = fenceValues[frameIndex] + 1;

    hr = commandQueue->Signal(
        fence.Get(),
        nextFenceValue);

    if (FAILED(hr))
        return false;

    fenceValues[frameIndex] = nextFenceValue;

    return true;
}

bool D3D12Module::renderFrame(const FLOAT clearColor[4])
{
    // Get the current back buffer index.
    const UINT frameIndex = swapChain->GetCurrentBackBufferIndex();

    // 1. Wait until the GPU has finished using this frame's allocator.
    if (!waitForFrame(frameIndex))
        return false;

    // Reset the allocator.
    HRESULT hr = commandAllocators[frameIndex]->Reset();

    if (FAILED(hr))
        return false;

    // 2. Reset the command list to begin recording.
    hr = commandList->Reset(
        commandAllocators[frameIndex].Get(),
        nullptr);

    if (FAILED(hr))
        return false;

    // Get the current back buffer.
    ID3D12Resource* backBuffer = backBuffers[frameIndex].Get();

    // Get the RTV handle for this back buffer.
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        rtvHeap->GetCPUDescriptorHandleForHeapStart();

    rtvHandle.ptr +=
        static_cast<SIZE_T>(frameIndex) * rtvDescriptorSize;

    // 3. Transition PRESENT -> RENDER_TARGET.
    auto barrierToRenderTarget =
        CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer,
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

    commandList->ResourceBarrier(
        1,
        &barrierToRenderTarget);

    // 4. Clear the current render target.
    commandList->ClearRenderTargetView(
        rtvHandle,
        clearColor,
        0,
        nullptr);

    // 5. Transition RENDER_TARGET -> PRESENT.
    auto barrierToPresent =
        CD3DX12_RESOURCE_BARRIER::Transition(
            backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT);

    commandList->ResourceBarrier(
        1,
        &barrierToPresent);

    // 6. Close the command list.
    hr = commandList->Close();

    if (FAILED(hr))
        return false;

    // Submit commands to the GPU.
    ID3D12CommandList* commandLists[] =
    {
        commandList.Get()
    };

    commandQueue->ExecuteCommandLists(
        1,
        commandLists);

    // 7. Present the frame.
    hr = swapChain->Present(1, 0);

    if (FAILED(hr))
        return false;

    // 8. Signal the fence for this back buffer.
    const UINT64 signalValue = nextFenceValue++;

    hr = commandQueue->Signal(
        fence.Get(),
        signalValue);

    if (FAILED(hr))
        return false;

    fenceValues[frameIndex] = signalValue;
}

void D3D12Module::render()
{
    //Rendering for exercise 2 is different.
    //const FLOAT clearColor[] =
    //{
    //     1.0f, 0.0f, 0.0f, 1.0f
    //};

    //if (!renderFrame(clearColor))
    //{
    //    // Handle rendering failure.
    //}
}

bool D3D12Module::Flush()
{
    const UINT64 fenceValue = nextFenceValue++;

    HRESULT hr = commandQueue->Signal(
        fence.Get(),
        fenceValue);

    if (FAILED(hr))
        return false;

    hr = fence->SetEventOnCompletion(
        fenceValue,
        fenceEvent);

    if (FAILED(hr))
        return false;

    DWORD result = WaitForSingleObject(
        fenceEvent,
        INFINITE);

    return result == WAIT_OBJECT_0;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12Module::getRTV(UINT index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap->GetCPUDescriptorHandleForHeapStart();

    handle.ptr += static_cast<SIZE_T>(index) * rtvDescriptorSize;
    return handle;
}

bool D3D12Module::createDepthStencilBuffer()
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

    if (FAILED(swapChain->GetDesc1(&swapChainDesc)))
        return false;

    CD3DX12_HEAP_PROPERTIES heapProps(
        D3D12_HEAP_TYPE_DEFAULT
    );

    CD3DX12_RESOURCE_DESC desc =
        CD3DX12_RESOURCE_DESC::Tex2D(
            DXGI_FORMAT_D32_FLOAT,
            swapChainDesc.Width,
            swapChainDesc.Height,
            1,
            1,
            1,
            0,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
        );

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&depthStencilBuffer)
    );

    if (FAILED(hr))
        return false;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = device->CreateDescriptorHeap(
        &heapDesc,
        IID_PPV_ARGS(&dsvHeap)
    );

    if (FAILED(hr))
        return false;

    device->CreateDepthStencilView(
        depthStencilBuffer.Get(),
        nullptr,
        dsvHeap->GetCPUDescriptorHandleForHeapStart()
    );

    return true;
}
D3D12_CPU_DESCRIPTOR_HANDLE D3D12Module::getDSV() const
{
    return dsvHeap->GetCPUDescriptorHandleForHeapStart();
}