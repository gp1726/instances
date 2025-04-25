cbuffer CameraBuffer : register(b0)
{
    matrix viewProjMatrix; // 64 bytes
    float3 cameraRight; // 12 bytes
    float padding1; // 4 bytes
    float3 cameraUp; // 12 bytes
    float padding2; // 4 bytes
    float3 cameraPosition; // 12 bytes
    float padding3; // 4 bytes
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};
struct InstanceDataStruct // Use a different name from the buffer variable
{
    float3 offset;
    float3 velocity;
    float density;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

StructuredBuffer<InstanceDataStruct> InstanceBuffer : register(t0); // Slot 0 for Instance SRV

PSInput main(VSInput input, uint instanceID : SV_InstanceID)
{
    PSInput output;
    InstanceDataStruct instanceData = InstanceBuffer[instanceID];

    // 1. Center position of the instance in world space
    float3 instanceCenterWorldPos = instanceData.offset;

    // 2. Calculate vector from instance center to camera
    float3 viewDir = normalize(cameraPosition - instanceCenterWorldPos);

    // 3. Calculate the billboard's right vector
    // Cross product of the (world/camera) up vector and the view direction.
    // Using cameraUp from the view matrix handles camera roll correctly.
    // Need to handle the case where viewDir is parallel to cameraUp (looking straight up/down)
    float3 billboardRight;
    // Check if view direction is too close to the camera's up vector
    if (abs(dot(viewDir, cameraUp)) > 0.999f)
    {
        // Looking straight up or down, use camera's right vector as billboard's right
        billboardRight = cameraRight; // Use camera's right vector (from cbuffer)
    }
    else
    {
        // Standard case: cross view direction with camera up
        billboardRight = normalize(cross(cameraUp, viewDir));
    }


    // 4. Calculate the billboard's up vector
    // Cross product of the view direction and the calculated billboard right vector.
    float3 billboardUp = normalize(cross(viewDir, billboardRight));

    float size = 0.25f;

    // 5. Calculate the world-space offset for this vertex using the *new* billboard vectors
    float3 vertexOffsetWorld = (billboardRight * input.position.x * size) + (billboardUp * input.position.y * size);

    // 6. Calculate the final world position of this vertex
    float3 finalWorldPos = instanceCenterWorldPos + vertexOffsetWorld;

    // 7. Transform to clip space
    output.position = mul(float4(finalWorldPos, 1.0f), viewProjMatrix);

    // Pass color (or calculate based on something else)
    output.color = float4(instanceData.density,0.3f,1.0f, 1.0f); // Pass vertex color through
    // output.color = float4(1.0, 1.0, 1.0, 1.0); // Or keep it white

    return output;
}