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
    
    //Apply gravity
    p.velocity.y += gravity * deltaTime;
    float hSq = densityRadius * densityRadius;
    //float restDensity = 10.0f; // Choose this value based on your fluid
    //float k = 10.0f; // Gas stiffness constant (tune this)
    float pressure_i = stiffnessConstant * max(p.density - restDensity, 0);
    float3 viscosityForce = 0.0f;

    for (uint j = 0; j < particleCount; j++)
    {
        if (j == idx)
            continue;
        float3 rij = dataIn[j].offset - p.offset;
        float distSq = dot(rij, rij); // Use squared distance for checks first

       
        // Check: Within radius AND distance is non-zero (use squared distance)
        if (distSq < hSq && distSq > 1e-9f) // Add epsilon check for distance squared
        {
            float dist = sqrt(distSq);
            float3 dir = rij / dist; // Safe normalization now

    // --- Calculate pressure_j, pressureTerm, gradW, pressureForce ONLY here ---
            float pressure_j = stiffnessConstant * max(dataIn[j].density - restDensity, 0);
            float safe_density_j = max(dataIn[j].density, 1e-6f);
            float pressureTerm = (pressure_i + pressure_j) / (2.0f * safe_density_j);
            float influence = CalculateDensityWeight_Spiky(dist, densityRadius);
            viscosityForce += viscosity * ((dataIn[j].velocity - p.velocity) * influence);

    // Gradient Calculation (only if within radius)
            float coeff = -45.0f / (3.14159265f * pow(densityRadius, 6)); // Use PI for better accuracy
            float gradW = coeff * pow(densityRadius - dist, 2); // Correct gradient for Spiky kernel

            float3 pressureForce = pressureTerm * gradW * dir;
            p.force += pressureForce + viscosityForce; // Accumulate force

        }
    }

    // --- Apply forces to velocity ---
    float safe_density_i = max(p.density, 1e-6f); // Use current particle's calculated density
    p.velocity += (p.force / safe_density_i) * deltaTime; // Apply acceleration a = F/rho

    float speedSq = dot(p.velocity, p.velocity);
    if (speedSq > (maxSpeed * maxSpeed))
    {
        // Normalize safely: check if speedSq is not zero
        if (speedSq > 1e-9f)
        {
            p.velocity = (p.velocity / sqrt(speedSq)) * maxSpeed; // Efficient normalization
        }
        else
        {
            p.velocity = float3(0, 0, 0); // Prevent NaN if velocity is exactly zero
        }
    }
    
    p.offset += p.velocity * deltaTime;
    
    
    if (p.offset.y < floorY)
    {
        p.offset.y = floorY;
        p.velocity.y *= -0.4;
    }
    if (p.offset.x < -boundary)
    {
        p.offset.x = -boundary;
        p.velocity.x *= -0.4;
    }
    if (p.offset.x > boundary)
    {
        p.offset.x = boundary;
        p.velocity.x *= -0.4;
    }
    if (p.offset.z < -boundary / 2.0f)
    {
        p.offset.z = -boundary / 2.0f;
        p.velocity.z *= -0.4;
    }
    if (p.offset.z > boundary / 2.0f)
    {
        p.offset.z = boundary / 2.0f;
        p.velocity.z *= -0.4;
    }
    
    
    dataOut[idx] = p;
}
