#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

struct VSInput
{
    float4 position : POSITION; // Location 0 in Vulkan
    float4 color : COLOR0; // Location 1 in Vulkan
    float4 uv : TEXCOORD0; // Location 2 in Vulkan
    float4 normal : NORMAL; // Location 3 in Vulkan
};

struct VSOutput
{
    float4 position : SV_Position; // Output to fragment shader
    float4 color : COLOR0; // Location 0 output
    float4 uv : TEXCOORD0; // Location 1 output
    float4 normal : NORMAL; // Location 2, flat interpolation
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
    output.position = mul(proj, mul(view, mul(identityMatrix, float4(input.position.xyz, 1.0))));

    // Pass through color, texture coordinate, and flat normal
    output.color = input.color;
    output.uv = input.uv;
    output.normal = input.normal;

    return output;
}