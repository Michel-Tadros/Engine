#include "Globals.h"
#include "D3D12Module.h"
#include <cstdio>

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

    // Select the first adapter ranked for high-performance graphics.
    hr = factory->EnumAdapterByGpuPreference(
        0,
        DXGI_GPU_PREFERENCE_MINIMUM_POWER,//My NVIDIA graphic card does not support DirectX feature 12_0 but the intel integrated one does.
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

    return true;
}
