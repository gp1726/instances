#include <Windows.h>
#include <cstdio>
#include <vector>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
using namespace DirectX;


ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;

ID3D11ComputeShader* g_computeShader = nullptr;
ID3D11InputLayout* g_inputLayout = nullptr;
ID3D11VertexShader* g_vertexShader = nullptr;
ID3D11PixelShader* g_pixelShader = nullptr;

ID3D11DepthStencilState* g_depthStencilState = nullptr;
ID3D11DepthStencilView* g_depthStencilView = nullptr;
ID3D11RasterizerState* g_rasterizerState = nullptr;

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};
struct InstanceData {
    XMFLOAT3 offset; // position offset
};
int g_currentgridSize = 2;
const float spacing = 1.0f;
UINT g_instanceCount = 0;

ID3D11Buffer* g_vertexBuffer = nullptr;
ID3D11Buffer* g_indexBuffer = nullptr;
ID3D11Buffer* g_instanceBuffer = nullptr;

bool Create() {
    /*
    * CLEAR BUFFERS TO MAINTAIN MEMORY FOR RESIZING
    */
    if (g_instanceBuffer) {
        g_instanceBuffer->Release();
        g_instanceBuffer = nullptr;
    }
    if (g_vertexBuffer) {
        g_vertexBuffer->Release();
        g_vertexBuffer = nullptr;
    }
    if (g_indexBuffer) {
        g_indexBuffer->Release();
        g_indexBuffer = nullptr;
    }

    Vertex cubeVertices[] = {
    { XMFLOAT3(-0.5f,  0.5f, -0.5f), XMFLOAT4(1, 0, 0, 1) }, // 0 - Top Left Back
    { XMFLOAT3(0.5f,  0.5f, -0.5f), XMFLOAT4(0, 1, 0, 1) }, // 1 - Top Right Back
    { XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(0, 0, 1, 1) }, // 2 - Bottom Left Back
    { XMFLOAT3(0.5f, -0.5f, -0.5f), XMFLOAT4(1, 1, 0, 1) }, // 3 - Bottom Right Back
    { XMFLOAT3(-0.5f,  0.5f,  0.5f), XMFLOAT4(1, 0, 1, 1) }, // 4 - Top Left Front
    { XMFLOAT3(0.5f,  0.5f,  0.5f), XMFLOAT4(0, 1, 1, 1) }, // 5 - Top Right Front
    { XMFLOAT3(-0.5f, -0.5f,  0.5f), XMFLOAT4(1, 1, 1, 1) }, // 6 - Bottom Left Front
    { XMFLOAT3(0.5f, -0.5f,  0.5f), XMFLOAT4(0.5f, 0.5f, 0.5f, 1) }, // 7 - Bottom Right Front
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

    /*InstanceData instances[] = {
    { XMFLOAT3(0.0f, 0.0f, 0.0f) },
    { XMFLOAT3(0.5f, 0.0f, 0.0f) },
    { XMFLOAT3(0.0f, 0.5f, 0.0f) }
    };*/
    int gridSize = g_currentgridSize;
    std::vector<InstanceData> instances(g_currentgridSize * g_currentgridSize * g_currentgridSize);

    
    for (int z = 0; z < gridSize; ++z) {
        for (int y = 0; y < gridSize; ++y) {
            for (int x = 0; x < gridSize; ++x) {
                int index = z * gridSize * gridSize + y * gridSize + x;
                float offsetX = (x - gridSize / 2) * spacing;
                float offsetY = (y - gridSize / 2) * spacing;
                float offsetZ = (z - gridSize / 2) * spacing;
                instances[index].offset = XMFLOAT3(offsetX, offsetY, offsetZ);
            }
        }
    }

    /*
    * Generates random heights for each x,z pair in gridSize*gridSize
    for (int z = 0; z < gridSize; ++z)
        for (int x = 0; x < gridSize; ++x)
        {
            int index = z * gridSize + x;
            float offsetX = (x - gridSize / 2) * spacing;
            float offsetZ = (z - gridSize / 2) * spacing;

            float offsetY = ((static_cast<float>(rand()) / RAND_MAX) * 5)-10; // height at this x,z

            instances[index].offset = XMFLOAT3(offsetX, offsetY, offsetZ);
        }
    */
    g_instanceCount = static_cast<UINT>(instances.size());
    D3D11_BUFFER_DESC instanceBufferDesc = {};
    instanceBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    instanceBufferDesc.ByteWidth = static_cast<UINT>(instances.size() * sizeof(InstanceData));
    instanceBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initInstanceData = {};
    initInstanceData.pSysMem = instances.data();

    g_device->CreateBuffer(&instanceBufferDesc, &initInstanceData, &g_instanceBuffer);
    
    //Create vertex buffer
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(cubeVertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = cubeVertices;

    HRESULT hr = g_device->CreateBuffer(&bufferDesc, &initData, &g_vertexBuffer);
    if (FAILED(hr)) return false;

    // Create index buffer
    D3D11_BUFFER_DESC indexDesc = {};
    indexDesc.Usage = D3D11_USAGE_DEFAULT;
    indexDesc.ByteWidth = sizeof(cubeIndices);
    indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = cubeIndices;

    hr = g_device->CreateBuffer(&indexDesc, &indexData, &g_indexBuffer);
    return SUCCEEDED(hr);

}

//CAMERA
struct CameraCB {
    DirectX::XMFLOAT4X4 viewProjMatrix;
    DirectX::XMFLOAT3 cameraRight;
    float padding1;
    DirectX::XMFLOAT3 cameraUp;
    float padding2;
};
struct Camera {
    XMFLOAT3 position = { 0.0f, 0.0f, -1.0f };
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

    g_context->UpdateSubresource(g_cameraCB, 0, nullptr, &cameraData, 0, 0);
}

void SetCameraPosition(float x, float y, float z) {
    g_camera.position = { x, y, z };
}

bool g_keys[256] = { false };
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
}


void CleanUpD3D() {

    if (g_device) g_device->Release();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_renderTargetView) g_renderTargetView->Release();

    if (g_depthStencilView) g_depthStencilView->Release();
    if (g_depthStencilState) g_depthStencilState->Release();
    if (g_rasterizerState) g_rasterizerState->Release();
    if (g_cameraCB) g_cameraCB->Release();

    if (g_indexBuffer) g_indexBuffer->Release();
    if (g_vertexBuffer) g_vertexBuffer->Release();

    if (g_instanceBuffer) g_instanceBuffer->Release();
    if (g_inputLayout) g_inputLayout->Release();
    if (g_vertexShader) g_vertexShader->Release();
    if (g_pixelShader) g_pixelShader->Release();
}

bool InitShaders() {

    //LOAD + COMPILE
    ID3DBlob* vsBlob = nullptr, * psBlob = nullptr, * gsBlob = nullptr, * errors = nullptr;
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


    //CREATE
    hr = g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Vertex Shader Creation Error", MB_OK); errors->Release(); }
        vsBlob->Release();
        psBlob->Release();
        gsBlob->Release();
        return false;
    }
    hr = g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader);
    if (FAILED(hr)) {
        if (errors) { MessageBoxA(nullptr, (char*)errors->GetBufferPointer(), "Pixel Shader Creation Error", MB_OK); errors->Release(); }
        vsBlob->Release();
        psBlob->Release();
        gsBlob->Release();
        return false;
    }
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "OFFSET",   0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
    };
    UINT numElements = ARRAYSIZE(layout);

    hr = g_device->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_inputLayout);
    if (FAILED(hr)) {
        MessageBoxA(nullptr, "Failed to create input layout", "Error", MB_OK);
        return false;
    }
    g_context->IASetInputLayout(g_inputLayout);
    
    vsBlob->Release();
    psBlob->Release();
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
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTargetView);
    backBuffer->Release();

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width);
    viewport.Height = static_cast<float>(height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    g_context->RSSetViewports(1, &viewport);

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
    rastDesc.CullMode = D3D11_CULL_BACK;
    rastDesc.FrontCounterClockwise = FALSE;
    g_device->CreateRasterizerState(&rastDesc, &g_rasterizerState);
    g_context->RSSetState(g_rasterizerState);
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
    switch (uMsg) {
    case WM_SIZE:
        ResizeD3D(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        g_keys[wParam & 0xFF] = true;
        return 0;
    case WM_KEYUP:
        g_keys[wParam & 0xFF] = false;
        return 0;
    
    case WM_MOUSEWHEEL: 
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);

        if (delta > 0)
            g_currentgridSize = min(g_currentgridSize + 1, 100); // cap max
        else
            g_currentgridSize = max(g_currentgridSize - 1, 1);   // cap min

        Create(); // regenerate instance buffer with new size!
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
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
    if (!CreateCameraBuffer()) {
        MessageBoxA(nullptr, "Failed to initialize Camera buffer", "Error", MB_OK);
        CleanUpD3D();
        return 0;
    }
    /*
    if (!InitComputeShader()) {
        MessageBoxA(nullptr, "Failed to initialize compute shader", "Error", MB_OK);
        CleanupD3D();
        return 0;
    }*/
    if (!InitShaders()) {
        MessageBoxA(nullptr, "Failed to initialize shaders", "Error", MB_OK);
        CleanUpD3D();
        return 0;
    }
    if (!Create()) {
        MessageBoxA(nullptr, "Failed to create triangle", "Error", MB_OK);
        CleanUpD3D();
        return 0;
    }

    LARGE_INTEGER frequency, prevTime, currentTime;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&prevTime);
    int frameCount = 0;
    double elapsed = 0.0;
    char title[128];
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg = {};
    float clearColor[4] = { 0.4f, 0.2f, 0.1f, 1.0f };
    // Main message loop
    UpdateCameraBuffer();
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            QueryPerformanceCounter(&currentTime);

            float deltaTime = static_cast<float>(currentTime.QuadPart - prevTime.QuadPart) / frequency.QuadPart;
            prevTime = currentTime;
            HandleCameraInput(deltaTime);
            UpdateCameraBuffer();
            // Set the camera constant buffer for the vertex shader

            
            // Clear render target and depth buffer
            g_context->OMSetRenderTargets(1, &g_renderTargetView, g_depthStencilView);
            g_context->ClearRenderTargetView(g_renderTargetView, clearColor);
            g_context->ClearDepthStencilView(g_depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

            g_context->VSSetConstantBuffers(0, 1, &g_cameraCB);
            ID3D11Buffer* buffers[2] = { g_vertexBuffer, g_instanceBuffer };
            UINT strides[2] = { sizeof(Vertex), sizeof(InstanceData) };
            UINT offsets[2] = { 0, 0 };
            g_context->IASetVertexBuffers(0, 2, buffers, strides, offsets);
            g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            g_context->IASetInputLayout(g_inputLayout);
            g_context->VSSetShader(g_vertexShader, nullptr, 0);
            g_context->PSSetShader(g_pixelShader, nullptr, 0);
            g_context->IASetIndexBuffer(g_indexBuffer, DXGI_FORMAT_R32_UINT, 0);
            g_context->DrawIndexedInstanced(36,g_instanceCount, 0, 0,0);

            // Update title
            elapsed += deltaTime;
            frameCount++;
            if (elapsed >= 1.0) {
                float fps = frameCount / elapsed;
                sprintf_s(title, sizeof(title),
                    "DX11 Particle Simulation | FPS: %.1f | Grid: %dx%dx%d | Pos: (%.2f, %.2f, %.2f)",
                    fps, g_currentgridSize, g_currentgridSize, g_currentgridSize,
                    g_camera.position.x,
                    g_camera.position.y,
                    g_camera.position.z
                );
                SetWindowTextA(hwnd, title);
                frameCount = 0;
                elapsed = 0.0;
            }

            g_swapChain->Present(1, 0);
        }
    }
    CleanUpD3D();
    return 0;
}