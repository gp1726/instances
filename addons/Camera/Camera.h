#pragma once
#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>
#include <d3d11.h>

class Camera
{
public:
    // --- Public Data Members (for simplicity, could use getters/setters) ---
    DirectX::XMFLOAT3 position;
    float yaw;   // Radians around Y axis
    float pitch; // Radians around X axis

    // --- Configuration ---
    float moveSpeed;
    float rotationSpeed; // Radians per second (or per input unit)

    // --- Matrices (calculated) ---
    DirectX::XMFLOAT4X4 viewMatrix;
    DirectX::XMFLOAT4X4 projMatrix;
    DirectX::XMFLOAT4X4 viewProjMatrix;
    DirectX::XMFLOAT4X4 invViewProjMatrix; // Needed for ray marching

public:
    Camera(); // Constructor

    // --- Initialization & Cleanup ---
    bool Initialize(ID3D11Device* device);
    void Release();

    // --- Update & Input ---
    // keys: Pointer to an array like g_keys[256]
    // deltaTime: Frame time in seconds
    void HandleInput(const bool keys[], float deltaTime);

    // Updates view/projection matrices based on current position/rotation and screen size
    // Must be called after HandleInput and before getting matrices or updating the buffer
    void UpdateMatrices(float screenWidth, float screenHeight);

    // Updates the D3D constant buffer
    // Call this after UpdateMatrices if you need the buffer updated on the GPU
    void UpdateBuffer(ID3D11DeviceContext* context);

    // --- Getters ---
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix() const;
    DirectX::XMMATRIX GetViewProjectionMatrix() const;
    DirectX::XMMATRIX GetInvViewProjectionMatrix() const;
    DirectX::XMVECTOR GetPositionXM() const;
    DirectX::XMFLOAT3 GetPosition() const; // Convenience
    DirectX::XMVECTOR GetForwardVectorXM() const;
    DirectX::XMVECTOR GetRightVectorXM() const;
    ID3D11Buffer* const* GetConstantBufferPtr() const; // Get the buffer for binding

private:
    // --- Internal Helper ---
    void Rotate(float deltaYaw, float deltaPitch); // Apply rotation, clamping pitch

    // --- D3D Resources ---
    // Constant buffer definition specific to camera needs (could be different from shader needs)
    struct CameraConstantBufferData {
        DirectX::XMFLOAT4X4 viewProj; // Or separate view/proj if needed by other shaders
        // Add other camera data shaders might need (e.g., position, up/right vectors)
        DirectX::XMFLOAT3 cameraPos;
        float padding1; // Ensure alignment
        DirectX::XMFLOAT3 cameraRight;
        float padding2;
        DirectX::XMFLOAT3 cameraUp;
        float padding3;
    };
    ID3D11Buffer* m_constantBuffer;
};
/*
* initialize Camera g_camera;
*
* g_camera.Release() in cleanup
*
* g_camera.Initialize(g_device) in InitD3D
*
* g_camera.UpdateMatrices(screenWidth, screenHeight) in Update
*
*/
#endif // CAMERA_H