#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float4 color : COLOR0; // Location 0 output
    float4 uv : TEXCOORD0; // Location 1 output
    float4 normal : NORMAL; // Location 2,
};

Texture2D textures[] : register(t0, space0);
SamplerState samplerState : register(s0);

cbuffer LightBuffer : register(b0)
{
    float3 lightDir; // Add this if lightDir needs to be adjustable via a uniform
};

float4 main(PSInput input) : SV_TARGET
{
    //Create normal as float4 and set to final color.
    //multiply normal by model matrix to get world normal.
    float3 worldNormal = mul((float3x3) transpose(inverse(model)), input.normal);
    worldNormal = normalize(worldNormal);
    
    //float4 randomIdColored = rand4(float4(input.normal.w, input.normal.w * 3, input.normal.w + 722, 1.0f));


    float4 finalColor = float4(worldNormal, 1.0);

    return finalColor;
}
