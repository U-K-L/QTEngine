struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

Texture2D textures[] : register(t0, space0);
SamplerState samplerState : register(s0);

float4 main(PSInput i) : SV_Target
{
    return float4(1, 1, 0, 1);
}
