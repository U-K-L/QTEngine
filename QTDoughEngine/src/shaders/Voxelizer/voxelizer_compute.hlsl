RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);

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

float3 BarycentricNormals(float3 center, uint3 index)
{
        
    float a = vertexBuffer[index.x].texCoord.w;
    float b = vertexBuffer[index.y].texCoord.w;
    float c = vertexBuffer[index.z].texCoord.w;
    
    float3 na = vertexBuffer[index.x].normal.xyz;
    float3 nb = vertexBuffer[index.y].normal.xyz;
    float3 nc = vertexBuffer[index.z].normal.xyz;
    
    float3 pa = vertexBuffer[index.x].position.xyz;
    float3 pb = vertexBuffer[index.y].position.xyz;
    float3 pc = vertexBuffer[index.z].position.xyz;

    float3 bary = Barycentric(pa, pb, pc, center);

    float3 normal = normalize(na * bary.x + nb * bary.y + nc * bary.z);
    
    return normal;
}

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float2 voxelSceneBounds = GetVoxelResolution(1.0f);
    uint3 gridSize = uint3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
    
    float2 voxelSceneBoundsL2 = GetVoxelResolution(2.0f);
    uint3 gridSizeL2 = uint3(voxelSceneBoundsL2.x, voxelSceneBoundsL2.x, voxelSceneBoundsL2.x);
    
    uint3 DTidL2 = floor(DTid / 2);
    
    float2 voxelSceneBoundsL3 = GetVoxelResolution(3.0f);
    uint3 gridSizeL3 = uint3(voxelSceneBoundsL3.x, voxelSceneBoundsL3.x, voxelSceneBoundsL3.x);
    
    uint3 DTidL3 = floor(DTid / 4);
    
    if (any(DTid >= gridSize))
        return;

    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert grid index to world-space center position
    float3 center = ((float3) DTid + 0.5f) * voxelSize - halfScene;

    
    float voxelSizeL2 = voxelSceneBoundsL2.y / voxelSceneBoundsL2.x;
    float halfSceneL2 = voxelSceneBoundsL2.y * 0.5f;

    // Convert grid index to world-space center position
    float3 centerL2 = ((float3) DTidL2 + 0.5f) * voxelSizeL2 - halfSceneL2;
    
    float voxelSizeL3 = voxelSceneBoundsL3.y / voxelSceneBoundsL3.x;
    float halfSceneL3 = voxelSceneBoundsL3.y * 0.5f;

    // Convert grid index to world-space center position
    float3 centerL3 = ((float3) DTidL3 + 0.5f) * voxelSizeL3 - halfSceneL3;

    float minDist = 100.0f;
    float minDistL2 = 100.0f;
    float minDistL3 = 100.0f;
    uint3 cachedIdx = 0;
    uint3 cachedIdxL2 = 0;
    uint3 cachedIdxL3 = 0;
    for (uint i = 0; i < 2580; i += 3)
    {
        uint3 idx = uint3(i, i + 1, i + 2);
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;

        float d = DistanceToTriangle(center, a, b, c);
        float dL2 = DistanceToTriangle(centerL2, a, b, c);
        float dL3 = DistanceToTriangle(centerL3, a, b, c);
        
        if (d < minDist)
            cachedIdx = idx;
        if(dL2 < minDistL2)
            cachedIdxL2 = idx;
        if (dL3 < minDistL3)
            cachedIdxL3 = idx;
        
        minDist = min(minDist, d);
        minDistL2 = min(minDistL2, dL2);
        minDistL3 = min(minDistL3, dL3);

    }

    //voxelsOut[voxelIndex].positionDistance.w = minDist;

    
    float3 normal = BarycentricNormals(center, cachedIdx);
    float3 normalL2 = BarycentricNormals(centerL2, cachedIdxL2);
    float3 normalL3 = BarycentricNormals(centerL3, cachedIdxL3);

    
    //interpolate between vertices. Do this for another mip level.
    /*
    float3 interpolant = 0.5f;
    float density = lerp(a, b, 0.5);
    density = lerp(density, c, 0.5);
    */

    uint voxelIndexL1 = DTid.x * gridSize.y * gridSize.z + DTid.y * gridSize.z + DTid.z;
    uint voxelIndexL2 = DTidL2.x * gridSizeL2.y * gridSizeL2.z + DTidL2.y * gridSizeL2.z + DTidL2.z;
    uint voxelIndexL3 = DTidL3.x * gridSizeL3.y * gridSizeL3.z + DTidL3.y * gridSizeL3.z + DTidL3.z;
    
    voxelsL1Out[voxelIndexL1].normalDistance = float4(normal, minDist);
    voxelsL2Out[voxelIndexL2].normalDistance = float4(normalL2, minDistL2);
    voxelsL3Out[voxelIndexL3].normalDistance = float4(normalL3, minDistL3);

}
