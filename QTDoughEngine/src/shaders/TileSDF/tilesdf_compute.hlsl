RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);

#include "../Helpers/ShaderHelpers.hlsl"


cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 texelSize;
    float isOrtho;
}

cbuffer Constants : register(b2, space0)
{
    float deltaTime; // offset 0
    float time; // offset 4
    float2 pad; // offset 8..15, total 16 bytes
};

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
RWStructuredBuffer<Brush> Brushes : register(u9, space1);

RWTexture3D<float> gBindless3DStorage[] : register(u5, space0);

// For reading
Texture3D<float> gBindless3D[] : register(t4, space0);

RWStructuredBuffer<uint> BrushesIndices : register(u10, space1);

RWStructuredBuffer<uint> TileBrushCounts : register(u11, space1);

StructuredBuffer<Particle> particlesL1In : register(t12, space1); // readonly
RWStructuredBuffer<Particle> particlesL1Out : register(u13, space1); // write

StructuredBuffer<ControlParticle> controlParticlesL1In : register(t14, space1); // readonly
RWStructuredBuffer<ControlParticle> controlParticlesL1Out : register(u15, space1); // write

// Filtered read using normalized coordinates and mipmaps
float Read3D(uint textureIndex, int3 coord)
{
    return gBindless3D[textureIndex].Load(int4(coord, 0));
}

float ReadWorldSDF(float3 worldPos)
{
    // Constants defining the world SDF volume (assuming these are defined in a helper)
    float worldHalfExtent = WORLD_SDF_BOUNDS * 0.5f;
    float voxelSize = WORLD_SDF_BOUNDS / WORLD_SDF_RESOLUTION;

    // Convert world position to integer texture coordinates
    int3 texCoord = int3(floor((worldPos + worldHalfExtent) / voxelSize));

    // Bounds check to ensure we don't sample outside the volume
    if (any(texCoord < 0) || any(texCoord >= WORLD_SDF_RESOLUTION))
    {
        return 64.0f; // Return a large distance if outside the world volume
    }

    // Sample the world SDF texture (assuming it's at bindless index 0)
    return gBindless3D[0].Load(int4(texCoord, 0));
}

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
    
    //Create an enlarged container to fit deformations.
    float3 extent = abs(maxBounds - minBounds);
    float maxExtent = max(extent.x, max(extent.y, extent.z)) * 0.25f;
    minBounds -= maxExtent;
    maxBounds += maxExtent;
    
    //Additional padding based on blending values. As blending more, AABB expands.
    float blendingPadding = brush.blend * 2 * maxExtent;
    minBounds -= blendingPadding;
    maxBounds += blendingPadding;
    
    //Pad container by 4 voxels
    float voxelPadding = 0.03125f * 4;
    minBounds -= voxelPadding;
    maxBounds += voxelPadding;
    


    return abs(maxBounds - minBounds);
}

void ParticlesSDF(uint3 DTid : SV_DispatchThreadID)
{
    Particle particle = particlesL1In[DTid.x];
    
    particle.position.xyz += randPos(DTid.x, particle.position.xyz);
    
    float pRad = 0.125f;
    
    float voxelSize = SCENE_BOUNDSL1 / VOXEL_RESOLUTIONL1;
    float3 halfScene = SCENE_BOUNDSL1 * 0.5f;

    float3 minPos = particle.position.xyz - pRad;
    float3 maxPos = particle.position.xyz + pRad;

    int3 minVoxel = floor((minPos + halfScene) / voxelSize);
    int3 maxVoxel = floor((maxPos + halfScene) / voxelSize);

    if (any(minVoxel > maxVoxel))
    {
        return;
    }
    
    for (int z = minVoxel.z; z <= maxVoxel.z; ++z)
        for (int y = minVoxel.y; y <= maxVoxel.y; ++y)
            for (int x = minVoxel.x; x <= maxVoxel.x; ++x)
            {
                int3 voxelIndex = int3(x, y, z);
                float3 worldPos = voxelSize * (float3(voxelIndex) - 0.5f * VOXEL_RESOLUTIONL1);

                float dist = sdSphere(worldPos, particle.position.xyz, pRad);

                uint flatIndex = Flatten3D(voxelIndex, VOXEL_RESOLUTIONL1);
                InterlockedMin(voxelsL1Out[flatIndex].distance, asuint(dist));

            }

    particlesL1Out[DTid.x].position = particle.position;

}

void UpdateControlPoints(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    
    // Determine which brush this control point belongs to
    // Assumes CAGE_VERTS is defined (e.g., as 26) in a helper header
    uint brushID = index / CAGE_VERTS;
    Brush brush = Brushes[brushID];

    // Read the CURRENT local position of the control point from the previous frame
    float3 local_pos = controlParticlesL1In[index].position.xyz;

    // --- Apply physics/animation ---
    // Applying simple gravity as an example
    float3 gravity = float3(0.0f, 0.0, 0.0098f);
    float3 new_local_pos = local_pos + (gravity * (1.0f - brush.stiffness));
    
    // --- Collision Detection ---

    float3 new_world_pos = mul(brush.model, float4(new_local_pos, 1.0)).xyz;

    float sdf_dist = ReadWorldSDF(new_world_pos);

    if (sdf_dist <= 0.2f)
    {
        // Collision detected! The point is inside or on a surface.
        // For a simple response, we just stop its movement by reverting to the previous position.
        new_local_pos = local_pos + (gravity * (1.0f - max(brush.stiffness, 0.9f) ));
    }
    
    //new_local_pos = local_pos;
    // Write the final, validated position back to the output buffer
    controlParticlesL1Out[index].position.xyz = new_local_pos;
}

[numthreads(8, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    float level = pc.lod;
    if(level == 5.0f)
    {
        ClearTileCount(DTid);
        return;
    }
    
    if (level == 2.0f)
    {
        ParticlesSDF(DTid);
        return;
    }
    
    if(level == 8.0f)
    {
        UpdateControlPoints(DTid);
    }
    
   //Do this per brush.
    uint brushID = DTid.x;
    Brush brush = Brushes[brushID];
    
    //This happens when aabbmax roughly equals aabbmin.
    /*
    if(brush.isDirty == 2)
        return;
    if(brush.id > MAX_BRUSHES)
        return;
    */
    /*
    bool needUpdate = brush.isDirty;
    if (needUpdate == false)
        return;
    */

    // Get actual AABB from vertices
    float3 minBounds, maxBounds;
    float3 extent = getAABBWorld(brush.vertexOffset, brush.vertexCount, minBounds, maxBounds, brush);
    
    float3 brushMin = minBounds;
    float3 brushMax = maxBounds;
    
    Brushes[brushID].invModel = inverse(brush.model);
    //brush.aabbmax = maxBounds;
    //brush.aabbmin = minBounds;
    
    float3 worldHalfExtent = WORLD_SDF_BOUNDS * 0.5f;
    float voxelSize = WORLD_SDF_BOUNDS / WORLD_SDF_RESOLUTION;
    float tileWorldSize = TILE_SIZE * voxelSize;
    int numOfTilesDim = (int) (WORLD_SDF_RESOLUTION / TILE_SIZE);
    
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
