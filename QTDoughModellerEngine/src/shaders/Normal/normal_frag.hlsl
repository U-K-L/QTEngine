cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 baseAlbedo;
}

struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

Texture2D textures[] : register(t0, space0);
SamplerState samplerState : register(s0);

cbuffer LightBuffer : register(b0)
{
    float3 lightDir; // Add this if lightDir needs to be adjustable via a uniform
};

float4 main(PSInput input) : SV_TARGET
{
    //Create normal as float4 and set to final color.
    //multiply normal by model matrix to get world normal.
    float4 finalColor = float4(mul(float4(input.normal, 1.0), model).xyz, 1.0);

    return finalColor;
}
