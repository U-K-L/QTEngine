cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 baseAlbedo;
    float4 sideAlbedo;
    float4 topAlbedo;
    float2 texelSize;
    float4 outerOutlineColor;
    float4 innerOutlineColor;
    uint ID;
};

struct GPULight
{
    float4 emission;
    float3 direction;
};