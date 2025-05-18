#include "../Helpers/ShaderHelpers.hlsl"

// HLSL Shader Code

// Define an unbounded array of textures and samplers
// Using register space0 to match Vulkan descriptor set 0
Texture2D textures[] : register(t0, space0); //Global
SamplerState samplers[] : register(s0, space0); //Global

// Define a structured buffer for unsigned int array
// Binding slot t1, space0 to match Vulkan descriptor set 1, binding 1
StructuredBuffer<uint> intArray : register(t1, space1);

struct Images
{
    uint BackgroundImage;
    uint AlbedoImage;
    uint NormalImage;
    uint PositionImage;
    uint DepthImage;
    uint OutlineImage;
    uint SDFImage;
};

Images InitImages()
{
    Images image;
    
    image.BackgroundImage = intArray[0];
    image.AlbedoImage = intArray[1];
    image.NormalImage = intArray[2];
    image.PositionImage = intArray[3];
    image.DepthImage = intArray[4];
    image.OutlineImage = intArray[5]; 
    image.SDFImage = intArray[6];
    
    return image;
}

cbuffer Constants : register(b2, space0)
{
    float deltaTime; // offset 0
    float time; // offset 4
    float2 pad; // offset 8..15, total 16 bytes
};

struct VSOutput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};
// Main pixel shader function
float4 main(VSOutput i) : SV_Target
{
    Images images = InitImages();
    float2 textureUVs = float2(i.uv.x, 1.0 - i.uv.y);
    float4 backgroundImage = textures[images.BackgroundImage].Sample(samplers[images.BackgroundImage], textureUVs);
    float4 albedoImage = textures[images.AlbedoImage].Sample(samplers[images.AlbedoImage], textureUVs);
    float4 normalImage = textures[images.NormalImage].Sample(samplers[images.NormalImage], textureUVs);
    float4 positionImage = textures[images.PositionImage].Sample(samplers[images.PositionImage], textureUVs);
    float4 depthImage = textures[images.DepthImage].Sample(samplers[images.DepthImage], textureUVs);
    float4 outlineImage = textures[images.OutlineImage].Sample(samplers[images.OutlineImage], textureUVs);
    float4 sdfImage = textures[images.SDFImage].Sample(samplers[images.SDFImage], textureUVs);
    
    //return sdfImage;

    float4 color = lerp(backgroundImage, albedoImage, albedoImage.w);

    float depth = depthImage.r;
    float linearDepth = LinearizeDepth(depth);
    // near and far plane CHANGE TO UNIFORMS
    float near_plane = 0.1;
    float far_plane = 100.0;
    // Visualize linear depth
    float4 outColor = linearDepth / far_plane;
    outColor = float4(outColor.xyz, 1.0);
    
    return sdfImage;
    float4 finalImage = lerp(float4(GammaEncode(color.xyz, 0.32875), color.w), outlineImage, outlineImage.w);
    //Debug voxels.
    return lerp(finalImage, sdfImage, saturate(length(sdfImage.xyz)) * 0.05 + 0.95);
    //return albedoImage;
    //return outlineImage;
    //return float4(rand(positionImage.a * 100), rand(positionImage.a * 100 + 1), rand(positionImage.a * 100 + 2), 1.0);
    //return outColor;
    //return float4(GammaEncode(albedoImage.xyz, 0.32875), 1);
    //float sins = sin(time);
    return finalImage;

}
