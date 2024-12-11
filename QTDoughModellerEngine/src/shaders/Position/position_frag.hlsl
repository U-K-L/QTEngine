#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    float3 normal : NORMAL; // Location 2,
    float3 worldPosition : TEXCOORD1; // Interpolated world position
};

Texture2D textures[] : register(t0, space0);
SamplerState samplerState : register(s0);

cbuffer LightBuffer : register(b0)
{
    float3 lightDir; // Add this if lightDir needs to be adjustable via a uniform
};

float4 main(PSInput input) : SV_TARGET
{
    
    float4 finalColor = float4(input.worldPosition.xyz, 1.0);

    return input.position.z*0.5;
}
