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

// Bindless 3D textures
Texture3D<float4> gBindless3D[] : register(t4, space0); // Read-only 3D textures
RWTexture3D<float4> gBindless3DStorage[] : register(u5, space0); // Read-write 3D textures


StructuredBuffer<ComputeVertex> vertexBuffer : register(t8, space1);

// Simple read from 3D texture
float4 Read3D(uint textureIndex, int3 coord)
{
    return gBindless3DStorage[textureIndex][coord];
}

// Simple write to 3D texture
void Write3D(uint textureIndex, int3 coord, float4 value)
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


void ComputePerTriangle(uint3 DTid : SV_DispatchThreadID)
{
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
    float3 triMin = min(a, min(b, c)) - voxelSize*4;
    float3 triMax = max(a, max(b, c)) + voxelSize*4;

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
                InterlockedMin(voxelsL1Out[index].distance, asuint(d));
                voxelsL1Out[index].normalDistance = float4(1, 0, 0, 1);
                
            }
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
    
    const float BIG = 1e30f;
    voxelsL1Out[voxelIndex].distance = asuint(voxelSize);
    voxelsL1Out[voxelIndex].normalDistance = 0;
    
    //if (voxelIndex > 6310128 && voxelIndex < 10568197)
        //voxelsL1Out[voxelIndex].distance = asuint(0.0f);
    
    voxelsL2Out[voxelIndexL2].distance = asuint(voxelsizeL2);
    voxelsL2Out[voxelIndexL2].normalDistance = 0;
    
    voxelsL3Out[voxelIndexL3].distance = asuint(voxelsizeL3);
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
        ComputePerTriangle(DTid);
        return;
    }
    
    if(sampleLevelL == 0.0f)
    {
        ClearVoxelData(DTid);
        return;
    }
    
    if (sampleLevelL == 1.0f)
    {
        /*
        int3 gridSize = int3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
        int3 coord = int3(DTid);
        uint flatIndex = coord.x * gridSize.y * gridSize.z + coord.y * gridSize.z + coord.z;

        float blurred = GaussianBlurSDF(coord, voxelsL1In, gridSize);
        voxelsL1Out[flatIndex].distance = asuint(blurred);
        voxelsL1Out[flatIndex].normalDistance = voxelsL1In[flatIndex].normalDistance;
        */
        //return;
    }
    float2 voxelSceneBoundsL3 = GetVoxelResolution(3.0f);
    uint3 gridSizeL3 = uint3(voxelSceneBoundsL3.x, voxelSceneBoundsL3.x, voxelSceneBoundsL3.x);
    uint voxelIndexL3 = DTid.x * gridSizeL3.y * gridSizeL3.z + DTid.y * gridSizeL3.z + DTid.z;


    


    

    
    

    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert grid index to world-space center position
    float3 center = ((float3) DTid + 0.5f) * voxelSize - halfScene;
        
    float3 voxelMin = center - voxelSize * 0.5f;
    float3 voxelMax = center + voxelSize * 0.5f;

    
    float minDist = 100.0f;
    uint3 cachedIdx = 0;
    
    if (sampleLevelL < 3.0f)
        if (voxelsL3Out[voxelIndexL3].normalDistance.w > 25.0f)
        {
            return;
        }
    
    for (uint i = 0; i < triangleCount; i += 1)
    {
        uint3 idx = uint3(i*3, i*3 + 1, i*3 + 2);
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;
        
        /*
        if (!TriangleAABBOverlap(a, b, c, voxelMin, voxelMax))
            continue;
        */
        float d = DistanceToTriangle(center, a, b, c);
        
        if (d < minDist)
            cachedIdx = idx;
        
        minDist = min(minDist, d);

    }
    
    float3 normal = BarycentricNormals(center, cachedIdx);



    if (sampleLevelL == 1.0f)
    {
        voxelsL1Out[voxelIndex].distance = asuint(minDist);
        voxelsL1Out[voxelIndex].normalDistance = float4(normal, 0);
    }
    if (sampleLevelL == 2.0f)
    {
        voxelsL2Out[voxelIndex].distance = asuint(minDist);
        voxelsL2Out[voxelIndex].normalDistance = float4(normal, 0);
    }

    if (sampleLevelL == 3.0f)
    {
        voxelsL3Out[voxelIndex].distance = asuint(minDist);
        voxelsL3Out[voxelIndex].normalDistance = float4(normal, 0);
    }


}
