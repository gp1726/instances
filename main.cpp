#include <Windows.h>
#include <cstdio>
#include <vector>
#include <random>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
using namespace DirectX;
#include "addons/imgui.h" 
#include "addons/imgui_impl_win32.h"
#include "addons/imgui_impl_dx11.h"
#include "addons/Camera/Camera.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
Camera g_camera;
bool g_keys[256] = { false };


ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;

ID3D11ComputeShader* g_computeForceShader = nullptr;
ID3D11ComputeShader* g_computeDensityShader = nullptr;
ID3D11InputLayout* g_inputLayout = nullptr;
ID3D11VertexShader* g_vertexShader = nullptr;
ID3D11PixelShader* g_pixelShader = nullptr;

ID3D11DepthStencilState* g_depthStencilState = nullptr;
ID3D11DepthStencilView* g_depthStencilView = nullptr;
ID3D11RasterizerState* g_rasterizerState = nullptr;

//BUFFERS
ID3D11Buffer* g_vertexBuffer = nullptr;
ID3D11Buffer* g_indexBuffer = nullptr;

ID3D11Buffer* g_instanceBuffers[2] = { nullptr,nullptr };
ID3D11ShaderResourceView* g_instanceSRVs[2] = { nullptr, nullptr };
ID3D11UnorderedAccessView* g_instanceUAVs[2] = { nullptr,nullptr };
ID3D11ShaderResourceView* nullSRV = nullptr;
ID3D11UnorderedAccessView* nullUAV = nullptr;
ID3D11Buffer* g_particleCB = nullptr;
int g_currentReadBuffer = 0;
//For quads/cube vertex offsets
struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};

struct InstanceData {
    XMFLOAT3 offset; // position offset
    XMFLOAT3 velocity;
    float density;
    XMFLOAT3 force;
    XMFLOAT3 predictedPosition;
    XMFLOAT2 padding;
};

struct ParticleConstants {
    UINT particleCount;
    float deltaTime;
    float gravity;
    float floorY;// Ensure CB size is multiple of 16 bytes
    
    float densityRadius;
    float maxSpeed;
    float collisionDamping;
    float restDensity;

    float stiffnessConstant;
    float boundary;
    float padding[2];
};

int g_currentgridSize = 10;
const float spacing = 0.6f;
UINT g_instanceCount = 0;
static float deltaTime = 1/165.0f;
static float gravityUI = -9.81f;
static float floorYUI = -15.0f;
static float densityRadiusUI = 1.53f;
static float maxSpeedUI = 10.0f;
static float viscosityUI = 0.1f;
static float restDensityUI = 6.5f;
static float stiffnessUI = 1000.0f;
static float boundaryUI = 10.0f;




//initialize vertex and index buffers for drawing quad geometry from two triangles
bool initGeometry() {
    if (g_vertexBuffer) {

        g_vertexBuffer->Release();
        g_vertexBuffer = nullptr;
    }
    if (g_indexBuffer) {
        g_indexBuffer->Release();
        g_indexBuffer = nullptr;
    }

    Vertex cubeVertices[] = {
        { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT4(0.2f, 1, 1, 1) }, // 0 - Top Left Back
        { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT4(1, 0.2f, 1, 1) }, // 1 - Top Right Back
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(1, 0.2f, 1, 1) }, // 2 - Bottom Left Back
        { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(0.2f, 1, 1, 1) }, // 3 - Bottom Right Back
        { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT4(0.2f, 1, 1, 1) }, // 4 - Top Left Front
        { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT4(1, 0.2f, 1, 1) }, // 5 - Top Right Front
        { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT4(1, 0.2f, 1, 1) }, // 6 - Bottom Left Front
        { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT4(0.2f, 1, 1, 1) }, // 7 - Bottom Right Front
    };

    UINT cubeIndices[] = {
        // Front face
        4, 6, 5,
        5, 6, 7,

        // Back face
        1, 3, 0,
        0, 3, 2,

        // Left face
        0, 2, 4,
        4, 2, 6,

        // Right face
        5, 7, 1,
        1, 7, 3,

        // Top face
        0, 4, 1,
        1, 4, 5,

        // Bottom face
        2, 3, 6,
        6, 3, 7
    };

    // Define vertices for a 2D Quad in the XY plane
    Vertex quadVertices[] = {
        { XMFLOAT3(-0.5f,  0.5f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }, // 0 - Top Left
        { XMFLOAT3(0.5f,  0.5f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }, // 1 - Top Right
        { XMFLOAT3(-0.5f, -0.5f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }, // 2 - Bottom Left
        { XMFLOAT3(0.5f, -0.5f, 0.0f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }  // 3 - Bottom Right
    };

    // Indices for the 2 triangles forming the quad
    UINT quadIndices[] = {
        0, 1, 2, // Top-left triangle
        2, 1, 3  // Bottom-right triangle
    };

    //Create vertex buffer
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(quadVertices); 
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = quadVertices;

    HRESULT hr = g_device->CreateBuffer(&bufferDesc, &initData, &g_vertexBuffer);
    if (FAILED(hr)) return false;

    // Create index buffer
    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_DEFAULT;
    indexDesc.ByteWidth = sizeof(quadIndices);
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = quadIndices;

    hr = g_device->CreateBuffer(&indexDesc, &indexData, &g_indexBuffer);
    return SUCCEEDED(hr);
}
//CLEAR BUFFERS TO MAINTAIN MEMORY FOR RESIZING
void ReleaseParticleBuffers() {
    for (int i = 0; i < 2; ++i) {
        if (g_instanceBuffers[i]) { g_instanceBuffers[i]->Release(); g_instanceBuffers[i] = nullptr; }
        if (g_instanceSRVs[i]) { g_instanceSRVs[i]->Release();    g_instanceSRVs[i] = nullptr; }
        if (g_instanceUAVs[i]) { g_instanceUAVs[i]->Release();    g_instanceUAVs[i] = nullptr; }
    }
}
//creates particle buffers with size = gridsize
bool CreateParticleBuffers() {

	ReleaseParticleBuffers();

    int gridSize = g_currentgridSize;
    std::vector<InstanceData> instances;

    // --- Random Number Generation Setup ---
    std::random_device rd;          // Obtain a random number from hardware entropy
    std::mt19937 gen(rd());         // Seed the Mersenne Twister generator
    // Define the range for velocity components (e.g., -0.05 to +0.05)
    std::uniform_real_distribution<float> distr(-1.0f, 1.0f);
    // --------------------------------------
    
    // Reserve some space to potentially avoid reallocations
    instances.reserve(g_currentgridSize * g_currentgridSize * g_currentgridSize);
    int renderExtent = gridSize;
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                
                InstanceData instance;
                instance.offset.x = (static_cast<float>(x) - static_cast<float>(renderExtent - 1) / 2.0f) * spacing;
                instance.offset.y = (static_cast<float>(y) - static_cast<float>(renderExtent - 1) / 2.0f) * spacing;
                instance.offset.z = (static_cast<float>(z) - static_cast<float>(renderExtent - 1) / 2.0f) * spacing;
                instance.velocity = XMFLOAT3(distr(gen), distr(gen), distr(gen));
                instance.force = XMFLOAT3(0, 0, 0);
                instance.predictedPosition = instance.offset;
                instances.push_back(instance);
                
            }
        }
    }

    //Instance buffer(s)
    g_instanceCount = static_cast<UINT>(instances.size());

    D3D11_BUFFER_DESC instanceBufferDesc = {};
    instanceBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceBufferDesc.ByteWidth = static_cast<UINT>(instances.size() * sizeof(InstanceData));
    instanceBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    instanceBufferDesc.CPUAccessFlags = 0;
    instanceBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    instanceBufferDesc.StructureByteStride = sizeof(InstanceData);

    D3D11_SUBRESOURCE_DATA initInstanceData = {};
    initInstanceData.pSysMem = instances.data();

    HRESULT hr;
	// Create the instance buffers
    hr = g_device->CreateBuffer(&instanceBufferDesc, &initInstanceData, &g_instanceBuffers[0]);
	if (FAILED(hr)) return false;

	hr = g_device->CreateBuffer(&instanceBufferDesc, &initInstanceData, &g_instanceBuffers[1]);
	if (FAILED(hr)) return false;
    


    // --- Create Views (SRV & UAV) for both buffers ---
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN; // Format must be UNKNOWN for structured buffers
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = g_instanceCount;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN; // Format must be UNKNOWN for structured buffers
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = g_instanceCount;
    uavDesc.Buffer.Flags = 0; // No append/consume needed for simple overwrite

    for (int i = 0; i < 2; ++i)
    {
        hr = g_device->CreateShaderResourceView(g_instanceBuffers[i], &srvDesc, &g_instanceSRVs[i]);
        if (FAILED(hr)) { printf("Failed to create instance SRV %d\n", i); return false; }

        hr = g_device->CreateUnorderedAccessView(g_instanceBuffers[i], &uavDesc, &g_instanceUAVs[i]);
        if (FAILED(hr)) { printf("Failed to create instance UAV %d\n", i); return false; }
    }

    g_currentReadBuffer = 0; // Start by reading from buffer 0

    return true;


}
//initialize constant buffer for compute shader
bool initComputeConstantBuffer() {
    if (g_particleCB) { g_particleCB->Release(); g_particleCB = nullptr; }

    D3D11_BUFFER_DESC cbDesc = {};
    // Use DYNAMIC if you might change particle count frequently, DEFAULT if it's fixed per CreateParticleBuffers call
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(ParticleConstants); // Make sure this matches struct size (should be multiple of 16)
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // Needed for DYNAMIC usage
    cbDesc.MiscFlags = 0;
    cbDesc.StructureByteStride = 0;

    // Ensure size is aligned (optional but good practice)
    if (cbDesc.ByteWidth % 16 != 0) {
        cbDesc.ByteWidth = (cbDesc.ByteWidth + 15) & ~15;
        printf("Warning: Compute constant buffer size wasn't multiple of 16, adjusted.\n");
    }

    HRESULT hr = g_device->CreateBuffer(&cbDesc, nullptr, &g_particleCB);
    if (FAILED(hr)) { printf("Failed to create compute constant buffer\n"); return false; }
    return true;
}
// Call this after CreateParticleBuffers() or when count changes
void UpdateComputeConstantBuffer(ParticleConstants updateTerms) {
    if (!g_particleCB || g_instanceCount == 0) return; // Nothing to update / buffer not ready

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = g_context->Map(g_particleCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr)) {
        ParticleConstants* cbData = static_cast<ParticleConstants*>(mappedResource.pData);
        cbData->particleCount = g_instanceCount;
        cbData->deltaTime = updateTerms.deltaTime;
        cbData->gravity = updateTerms.gravity;
        cbData->floorY = updateTerms.floorY;

        cbData->densityRadius = updateTerms.densityRadius;
        cbData->maxSpeed = updateTerms.maxSpeed;
        cbData->collisionDamping = updateTerms.collisionDamping;
        cbData->restDensity = updateTerms.restDensity;

        cbData->stiffnessConstant = updateTerms.stiffnessConstant;
        cbData->boundary = updateTerms.boundary;

        // cbData->padding[...] is automatically handled by the struct definition
        g_context->Unmap(g_particleCB, 0);
    }
    else {
        printf("Failed to map compute constant buffer for update.\n");
    }
}

/*
//CAMERA
struct CameraCB {
    DirectX::XMFLOAT4X4 viewProjMatrix;
    DirectX::XMFLOAT3 cameraRight;
    float padding1;
    DirectX::XMFLOAT3 cameraUp;
    float padding2;
    DirectX::XMFLOAT3 cameraPosition;
    float padding3;
};
struct Camera {
    XMFLOAT3 position = { 0.0f, 0.0f, -50.0f };
    float yaw = 0.0f;   // rotation around Y axis (horizontal)
    float pitch = 0.0f; // rotation around X axis (vertical)
};
Camera g_camera;
ID3D11Buffer* g_cameraCB = nullptr;
bool CreateCameraBuffer() {
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.ByteWidth = sizeof(CameraCB);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    HRESULT hr = g_device->CreateBuffer(&cbDesc, nullptr, &g_cameraCB);
    return SUCCEEDED(hr);
}
void UpdateCameraBuffer() {
    RECT rect;
    GetClientRect(GetActiveWindow(), &rect); // Dynamically get window size
    float aspectRatio = static_cast<float>(rect.right - rect.left) / (rect.bottom - rect.top);
    // Calculate the view matrix
    XMVECTOR forward = XMVectorSet(
        cosf(g_camera.pitch) * sinf(g_camera.yaw),
        sinf(g_camera.pitch),
        cosf(g_camera.pitch) * cosf(g_camera.yaw),
        0.0f
    );
    XMVECTOR eyePosition = XMLoadFloat3(&g_camera.position);
    XMVECTOR focusPoint = XMVectorAdd(eyePosition, forward);
    XMVECTOR upDirection = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX viewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

    // Calculate the projection matrix
    XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 1000.0f);

    // Combine view and projection matrices
    XMMATRIX viewProjMatrix = XMMatrixMultiply(viewMatrix, projMatrix);
    

    // Update the constant buffer
    CameraCB cameraData = {};
    XMStoreFloat4x4(&cameraData.viewProjMatrix, XMMatrixTranspose(viewProjMatrix)); // Transpose for HLSL
    cameraData.cameraRight = { viewMatrix.r[0].m128_f32[0], viewMatrix.r[0].m128_f32[1], viewMatrix.r[0].m128_f32[2] };
    cameraData.cameraUp = { viewMatrix.r[1].m128_f32[0], viewMatrix.r[1].m128_f32[1], viewMatrix.r[1].m128_f32[2] };
    cameraData.cameraPosition = g_camera.position;
    g_context->UpdateSubresource(g_cameraCB, 0, nullptr, &cameraData, 0, 0);
}
void SetCameraPosition(float x, float y, float z) {
    g_camera.position = { x, y, z };
}

void RotateCamera(float deltaYaw, float deltaPitch) {
    g_camera.yaw += deltaYaw;
    g_camera.pitch += deltaPitch;

    // Clamp pitch to avoid gimbal lock
    const float maxPitch = XM_PIDIV2 - 0.01f; // Slightly less than 90 degrees
    if (g_camera.pitch > maxPitch) g_camera.pitch = maxPitch;
    if (g_camera.pitch < -maxPitch) g_camera.pitch = -maxPitch;
}
void HandleCameraInput(float deltaTime) {
    const float moveSpeed = 10.0f * deltaTime;
    const float rotationSpeed = 1.0f * deltaTime;

    // Calculate forward and right vectors (XZ plane only)
    XMVECTOR forward = XMVectorSet(
        sinf(g_camera.yaw),
        0.0f,
        cosf(g_camera.yaw),
        0.0f
    );
    XMVECTOR right = XMVectorSet(
        cosf(g_camera.yaw),
        0.0f,
        -sinf(g_camera.yaw),
        0.0f
    );

    XMVECTOR move = XMVectorZero();

    if (g_keys['W']) move = XMVectorAdd(move, forward);
    if (g_keys['S']) move = XMVectorSubtract(move, forward);
    if (g_keys['A']) move = XMVectorSubtract(move, right);
    if (g_keys['D']) move = XMVectorAdd(move, right);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // world up

    if (g_keys['E']) move = XMVectorAdd(move, up);      // move up
    if (g_keys['Q']) move = XMVectorSubtract(move, up); // move down

    move = XMVectorScale(move, moveSpeed);

    XMVECTOR position = XMLoadFloat3(&g_camera.position);
    position = XMVectorAdd(position, move);
    XMStoreFloat3(&g_camera.position, position);

    if (g_keys[VK_UP])    RotateCamera(0.0f, rotationSpeed);
    if (g_keys[VK_DOWN])  RotateCamera(0.0f, -rotationSpeed);
    if (g_keys[VK_LEFT])  RotateCamera(-rotationSpeed, 0.0f);
    if (g_keys[VK_RIGHT]) RotateCamera(rotationSpeed, 0.0f);
}*/

//D3D
//D3D Cleanup
void CleanUpD3D() {
    // Release Buffers & Views first
    ReleaseParticleBuffers();
    if (g_vertexBuffer) { g_vertexBuffer->Release(); g_vertexBuffer = nullptr; }
    if (g_indexBuffer) { g_indexBuffer->Release(); g_indexBuffer = nullptr; }
    //if (g_cameraCB) { g_cameraCB->Release(); g_cameraCB = nullptr; }
    g_camera.Release();
    if (g_particleCB) { g_particleCB->Release(); g_particleCB = nullptr; }


    // Release Shaders & Layout
    if (g_inputLayout) { g_inputLayout->Release(); g_inputLayout = nullptr; }
    if (g_vertexShader) { g_vertexShader->Release(); g_vertexShader = nullptr; }
    if (g_pixelShader) { g_pixelShader->Release(); g_pixelShader = nullptr; }
    if (g_computeDensityShader) { g_computeDensityShader->Release(); g_computeDensityShader = nullptr; }
    if (g_computeForceShader) { g_computeForceShader->Release(); g_computeForceShader = nullptr; }

    // Release States
    if (g_depthStencilState) { g_depthStencilState->Release(); g_depthStencilState = nullptr; }
    if (g_rasterizerState) { g_rasterizerState->Release(); g_rasterizerState = nullptr; }


    // Release Core D3D Objects
    if (g_renderTargetView) { g_renderTargetView->Release(); g_renderTargetView = nullptr; }
    if (g_depthStencilView) { g_depthStencilView->Release(); g_depthStencilView = nullptr; }
    // Note: Depth buffer texture itself is released implicitly when view is released if ref count is 0
    if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
    if (g_context) { g_context->Release(); g_context = nullptr; }
    if (g_device) { g_device->Release(); g_device = nullptr; }

}
bool InitShaders() {

    //LOAD + COMPILE
    ID3DBlob* csBlob1 = nullptr, *csBlob2 =nullptr, * vsBlob = nullptr, * psBlob = nullptr, * errors = nullptr;
    HRESULT hr = D3DCompileFromFile(L"VertexShader.hlsl", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errors);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Vertex Shader Compile Error", MB_OK); errors->Release(); }
        return false;
    }
    hr = D3DCompileFromFile(L"PixelShader.hlsl", nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errors);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Pixel Shader Compile Error", MB_OK); errors->Release(); }
        return false;
    }
	hr = D3DCompileFromFile(L"densityComputeShader.hlsl", nullptr, nullptr, "main", "cs_5_0", 0, 0, &csBlob1, &errors);
	if (FAILED(hr)) {
		if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Density Compute Shader Compile Error", MB_OK); errors->Release(); }
		return false;
	}
    hr = D3DCompileFromFile(L"forceComputeShader.hlsl", nullptr, nullptr, "main", "cs_5_0", 0, 0, &csBlob2, &errors);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Force Compute Shader Compile Error", MB_OK); errors->Release(); }
        return false;
    }


    //CREATE
    hr = g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Vertex Shader Creation Error", MB_OK); errors->Release(); }
        vsBlob->Release();
        psBlob->Release();
		csBlob1->Release();
        csBlob2->Release();
        return false;
    }
    hr = g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Pixel Shader Creation Error", MB_OK); errors->Release(); }
        vsBlob->Release();
        psBlob->Release();
		csBlob1->Release();
        csBlob2->Release();
        return false;
    }
	hr = g_device->CreateComputeShader(csBlob1->GetBufferPointer(), csBlob1->GetBufferSize(), nullptr, &g_computeDensityShader);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Density Compute Shader Creation Error", MB_OK); errors->Release(); }
        vsBlob->Release();
        psBlob->Release();
        csBlob1->Release();
        csBlob2->Release();
        return false;
    }
    hr = g_device->CreateComputeShader(csBlob2->GetBufferPointer(), csBlob2->GetBufferSize(), nullptr, &g_computeForceShader);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Force Compute Shader Creation Error", MB_OK); errors->Release(); }
        vsBlob->Release();
        psBlob->Release();
        csBlob1->Release();
        csBlob2->Release();
        return false;
    }



    D3D11_INPUT_ELEMENT_DESC layout[] = {
        //Vertex Data
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },        
    };
    UINT numElements = ARRAYSIZE(layout);

    hr = g_device->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_inputLayout);
    if (FAILED(hr)) {
        MessageBoxA(nullptr, "Failed to create input layout", "Error", MB_OK);
        vsBlob->Release();
        psBlob->Release();
        csBlob1->Release();
        csBlob2->Release();
        return false;
    }
    
    
    vsBlob->Release();
    psBlob->Release();
    csBlob1->Release();
    csBlob2->Release();
    if (errors) errors->Release();
    return SUCCEEDED(hr);

}

bool InitD3D(HWND hwnd) {

    RECT rect;
    GetClientRect(hwnd, &rect);
    UINT width = rect.right - rect.left;
    UINT height = rect.bottom - rect.top;
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG,
        featureLevels, 1, D3D11_SDK_VERSION, &scd, &g_swapChain, &g_device, &featureLevel, &g_context
    );
    if (FAILED(hr)) return false;



    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    hr = g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTargetView);
    if (FAILED(hr)) return false;
    backBuffer->Release();



    // Create depth stencil buffer
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* depthBuffer = nullptr;
    hr = g_device->CreateTexture2D(&depthDesc, nullptr, &depthBuffer);
    if (FAILED(hr)) return false;

    hr = g_device->CreateDepthStencilView(depthBuffer, nullptr, &g_depthStencilView);
    depthBuffer->Release();
    if (FAILED(hr)) return false;

    // --- Create Depth Stencil State ---
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = true;
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
    hr = g_device->CreateDepthStencilState(&dsDesc, &g_depthStencilState);
    if (FAILED(hr)) return false;
    g_context->OMSetDepthStencilState(g_depthStencilState, 1u);

    // Bind depth + color render targets
    g_context->OMSetRenderTargets(1, &g_renderTargetView, g_depthStencilView);

    //Rasterizer..?
    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FrontCounterClockwise = FALSE;
    g_device->CreateRasterizerState(&rastDesc, &g_rasterizerState);
    //g_context->RSSetState(g_rasterizerState);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    g_context->RSSetViewports(1, &viewport);
    if (!g_camera.Initialize(g_device)) { // Use the camera's initialize method
        MessageBoxA(hwnd, "Failed to initialize Camera Buffer", "Error", MB_OK);
        return false;
    }
    return true;
}
void ResizeD3D(HWND hwnd) {
    if (!g_swapChain || !g_context || !g_device) return;

    // Release existing views
    if (g_renderTargetView) { g_renderTargetView->Release(); g_renderTargetView = nullptr; }
    if (g_depthStencilView) { g_depthStencilView->Release(); g_depthStencilView = nullptr; }

    // Get new client size
    RECT rect;
    GetClientRect(hwnd, &rect);
    UINT width = rect.right - rect.left;
    UINT height = rect.bottom - rect.top;

    // Resize swap chain buffers
    HRESULT hr = g_swapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    if (FAILED(hr)) return;

    // Recreate render target view
    ID3D11Texture2D* backBuffer = nullptr;
    hr = g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr)) return;

    hr = g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTargetView);
    backBuffer->Release();
    if (FAILED(hr)) return;

    // Recreate depth stencil buffer + view
    ID3D11Texture2D* depthBuffer = nullptr;
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = g_device->CreateTexture2D(&depthDesc, nullptr, &depthBuffer);
    if (FAILED(hr)) return;

    hr = g_device->CreateDepthStencilView(depthBuffer, nullptr, &g_depthStencilView);
    depthBuffer->Release();
    if (FAILED(hr)) return;

    // Set new viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    g_context->RSSetViewports(1, &viewport);

    // Re-bind render targets
    g_context->OMSetRenderTargets(1, &g_renderTargetView, g_depthStencilView);
}
bool g_isPaused = false;
/*********************************************************
**
**
**
**   FUNCTION FOR HANDLING WINDOW INPUTS LIKE KEYPRESSES
**
**
**
**********************************************************/
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true; // ImGui handled the message, stop further processing

    switch (uMsg) {
    case WM_SIZE:
        ResizeD3D(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == 'P') { // Check if the key pressed is 'P'
            g_isPaused = !g_isPaused; // Toggle the pause state
            return 0; // Indicate we handled this key press
        }
        g_keys[wParam & 0xFF] = true;
        return 0;
    case WM_KEYUP:
        g_keys[wParam & 0xFF] = false;
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"ParticleSimWindow";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"DX11 Particle Simulation",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr
    );
    if (hwnd == nullptr) return 0;

    if (!InitD3D(hwnd)) {
        MessageBoxA(nullptr, "Failed to initialize D3D", "Error", MB_OK);
        return 0;
    }
    if (!InitShaders()) {
        MessageBoxA(nullptr, "Failed to initialize shaders", "Error", MB_OK);
        CleanUpD3D();
        return 0;
    }
    if (!initGeometry()) {
        MessageBoxA(nullptr, "Failed to init quad geometry", "Error", MB_OK);
        CleanUpD3D();
        return 0;
    }
    if (!CreateParticleBuffers()) {
        MessageBoxA(nullptr, "Failed to create particle buffers", "Error", MB_OK);
        CleanUpD3D();
        return 0;
    }
    if (!initComputeConstantBuffer()) {
        MessageBoxA(nullptr, "Failed to init compute constant buffer", "Error", MB_OK);
        CleanUpD3D();
        return 0;
    }

    // --- INITIALIZE IMGUI ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    
    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    if (!ImGui_ImplWin32_Init(hwnd)) {
        MessageBoxA(nullptr, "Failed to initialize ImGui Win32 backend", "Error", MB_OK);
        CleanUpD3D(); // Clean up D3D stuff before exiting
        return 0;
    }
    if (!ImGui_ImplDX11_Init(g_device, g_context)) {
        MessageBoxA(nullptr, "Failed to initialize ImGui DX11 backend", "Error", MB_OK);
        ImGui_ImplWin32_Shutdown(); // Clean up Win32 backend
        ImGui::DestroyContext();
        CleanUpD3D(); // Clean up D3D stuff
        return 0;
    }
    // -----------------------


    
    
    char title[128];
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg = {};
    float clearColor[4] = { 0.4f, 0.4f, 0.4f, 1.0f };

    UpdateComputeConstantBuffer({g_instanceCount,0.01f,gravityUI,floorYUI,densityRadiusUI,maxSpeedUI,viscosityUI,restDensityUI,stiffnessUI,boundaryUI});
    //g_camera.UpdateBuffer(g_context);
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    float screenWidth = static_cast<float>(clientRect.right - clientRect.left);
    float screenHeight = static_cast<float>(clientRect.bottom - clientRect.top);
    g_camera.UpdateMatrices(screenWidth, screenHeight);
    bool show_control_window = true;


    // Main message loop
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            //QueryPerformanceCounter(&currentTime);

            //float deltaTime = static_cast<float>(currentTime.QuadPart - prevTime.QuadPart) / frequency.QuadPart;
            //prevTime = currentTime;

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();


            if (show_control_window)
            {
                ImGui::Begin("Controls", &show_control_window); // Create a window

                ImGui::Text("Particle Simulation Settings");
                ImGui::Separator();

                // Temporary variable to hold grid size from slider
                static int currentGridSizeUI = g_currentgridSize;
                if (ImGui::SliderInt("Grid Size", &currentGridSizeUI, 1, 64)) // Limit max for performance sanity
                {
                    // Clamp the value just in case
                    currentGridSizeUI = max(1, min(currentGridSizeUI, 64));
                    if (currentGridSizeUI != g_currentgridSize) {
                        g_currentgridSize = currentGridSizeUI;
                        // Regenerate buffers when the slider value *changes*
                        ReleaseParticleBuffers(); // Explicitly release first
                        if (CreateParticleBuffers()) {
                            // Update the constant buffer immediately as particle count changed
                            // We'll update again below with the latest deltaTime, but ensure count is right now.
                            UpdateComputeConstantBuffer({g_instanceCount,deltaTime,gravityUI,floorYUI,densityRadiusUI,
                                maxSpeedUI,viscosityUI,restDensityUI,stiffnessUI, boundaryUI});
                        }
                        else {
                            // Handle error - maybe revert grid size?
                            MessageBoxA(hwnd, "Failed to recreate particle buffers for new size!", "Error", MB_OK);
                            // Consider reverting currentGridSizeUI = g_currentgridSize;
                        }
                    }


                }

                ImGui::Separator();
                ImGui::Text("Simulation Parameters:");

                // Get current values for UI (read from CB or have separate variables)
                // Reading directly from CB is tricky as it's GPU-side.
                // It's better to have CPU-side variables that you use to update the CB.

                bool constantsChanged = false;
                constantsChanged |= ImGui::DragFloat("Gravity", &gravityUI, 0.1f, -50.0f, 0.0f);
                constantsChanged |= ImGui::DragFloat("Floor Y", &floorYUI, 0.5f, -100.0f, 0.0f);
                constantsChanged |= ImGui::DragFloat("Density Radius", &densityRadiusUI, 0.01f, 0.0f, 3.0f);
                constantsChanged |= ImGui::DragFloat("Rest Density", &restDensityUI, 0.1f, 0.0f, 50.0f);
                constantsChanged |= ImGui::DragFloat("Max Speed", &maxSpeedUI, 1.0f, 0.0f, 100.0f);
                constantsChanged |= ImGui::DragFloat("Viscosity", &viscosityUI, 0.01f, 0.0f, 1.0f);
                constantsChanged |= ImGui::DragFloat("Stiffness", &stiffnessUI, 0.1f, 0.0f, 3000.0f);
                constantsChanged |= ImGui::DragFloat("Boundary", &boundaryUI, 1.0f, 1.0f, 100.0f);
                // If constants changed via UI, update the CB *before* the compute shader runs
                if (constantsChanged) {
                    // We need to update the constant buffer. We can reuse UpdateComputeConstantBuffer
                    // but maybe pass the new values, or modify it to read from these static vars.
                    // Let's modify UpdateComputeConstantBuffer slightly.
                    UpdateComputeConstantBuffer({ g_instanceCount, deltaTime,gravityUI,floorYUI,
                        densityRadiusUI,maxSpeedUI,viscosityUI,restDensityUI, stiffnessUI, boundaryUI });
                }


                ImGui::Separator();
                ImGui::Text("Camera Position: (%.2f, %.2f, %.2f)", g_camera.position.x, g_camera.position.y, g_camera.position.z);
                ImGui::Text("Particle Count: %u", g_instanceCount);
                ImGui::Text("FPS: %.1f", io.Framerate);


                ImGui::End();
            }
            // --- END IMGUI UI DEFINITION ---

            ImGuiIO& io = ImGui::GetIO();
            if (!io.WantCaptureKeyboard) { // Only move camera if ImGui doesn't need keyboard
                g_camera.HandleInput(g_keys,deltaTime);
            }
            // ADD THIS SECTION: Update camera matrices after input and before updating the GPU buffer
            RECT clientRectLoop;
            GetClientRect(hwnd, &clientRectLoop);
            float currentScreenWidth = static_cast<float>(clientRectLoop.right - clientRectLoop.left);
            float currentScreenHeight = static_cast<float>(clientRectLoop.bottom - clientRectLoop.top);

            if (currentScreenWidth > 0 && currentScreenHeight > 0) { // Avoid issues if minimized
                g_camera.UpdateMatrices(currentScreenWidth, currentScreenHeight);
            }
            //Handle camera
            //HandleCameraInput(deltaTime);
            g_camera.UpdateBuffer(g_context);

            UpdateComputeConstantBuffer({ g_instanceCount,deltaTime , gravityUI,floorYUI,densityRadiusUI,maxSpeedUI,viscosityUI, restDensityUI, stiffnessUI, boundaryUI});
            if (!g_isPaused) {
                //COMPUTE SHADER PAUSE
                // 1. Set Compute Shader
                g_context->CSSetShader(g_computeDensityShader, nullptr, 0);

                // 2. Set Input Resource (SRV) - Read from the current buffer
                int readIdx = g_currentReadBuffer;


                // 3. Set Output Resource (UAV) - Write to the *other* buffer
                int writeIdx = 1 - g_currentReadBuffer;

                g_context->CSSetShaderResources(0, 1, &g_instanceSRVs[readIdx]); // t0

                UINT initialCounts = -1; // Use -1 if not using append/consume counters
                g_context->CSSetUnorderedAccessViews(0, 1, &g_instanceUAVs[writeIdx], &initialCounts); // u0
                // 4. Set Constant Buffer
                g_context->CSSetConstantBuffers(0, 1, &g_particleCB); // b0

                // 5. Dispatch
                // Calculate number of thread groups needed
                UINT dispatchWidth = (g_instanceCount + 255) / 256; // Match [numthreads(256, 1, 1)]
                g_context->Dispatch(dispatchWidth, 1, 1);


                g_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);


                // Pass 2 for force integration
                g_context -> CSSetShader(g_computeForceShader, nullptr, 0);
                g_context->CSSetShaderResources(0, 1, &g_instanceSRVs[writeIdx]);
                g_context->CSSetUnorderedAccessViews(0, 1, &g_instanceUAVs[readIdx], &initialCounts); // u0
                g_context->CSSetConstantBuffers(0, 1, &g_particleCB); // b0
                g_context->Dispatch(dispatchWidth, 1, 1);

                // 6. Unbind UAV and SRV from Compute Shader stage

                g_context->CSSetUnorderedAccessViews(0, 1, &nullUAV, &initialCounts);
                
                g_context->CSSetShaderResources(0, 1, &nullSRV);
                g_context->CSSetShader(nullptr, nullptr, 0); // Unbind compute shader itself
                g_currentReadBuffer = readIdx;
            }
            //ALWAYS RUN REST OF CONTEXT TO MAINTAIN PREVIOUS DRAWING
            // Clear render target and depth buffer
            g_context->OMSetRenderTargets(1, &g_renderTargetView, g_depthStencilView);
            g_context->ClearRenderTargetView(g_renderTargetView, clearColor);
            g_context->ClearDepthStencilView(g_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            // 2. Set States (Rasterizer, Depth)
            g_context->RSSetState(g_rasterizerState);
            g_context->OMSetDepthStencilState(g_depthStencilState, 1u);

            // Set layout and topology
            g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_context->IASetInputLayout(g_inputLayout);

            // Set vertex + pixel shaders
            g_context->VSSetShader(g_vertexShader, nullptr, 0);
            g_context->PSSetShader(g_pixelShader, nullptr, 0);


            //Set vertex buffer
            int bufferToDraw = g_currentReadBuffer;
            ID3D11Buffer* vertexBuffer[] = { g_vertexBuffer };
            UINT stride[] = { sizeof(Vertex) };
            UINT offset[] = { 0 };
            g_context->IASetVertexBuffers(0, 1, vertexBuffer, stride, offset);
            // Bind SRV to slot 0
            g_context->VSSetShaderResources(0, 1, &g_instanceSRVs[bufferToDraw]);
            //Set index buffer
            g_context->IASetIndexBuffer(g_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            //Set camera constant buffer
            g_context->VSSetConstantBuffers(0, 1, g_camera.GetConstantBufferPtr());

            g_context->DrawIndexedInstanced(6, g_instanceCount, 0, 0, 0);
            // *** UNBIND VS SHADER RESOURCE *** (Good practice)

            g_context->VSSetShaderResources(0, 1, &nullSRV); // Unbind slot t0
            // --- IMGUI: RENDER ---
            ImGui::Render();
            // Ensure RTV is bound again IF your render pass unbinds it (it doesn't seem to)
            // g_context->OMSetRenderTargets(1, &g_renderTargetView, g_depthStencilView);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            // ------------------

            // Update title
            //elapsed += deltaTime;
            //frameCount++;

            g_swapChain->Present(1, 0);
            
        }
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanUpD3D();
    return 0;

}