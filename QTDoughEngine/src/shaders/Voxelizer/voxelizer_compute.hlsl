RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);

#include "../Helpers/ShaderHelpers.hlsl"

#define VOXEL_RESOLUTION 64
#define SCENE_BOUNDS 10.0f

struct Voxel
{
    float4 positionDistance;
    float4 normalDensity;
    float4 occuipiedInfo;
};

struct ComputeVertex
{
    float4 position;
    float4 texCoord;
    float4 normal;
};

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float2 texelSize;
}

StructuredBuffer<Voxel> voxelsIn : register(t2, space1); // readonly
RWStructuredBuffer<Voxel> voxelsOut : register(u3, space1); // write


StructuredBuffer<ComputeVertex> vertexBuffer : register(t4, space1);
StructuredBuffer<uint3> indexBuffer : register(t5, space1);

float DistanceToTriangle(float3 p, float3 a, float3 b, float3 c)
{
    float3 ba = b - a, pa = p - a;
    float3 cb = c - b, pb = p - b;
    float3 ac = a - c, pc = p - c;
    float3 nor = cross(ba, ac);

    float signTri =
        sign(dot(cross(ba, nor), pa)) +
        sign(dot(cross(cb, nor), pb)) +
        sign(dot(cross(ac, nor), pc));

    float d = min(min(
        length(ba * clamp(dot(ba, pa) / dot(ba, ba), 0.0, 1.0) - pa),
        length(cb * clamp(dot(cb, pb) / dot(cb, cb), 0.0, 1.0) - pb)),
        length(ac * clamp(dot(ac, pc) / dot(ac, ac), 0.0, 1.0) - pc));

    float distToPlane = abs(dot(nor, pa)) / length(nor);
    return (signTri >= 2.0) ? distToPlane : d;
}



[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint3 gridSize = uint3(VOXEL_RESOLUTION, VOXEL_RESOLUTION, VOXEL_RESOLUTION);
    if (any(DTid >= gridSize))
        return;

    uint voxelIndex = DTid.x * gridSize.y * gridSize.z + DTid.y * gridSize.z + DTid.z;
    float3 center = voxelsIn[voxelIndex].positionDistance.xyz;

    float minDist = 100.0f;
    for (uint i = 0; i < 4440; ++i)
    {
        uint3 idx = indexBuffer[i];
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;

        float d = DistanceToTriangle(center, a, b, c);
        minDist = min(minDist, d);
    }

    voxelsOut[voxelIndex].positionDistance.w = minDist;
    //voxelsOut[voxelIndex].positionDistance = float4(1, 0, 0, 1);
}
