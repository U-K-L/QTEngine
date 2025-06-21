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

RWTexture3D<float> gBindless3DStorage[] : register(u5, space0);

RWStructuredBuffer<uint> BrushesIndices : register(u10, space1);

RWStructuredBuffer<uint> TileBrushCounts : register(u11, space1);

// Unfiltered write to RWTexture3D
void Write3D(uint textureIndex, int3 coord, float value)
{
    gBindless3DStorage[textureIndex][coord] = value;
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
    
    return abs(maxBounds - minBounds);
}

void ClearTileCount(uint3 DTid : SV_DispatchThreadID)
{
    TileBrushCounts[DTid.x] = 0;
}


float3 getAABBWorld(uint vertexOffset, uint vertexCount,
               out float3 minBounds, out float3 maxBounds, in Brush brush)
{
    minBounds = float3(1e30, 1e30, 1e30);
    maxBounds = float3(-1e30, -1e30, -1e30);

    for (uint i = 0; i < vertexCount; i += 3)
        for (uint j = 0; j < 3; ++j)
        {
            float4 lp = float4(vertexBuffer[vertexOffset + i + j].position.xyz, 1.0);
            float3 pos = mul(brush.model, lp).xyz;
            minBounds = min(minBounds, pos);
            maxBounds = max(maxBounds, pos);
        }

    return abs(maxBounds - minBounds);
}


[numthreads(8, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    float level = pc.lod;
    if(level > 4.0f)
    {
        ClearTileCount(DTid);
        return;
    }
    
   //Do this per brush.
    uint brushID = DTid.x;
    Brush brush = Brushes[brushID];
    
    
    // Get actual AABB from vertices
    float3 minBounds = brush.aabbmin;
    float3 maxBounds = brush.aabbmax;
    float3 extent = abs(maxBounds - minBounds);
    
    float3 brushMin = minBounds;
    float3 brushMax = maxBounds;
    
    float3 worldHalfExtent = SCENE_BOUNDSL1 * 0.5f;
    float voxelSize = SCENE_BOUNDSL1 / VOXEL_RESOLUTIONL1;
    float tileWorldSize = TILE_SIZE * voxelSize;
    int numOfTilesDim = (int) (VOXEL_RESOLUTIONL1 / TILE_SIZE);
    
    int3 minTile = floor((brushMin + worldHalfExtent) / tileWorldSize);
    int3 maxTile = floor((brushMax + worldHalfExtent) / tileWorldSize);

    for (int z = minTile.z; z <= maxTile.z; ++z)
        for (int y = minTile.y; y <= maxTile.y; ++y)
            for (int x = minTile.x; x <= maxTile.x; ++x)
            {
                int3 tileCoord = int3(x, y, z);
                uint tileIndex = Flatten3D(tileCoord, numOfTilesDim);
                
                // Atomically reserve a slot in this tile's brush list
                uint writeIndex;
                InterlockedAdd(TileBrushCounts[tileIndex], 1, writeIndex);

                if (writeIndex < TILE_MAX_BRUSHES)
                {
                    uint offset = tileIndex * TILE_MAX_BRUSHES + writeIndex;
                    BrushesIndices[offset] = brushID;
                }

            }
}
