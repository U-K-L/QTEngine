cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 midtone;
    float4 shadow;
    float4 highlight;
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

struct GameObjectShaderData
{
    float4x4 transform;
    float4 Midtone;
    float4 Highlight;
    float4 Shadow;

};