// HLSL Shader Code

// Define an unbounded array of textures and samplers
// Using register space0 to match Vulkan descriptor set 0
Texture2D textures[] : register(t0, space0);
SamplerState samplers[] : register(s0, space0);

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
    float2 textureUVs = float2(i.uv.x, 1.0 - i.uv.y);

    // Sample the texture using the bindless array
    float4 color = textures[0].Sample(samplers[0], textureUVs);

    return color;
}
