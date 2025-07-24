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

    // Pass position to output
    output.position = float4(input.position, 1);

    // Pass UV coordinates to output
    output.uv = input.uv;

    return output;
}
