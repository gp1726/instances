struct InstanceData
{
    float3 offset;
    float3 velocity;
    float density;
};


StructuredBuffer<InstanceData> dataIn : register(t0);
RWStructuredBuffer<InstanceData> dataOut : register(u0);

cbuffer ParticleCB : register(b0)
{
    uint particleCount;
    float deltaTime;
    float gravity;
    float floorY;
    float densityRadius;
    float repulsionRadius;
    float maxSpeed;
    float collisionDamping;
};

// Example simple smoothing kernel (Quadratic)
// h = radius of influence
// distSq = squared distance to neighbor
float CalculateDensityWeight(float distSq, float h)
{
    float hSq = h * h;
    if (distSq < hSq)
    {
        // Example: (1 - (r/h)^2)^2 -> (1 - distSq/hSq)^2
        float term = 1.0f - distSq / hSq;
        return term * term; // Adjust normalization/exponent as needed
    }
    return 0.0f;
}

[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    uint idx = id.x;
    if (idx >= particleCount)
        return;
    
    InstanceData p = dataIn[idx];
    
    //Apply gravity
    p.velocity.y += gravity * deltaTime;
    
    
    // --- Calculate Density and Repulsion (O(N^2) - NEEDS OPTIMIZATION FOR LARGE N) ---
    float calculated_density = 0.0f; // Start with zero density contribution
    float3 repulsionForce = float3(0, 0, 0);
    
    float density = 0.0f;
    for (uint i = 0; i < particleCount; i++)
    {
        if (i == idx)
            continue;
        
        InstanceData other = dataIn[i];
        float3 vecToOther = p.offset - other.offset;
        float dstSquared = dot(vecToOther, vecToOther);
        
        calculated_density += CalculateDensityWeight(dstSquared, densityRadius);
    
        if (dstSquared < (repulsionRadius * repulsionRadius)  && dstSquared > 0.00001f)
        {
            float3 dir = normalize(p.offset - other.offset);
            float repulsionConstant = 2.0f; // Experiment! Higher = stronger repulsion
            float forceMagnitude = repulsionConstant / max(dstSquared, 0.01f); // Inverse square, clamp min dist
            //float3 acceleration = dir * forceMagnitude; // F = ma, assume mass = 1
            //p.velocity += acceleration * deltaTime;
            repulsionForce += dir * forceMagnitude;
        }
    }
    p.density = calculated_density;
    
    p.velocity += repulsionForce * deltaTime;
    
        // --- Clamp Speed (Example: Magnitude Clamp) ---
    /*float speedSq = dot(p.velocity, p.velocity);
    if (speedSq > (maxSpeed * maxSpeed))
    {
        p.velocity = normalize(p.velocity) * maxSpeed;
    }*/
    
    p.offset += p.velocity * deltaTime;
    
    
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