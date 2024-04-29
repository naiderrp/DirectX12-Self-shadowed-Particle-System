struct VS_INPUT
{
    float4 color    : COLOR;
    uint id         : SV_VERTEXID;
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float shadow : SHADOW;
    float radius : RADIUS;
};

cbuffer ConstantBuffer : register(b0)
{
    row_major float4x4 wvpMat;
    row_major float4x4 invViewMat;
};

struct Particle
{
    float3 pos;
    float radius;
    float opacity;
};

StructuredBuffer<Particle> sbParticles : register(t0);
RWBuffer<float> sbShadows : register(u0);

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    
    float4 pos = float4(sbParticles[input.id].pos, 1.0f);
    
    output.pos = pos;
    output.color = input.color;
    output.radius = sbParticles[input.id].radius;
    
    float4 color = input.color;
    
    if (input.id == 1024) // light source
    {
        color = float4(1.0, 1.0, 1.0, 1.0);
    }
    
    output.color = color;
    output.shadow = sbShadows[input.id];
    
    return output;
}