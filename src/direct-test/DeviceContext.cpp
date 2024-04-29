#include "DeviceContext.h"

DeviceContext::DeviceContext(Win32Application& window, const std::vector<Particle>& particles) 
    : window(window), m_Particles(particles)
{
    m_camera.Init({ 0.0f, 0.0f, 1500.0f });
    m_camera.SetMoveSpeed(250.0f);

    InitD3D(window);
}

DeviceContext::~DeviceContext()
{
    WaitForPreviousFrame();
    Cleanup();
}

void DeviceContext::InitD3D(Win32Application& window)
{
    CreateDevice();

    D3D12_COMMAND_QUEUE_DESC cqDesc = {};
    cqDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    cqDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // direct means the gpu can directly execute this command queue

    m_Device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&m_CommandQueue));

    CreateSwapchain(window);

    CreateCommandQueues();

    CreateBackBuffers();

    CreateSynchronizaionPrimitives();

    CreateBufferResources();

    CreateRootSignatures();

    CreateGraphicsPSO();

    CreateComputePSO();

    CreateVertexBuffer();

    CreateDepthResources(window.Width, window.Height);

    CreateConstantBuffers();

    m_CommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
    m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    m_FenceValue[frameIndex]++;
    m_CommandQueue->Signal(m_Fence[frameIndex].Get(), m_FenceValue[frameIndex]);

    CreateVertexBufferView(window.Width, window.Height);

    LoadMatrices(window.m_aspectRatio);
}

void DeviceContext::CreateDevice()
{
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }

    ComPtr<ID3D12Debug> spDebugController0;
    ComPtr<ID3D12Debug1> spDebugController1;

    D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController0));
    spDebugController0->QueryInterface(IID_PPV_ARGS(&spDebugController1));
    spDebugController1->SetEnableGPUBasedValidation(true);

    IDXGIFactory4* dxgiFactory;
    CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

    IDXGIAdapter1* adapter; // adapters are the graphics card (this includes the embedded graphics on the motherboard)

    int adapterIndex = 0; // we'll start looking for directx 12  compatible graphics devices starting at index 0

    bool adapterFound = false; // set this to true when a good one was found

    // find first hardware gpu that supports d3d 12
    while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // we dont want a software device
            continue;
        }

        // we want a device that is compatible with direct3d 12 (feature level 11 or higher)
        auto hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr);
        if (SUCCEEDED(hr))
        {
            adapterFound = true;
            break;
        }

        ++adapterIndex;
    }

    D3D12CreateDevice(
        adapter,
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_Device)
    );
}

void DeviceContext::CreateCommandQueues()
{
    // -- Create the Command Allocators -- //

    for (int i = 0; i < frameBufferCount; i++)
    {
        m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator[i]));
    }

    // -- Create a Command List -- //

    m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator[frameIndex].Get(), NULL, IID_PPV_ARGS(&m_CommandList));

    // Create a Compute Command List

    D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
    computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;

    m_Device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_ComputeCommandQueue));

    m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_ComputeCommandAllocator));

    m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_ComputeCommandAllocator.Get(), NULL, IID_PPV_ARGS(&m_ComputeCommandList));
}

void DeviceContext::CreateSwapchain(Win32Application& window)
{
    DXGI_MODE_DESC backBufferDesc = {};
    backBufferDesc.Width = window.Width;
    backBufferDesc.Height = window.Height;
    backBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    // describe our multi-sampling. We are not multi-sampling, so we set the count to 1 (we need at least one sample of course)
    DXGI_SAMPLE_DESC sampleDesc = {};
    sampleDesc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = frameBufferCount; // number of buffers we have
    swapChainDesc.BufferDesc = backBufferDesc; // our back buffer description
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // this says the pipeline will render to this swap chain
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // dxgi will discard the buffer (data) after we call present
    swapChainDesc.OutputWindow = window.hwnd; // handle to our window
    swapChainDesc.SampleDesc = sampleDesc; // our multi-sampling description
    swapChainDesc.Windowed = !window.mFullscreen; // set to true, then if in fullscreen must call SetFullScreenState with true for full screen to get uncapped fps

    IDXGISwapChain* tempSwapChain;

    IDXGIFactory4* dxgiFactory;
    CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));

    dxgiFactory->CreateSwapChain(
        m_CommandQueue.Get(), // the queue will be flushed once the swap chain is created
        &swapChainDesc, // give it the swap chain description we created above
        &tempSwapChain // store the created swap chain in a temp IDXGISwapChain interface
    );

    m_SwapChain = static_cast<IDXGISwapChain3*>(tempSwapChain);

    frameIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void DeviceContext::CreateBackBuffers()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = frameBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvDescriptorHeap));

    rtvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < frameBufferCount; i++)
    {
        m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));

        m_Device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);

        rtvHandle.Offset(1, rtvDescriptorSize);
    }
}

void DeviceContext::CreateBufferResources()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    m_Device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap));

    srvDescriptorSize = m_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    {
        UINT sb_ShadowsSize = m_Particles.size() * sizeof(float);
        D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE(D3D12_HEAP_TYPE_DEFAULT));
        D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sb_ShadowsSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

        D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sb_ShadowsSize);

        m_Device->CreateCommittedResource(
            &defaultHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_sbShadows));

        m_Device->CreateCommittedResource(
            &uploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &uploadBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_sbShadowsUpload));

        std::vector<float> initialShadowsData(m_Particles.size(), 1.0f);

        D3D12_SUBRESOURCE_DATA shadowsData = {};
        shadowsData.pData = reinterpret_cast<UINT8*>(&initialShadowsData[0]);
        shadowsData.RowPitch = sb_ShadowsSize;
        shadowsData.SlicePitch = shadowsData.RowPitch;

        UpdateSubresources<1>(m_CommandList.Get(), m_sbShadows.Get(), m_sbShadowsUpload.Get(), 0, 0, 1, &shadowsData);
        m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_sbShadows.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
        //uavDesc.Format = DXGI_FORMAT_R32_UINT;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = m_Particles.size();
        uavDesc.Buffer.StructureByteStride = 0; // sizeof(float)
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, srvDescriptorSize);
        m_Device->CreateUnorderedAccessView(m_sbShadows.Get(), nullptr, &uavDesc, uavHandle);

    }

    {
        // compute constant buffer

        m_cbSunDir.sunDir = XMFLOAT3(-700, 500, 0);

        m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
            D3D12_HEAP_FLAG_NONE, // no flags
            &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
            D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
            nullptr, // we do not have use an optimized clear value for constant buffers
            IID_PPV_ARGS(&m_cbSunDirUploadHeap)
        );

        BYTE* mappedData = nullptr;
        m_cbSunDirUploadHeap->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

        memcpy(mappedData, &m_cbSunDir, sizeof(m_cbSunDir));

        m_cbSunDirUploadHeap->Unmap(0, nullptr);
    }

    UINT dataSize = m_Particles.size() * sizeof(Particle);
    D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);

    m_Device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_sbParticles));

    m_Device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_sbParticlesUpload));
    
    m_Particles.back().pos = m_cbSunDir.sunDir; // light source
    m_Particles.back().radius = 70.0f;

    D3D12_SUBRESOURCE_DATA particleData = {};
    particleData.pData = reinterpret_cast<UINT8*>(&m_Particles[0]);
    particleData.RowPitch = dataSize;
    particleData.SlicePitch = particleData.RowPitch;

    UpdateSubresources<1>(m_CommandList.Get(), m_sbParticles.Get(), m_sbParticlesUpload.Get(), 0, 0, 1, &particleData);
    m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_sbParticles.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = m_Particles.size();
    srvDesc.Buffer.StructureByteStride = sizeof(Particle);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle(m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 0, srvDescriptorSize);
    m_Device->CreateShaderResourceView(m_sbParticles.Get(), &srvDesc, srvHandle);
}

void DeviceContext::CreateSynchronizaionPrimitives()
{
    for (int i = 0; i < frameBufferCount; i++)
    {
        m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence[i]));

        m_FenceValue[i] = 0; // set the initial m_Fence value to 0
    }

    m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    m_Device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_threadFences));

    m_threadFenceEvents = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_threadFenceEvents == nullptr)
    {
        HRESULT_FROM_WIN32(GetLastError());
    }
}

void DeviceContext::CreateRootSignatures()
{
    // graphics root signature
    {
        // create a root descriptor, which explains where to find the data for this root parameter
        D3D12_ROOT_DESCRIPTOR rootCBVDescriptor;
        rootCBVDescriptor.RegisterSpace = 0;
        rootCBVDescriptor.ShaderRegister = 0;

        D3D12_DESCRIPTOR_RANGE range = {};
        range.BaseShaderRegister = 0;
        range.NumDescriptors = 1;
        range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        range.OffsetInDescriptorsFromTableStart = 0;

        D3D12_DESCRIPTOR_RANGE uavRange = {};
        uavRange.BaseShaderRegister = 0;
        uavRange.NumDescriptors = 1;
        uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        uavRange.OffsetInDescriptorsFromTableStart = 0;

        // create a root parameter and fill it out
        CD3DX12_ROOT_PARAMETER  rootParameters[3];
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // this is a constant buffer view root descriptor
        rootParameters[0].Descriptor = rootCBVDescriptor; // this is the root descriptor for this root parameter
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // our pixel shader will be the only shader accessing this parameter for now

        rootParameters[1].InitAsDescriptorTable(1, &range);
        rootParameters[2].InitAsDescriptorTable(1, &uavRange);

        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(_countof(rootParameters),
            rootParameters, // a pointer to the beginning of our root parameters array
            0,
            nullptr,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | // we can deny shader stages here for better performance
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
        );

        ID3DBlob* signature;
        D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
        m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_GraphicsRootSignature));
    }

    // compute root signature
    {
        D3D12_ROOT_DESCRIPTOR computeCBVDescriptor;
        computeCBVDescriptor.RegisterSpace = 0;
        computeCBVDescriptor.ShaderRegister = 0;

        D3D12_DESCRIPTOR_RANGE compRange = {};
        compRange.BaseShaderRegister = 0;
        compRange.NumDescriptors = 1;
        compRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        compRange.OffsetInDescriptorsFromTableStart = 0;

        D3D12_DESCRIPTOR_RANGE compUavRange = {};
        compUavRange.BaseShaderRegister = 0;
        compUavRange.NumDescriptors = 1;
        compUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        compUavRange.OffsetInDescriptorsFromTableStart = 0;

        // create a root parameter and fill it out
        CD3DX12_ROOT_PARAMETER  computeRootParameters[3];
        computeRootParameters[0].InitAsDescriptorTable(1, &compRange);
        computeRootParameters[1].InitAsDescriptorTable(1, &compUavRange);

        computeRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // this is a constant buffer view root descriptor
        computeRootParameters[2].Descriptor = computeCBVDescriptor; // this is the root descriptor for this root parameter
        computeRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // our pixel shader will be the only shader accessing this parameter for now


        CD3DX12_ROOT_SIGNATURE_DESC compRootSignatureDesc;
        compRootSignatureDesc.Init(_countof(computeRootParameters),
            computeRootParameters, // a pointer to the beginning of our root parameters array
            0,
            nullptr
        );

        ID3DBlob* compSignature;
        D3D12SerializeRootSignature(&compRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &compSignature, nullptr);
        m_Device->CreateRootSignature(0, compSignature->GetBufferPointer(), compSignature->GetBufferSize(), IID_PPV_ARGS(&m_ComputeRootSignature));
    }
}

void DeviceContext::CreateGraphicsPSO()
{
    // compile vertex shader
    ID3DBlob* vertexShader; // d3d blob for holding vertex shader bytecode
    ID3DBlob* errorBuff; // a buffer holding the error data if any
    auto hr = D3DCompileFromFile(L"VertexShader.hlsl",
        nullptr,
        nullptr,
        "VSMain",
        "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &vertexShader,
        &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
    }

    // fill out a shader bytecode structure, which is basically just a pointer
    // to the shader bytecode and the size of the shader bytecode
    D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
    vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
    vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

    // compile geometry shader

    ID3DBlob* geometryShader; // d3d blob for holding vertex shader bytecode
    hr = D3DCompileFromFile(L"GeometryShader.hlsl",
        nullptr,
        nullptr,
        "GSMain",
        "gs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &geometryShader,
        &errorBuff);
    if (FAILED(hr))
    {
        OutputDebugStringA((char*)errorBuff->GetBufferPointer());
    }

    D3D12_SHADER_BYTECODE geometryShaderBytecode = {};
    geometryShaderBytecode.BytecodeLength = geometryShader->GetBufferSize();
    geometryShaderBytecode.pShaderBytecode = geometryShader->GetBufferPointer();

    // compile pixel shader
    ID3DBlob* pixelShader;
    D3DCompileFromFile(L"PixelShader.hlsl",
        nullptr,
        nullptr,
        "PSMain",
        "ps_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &pixelShader,
        &errorBuff
    );

    // fill out shader bytecode structure for pixel shader
    D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
    pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
    pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

    // create input layout

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};

    inputLayoutDesc.NumElements = _countof(inputLayout);
    inputLayoutDesc.pInputElementDescs = inputLayout;

    DXGI_SAMPLE_DESC sampleDesc = {};
    sampleDesc.Count = 1; // multisample count (no multisampling, so we just put 1, since we still need 1 sample)

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
    psoDesc.InputLayout = inputLayoutDesc; // the structure describing our input layout
    psoDesc.pRootSignature = m_GraphicsRootSignature.Get(); // the root signature that describes the input data this pso needs
    psoDesc.VS = vertexShaderBytecode; // structure describing where to find the vertex shader bytecode and how large it is
    psoDesc.GS = geometryShaderBytecode;
    psoDesc.PS = pixelShaderBytecode; // same as VS but for pixel shader
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT; // type of topology we are drawing
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // format of the render target
    psoDesc.SampleDesc = sampleDesc; // must be the same sample description as the swapchain and depth/stencil buffer
    psoDesc.SampleMask = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT); // a default rasterizer state.
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT); // a default blent state.
    psoDesc.NumRenderTargets = 1; // we are only binding one render target
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT); // a default depth stencil state
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // create the pso
    m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineStateObject));
}

void DeviceContext::Cleanup()
{
    // wait for the gpu to finish all frames
    for (int i = 0; i < frameBufferCount; ++i)
    {
        frameIndex = i;
        WaitForPreviousFrame();
    }

    // get swapchain out of full screen before exiting
    BOOL fs = false;
    if (m_SwapChain->GetFullscreenState(&fs, NULL))
        m_SwapChain->SetFullscreenState(false, NULL);

    //SAFE_RELEASE(m_Device);
    //SAFE_RELEASE(m_SwapChain);
    //SAFE_RELEASE(m_CommandQueue);
    //SAFE_RELEASE(m_rtvDescriptorHeap);
    //SAFE_RELEASE(m_CommandList);

    //for (int i = 0; i < frameBufferCount; ++i)
    //{
    //    SAFE_RELEASE(m_renderTargets[i]);
    //    SAFE_RELEASE(m_CommandAllocator[i]);
    //    SAFE_RELEASE(m_Fence[i]);
    //};

    //SAFE_RELEASE(m_PipelineStateObject);
    //SAFE_RELEASE(m_GraphicsRootSignature);
    //SAFE_RELEASE(m_VertexBuffer);

    //SAFE_RELEASE(m_depthStencilBuffer);
    //SAFE_RELEASE(m_dsDescriptorHeap);

    //for (int i = 0; i < frameBufferCount; ++i)
    //{
    //    SAFE_RELEASE(m_constantBufferUploadHeaps[i]);
    //};
}

void DeviceContext::CreateComputePSO()
{
    std::string x_str = std::to_string(THREAD_X);
    std::string y_str = std::to_string(THREAD_Y);

    LPCSTR THREAD_X_VALUE = x_str.c_str();
    LPCSTR THREAD_Y_VALUE = y_str.c_str();

    D3D_SHADER_MACRO defines[] = {
        {"THREAD_X", THREAD_X_VALUE},
        {"THREAD_Y", THREAD_Y_VALUE},
        {NULL, NULL }
    };

    ID3DBlob* computeShader;
    D3DCompileFromFile(L"ComputeShader.hlsl",
        defines,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "CSMain",
        "cs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        &computeShader,
        nullptr
    );

    D3D12_SHADER_BYTECODE computeShaderBytecode = {};
    computeShaderBytecode.BytecodeLength = computeShader->GetBufferSize();
    computeShaderBytecode.pShaderBytecode = computeShader->GetBufferPointer();


    D3D12_COMPUTE_PIPELINE_STATE_DESC computePSOdesc = {};
    computePSOdesc.CS = computeShaderBytecode;
    computePSOdesc.pRootSignature = m_ComputeRootSignature.Get();

    m_Device->CreateComputePipelineState(&computePSOdesc, IID_PPV_ARGS(&m_ComputePipelineStateObject));
}

void DeviceContext::CreateVertexBuffer()
{
    m_VertexList.resize(m_Particles.size());

    for (int i = 0; i < m_VertexList.size(); ++i) {
        m_VertexList[i].color = XMFLOAT4(0.0, 1.0, 1.0, 1.0);
    }

    int vBufferSize = sizeof(Vertex) * m_VertexList.size();

    m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
        // from the upload heap to this heap
        nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
        IID_PPV_ARGS(&m_VertexBuffer));

    m_VertexBuffer->SetName(L"Vertex Buffer Resource Heap");

    ID3D12Resource* vBufferUploadHeap;
    m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
        nullptr,
        IID_PPV_ARGS(&vBufferUploadHeap));
    vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<UINT8*>(&m_VertexList[0]); // pointer to our vertex array
    vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
    vertexData.SlicePitch = vertexData.RowPitch; // also the size of our triangle vertex data

    UpdateSubresources<1>(m_CommandList.Get(), m_VertexBuffer.Get(), vBufferUploadHeap, 0, 0, 1, &vertexData);
    m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_VertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));
}

void DeviceContext::CreateDepthResources(uint32_t width, uint32_t height)
{
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    m_Device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsDescriptorHeap));
    
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
    depthStencilDesc.Texture2D.MipSlice = 0;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    m_Device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&m_depthStencilBuffer)
    );
    m_dsDescriptorHeap->SetName(L"Depth/Stencil Resource Heap");

    m_Device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &depthStencilDesc, m_dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void DeviceContext::CreateConstantBuffers()
{
    for (int i = 0; i < frameBufferCount; ++i)
    {
        m_Device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // this heap will be used to upload the constant buffer data
            D3D12_HEAP_FLAG_NONE, // no flags
            &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64), // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
            D3D12_RESOURCE_STATE_GENERIC_READ, // will be data that is read from so we keep it in the generic read state
            nullptr, // we do not have use an optimized clear value for constant buffers
            IID_PPV_ARGS(&m_constantBufferUploadHeaps[i])
        );
        
        m_constantBufferUploadHeaps[i]->SetName(L"Constant Buffer Upload Resource Heap");

        ZeroMemory(&m_cbPerObject, sizeof(m_cbPerObject));

        CD3DX12_RANGE readRange(0, 0);    // We do not intend to read from this resource on the CPU. (so end is less than or equal to begin)

        m_constantBufferUploadHeaps[i]->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvGPUAddress[i]));

        // Because of the constant read alignment requirements, constant buffer views must be 256 bit aligned. Our buffers are smaller than 256 bits,
        // so we need to add spacing between the two buffers, so that the second buffer starts at 256 bits from the beginning of the resource heap.
        memcpy(m_cbvGPUAddress[i], &m_cbPerObject, sizeof(m_cbPerObject)); // cube1's constant buffer data
    }
}

void DeviceContext::CreateVertexBufferView(uint32_t width, uint32_t height)
{
    int vBufferSize = sizeof(Vertex) * m_VertexList.size();

    m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.SizeInBytes = vBufferSize;
    m_VertexBufferView.StrideInBytes = sizeof(Vertex);

    m_Viewport.TopLeftX = 0;
    m_Viewport.TopLeftY = 0;
    m_Viewport.Width = width;
    m_Viewport.Height = height;
    m_Viewport.MinDepth = 0.0f;
    m_Viewport.MaxDepth = 1.0f;

    m_ScissorRect.left = 0;
    m_ScissorRect.top = 0;
    m_ScissorRect.right = width;
    m_ScissorRect.bottom = height;
}

void DeviceContext::WaitForPreviousFrame()
{
    // swap the current rtv buffer index so we draw on the correct buffer
    frameIndex = m_SwapChain->GetCurrentBackBufferIndex();

    // if the current m_Fence value is still less than "fenceValue", then we know the GPU has not finished executing
    // the command queue since it has not reached the "commandQueue->Signal(m_Fence, fenceValue)" command
    if (m_Fence[frameIndex]->GetCompletedValue() < m_FenceValue[frameIndex])
    {
        m_Fence[frameIndex]->SetEventOnCompletion(m_FenceValue[frameIndex], m_FenceEvent);
        
        // wait until the m_Fence has triggered the event that it's current value has reached "fenceValue". once it's value
        // has reached "fenceValue", we know the command queue has finished executing
        WaitForSingleObject(m_FenceEvent, INFINITE);
    }
    m_FenceValue[frameIndex]++;
}

void DeviceContext::LoadMatrices(float aspectRatio)
{
    m_Position = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); // set cube 1's position
    XMVECTOR posVec = XMLoadFloat4(&m_Position); // create xmvector for cube1's position

    XMMATRIX rotXMat = XMMatrixRotationX(0.0001f); // 0.0001
    XMMATRIX rotYMat = XMMatrixRotationY(0.0002f); // 0.0002
    XMMATRIX rotZMat = XMMatrixRotationZ(0.0015f); // 0.0003

    // add rotation to rotation matrix and store it
    XMMATRIX rotMat = XMLoadFloat4x4(&m_RotMat) * rotXMat * rotYMat * rotZMat;
    XMStoreFloat4x4(&m_RotMat, rotMat);

    // create translation matrix from position vector
    XMMATRIX translationMat = XMMatrixTranslationFromVector(XMLoadFloat4(&m_Position));

    XMMATRIX worldMat = rotMat * translationMat;

    XMMATRIX modelViewMatrix = XMMatrixMultiply(worldMat, m_camera.GetViewMatrix());

    // copy our ConstantBuffer instance to the mapped constant buffer resource

    XMStoreFloat4x4(&m_cbPerObject.wvpMat, XMMatrixMultiply(m_camera.GetViewMatrix(), m_camera.GetProjectionMatrix(0.8f, aspectRatio, 1.0f, 5000.0f)));
    XMStoreFloat4x4(&m_cbPerObject.invViewMat, XMMatrixInverse(nullptr, m_camera.GetViewMatrix()));

    memcpy(m_cbvGPUAddress[frameIndex], &m_cbPerObject, sizeof(m_cbPerObject));
}