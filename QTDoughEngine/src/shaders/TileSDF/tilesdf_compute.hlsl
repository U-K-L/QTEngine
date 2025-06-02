RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);

#include "../Helpers/ShaderHelpers.hlsl"


cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float2 texelSize;
}

struct PushConsts
{
    float lod;
    uint triangleCount;
};

[[vk::push_constant]]
PushConsts pc;


StructuredBuffer<Voxel> voxelsL1In : register(t2, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL1Out : register(u3, space1); // write

StructuredBuffer<Voxel> voxelsL2In : register(t4, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL2Out : register(u5, space1); // write

StructuredBuffer<Voxel> voxelsL3In : register(t6, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL3Out : register(u7, space1); // write


StructuredBuffer<ComputeVertex> vertexBuffer : register(t8, space1);
StructuredBuffer<Brush> Brushes : register(t9, space1);

RWTexture3D<snorm float> gBindless3DStorage[] : register(u5, space0);

// Unfiltered write to RWTexture3D
void Write3D(uint textureIndex, int3 coord, float value)
{
    gBindless3DStorage[textureIndex][coord] = value;
}


void ComputePerTriangle(uint3 DTid : SV_DispatchThreadID)
{
    //int3 volumeIndex = int3(DTid);
    
    //Write3D(0, volumeIndex, 1.0f);
    uint triangleCount = pc.triangleCount;
    uint triangleIndex = DTid.x;
    if (triangleIndex >= triangleCount)
        return;
    
    uint baseIndex = triangleIndex * 3;
    
    uint3 idx = uint3(baseIndex, baseIndex + 1, baseIndex + 2);

    float3 a = vertexBuffer[idx.x].position.xyz;
    float3 b = vertexBuffer[idx.y].position.xyz;
    float3 c = vertexBuffer[idx.z].position.xyz;
    
    float2 voxelSceneBounds = GetVoxelResolution(1.0f);
    
    float voxelSize = 0.03125f;
    float3 gridOrigin = voxelSceneBounds.y * 0.5;
    
    //Max and min bounding point, the two corners in world space.
    float3 triMin = min(a, min(b, c)) - voxelSize;
    float3 triMax = max(a, max(b, c)) + voxelSize;

    float3 p0 = float3(-1, -1, -1);
    float3 p1 = float3(1, 1, 1);
    //Convert the two corners into max and min indices to iterate over.
    int3 vMin = HashPositionToVoxelIndex3(triMin, voxelSceneBounds.y, voxelSceneBounds.x);

    int3 vMax = HashPositionToVoxelIndex3(triMax, voxelSceneBounds.y, voxelSceneBounds.x);
    
    for (int z = vMin.z; z <= vMax.z; ++z)
        for (int y = vMin.y; y <= vMax.y; ++y)
            for (int x = vMin.x; x <= vMax.x; ++x)
            {
                int3 v = int3(x, y, z);
                uint index = Flatten3D(v, voxelSceneBounds.x);
                float3 center = ((float3) v + 0.5f) * voxelSize - gridOrigin;

                float d = DistanceToTriangle(center, a, b, c);
                //voxelsL1Out[index].distance = asuint(0.0f);
                //InterlockedMin(voxelsL1Out[index].distance, asuint(d));
                //voxelsL1Out[index].normalDistance = float4(1, 0, 0, 1);
                int3 volumeIndex = v;
    
                Write3D(0, volumeIndex, 0.0f);
                //gBindless3DStorage[0]
                
            }
}

float3 getAABB(uint vertexOffset, uint vertexCount, out float3 minBounds, out float3 maxBounds)
{
    minBounds = float3(1e30, 1e30, 1e30);
    maxBounds = float3(-1e30, -1e30, -1e30);
    
    for (uint i = 0; i < vertexCount; i += 3) // Skip by 3 for triangles
    {
        for (uint j = 0; j < 3; j++) // Process each vertex of the triangle
        {
            float3 pos = vertexBuffer[vertexOffset + i + j].position.xyz;
            minBounds = min(minBounds, pos);
            maxBounds = max(maxBounds, pos);
        }
    }
    
    return maxBounds - minBounds;
}

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    uint index = pc.lod;
    
    Brush brush = Brushes[index];
    
    float2 voxelSceneBounds = float2(256.0f, 8.0f);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Get actual AABB from vertices
    float3 minBounds, maxBounds; //The min and max bounds. For an object from [-8, 8] this is that value.
    float3 extent = getAABB(brush.vertexOffset, brush.vertexCount, minBounds, maxBounds); //The extent, which is 16 now.
    float3 center = (minBounds + maxBounds) * 0.5f; //Need to find the center, which should be 0.
    float maxExtent = max(extent.x, max(extent.y, extent.z)); //Finally the maximum size of the box, which is 16.
    

    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution; //This is the center of a voxel.
    float3 samplePos = uvw * 2.0f - 1.0f; // [-1, 1] in texture space
    
    // Transform from normalized [-1,1] space to vertex space
    float3 worldPos = samplePos * (maxExtent * 0.5f) + center;

    float minDist = 1.0f;
    
    for (uint i = brush.vertexOffset; i < brush.vertexOffset + brush.vertexCount; i += 3)
    {
        uint3 idx = uint3(i, i + 1, i + 2);
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;

        float d = DistanceToTriangle(worldPos, a, b, c);
        
        minDist = min(minDist, d);

    }

    //We have successfully writen all values to the volume texture centered at the mesh center.
    Write3D(brush.textureID, int3(DTid), minDist);
    
}
