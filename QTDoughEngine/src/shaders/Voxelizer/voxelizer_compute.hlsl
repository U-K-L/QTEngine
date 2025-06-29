﻿
#include "../Helpers/ShaderHelpers.hlsl"

RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
SamplerState samplers[] : register(s0, space0); //Global

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float2 texelSize;
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

// For reading
Texture3D<float> gBindless3D[] : register(t4, space0);

// For writing
RWTexture3D<float> gBindless3DStorage[] : register(u5, space0);

RWStructuredBuffer<Brush> Brushes : register(u9, space1);


StructuredBuffer<ComputeVertex> vertexBuffer : register(t8, space1);

StructuredBuffer<uint> BrushesIndices : register(t10, space1);
RWStructuredBuffer<uint> TileBrushCounts : register(u11, space1);




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



float Read3DTransformed(in Brush brush, float3 worldPos)
{
    
    // 1. Transform from world space to local mesh space
    float3 localPos = mul(inverse(brush.model), float4(worldPos, 1.0f)).xyz;

    // 2. Get AABB in local mesh space
    float3 minBounds = brush.aabbmin;
    float3 maxBounds = brush.aabbmax;
    float3 extent = abs(maxBounds - minBounds);
    float maxExtent = max(extent.x, max(extent.y, extent.z));
    float3 center = 0.5f * (minBounds + maxBounds);


    // 3. Normalize localPos using AABB
    float3 normalized = (localPos - center) / (0.5f * maxExtent);

    // 4. Convert to voxel coord space
    float3 uvw = (normalized + 1.0f) * 0.5f;
    float3 voxelCoordf = uvw * brush.resolution;
    int3 voxelCoord = int3(voxelCoordf);
    
    
    int3 res = int3(brush.resolution, brush.resolution, brush.resolution);
    if (any(voxelCoord < 0) || any(voxelCoord >= res))
        return 1;
    
    return gBindless3D[brush.textureID].Load(int4(voxelCoord, 0));
}


// Filtered read using normalized coordinates and mipmaps
float Read3D(uint textureIndex, int3 coord)
{    
    return gBindless3D[textureIndex].Load(int4(coord, 0));
}

// Unfiltered write to RWTexture3D
void Write3D(uint textureIndex, int3 coord, float value)
{
    gBindless3DStorage[textureIndex][coord] = value;
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

bool TriangleOutsideVoxel(float3 a, float3 b, float3 c, float3 voxelMin, float3 voxelMax)
{
    float eps = 0.0025f;

    return
        (a.x > voxelMax.x + eps && b.x > voxelMax.x + eps && c.x > voxelMax.x + eps) || // right
        (a.x < voxelMin.x - eps && b.x < voxelMin.x - eps && c.x < voxelMin.x - eps) || // left
        (a.y > voxelMax.y + eps && b.y > voxelMax.y + eps && c.y > voxelMax.y + eps) || // above
        (a.y < voxelMin.y - eps && b.y < voxelMin.y - eps && c.y < voxelMin.y - eps) || // below
        (a.z > voxelMax.z + eps && b.z > voxelMax.z + eps && c.z > voxelMax.z + eps) || // in front
        (a.z < voxelMin.z - eps && b.z < voxelMin.z - eps && c.z < voxelMin.z - eps); // behind
}

void ClearVoxelData(uint3 DTid : SV_DispatchThreadID)
{
    float2 voxelSceneBounds = GetVoxelResolution(1.0f);
    uint3 gridSize = uint3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
    uint voxelIndex = DTid.x * gridSize.y * gridSize.z + DTid.y * gridSize.z + DTid.z;
    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    
    float2 voxelSceneBoundsL2 = GetVoxelResolution(2.0f);
    uint3 gridSizeL2 = uint3(voxelSceneBoundsL2.x, voxelSceneBoundsL2.x, voxelSceneBoundsL2.x);
    uint voxelIndexL2 = DTid.x * gridSizeL2.y * gridSizeL2.z + DTid.y * gridSizeL2.z + DTid.z;
    
    float voxelsizeL2 = voxelSceneBoundsL2.y / voxelSceneBoundsL2.x;

    float2 voxelSceneBoundsL3 = GetVoxelResolution(3.0f);
    uint3 gridSizeL3 = uint3(voxelSceneBoundsL3.x, voxelSceneBoundsL3.x, voxelSceneBoundsL3.x);
    uint voxelIndexL3 = DTid.x * gridSizeL3.y * gridSizeL3.z + DTid.y * gridSizeL3.z + DTid.z;
    
    float voxelsizeL3 = voxelSceneBoundsL3.y / voxelSceneBoundsL3.x;
    
    const float BIG = 100;
    voxelsL1Out[voxelIndex].distance = asuint(BIG);
    voxelsL1Out[voxelIndex].normalDistance = 0;
    
    //Write3D(0, int3(DTid), 1.0f);
    
    //if (voxelIndex > 6310128 && voxelIndex < 10568197)
        //voxelsL1Out[voxelIndex].distance = asuint(0.0f);
    
    voxelsL2Out[voxelIndexL2].distance = asuint(BIG);
    voxelsL2Out[voxelIndexL2].normalDistance = 0;
    
    voxelsL3Out[voxelIndexL3].distance = asuint(BIG);
    voxelsL3Out[voxelIndexL3].normalDistance = 0;
}

float GaussianBlurSDF(int3 coord, StructuredBuffer<Voxel> inputBuffer, int3 gridSize)
{
    float blurredDist = 0.0f;

    [unroll]
    for (int z = -1; z <= 1; z++)
    [unroll]
        for (int y = -1; y <= 1; y++)
    [unroll]
            for (int x = -1; x <= 1; x++)
            {
                int3 offset = int3(x, y, z);
                int3 sampleCoord = coord + offset;

        // Clamp to valid range
                sampleCoord = clamp(sampleCoord, int3(0, 0, 0), gridSize - 1);

                uint flatIndex = sampleCoord.x * gridSize.y * gridSize.z + sampleCoord.y * gridSize.z + sampleCoord.z;
                float sdfValue = asfloat(inputBuffer[flatIndex].distance);

        // Separable Gaussian: 1-2-1 per axis
                float wx = (x == 0) ? 2.0f : 1.0f;
                float wy = (y == 0) ? 2.0f : 1.0f;
                float wz = (z == 0) ? 2.0f : 1.0f;
                float weight = wx * wy * wz;

                blurredDist += sdfValue * weight;
            }

    return blurredDist / 64.0f; // Total kernel weight
}


void DeformBrush(uint3 DTid : SV_DispatchThreadID)
{
    uint index = pc.triangleCount;
    
    Brush brush = Brushes[index];
    
    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution;
    float3 samplePos = uvw * 2.0f - 1.0f;
    
    //float3 _Time = time * 0.1f;
    
    //samplePos = float3(cos(_Time.x), sin(_Time.x), sin(_Time.x));

    // Reconstruct voxel coordinate (floating point):
    float3 voxelCoordF = ((samplePos + 1.0f) * 0.5f) * brush.resolution - 0.5f;
    
    // Round to nearest integer
    int3 coords = int3(voxelCoordF);

    
    float minDist = 100.0f;


    
    //We have successfully writen all values to the volume texture centered at the mesh center.
    float oldminDist = Read3D(brush.textureID, int3(DTid));
    
    Write3D(brush.textureID2, coords, oldminDist); //Add old value here.
    

}


void CreateBrush(uint3 DTid : SV_DispatchThreadID)
{
    uint index = pc.triangleCount;
    
    Brush brush = Brushes[index];
    
    // Get actual AABB from vertices
    float3 minBounds, maxBounds; //The min and max bounds. For an object from [-8, 8] this is that value.
    float3 extent = getAABB(brush.vertexOffset, brush.vertexCount, minBounds, maxBounds); //The extent, which is 16 now.
    float3 center = (minBounds + maxBounds) * 0.5f; //Need to find the center, which should be 0.
    float maxExtent = max(extent.x, max(extent.y, extent.z)); //Finally the maximum size of the box, which is 16.
    
    //Brushes[index].maxExtent = maxExtent;
    //Brushes[index].center = center;
    //Brushes[index].invModel = inverse(Brushes[index].model);
    Brushes[index].aabbmax = maxBounds;
    Brushes[index].aabbmin = minBounds;


    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution; //This is the center of a voxel.
    float3 samplePos = uvw * 2.0f - 1.0f; // [-1, 1] in texture space
    
    // Transform from normalized [-1,1] space to vertex space
    float3 worldPos = samplePos * (maxExtent * 0.5f) + center;


    float minDist = 100.0f;
    
    for (uint i = brush.vertexOffset; i < brush.vertexOffset + brush.vertexCount; i += 3)
    {
        uint3 idx = uint3(i, i + 1, i + 2);
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;

        float d = DistanceToTriangle(worldPos, a, b, c);
        
        //d = saturate(d);
        
        minDist = min(minDist, d);

    }

    //We have successfully writen all values to the volume texture centered at the mesh center.
    Write3D(brush.textureID, int3(DTid), minDist);
}

float4 GetVoxelValue(float sampleLevel, int index)
{
    float4 v1 = float4(voxelsL1In[index].normalDistance.xyz, asfloat(voxelsL1In[index].distance));
    float4 v2 = float4(voxelsL2In[index].normalDistance.xyz, asfloat(voxelsL2In[index].distance));
    float4 v3 = float4(voxelsL3In[index].normalDistance.xyz, asfloat(voxelsL3In[index].distance));

    float s1 = step(sampleLevel, 1.01f);
    float s2 = step(sampleLevel, 2.01f) - s1;
    float s3 = 1.0f - s1 - s2;

    return v1 * s1 + v2 * s2 + v3 * s3;
}


float FSMUpdate(int3 pos, int sweepDirection, float sampleLevel)
{
    float neighbors[3];

    // Choose direction based on sweepDirection
    
    int3 dx = (sweepDirection & 1) != 0 ? int3(1, 0, 0) : int3(-1, 0, 0);
    int3 dy = (sweepDirection & 2) != 0 ? int3(0, 1, 0) : int3(0, -1, 0);
    int3 dz = (sweepDirection & 4) != 0 ? int3(0, 0, 1) : int3(0, 0, -1);
    
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    uint3 gridSize = uint3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
   
    
    int3 px = clamp(pos + dx, 0, gridSize - 1);
    uint nIdx = Flatten3D(px, gridSize.x);
    int3 py = clamp(pos + dy, 0, gridSize - 1);
    uint nIdy = Flatten3D(py, gridSize.y);
    int3 pz = clamp(pos + dz, 0, gridSize - 1);
    uint nIdz = Flatten3D(pz, gridSize.z);
    
    neighbors[0] = GetVoxelValue(sampleLevel, nIdx).w;
    neighbors[1] = GetVoxelValue(sampleLevel, nIdy).w;
    neighbors[2] = GetVoxelValue(sampleLevel, nIdz).w;
    
    if ((neighbors[0] > 2.0f) && (neighbors[1] > 2.0f) && (neighbors[2] > 2.0f))
        return -1.0f;

    // Sort neighbors (smallest first)
    for (int i = 0; i < 2; i++)
        for (int j = i + 1; j < 3; j++)
            if (neighbors[i] > neighbors[j])
            {
                float tmp = neighbors[i];
                neighbors[i] = neighbors[j];
                neighbors[j] = tmp;
            }

    float a = neighbors[0], b = neighbors[1], c = neighbors[2];
    float h = voxelSize;

    // Solve the quadratic: (u - a)^2 + (u - b)^2 + ... = h^2
    float u = a + h;
    if (u > b)
    {
        float sum = a + b;
        float tmp = 2.0 * h * h - (a - b) * (a - b);
        if (tmp > 0)
        {
            u = (sum + sqrt(tmp)) * 0.5;
            if (u > c)
            {
                sum = a + b + c;
                tmp = 3 * h * h - (a - b) * (a - b) - (b - c) * (b - c) - (c - a) * (c - a);
                if (tmp > 0)
                    u = (sum + sqrt(tmp)) * 0.333333f;
            }
        }
    }

    return min(u, 1.0f);
}


[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    
    float sampleLevelL = pc.lod;
    uint triangleCount = pc.triangleCount;
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevelL);
    uint3 gridSize = uint3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
    uint voxelIndex = DTid.x * gridSize.y * gridSize.z + DTid.y * gridSize.z + DTid.z;
    
    if (sampleLevelL == 5.0f)
    {
        CreateBrush(DTid);
        return;
    }
    
    if (sampleLevelL == 14.0f)
    {
        DeformBrush(DTid);
        return;
    }
    
    if(sampleLevelL == 0.0f)
    {
        ClearVoxelData(DTid);
        return;
    }

    float2 voxelSceneBoundsL1 = GetVoxelResolution(1.0f);
    uint3 gridSizeL1 = uint3(voxelSceneBoundsL1.x, voxelSceneBoundsL1.x, voxelSceneBoundsL1.x);
    uint voxelIndexL1 = DTid.x * gridSizeL1.y * gridSizeL1.z + DTid.y * gridSizeL1.z + DTid.z;
    
    
        
    //Every value after 6 is a sweep direction until 13.
    if (sampleLevelL >= 6.0f && sampleLevelL < 14.0f)
    {
        int3 dx = ((int)(pc.lod - 6.0f) & 1) != 0 ? int3(1, 0, 0) : int3(-1, 0, 0);
        int3 dy = ((int)(pc.lod - 6.0f) & 2) != 0 ? int3(0, 1, 0) : int3(0, -1, 0);
        int3 dz = ((int)(pc.lod - 6.0f) & 4) != 0 ? int3(0, 0, 1) : int3(0, 0, -1);
        
        int3 coord;
        coord.x = (dx.x > 0) ? DTid.x : (gridSize.x - 1 - DTid.x);
        coord.y = (dy.y > 0) ? DTid.y : (gridSize.y - 1 - DTid.y);
        coord.z = (dz.z > 0) ? DTid.z : (gridSize.z - 1 - DTid.z);

        float dl1 = FSMUpdate(coord, pc.lod - 6.0f, 1.0f);
        
        if(dl1 < 0)
            return;
        
        //float dl2 = FSMUpdate(coord, pc.lod - 6.0f, 2.0f);
        //float dl3 = FSMUpdate(coord, pc.lod - 6.0f, 3.0f);
        
        float dl1Old = asfloat(voxelsL1In[voxelIndexL1].distance);
        //float dl2Old = asfloat(voxelsL2In[voxelIndexL2].distance);
        //float dl3Old = asfloat(voxelsL3In[voxelIndexL3].distance);
        

        
        voxelsL1Out[voxelIndexL1].distance = asuint(min(dl1, dl1Old));
        //voxelsL2Out[voxelIndexL2].distance = asuint(min(dl2, dl2Old));
        //voxelsL3Out[voxelIndexL3].distance = asuint(min(dl3, dl3Old));

        return;
    }

    
    

    float2 voxelSceneBoundsL2 = GetVoxelResolution(2.0f);
    uint3 gridSizeL2 = uint3(voxelSceneBoundsL2.x, voxelSceneBoundsL2.x, voxelSceneBoundsL2.x);
    uint voxelIndexL2 = DTid.x * gridSizeL2.y * gridSizeL2.z + DTid.y * gridSizeL2.z + DTid.z;

    float2 voxelSceneBoundsL3 = GetVoxelResolution(3.0f);
    uint3 gridSizeL3 = uint3(voxelSceneBoundsL3.x, voxelSceneBoundsL3.x, voxelSceneBoundsL3.x);
    uint voxelIndexL3 = DTid.x * gridSizeL3.y * gridSizeL3.z + DTid.y * gridSizeL3.z + DTid.z;
    
    

    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert grid index to world-space center position. center = dtid * vs - hs => (center + hs) / vs = dtid
    float3 center = ((float3) DTid + 0.5f) * voxelSize - halfScene;

    float3 voxelMin = center - voxelSize * 0.5f;
    float3 voxelMax = center + voxelSize * 0.5f;

    
    float minDist = 64.0f;
    uint3 cachedIdx = 0;
    
    
    float tileWorldSize = TILE_SIZE * voxelSize;
    int numTilesPerAxis = VOXEL_RESOLUTIONL1 / TILE_SIZE;

    int3 tileCoord = floor((center + halfScene) / tileWorldSize);

    tileCoord = clamp(tileCoord, int3(0, 0, 0), int3(numTilesPerAxis - 1, numTilesPerAxis - 1, numTilesPerAxis - 1));

    uint tileIndex = Flatten3D(tileCoord, numTilesPerAxis);
    

    //Find the distance field closes to this voxel.
    uint brushCount = TileBrushCounts[tileIndex];
    for (uint i = 0; i < brushCount; i++)
    {
        uint offset = tileIndex * TILE_MAX_BRUSHES + i;
        uint index = BrushesIndices[offset];
        
        if (index >= 4294967295)
            break;
        
        Brush brush = Brushes[index];
                
        float d = Read3DTransformed(brush, center);

        minDist = min(minDist, d);

    }
    
    if(minDist > 63.0f)
        return;

    if (sampleLevelL == 1.0f)
    {

        voxelsL1Out[voxelIndex].distance = asuint(minDist);
        voxelsL1Out[voxelIndex].normalDistance = float4(1, 0, 0, 1); //float4(normal, 0);
        return;
    }
    
    if (sampleLevelL == 2.0f)
    {
        voxelsL2Out[voxelIndex].distance = asuint(minDist);
        voxelsL2Out[voxelIndex].normalDistance = float4(1, 0, 0, 1); //float4(normal, 0);
        return;
    }
    
    if (sampleLevelL == 3.0f)
    {
        voxelsL3Out[voxelIndex].distance = asuint(minDist);
        voxelsL3Out[voxelIndex].normalDistance = float4(1, 0, 0, 1); //float4(normal, 0);
        return;
    }
}