struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
     // Calculate the distance from the center of the quad (UV space)
    float2 center = float2(0.5, 0.5); // Center of the quad in UV space
    float distance = length(input.uv - center);

    // Discard fragments outside the circle
    if (distance > 0.25) // Radius of the circle is 0.5 (half the quad size)
    {
        discard;
    }
    return input.color;
}