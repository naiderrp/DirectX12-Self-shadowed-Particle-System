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

groupshared float3 sunBasis[THREAD_X * THREAD_Y];

int Sign(float value);

[numthreads(THREAD_X, THREAD_Y, 1)]
void CSMain(uint3 groupID : SV_GroupID, uint3 dispatchID : SV_DispatchThreadID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
    uint index = groupID.x * THREAD_X + groupThreadID.x + (groupID.y * THREAD_Y + groupThreadID.y) * THREAD_X;

    uint particlesCount = THREAD_X * THREAD_Y;

    /*
         if (index >= particlesCount)
         {
            return;
         }
    
        cannot be used here.
        It causes thread-divergent control flow before sync (the early exit error).
    */

    if (index < particlesCount)
    {
        int sunDirX = sunDir.x;
        int sunDirY = sunDir.y;
        int sunDirZ = sunDir.z;

        float3 up;
        up.x = sunDirX ^ (Sign(sunDirX) * 1);
        up.y = sunDirY ^ (Sign(sunDirY) * 1);
        up.z = sunDirZ ^ (Sign(sunDirZ) * 1);
        
        if (length(up) == 0.0f)
        {
            if (Sign(sunDirY) == Sign(sunDirZ))
            {
                up = float3(0.0f, -1.0f, 1.0f);
            }
            else
            {
                up = float3(0.0f, 1.0f, 1.0f);
            }
        }
        
        float3 forward = cross(sunDir, up);

        float3 curPos = sbParticles[index].pos;

        float3 sunBasisPos = float3(
            dot(curPos, sunDir),
            dot(curPos, up),
            dot(curPos, forward)
        );

        sunBasis[index] = sunBasisPos;
    }
    
    GroupMemoryBarrierWithGroupSync();

    if (index >= particlesCount)
    {
        return;
    }
    
    float radius = sbParticles[index].radius;
    
    float3 sunBasisPos = sunBasis[index];
    
    for (int i = 0; i < particlesCount; ++i)
    {
        if (i == index)
        {
            continue;
        }
        float otherRadius = sbParticles[i].radius;
        
        float3 sunBasisOtherPos = sunBasis[i];
        
        float3 dirToOther = sunBasisOtherPos - sunBasisPos;
    
        if (dirToOther.x >= 0.0f && length(dirToOther.yz) <= radius + otherRadius)
        {
            sbShadows[index] *= (1.0f - sbParticles[i].opacity);
        }        
    }
}

int Sign(float value)
{
    return (value != 0) ? value / abs(value) : 1;
}
