// HLSL vertex shader equivalent

cbuffer UniformBufferObject : register(b0)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
}

struct VSInput
{
    float3 inPosition : POSITION; // Location 0 in Vulkan
    float3 inColor : COLOR0; // Location 1 in Vulkan
    float2 inTexCoord : TEXCOORD0; // Location 2 in Vulkan
    float3 inNormals : NORMAL; // Location 3 in Vulkan
};

struct VSOutput
{
    float4 position : SV_POSITION; // Output to fragment shader
    float3 fragColor : COLOR0; // Location 0 output
    float2 fragTexCoord : TEXCOORD0; // Location 1 output
    nointerpolation float3 fragNormal : NORMAL; // Location 2, flat interpolation
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
    output.position = mul(proj, mul(view, mul(model, float4(input.inPosition, 1.0))));

    // Pass through color, texture coordinate, and flat normal
    output.fragColor = input.inColor;
    output.fragTexCoord = input.inTexCoord;
    output.fragNormal = input.inNormals;

    return output;
}
