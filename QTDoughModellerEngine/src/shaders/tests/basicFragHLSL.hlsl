// HLSL fragment shader equivalent

Texture2D texSampler : register(t1); // Binding for texture sampler

struct PSInput {
    float3 fragColor : COLOR0;          // Location 0 in Vulkan
    float2 fragTexCoord : TEXCOORD0;    // Location 1 in Vulkan
    nointerpolation float3 fragNormal : NORMAL; // Location 2, flat interpolation
};

cbuffer LightBuffer : register(b0) {
    float3 lightDir; // Add this if lightDir needs to be adjustable via a uniform
};

float4 main(PSInput input) : SV_TARGET {
    // Normalize light direction
    float3 normLightDir = normalize(float3(0.5, 0.5, 0.0));

    // Dot product for lighting intensity, adjusted for [-1, 1] to [0, 1] range
    float nDotL = dot(input.fragNormal, normLightDir) * 0.5 + 0.5;

    // Output color based on the lighting intensity
    float4 outColor = float4(nDotL, nDotL, nDotL, 1.0);

    // Uncomment to use texture sampling
    // outColor = texSampler.Sample(samplerState, input.fragTexCoord);

    // Uncomment to visualize normals in world space
    // float3 normalsFrag = input.fragNormal * 0.5 + 0.5;
    // outColor = float4(normalsFrag, 1.0);

    return outColor;
}
