#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

// Define an unbounded array of textures and samplers
// Using register space0 to match Vulkan descriptor set 0
Texture2D textures[] : register(t0, space0); //Global
SamplerState samplers[] : register(s0, space0); //Global

// Define a structured buffer for unsigned int array
// Binding slot t1, space0 to match Vulkan descriptor set 1, binding 1
StructuredBuffer<uint> intArray : register(t1, space1);

struct Images
{
    uint AnimeGirl;
};

Images InitImages()
{
    Images image;
    image.AnimeGirl = intArray[0];
    
    return image;
}
cbuffer LightBuffer : register(b0)
{
    float3 lightDir; // Add this if lightDir needs to be adjustable via a uniform
};

float4 main(PSInput input) : SV_TARGET
{
    Images images = InitImages();
    float2 textureUVs = float2(input.uv.x, 1.0 - input.uv.y);
    float4 animeGirl = textures[images.AnimeGirl].Sample(samplers[images.AnimeGirl], textureUVs);
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
    
    finalColor.a = 1.0;

    return lerp(finalColor, animeGirl, animeGirl.w);
}
