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

struct VSInput
{
    float3 position : POSITION; // Location 0 in Vulkan
    float3 color : COLOR0; // Location 1 in Vulkan
    float2 uv : TEXCOORD0; // Location 2 in Vulkan
    float3 normal : NORMAL; // Location 3 in Vulkan
};

struct VSOutput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

VSOutput main(VSInput input)
{
    VSOutput output;

    // Identity matrix (not needed in HLSL, but shown here for reference)
    float4x4 identityMatrix = float4x4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    );

    // Compute position in clip space
    output.position = mul(proj, mul(view, mul(identityMatrix, float4(input.position, 1.0))));

    // Pass through color, texture coordinate, and flat normal
    output.color = input.color;
    output.uv = input.uv;
    output.normal = input.normal;

    return output;
}