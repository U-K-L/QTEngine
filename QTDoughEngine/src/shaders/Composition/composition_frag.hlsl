#include "../Helpers/ShaderHelpers.hlsl"

// HLSL Shader Code

// Define an unbounded array of textures and samplers
// Using register space0 to match Vulkan descriptor set 0
Texture2D textures[] : register(t0, space0); //Global
SamplerState samplers[] : register(s0, space0); //Global

// Define a structured buffer for unsigned int array
// Binding slot t1, space0 to match Vulkan descriptor set 1, binding 1
StructuredBuffer<uint> intArray : register(t1, space1);

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 texelSize;
    float isOrtho;
}

struct Images
{
    uint BackgroundImage;
    uint AlbedoImage;
    uint NormalImage;
    uint PositionImage;
    uint DepthImage;
    uint OutlineImage;
    uint SDFAlbedoImage;
    uint SDFNormalImage;
    uint SDFPositionPass;
    uint CombineSDFRasterPass;
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
    image.SDFAlbedoImage = intArray[6];
    image.SDFNormalImage = intArray[7];
    image.SDFPositionPass = intArray[8];
    image.CombineSDFRasterPass = intArray[9];
    
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

float3 countColors(float count)
{
    float red = count / 256;
    float blue = count / 64;
    float green = count / 16;
    
    //Green if less than 16 steps
    if (count < 32)
    {
        red = 0;
        blue = 0;
    }
    
    //Blue
    if (count > 32 && count < 64)
    {
        red = 0;
        green = 0;
    }
    
    //Teal
    if (count < 128 && count > 64)
    {
        red = 0;
        blue = count / 128;
        green = count / 96;
    }
    
    //Red
    if (count > 128 && count < 256)
    {
        blue = 0;
        green = 0;
    }
    
    //White
    if(count > 256)
    {
        blue = count / 512;
        green = count / 328;

    }
    
    return float3(red, green, blue);
}

// Main pixel shader function
float4 main(VSOutput i) : SV_Target
{
    Images images = InitImages();
    float2 textureUVs = float2(i.uv.x, 1.0 - i.uv.y);// * 0.25f;
    float4 backgroundImage = textures[images.BackgroundImage].Sample(samplers[images.BackgroundImage], textureUVs);
    float4 albedoImage = textures[images.AlbedoImage].Sample(samplers[images.AlbedoImage], textureUVs);
    float4 normalImage = textures[images.NormalImage].Sample(samplers[images.NormalImage], textureUVs);
    float4 positionImage = textures[images.PositionImage].Sample(samplers[images.PositionImage], textureUVs);
    float4 depthImage = textures[images.DepthImage].Sample(samplers[images.DepthImage], textureUVs);
    float4 outlineImage = textures[images.OutlineImage].Sample(samplers[images.OutlineImage], textureUVs);
    float4 sdfImage = textures[images.SDFAlbedoImage].Sample(samplers[images.SDFAlbedoImage], textureUVs);
    float4 sdfNormalImage = textures[images.SDFNormalImage].Sample(samplers[images.SDFNormalImage], textureUVs);
    float4 sdfPositionImage = textures[images.SDFPositionPass].Sample(samplers[images.SDFPositionPass], textureUVs);
    float4 combineSDFRasterImage = textures[images.CombineSDFRasterPass].Sample(samplers[images.CombineSDFRasterPass], textureUVs);
    
    return combineSDFRasterImage;
    //Compose the normals together, will be done in a different pass in the future.
    return lerp(sdfNormalImage, normalImage, 0.5);
        
    //float4 heatMap = float4(countColors(sdfNormalImage.w), 1);
    //return heatMap;
    //return sdfNormalImage;
    //return normalImage;
    //return outlineImage;
    //return sdfImage;

    float4 color = lerp(backgroundImage, sdfImage, sdfImage.w);

    //return color;
    /*
    float depth = depthImage.r;
    float linearDepth = LinearizeDepth(depth);
    // near and far plane CHANGE TO UNIFORMS
    float near_plane = 0.1;
    float far_plane = 100.0;
    // Visualize linear depth
    float4 outColor = linearDepth / far_plane;
    outColor = float4(outColor.xyz, 1.0);
*/
    
    //return sdfImage;
    //float4 finalImage = lerp(float4(GammaEncode(color.xyz, 0.32875), color.w), outlineImage, outlineImage.w);
    float4 finalImage = lerp(float4(GammaEncode(color.xyz, 0.32875), color.w), outlineImage, outlineImage.w);
    //Debug voxels.
    //return lerp(finalImage, sdfImage, 0.9);
    //return albedoImage;
    //return outlineImage;
    //return float4(rand(positionImage.a * 100), rand(positionImage.a * 100 + 1), rand(positionImage.a * 100 + 2), 1.0);
    //return outColor;
    //return float4(GammaEncode(albedoImage.xyz, 0.32875), 1);
    //float sins = sin(time);
    return finalImage;

}
