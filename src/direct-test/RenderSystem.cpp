#include "RenderSystem.h"

RenderSystem::RenderSystem(Win32Application& window) : m_GPU(window, m_Particles)
{
}

void RenderSystem::RecordDrawingCommands()
{
    HRESULT hr;

    m_GPU.WaitForPreviousFrame();

    m_GPU.m_CommandAllocator[m_GPU.frameIndex]->Reset();

    m_GPU.m_CommandList->Reset(m_GPU.m_CommandAllocator[m_GPU.frameIndex].Get(), m_GPU.m_PipelineStateObject.Get());

    // transition the "frameIndex" render target from the present state to the render target state so the command list draws to it starting from here
    m_GPU.m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GPU.m_renderTargets[m_GPU.frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_GPU.m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_GPU.frameIndex, m_GPU.rtvDescriptorSize);

    // get a handle to the depth/stencil buffer
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_GPU.m_dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    m_GPU.m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f }; // 0.0f, 0.2f, 0.4f, 1.0f
    m_GPU.m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    m_GPU.m_CommandList->ClearDepthStencilView(m_GPU.m_dsDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    ID3D12DescriptorHeap* heaps[] = { m_GPU.m_srvDescriptorHeap.Get() };
    m_GPU.m_CommandList->SetDescriptorHeaps(_countof(heaps), heaps);

    m_GPU.m_CommandList->SetGraphicsRootSignature(m_GPU.m_GraphicsRootSignature.Get()); // set the root signature

    m_GPU.m_CommandList->RSSetViewports(1, &m_GPU.m_Viewport); // set the viewports
    m_GPU.m_CommandList->RSSetScissorRects(1, &m_GPU.m_ScissorRect); // set the scissor rects
    m_GPU.m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST); // set the primitive topology
    m_GPU.m_CommandList->IASetVertexBuffers(0, 1, &m_GPU.m_VertexBufferView); // set the vertex buffer (using the vertex buffer view)

    m_GPU.m_CommandList->SetGraphicsRootConstantBufferView(0, m_GPU.m_constantBufferUploadHeaps[m_GPU.frameIndex]->GetGPUVirtualAddress());

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_GPU.m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 0, m_GPU.srvDescriptorSize);
    m_GPU.m_CommandList->SetGraphicsRootDescriptorTable(1, srvHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle(m_GPU.m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_GPU.srvDescriptorSize);
    m_GPU.m_CommandList->SetGraphicsRootDescriptorTable(2, uavHandle);

    m_GPU.m_CommandList->DrawInstanced(m_Particles.size(), 1, 0, 0);

    // transition the "frameIndex" render target from the render target state to the present state. If the debug layer is enabled, you will receive a
    // warning if present is called on the render target when it's not in the present state
    m_GPU.m_CommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GPU.m_renderTargets[m_GPU.frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    m_GPU.m_CommandList->Close();
}

void RenderSystem::Render()
{
    static bool runOnce = true;

    if (runOnce) {
        RunSimulation();
        
        //ReadDataFromComputePipeline();

        runOnce = false;
    }

    RecordDrawingCommands();

    ID3D12CommandList* ppCommandLists[] = { m_GPU.m_CommandList.Get() };
    m_GPU.m_CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // this command goes in at the end of our command queue. we will know when our command queue 
    // has finished because the m_Fence value will be set to "fenceValue" from the GPU since the command
    // queue is being executed on the GPU
    m_GPU.m_CommandQueue->Signal(m_GPU.m_Fence[m_GPU.frameIndex].Get(), m_GPU.m_FenceValue[m_GPU.frameIndex]);

    // present the current backbuffer
    m_GPU.m_SwapChain->Present(0, 0);

    m_GPU.window.UpdateFPS();

    //ReadDataFromComputePipeline();
}

void RenderSystem::ReadDataFromComputePipeline()
{
    const int bufferSize = sizeof(float) * m_Particles.size();

    D3D12_HEAP_PROPERTIES readbackHeapProperties{ CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK) };
    D3D12_RESOURCE_DESC readbackBufferDesc{ CD3DX12_RESOURCE_DESC::Buffer(bufferSize) };

    m_GPU.m_Device->CreateCommittedResource(
        &readbackHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &readbackBufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_GPU.m_ReadbackBuffer)
    );

    D3D12_RESOURCE_BARRIER outputBufferResourceBarrier
    {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_GPU.m_sbShadows.Get(),
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_SOURCE)
    };
    m_GPU.m_ComputeCommandList->ResourceBarrier(1, &outputBufferResourceBarrier);

    m_GPU.m_ComputeCommandList->CopyResource(m_GPU.m_ReadbackBuffer.Get(), m_GPU.m_sbShadows.Get());

    outputBufferResourceBarrier =
    {
        CD3DX12_RESOURCE_BARRIER::Transition(
            m_GPU.m_sbShadows.Get(),
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
    };
    m_GPU.m_ComputeCommandList->ResourceBarrier(1, &outputBufferResourceBarrier);

    m_GPU.m_ComputeCommandList->Close();

    ID3D12CommandList* cmdsLists[] = { m_GPU.m_ComputeCommandList.Get() };
    m_GPU.m_ComputeCommandQueue->ExecuteCommandLists(1, cmdsLists);

    UINT64 threadFenceValue = InterlockedIncrement(&m_GPU.m_threadFenceValues);
    m_GPU.m_ComputeCommandQueue->Signal(m_GPU.m_threadFences.Get(), threadFenceValue);
    m_GPU.m_threadFences.Get()->SetEventOnCompletion(threadFenceValue, m_GPU.m_threadFenceEvents);
    WaitForSingleObject(m_GPU.m_threadFenceEvents, INFINITE);

    m_GPU.m_ComputeCommandAllocator->Reset();
    m_GPU.m_ComputeCommandList->Reset(m_GPU.m_ComputeCommandAllocator.Get(), m_GPU.m_ComputePipelineStateObject.Get());

    float* mappedData = nullptr;
    m_GPU.m_ReadbackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));

    std::ofstream fout("results.txt");

    for (int i = 0; i < m_Particles.size(); ++i)
    {
        fout << "( " << mappedData[i] << ", " << " )" << std::endl;
    }

    m_GPU.m_ReadbackBuffer->Unmap(0, nullptr);
}

void RenderSystem::RunSimulation()
{
    m_GPU.m_ComputeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GPU.m_sbShadows.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    m_GPU.m_ComputeCommandList->SetPipelineState(m_GPU.m_ComputePipelineStateObject.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_GPU.m_srvDescriptorHeap.Get() };
    m_GPU.m_ComputeCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    m_GPU.m_ComputeCommandList->SetComputeRootSignature(m_GPU.m_ComputeRootSignature.Get());

    CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(m_GPU.m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    m_GPU.m_ComputeCommandList->SetComputeRootDescriptorTable(0, srvHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandle(m_GPU.m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, m_GPU.srvDescriptorSize);
    m_GPU.m_ComputeCommandList->SetComputeRootDescriptorTable(1, uavHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(m_GPU.m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 2, m_GPU.srvDescriptorSize);
    m_GPU.m_ComputeCommandList->SetComputeRootConstantBufferView(2, m_GPU.m_cbSunDirUploadHeap->GetGPUVirtualAddress());

    m_GPU.m_ComputeCommandList->Dispatch(m_GPU.THREAD_X, m_GPU.THREAD_Y, 1);

    m_GPU.m_ComputeCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_GPU.m_sbShadows.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    m_GPU.m_ComputeCommandList->Close();
    ID3D12CommandList* ppCommandLists[] = { m_GPU.m_ComputeCommandList.Get() };

    m_GPU.m_ComputeCommandQueue->ExecuteCommandLists(1, ppCommandLists);

    UINT64 threadFenceValue = InterlockedIncrement(&m_GPU.m_threadFenceValues);
    m_GPU.m_ComputeCommandQueue->Signal(m_GPU.m_threadFences.Get(), threadFenceValue);
    m_GPU.m_threadFences.Get()->SetEventOnCompletion(threadFenceValue, m_GPU.m_threadFenceEvents);
    WaitForSingleObject(m_GPU.m_threadFenceEvents, INFINITE);

    m_GPU.m_ComputeCommandAllocator->Reset();
    m_GPU.m_ComputeCommandList->Reset(m_GPU.m_ComputeCommandAllocator.Get(), m_GPU.m_ComputePipelineStateObject.Get());
}

void RenderSystem::MainLoop()
{
    MSG msg;
    ZeroMemory(&msg, sizeof(MSG));

    while (Util::RunningApp)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                break;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            m_GPU.LoadMatrices(m_GPU.window.m_aspectRatio);
            Render();
        }
    }
}
