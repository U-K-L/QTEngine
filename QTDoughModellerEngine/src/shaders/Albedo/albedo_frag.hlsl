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
    // Normalize light direction
    float3 normLightDir = normalize(float3(0.5, 0.5, 0.0));

    // Dot product for lighting intensity, adjusted for [-1, 1] to [0, 1] range
    float nDotL = dot(input.normal, normLightDir) * 0.5 + 0.5;

    // Output color based on the lighting intensity
    float4 outColor = float4(nDotL, nDotL, nDotL, 1.0);

    // Uncomment to use texture sampling
    // outColor = texSampler.Sample(samplerState, input.fragTexCoord);

    // Uncomment to visualize normals in world space
    // float3 normalsFrag = input.fragNormal * 0.5 + 0.5;
    // outColor = float4(normalsFrag, 1.0);

    return baseAlbedo;
}
