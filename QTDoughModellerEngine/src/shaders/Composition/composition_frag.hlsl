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
};

Images InitImages()
{
    Images image;
    
    image.BackgroundImage = intArray[0];
    image.AlbedoImage = intArray[1];
    
    return image;
}

// Define any constants or uniforms you need
cbuffer Constants : register(b0, space0)
{
    uint offscreenImageIndex;
    // Add any other constants you might need
}

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
    float4 backgroundImage = textures[images.BackgroundImage].Sample(samplers[0], textureUVs);
    float4 albedoImage = textures[images.AlbedoImage].Sample(samplers[0], textureUVs);
    
    float4 color = albedoImage;

    return color;
}
