struct Particle
{
    float3  pos;
    float   radius;
    float   opacity;
};

StructuredBuffer<Particle> sbParticles : register(t0);
RWBuffer<float> sbShadows : register(u0);

cbuffer cbCS : register(b0)
{
    float3  sunDir;
}

bool NeverIntersects(float3 particlePos, float3 otherParticlePos, float3 lightPos);

[numthreads(THREAD_X, THREAD_Y, 1)]
void CSMain(uint3 groupID : SV_GroupID, uint3 dispatchID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    uint index = groupID.x * THREAD_X + groupThreadID.x + (groupID.y * THREAD_Y + groupThreadID.y) * THREAD_X;
        
    uint particlesCount = THREAD_X * THREAD_Y;
    
    if (index > particlesCount)
    {
        return;
    }
        
    float3 lightPos = sunDir;
    Particle particle = sbParticles[index];
    
    sbShadows[index] = particle.opacity;
    
    float3 direction = normalize(lightPos - particle.pos); // direction from a particle to sun
    float3 origin = particle.pos;
        
    for (int i = 0; i < particlesCount; ++i)
    {
        if (i == index)
        {
            continue;
        }
        
        Particle otherParticle = sbParticles[i];
        
        if (NeverIntersects(particle.pos, otherParticle.pos, lightPos))
        {
            continue;
        }

        float x0 = otherParticle.pos.x;
        float y0 = otherParticle.pos.y;
        float z0 = otherParticle.pos.z;
        float radius = otherParticle.radius;
    
        float a = pow(direction.x, 2) + pow(direction.y, 2) + pow(direction.z, 2);
        float b = 2 * (direction.x * (origin.x - x0) + direction.y * (origin.y - y0) + direction.z * (origin.z - z0));
        float c = pow(origin.x - x0, 2) + pow(origin.y - y0, 2) + pow(origin.z - z0, 2) - pow(radius, 2);
    
        float D = b * b - 4 * a * c;
    
        if (D < 0.0)
        {
            continue;
        }
        sbShadows[index] *= 0.9;
    }
}

bool NeverIntersects(float3 particlePos, float3 otherParticlePos, float3 lightPos)
{
    if (lightPos.x >= particlePos.x && lightPos.y <= particlePos.y && lightPos.z >= particlePos.z)
    {
        if (otherParticlePos.x < particlePos.x || otherParticlePos.y > particlePos.y || otherParticlePos.z < particlePos.z)
        {
            return true;
        }
    }
    if (lightPos.x <= particlePos.x && lightPos.y <= particlePos.y && lightPos.z >= particlePos.z)
    {
        if (otherParticlePos.x > particlePos.x || otherParticlePos.y > particlePos.y || otherParticlePos.z < particlePos.z)
        {
            return true;
        }
    }
    if (lightPos.x <= particlePos.x && lightPos.y >= particlePos.y && lightPos.z <= particlePos.z)
    {
        if (otherParticlePos.x > particlePos.x || otherParticlePos.y < particlePos.y || otherParticlePos.z > particlePos.z)
        {
            return true;
        }
    }
    if (lightPos.x <= particlePos.x && lightPos.y <= particlePos.y && lightPos.z <= particlePos.z)
    {
        if (otherParticlePos.x > particlePos.x || otherParticlePos.y > particlePos.y || otherParticlePos.z > particlePos.z)
        {
            return true;
        }
    }
    if (lightPos.x >= particlePos.x && lightPos.y >= particlePos.y && lightPos.z >= particlePos.z)
    {
        if (otherParticlePos.x < particlePos.x || otherParticlePos.y < particlePos.y || otherParticlePos.z < particlePos.z)
        {
            return true;
        }
    }
    if (lightPos.x <= particlePos.x && lightPos.y >= particlePos.y && lightPos.z >= particlePos.z)
    {
        if (otherParticlePos.x > particlePos.x || otherParticlePos.y < particlePos.y || otherParticlePos.z < particlePos.z)
        {
            return true;
        }
    }
    if (lightPos.x >= particlePos.x && lightPos.y >= particlePos.y && lightPos.z <= particlePos.z)
    {
        if (otherParticlePos.x < particlePos.x || otherParticlePos.y < particlePos.y || otherParticlePos.z > particlePos.z)
        {
            return true;
        }
    }
    if (lightPos.x >= particlePos.x && lightPos.y <= particlePos.y && lightPos.z <= particlePos.z)
    {
        if (otherParticlePos.x < particlePos.x || otherParticlePos.y > particlePos.y || otherParticlePos.z > particlePos.z)
        {
            return true;
        }
    }
    return false;
}
