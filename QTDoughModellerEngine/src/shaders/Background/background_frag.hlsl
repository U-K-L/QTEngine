struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

float4 main(PSInput input) : SV_Target
{
    // Use UV coordinates to create a gradient or sample a texture
    // For a gradient background
    return float4(input.uv.x, input.uv.y, 0.0f, 1.0f);
}
