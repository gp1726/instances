struct InstanceData
{
    float3 offset;
    float3 velocity;
};


StructuredBuffer<InstanceData> dataIn : register(t0);
RWStructuredBuffer<InstanceData> dataOut : register(u0);

cbuffer ParticleCB : register(b0)
{
    uint particleCount;
    float deltaTime;
    float gravity;
    float floorY;
};

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;
    if (idx >= particleCount)
        return;
    
    InstanceData p = dataIn[idx];
    
    
    p.velocity.y += gravity * deltaTime;
    if (length(p.velocity) > 500.0)
    {
        p.velocity -= 0.1;
    }
    p.offset += p.velocity * deltaTime;
    
        for (uint i = 0; i < particleCount; i++)
        {
            if (i == idx)
                continue;
            InstanceData other = dataIn[i];
            float dist = length(p.offset - other.offset);
            if (dist < 0.5f && dist > 0.0001f)
            {
                float3 dir = normalize(p.offset - other.offset);
            //p.velocity += dir * 2.0f;
                float repulsionConstant = 2.0f; // Experiment! Higher = stronger repulsion
                float forceMagnitude = repulsionConstant / max(dist * dist, 0.01f); // Inverse square, clamp min dist
                float3 acceleration = dir * forceMagnitude; // F = ma, assume mass = 1
                p.velocity += acceleration * deltaTime;
            }
        }
        if (p.offset.y < floorY)
        {
            p.offset.y = floorY;
            p.velocity.y *= -0.4;
        }
        if (p.offset.x < -10.0)
        {
            p.offset.x = -10.0f;
            p.velocity.x *= -0.4;
        }
        if (p.offset.x > 10.0)
        {
            p.offset.x = 10.0f;
            p.velocity.x *= -0.4;
        }
        if (p.offset.z < -5.0)
        {
            p.offset.z = -5.0f;
            p.velocity.z *= -0.4;
        }
        if (p.offset.z > 5.0)
        {
            p.offset.z = 5.0f;
            p.velocity.z *= -0.4;
        }
    
    
        dataOut[idx] = p;
    }