#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

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
    float3 normal : NORMAL; // Location 2, flat interpolation
    float3 worldPosition : TEXCOORD1; // world position
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

    float4 worldPos = mul(model, float4(input.position, 1.0));
    output.worldPosition = worldPos.xyz;
    // Compute position in clip space
    output.position = mul(proj, mul(view, mul(model, float4(input.position, 1.0))));

    // Pass through color, texture coordinate, and flat normal
    output.color = input.color;
    output.uv = input.uv;
    output.normal = input.normal;

    return output;
}