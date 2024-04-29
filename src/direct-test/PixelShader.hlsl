struct GS_OUTPUT
{
    float2 tex      : TEXCOORD0;
    float4 pos      : SV_POSITION;
    float4 color    : COLOR;
    float shadow    : SHADOW;
    float radius    : RADIUS;
};

float4 PSMain(GS_OUTPUT input) : SV_TARGET
{
    float2 center = float2(0.5, 0.5);
    float distance = length(input.tex.xy - center);
    
    float radius = 0.5f;
    
    if (distance > radius)
    {
        discard;
    }

    float shadow = input.shadow;
    
    float4 diffuseColor = input.color;
    
    float4 resultColor = diffuseColor * shadow;
    
    return resultColor;
}