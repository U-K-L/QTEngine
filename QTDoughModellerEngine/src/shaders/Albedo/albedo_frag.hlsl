#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

Texture2D textures[] : register(t0, space0);
SamplerState samplerState : register(s0);

cbuffer LightBuffer : register(b0)
{
    float3 lightDir; // Add this if lightDir needs to be adjustable via a uniform
};

float4 main(PSInput input) : SV_TARGET
{
    float thresholdX = 0.2;
    float thresholdY = 0.6;
    float thresholdZ = 0.8;
    // Normalize light direction
    float3 normLightDir = normalize(float3(0.5, 0.5, 0.0));

    // Dot product for lighting intensity, adjusted for [-1, 1] to [0, 1] range
    float NdotL = dot(input.normal, normLightDir) * 0.5 + 0.5;
    
    
    float4 midTones = baseAlbedo * step(thresholdX, NdotL);
    float4 shadows = (baseAlbedo * 0.5) * step(NdotL, thresholdY);
    float4 highlights = (baseAlbedo* 2.0) * step(thresholdZ, NdotL);

    float4 finalColor = max(midTones, shadows);
    finalColor = max(finalColor, highlights);
    
    finalColor = float4(finalColor.rgb, 1.0);

    return finalColor;
}
