struct VS_OUTPUT
{
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float shadow : SHADOW;
    float radius : RADIUS;
    float opacity : OPACITY;
};

struct GS_OUTPUT
{
    float2 tex : TEXCOORD0;
    float4 pos : SV_POSITION;
    float4 color : COLOR;
    float shadow : SHADOW;
    float radius : RADIUS;
    float opacity : OPACITY;
};

cbuffer ConstantBuffer : register(b0)
{
    row_major float4x4 wvpMat;
    row_major float4x4 invViewMat;
};

//
// GS for rendering point sprite particles.  Takes a point and turns 
// it into 2 triangles.

cbuffer cbImmutable
{
    static float3 g_positions[4] =
    {
        float3(-1, 1, 0),
        float3(1, 1, 0),
        float3(-1, -1, 0),
        float3(1, -1, 0),
    };
    
    static float2 g_texcoords[4] =
    {
        float2(0, 0),
        float2(1, 0),
        float2(0, 1),
        float2(1, 1),
    };
};

[maxvertexcount(4)]
void GSMain(point VS_OUTPUT input[1], inout TriangleStream<GS_OUTPUT> SpriteStream)
{
    GS_OUTPUT output;
    float radius = input[0].radius;
    // Emit two new triangles.
    for (int i = 0; i < 4; i++)
    {
        float3 position = g_positions[i] * radius;
        position = mul(position, (float3x3) invViewMat) + input[0].pos;
        
        output.pos = mul(float4(position, 1.0), wvpMat);
        output.tex = g_texcoords[i];
        output.color = input[0].color;
        output.shadow = input[0].shadow;
        output.radius = input[0].radius;
        output.opacity = input[0].opacity;
        
        SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}
