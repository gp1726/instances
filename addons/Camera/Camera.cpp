#include "Camera.h"
#include <Windows.h> // For VK_ defines if HandleInput uses them directly

using namespace DirectX;

Camera::Camera() :
    position(0.0f, 0.0f, -50.0f),
    yaw(0.0f),
    pitch(0.0f),
    moveSpeed(5.0f),      // Units per second
    rotationSpeed(XM_PI), // Radians per second
    m_constantBuffer(nullptr)
{
    // Initialize matrices to identity
    XMStoreFloat4x4(&viewMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&projMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&viewProjMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&invViewProjMatrix, XMMatrixIdentity());
}

bool Camera::Initialize(ID3D11Device* device)
{
    // Create constant buffer
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC; // Use DYNAMIC for CPU updates
    cbDesc.ByteWidth = sizeof(CameraConstantBufferData);
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // Allow CPU writing

    HRESULT hr = device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    return SUCCEEDED(hr);
}

void Camera::Release()
{
    if (m_constantBuffer) {
        m_constantBuffer->Release();
        m_constantBuffer = nullptr;
    }
}

void Camera::Rotate(float deltaYaw, float deltaPitch)
{
    yaw += deltaYaw;
    pitch += deltaPitch;

    // Keep yaw in [0, 2*PI) range
    if (yaw >= XM_2PI) yaw -= XM_2PI;
    if (yaw < 0.0f) yaw += XM_2PI;

    // Clamp pitch to avoid gimbal lock and flipping
    const float maxPitch = XM_PIDIV2 - 0.01f; // Slightly less than 90 degrees
    pitch = max(min(pitch, maxPitch), -maxPitch);
}

void Camera::HandleInput(const bool keys[], float deltaTime)
{
    // Calculate movement amounts
    float effectiveMoveSpeed = moveSpeed * deltaTime;
    float effectiveRotationSpeed = rotationSpeed * deltaTime;

    // Calculate forward and right vectors based on current yaw (no pitch influence on planar movement)
    XMVECTOR forwardXZ = XMVectorSet(sinf(yaw), 0.0f, cosf(yaw), 0.0f);
    XMVECTOR rightXZ = XMVectorSet(cosf(yaw), 0.0f, -sinf(yaw), 0.0f);
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // Accumulate movement vector
    XMVECTOR moveVec = XMVectorZero();
    if (keys['W']) moveVec = XMVectorAdd(moveVec, forwardXZ);
    if (keys['S']) moveVec = XMVectorSubtract(moveVec, forwardXZ);
    if (keys['A']) moveVec = XMVectorSubtract(moveVec, rightXZ);
    if (keys['D']) moveVec = XMVectorAdd(moveVec, rightXZ);
    if (keys['E']) moveVec = XMVectorAdd(moveVec, worldUp); // Move up
    if (keys['Q']) moveVec = XMVectorSubtract(moveVec, worldUp); // Move down

    // Normalize diagonal movement, scale by speed, and add to position
    XMVECTOR currentPos = XMLoadFloat3(&position);
    // Only normalize if length is significant to avoid issues with zero vector
    if (XMVectorGetX(XMVector3LengthSq(moveVec)) > 1e-6f)
    {
        moveVec = XMVector3Normalize(moveVec);
    }
    currentPos = XMVectorMultiplyAdd(moveVec, XMVectorReplicate(effectiveMoveSpeed), currentPos); // pos += moveVec * speed
    XMStoreFloat3(&position, currentPos);


    // Handle rotation
    float deltaYaw = 0.0f;
    float deltaPitch = 0.0f;
    if (keys[VK_LEFT]) deltaYaw -= effectiveRotationSpeed;
    if (keys[VK_RIGHT]) deltaYaw += effectiveRotationSpeed;
    if (keys[VK_UP]) deltaPitch += effectiveRotationSpeed;   // Inverted Y typically not needed here
    if (keys[VK_DOWN]) deltaPitch -= effectiveRotationSpeed;

    Rotate(deltaYaw, deltaPitch);
}

void Camera::UpdateMatrices(float screenWidth, float screenHeight)
{
    // Ensure valid screen dimensions
    if (screenWidth <= 0 || screenHeight <= 0) return;

    // Calculate forward, right, and up vectors based on yaw and pitch
    XMVECTOR forward = GetForwardVectorXM();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // Initial up
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
    up = XMVector3Normalize(XMVector3Cross(forward, right)); // Recalculate up based on actual forward/right

    // Calculate view matrix
    XMVECTOR eyePosition = XMLoadFloat3(&position);
    XMVECTOR focusPoint = XMVectorAdd(eyePosition, forward);
    XMMATRIX view = XMMatrixLookAtLH(eyePosition, focusPoint, up);

    // Calculate projection matrix
    float aspectRatio = screenWidth / screenHeight;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspectRatio, 0.1f, 1000.0f); // Use a far plane suitable for your scene

    // Store matrices
    XMStoreFloat4x4(&viewMatrix, view);
    XMStoreFloat4x4(&projMatrix, proj);
    XMStoreFloat4x4(&viewProjMatrix, XMMatrixMultiply(view, proj));
    XMStoreFloat4x4(&invViewProjMatrix, XMMatrixInverse(nullptr, XMLoadFloat4x4(&viewProjMatrix)));

    // Store right and up vectors (used by some effects or constant buffers)
   // m_right = right; // If needed as member variables
   // m_up = up;
}

void Camera::UpdateBuffer(ID3D11DeviceContext* context)
{
    if (!m_constantBuffer) return;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = context->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        CameraConstantBufferData* dataPtr = (CameraConstantBufferData*)mappedResource.pData;

        // Transpose matrices for HLSL (column-major)
        XMStoreFloat4x4(&dataPtr->viewProj, XMMatrixTranspose(XMLoadFloat4x4(&viewProjMatrix)));

        dataPtr->cameraPos = position;
        dataPtr->padding1 = 0.0f; // Explicitly zero padding

        // Get Right and Up vectors from the View Matrix (already calculated in UpdateMatrices)
        // View Matrix structure (row-major in XMMATRIX):
        // [ right.x  up.x  forward.x  0 ]
        // [ right.y  up.y  forward.y  0 ]
        // [ right.z  up.z  forward.z  0 ]
        // [ -dot(right, pos) -dot(up, pos) -dot(forward, pos) 1 ]
        // We need the first two columns (or rows if transposed) for right/up vectors
        const XMMATRIX view = XMLoadFloat4x4(&viewMatrix);
        dataPtr->cameraRight = XMFLOAT3(view.r[0].m128_f32[0], view.r[0].m128_f32[1], view.r[0].m128_f32[2]);
        dataPtr->cameraUp = XMFLOAT3(view.r[1].m128_f32[0], view.r[1].m128_f32[1], view.r[1].m128_f32[2]);
        dataPtr->padding2 = 0.0f;
        dataPtr->padding3 = 0.0f;


        context->Unmap(m_constantBuffer, 0);
    }
}

// --- Getters Implementation ---

DirectX::XMMATRIX Camera::GetViewMatrix() const {
    return XMLoadFloat4x4(&viewMatrix);
}

DirectX::XMMATRIX Camera::GetProjectionMatrix() const {
    return XMLoadFloat4x4(&projMatrix);
}

DirectX::XMMATRIX Camera::GetViewProjectionMatrix() const {
    return XMLoadFloat4x4(&viewProjMatrix);
}

DirectX::XMMATRIX Camera::GetInvViewProjectionMatrix() const {
    return XMLoadFloat4x4(&invViewProjMatrix);
}

DirectX::XMVECTOR Camera::GetPositionXM() const {
    return XMLoadFloat3(&position);
}

DirectX::XMFLOAT3 Camera::GetPosition() const {
    return position;
}

DirectX::XMVECTOR Camera::GetForwardVectorXM() const {
    return XMVectorSet(
        cosf(pitch) * sinf(yaw),
        sinf(pitch),
        cosf(pitch) * cosf(yaw),
        0.0f
    );
}

DirectX::XMVECTOR Camera::GetRightVectorXM() const {
    // Calculate forward and global up
    XMVECTOR forward = GetForwardVectorXM();
    XMVECTOR globalUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // Calculate right vector using cross product
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(globalUp, forward));
    return right;
}

ID3D11Buffer* const* Camera::GetConstantBufferPtr() const {
    return &m_constantBuffer;
}