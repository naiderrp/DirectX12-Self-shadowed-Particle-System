#pragma once
#include "config.h"
#include "Win32Application.hpp"

#include "Camera.h"

#include "Particle.hpp"

#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }

struct DeviceContext
{
    DeviceContext(Win32Application& window, const std::vector<Particle>& particles);

    DeviceContext(const DeviceContext&) = delete;
    DeviceContext(DeviceContext&&) = default;

    DeviceContext& operator=(const DeviceContext&) = delete;
    DeviceContext& operator=(DeviceContext&&) = default;

    ~DeviceContext();

    void InitD3D(Win32Application& window);

    void CreateDevice();

    void CreateCommandQueues();

    void CreateSwapchain(Win32Application& window);

    void CreateBackBuffers();

    void CreateBufferResources();

    void CreateSynchronizaionPrimitives();

    void CreateRootSignatures();

    void CreateGraphicsPSO();

    void CreateComputePSO();

    void CreateVertexBuffer();

    void CreateDepthResources(uint32_t width, uint32_t height);

    void CreateConstantBuffers();

    void CreateVertexBufferView(uint32_t width, uint32_t height);

    void LoadMatrices(float aspectRatio);

    void Cleanup();

    void WaitForPreviousFrame();

public:
    static const size_t frameBufferCount = 3;

    std::vector<Particle> m_Particles;

    struct Vertex {
        XMFLOAT4 color = { 1.0, 0.0, 0.0, 1.0 };
    };

    std::vector<Vertex> m_VertexList;

    ComPtr<ID3D12Device> m_Device;

    ComPtr<IDXGISwapChain3> m_SwapChain;

    ComPtr<ID3D12CommandQueue> m_CommandQueue;

    ComPtr<ID3D12CommandQueue> m_ComputeCommandQueue;
    ComPtr<ID3D12CommandAllocator> m_ComputeCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_ComputeCommandList;

    ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;

    ComPtr<ID3D12Resource> m_renderTargets[frameBufferCount];

    const int THREAD_X = 32;
    const int THREAD_Y = 32;

    ComPtr<ID3D12Resource> m_sbParticles;
    ComPtr<ID3D12Resource> m_sbParticlesUpload;
    ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap;
    int srvDescriptorSize;

    ComPtr<ID3D12Resource> m_sbShadows;
    ComPtr<ID3D12Resource> m_sbShadowsUpload;

    ComPtr<ID3D12CommandAllocator> m_CommandAllocator[frameBufferCount];

    ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    ComPtr<ID3D12Fence> m_Fence[frameBufferCount];

    HANDLE m_FenceEvent;

    UINT64 m_FenceValue[frameBufferCount];

    ComPtr<ID3D12Fence> m_threadFences;
    HANDLE m_threadFenceEvents;

    UINT64 m_renderContextFenceValues = 0;
    UINT64 m_threadFenceValues = 0;

    int frameIndex;

    int rtvDescriptorSize;

    ComPtr<ID3D12PipelineState> m_PipelineStateObject;
    ComPtr<ID3D12PipelineState> m_ComputePipelineStateObject;

    ComPtr<ID3D12RootSignature> m_GraphicsRootSignature;
    ComPtr<ID3D12RootSignature> m_ComputeRootSignature;

    D3D12_VIEWPORT m_Viewport;

    D3D12_RECT m_ScissorRect;

    ComPtr<ID3D12Resource> m_VertexBuffer;

    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;

    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    ComPtr<ID3D12DescriptorHeap> m_dsDescriptorHeap;

    ComPtr<ID3D12DescriptorHeap> m_mainDescriptorHeap[frameBufferCount];
    ComPtr<ID3D12Resource> m_constantBufferUploadHeap[frameBufferCount];

    ComPtr<ID3D12Resource> m_ReadbackBuffer;

    struct ConstantBufferPerObject {
        DirectX::XMFLOAT4X4 wvpMat;
        DirectX::XMFLOAT4X4 invViewMat;
    };

    struct ComputeConstantBuffer {
        DirectX::XMFLOAT3 sunDir;
    };

    int ConstantBufferPerObjectAlignedSize = (sizeof(ConstantBufferPerObject) + 255) & ~255;

    ConstantBufferPerObject m_cbPerObject;

    ComputeConstantBuffer m_cbSunDir;
    ComPtr<ID3D12Resource> m_cbSunDirUploadHeap;

    ComPtr<ID3D12Resource> m_constantBufferUploadHeaps[frameBufferCount];

    UINT8* m_cbvGPUAddress[frameBufferCount];

    DirectX::XMFLOAT4X4 m_WorldMat;
    DirectX::XMFLOAT4X4 m_RotMat;
    DirectX::XMFLOAT4 m_Position;

    Camera m_camera = {};

    Win32Application& window;
};
