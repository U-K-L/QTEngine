﻿RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);

#include "../Helpers/ShaderHelpers.hlsl"


cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float2 texelSize;
}

StructuredBuffer<Voxel> voxelsL1In : register(t2, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL1Out : register(u3, space1); // write

StructuredBuffer<Voxel> voxelsL2In : register(t4, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL2Out : register(u5, space1); // write

StructuredBuffer<Voxel> voxelsL3In : register(t6, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL3Out : register(u7, space1); // write


StructuredBuffer<ComputeVertex> vertexBuffer : register(t8, space1);

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

float SignedDistanceToTriangle(float3 p, float3 a, float3 b, float3 c)
{
    float3 normal = normalize(cross(b - a, c - a));
    float unsignedDist = DistanceToTriangle(p, a, b, c);
    float signTri = sign(dot(p - a, normal));
    return signTri * unsignedDist;
}

float smin(float a, float b, float k)
{
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return lerp(b, a, h) - k * h * (1.0 - h);
}


[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float sampleLevel = GetSampleLevel(0, 0);
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    uint3 gridSize = uint3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
    if (any(DTid >= gridSize))
        return;

    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.y;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert grid index to world-space center position
    float3 center = ((float3) DTid + 0.5f) * voxelSize - halfScene;


    float minDist = 100.0f;
    uint3 cachedIdx = 0;
    for (uint i = 0; i < 36; i += 3)
    {
        uint3 idx = uint3(i, i + 1, i + 2);
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;

        float d = DistanceToTriangle(center, a, b, c);
        if (d < minDist)
            cachedIdx = idx;
        minDist = min(minDist, d);
        

    }

    //voxelsOut[voxelIndex].positionDistance.w = minDist;
    
    float a = vertexBuffer[cachedIdx.x].texCoord.w;
    float b = vertexBuffer[cachedIdx.y].texCoord.w;
    float c = vertexBuffer[cachedIdx.z].texCoord.w;
    
    float3 na = vertexBuffer[cachedIdx.x].normal.xyz;
    float3 nb = vertexBuffer[cachedIdx.y].normal.xyz;
    float3 nc = vertexBuffer[cachedIdx.z].normal.xyz;
    
    float3 pa = vertexBuffer[cachedIdx.x].position.xyz;
    float3 pb = vertexBuffer[cachedIdx.y].position.xyz;
    float3 pc = vertexBuffer[cachedIdx.z].position.xyz;

    float3 bary = Barycentric(pa, pb, pc, center);

    float3 normal = normalize(na * bary.x + nb * bary.y + nc * bary.z);
    
    //interpolate between vertices. Do this for another mip level.
    /*
    float3 interpolant = 0.5f;
    float density = lerp(a, b, 0.5);
    density = lerp(density, c, 0.5);
    */
    uint voxelIndexL1 = DTid.x * gridSize.y * gridSize.z + DTid.y * gridSize.z + DTid.z;
    uint voxelIndexL2 = floor(voxelIndexL1 / 2);
    uint voxelIndexL3 = floor(voxelIndexL2 / 2);
    
    voxelsL1Out[voxelIndexL1].normalDistance = float4(normal, minDist);
    voxelsL2Out[voxelIndexL2].normalDistance = float4(normal, minDist);
    voxelsL3Out[voxelIndexL3].normalDistance = float4(normal, minDist);
    
    //voxelsOut[voxelIndex].positionDistance = float4(1, 0, 0, 1);
}
