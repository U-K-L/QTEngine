#include "../Helpers/ShaderHelpers.hlsl"

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 baseAlbedo;
    float2 texelSize;
    float4 outerOutlineColor;
    float4 innerOutlineColor;
    uint ID;
}

struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

struct PSOutput
{
    float4 color0 : SV_Target0; // Will map to color attachment #0
    float4 color1 : SV_Target1; // Will map to color attachment #1
    float4 color2 : SV_Target2; // Will map to color attachment #2
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

PSOutput main(PSInput input)
{
    PSOutput output;
    Images images = InitImages();
    float2 textureUVs = float2(input.uv.x, 1.0 - input.uv.y);
    float4 animeGirl = textures[images.AnimeGirl].Sample(samplers[images.AnimeGirl], textureUVs);
    float thresholdX = 0.2;
    float thresholdY = 0.6;
    float thresholdZ = 0.8;
    
    float4 vertexColor = float4(input.color, 1.0);
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
    //return animeGirl;
    output.color0 = lerp(finalColor, animeGirl, 0);
    output.color1 = outerOutlineColor;
    output.color2 = innerOutlineColor;
    return output;
}
