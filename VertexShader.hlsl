cbuffer CameraBuffer : register(b0)
{
    matrix viewProjMatrix; // 64 bytes
    float3 cameraRight; // 12 bytes
    float padding1; // 4 bytes
    float3 cameraUp; // 12 bytes
    float padding2; // 4 bytes
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 offset : OFFSET;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 worldPos = float4(input.position + input.offset, 1.0f);
    output.position = mul(worldPos, viewProjMatrix);
    output.color = input.color;
    return output;
}