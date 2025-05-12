struct InstanceData
{
    float3 offset;
    float3 velocity;
    float density;
    float3 force;
    float3 predictedPosition;
    float2 padding;
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
    float maxSpeed;
    float viscosity;
    float restDensity;
    
    float stiffnessConstant;
    float boundary;
    
    float2 padding;
};

// --- SPH Kernel Constants ---
static const float PI = 3.14159265f;
static const float POLY6_COEFF = 315.0f / (64.0f * PI * pow(densityRadius, 9)); // If you switch to Poly6 later
static const float SPIKY_GRAD_COEFF = -45.0f / (PI * pow(densityRadius, 6));
static const float SPIKY_KERN_COEFF = 15.0f / (PI * pow(densityRadius, 6)); // Normalization for Spiky Kernel itself

// Spiky Kernel for Density Calculation
// h = radius of influence
// dist = distance to neighbor (NOT squared)
float CalculateDensityWeight_Spiky(float dist, float h)
{
    if (dist < h && dist >= 0.0f) // Check if within radius
    {
        float x = h - dist;
        return SPIKY_KERN_COEFF * x * x * x; // (h - r)^3 term
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
    p.force = float3(0, 0, 0);
    

    
    p.predictedPosition = p.offset + p.velocity * deltaTime;
    
    // --- Calculate Density and Repulsion (O(N^2) - NEEDS OPTIMIZATION FOR LARGE N) ---
    float calculated_density = 0.0f; // Start with zero density contribution
    float3 repulsionForce = float3(0, 0, 0);
    
    float density = 0.0f;
    
    
    float hSq = densityRadius * densityRadius;
    
    for (uint i = 0; i < particleCount; i++)
    {
        if (i == idx)
        {
            calculated_density += CalculateDensityWeight_Spiky(0.0f, densityRadius);
            continue;
        }
        
        InstanceData other = dataIn[i];
        float3 vecToOther = p.predictedPosition - other.predictedPosition;
        float dstSquared = dot(vecToOther, vecToOther);
        
        if (dstSquared < hSq) // Check using squared distance first for efficiency
        {
            float dist = sqrt(dstSquared); // Calculate actual distance only if needed
            calculated_density += CalculateDensityWeight_Spiky(dist, densityRadius); // Pass actual distance
        }
        
    }
    p.density = calculated_density;
    
    
    dataOut[idx] = p;
}
