﻿﻿
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
Texture3D<float2> gBindless3D[] : register(t4, space0);

// For writing
RWTexture3D<float2> gBindless3DStorage[] : register(u5, space0);

RWStructuredBuffer<Brush> Brushes : register(u9, space1);


StructuredBuffer<ComputeVertex> vertexBuffer : register(t8, space1);

StructuredBuffer<uint> BrushesIndices : register(t10, space1);
RWStructuredBuffer<uint> TileBrushCounts : register(u11, space1);

StructuredBuffer<ControlParticle> controlParticlesL1In : register(t14, space1); // readonly
RWStructuredBuffer<ControlParticle> controlParticlesL1Out : register(u15, space1); // write

RWStructuredBuffer<uint> GlobalIDCounter : register(u16, space1);

RWStructuredBuffer<ComputeVertex> meshingVertices : register(u17, space1);


float3 getAABB(uint vertexOffset, uint vertexCount, out float3 minBounds, out float3 maxBounds, in Brush brush)
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
    
    float3 extent = abs(maxBounds - minBounds);
    float maxExtent = max(extent.x, max(extent.y, extent.z)) * 0.25f;
    minBounds -= maxExtent;
    maxBounds += maxExtent;
    
    float blendingPadding = brush.blend * 2 * maxExtent;
    minBounds -= blendingPadding;
    maxBounds += blendingPadding;
    
    float voxelPadding = 0.03125f * 4;
    minBounds -= voxelPadding;
    maxBounds += voxelPadding;
    
    return abs(maxBounds - minBounds);
}



float2 Read3DTransformed(in Brush brush, float3 worldPos)
{
    
    // 1. Transform from world space to local mesh space
    float3 localPos = mul(brush.invModel, float4(worldPos, 1.0f)).xyz;

    // 2. Get AABB in local mesh space
    float maxExtent = brush.aabbmax.w;
    float3 center = brush.center.xyz;


    // 3. Normalize localPos using AABB
    float3 normalized = (localPos - center) / (0.5f * maxExtent);

    // 4. Convert to voxel coord space
    float3 uvw = (normalized + 1.0f) * 0.5f;
    float3 voxelCoordf = uvw * brush.resolution;
    int3 voxelCoord = int3(voxelCoordf);
    
    
    int3 res = int3(brush.resolution, brush.resolution, brush.resolution);
    if (any(voxelCoord < 0) || any(voxelCoord >= res))
        return 1.0f;
    
    return gBindless3D[brush.textureID2].Load(int4(voxelCoord, 0));
}


// Filtered read using normalized coordinates and mipmaps
float2 Read3D(uint textureIndex, int3 coord)
{    
    return gBindless3D[textureIndex].Load(int4(coord, 0));
}

float2 Read3DMip(uint textureIndex, int3 coord, int level)
{
    return gBindless3D[textureIndex].Load(int4(coord, level));
}

float2 Read3DTrilinear(uint textureIndex, float3 uvw, float mipLevel)
{
    return gBindless3D[textureIndex].SampleLevel(samplers[0], uvw, mipLevel);
}


// Unfiltered write to RWTexture3D
void Write3D(uint textureIndex, int3 coord, float value)
{
    gBindless3DStorage[textureIndex][coord] = value;
}

void Write3D(uint textureIndex, int3 coord, float2 value)
{
    gBindless3DStorage[textureIndex][coord] = value;
}

void Write3DID(uint textureIndex, int3 coord, float value)
{
    gBindless3DStorage[textureIndex][coord].y = value;
}

void Write3DDist(uint textureIndex, int3 coord, float value)
{
    gBindless3DStorage[textureIndex][coord].x = value;
}

float2 HardwareTrilinearSample(uint textureIndex, float3 uvw)
{
    return gBindless3D[textureIndex].SampleLevel(samplers[0], uvw, 0);
}

float2 GetVoxelValueTexture(int textureId, float3 uvw)
{
    return HardwareTrilinearSample(textureId, uvw);
}

float SignedDistanceToTriangle(float3 p, float3 a, float3 b, float3 c)
{
    float3 normal = normalize(cross(b - a, c - a));
    float unsignedDist = DistanceToTriangle(p, a, b, c);
    float signTri = sign(dot(p - a, normal));
    return signTri * unsignedDist;
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
    int3 DTL1 = DTid / 2;
    float2 voxelSceneBoundsl1 = GetVoxelResolution(0.0f);
    voxelsL1Out[Flatten3DR(DTL1, voxelSceneBoundsl1.x)].uniqueId = 0;
    voxelsL1Out[Flatten3DR(DTL1, voxelSceneBoundsl1.x)].normalDistance.w = 0.00125f;
    voxelsL1Out[Flatten3DR(DTL1, voxelSceneBoundsl1.x)].normalDistance.x = 0;
    Write3DDist(0, DTid, DEFUALT_EMPTY_SPACE);
}

void InitVoxelData(uint3 DTid : SV_DispatchThreadID)
{
    float2 voxelSceneBounds = GetVoxelResolution(0.0f);
    uint3 gridSize = uint3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
    uint voxelIndex = DTid.x * gridSize.y * gridSize.z + DTid.y * gridSize.z + DTid.z;
    
    float2 clearValue = float2(DEFUALT_EMPTY_SPACE, NO_LABELF());
    //voxelsL1Out[voxelIndex].id = NO_LABELF();
    Write3D(0, DTid, clearValue);
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


static const float EPS = 1e-6f;

#define ADD_W(idx, w) { num += (w) * gDiff[idx]; denom += (w); }

// one triangle → three weights
#define ACCUM_WEIGHTS(idx0, idx1, idx2)                              \
{                                                          \
    float3 r0 = relative_positions[idx0];          \
    float3 r1 = relative_positions[idx1];          \
    float3 r2 = relative_positions[idx2];          \
                                                           \
    float l0 = lenFast(lengths[idx0]);                                \
    float l1 = lenFast(lengths[idx1]);                                \
    float l2 = lenFast(lengths[idx2]);                                \
                                                           \
    float t01 = TAN_HALF(r0,r1,l0,l1);                     \
    float t12 = TAN_HALF(r1,r2,l1,l2);                     \
    float t20 = TAN_HALF(r2,r0,l2,l0);                     \
                                                           \
    ADD_W(idx0, (t20 + t01) * rcp(l0));                    \
    ADD_W(idx1, (t01 + t12) * rcp(l1));                    \
    ADD_W(idx2, (t12 + t20) * rcp(l2));                    \
}


//Call this via an indirect dispatch whenever physics detects changes.
groupshared float3 gDiff[26];
void DeformBrush(uint3 DTid : SV_DispatchThreadID, uint gIndex : SV_GroupIndex, uint3 lThreadID : SV_GroupThreadID)
{
    uint3 voxelCoord = DTid;
    uint brushID = pc.triangleCount;
    Brush brush = Brushes[brushID];

    if (any(voxelCoord >= brush.resolution))
        return;
    
    if (gIndex < 26)
        gDiff[gIndex] = controlParticlesL1In[gIndex + CAGE_VERTS * brushID].position.xyz -
                        canonicalControlPoints[gIndex];
    GroupMemoryBarrierWithGroupSync();

    float3 uvw = (float3(voxelCoord) + 0.5f) / brush.resolution;
    float3 posLocal = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, uvw);
    float3 p = 2.0f * ((posLocal - brush.aabbmin.xyz) /
                              (brush.aabbmax.xyz - brush.aabbmin.xyz)) - 1.0f;
    
    // Local arrays to hold pre-calculated values
    float3 relative_positions[26];
    float lengths[26];

    // Pre-calculate all vectors and lengths once
    for (int i = 0; i < 26; ++i)
    {
        relative_positions[i] = canonicalControlPoints[i] - p;
        lengths[i] = lenFast(relative_positions[i]);
    }


    float3 num = float3(0, 0, 0);
    float denom = 0.0f;
    
    /*
    // ---- 48 triangles – fully unrolled ------------------------------------
    ACCUM_WEIGHTS( 0,  8, 20); ACCUM_WEIGHTS( 8,  1, 20);
    ACCUM_WEIGHTS( 1, 13, 20); ACCUM_WEIGHTS(13,  3, 20);
    ACCUM_WEIGHTS( 3,  9, 20); ACCUM_WEIGHTS( 9,  2, 20);
    ACCUM_WEIGHTS( 2, 12, 20); ACCUM_WEIGHTS(12,  0, 20);

    ACCUM_WEIGHTS( 4, 10, 21); ACCUM_WEIGHTS(10,  5, 21);
    ACCUM_WEIGHTS( 5, 15, 21); ACCUM_WEIGHTS(15,  7, 21);
    ACCUM_WEIGHTS( 7, 11, 21); ACCUM_WEIGHTS(11,  6, 21);
    ACCUM_WEIGHTS( 6, 14, 21); ACCUM_WEIGHTS(14,  4, 21);

    ACCUM_WEIGHTS( 0, 16, 22); ACCUM_WEIGHTS(16,  4, 22);
    ACCUM_WEIGHTS( 4, 14, 22); ACCUM_WEIGHTS(14,  6, 22);
    ACCUM_WEIGHTS( 6, 18, 22); ACCUM_WEIGHTS(18,  2, 22);
    ACCUM_WEIGHTS( 2, 12, 22); ACCUM_WEIGHTS(12,  0, 22);

    ACCUM_WEIGHTS( 1, 17, 23); ACCUM_WEIGHTS(17,  5, 23);
    ACCUM_WEIGHTS( 5, 15, 23); ACCUM_WEIGHTS(15,  7, 23);
    ACCUM_WEIGHTS( 7, 19, 23); ACCUM_WEIGHTS(19,  3, 23);
    ACCUM_WEIGHTS( 3, 13, 23); ACCUM_WEIGHTS(13,  1, 23);

    ACCUM_WEIGHTS( 0,  8, 24); ACCUM_WEIGHTS( 8,  1, 24);
    ACCUM_WEIGHTS( 1, 17, 24); ACCUM_WEIGHTS(17,  5, 24);
    ACCUM_WEIGHTS( 5, 10, 24); ACCUM_WEIGHTS(10,  4, 24);
    ACCUM_WEIGHTS( 4, 16, 24); ACCUM_WEIGHTS(16,  0, 24);

    ACCUM_WEIGHTS( 2,  9, 25); ACCUM_WEIGHTS( 9,  3, 25);
    ACCUM_WEIGHTS( 3, 19, 25); ACCUM_WEIGHTS(19,  7, 25);
    ACCUM_WEIGHTS( 7, 11, 25); ACCUM_WEIGHTS(11,  6, 25);
    ACCUM_WEIGHTS( 6, 18, 25); ACCUM_WEIGHTS(18,  2, 25);
    */

    const float power = 2.0f;

    for (int i = 0; i < 26; ++i)
    {
        // w = 1 / (distance^k)
        float w = rcp(pow(max(lengths[i], EPS), power));
    
        num += w * gDiff[i];
        denom += w;
    }
    float3 delta = num * rcp(max(denom, EPS));

    // ---- sample SDF & write -----------------------------------------------
    float3 posSample = posLocal + delta;
    float3 uvwS = (posSample - brush.aabbmin.xyz) /
                  (brush.aabbmax.xyz - brush.aabbmin.xyz);
    float3 texel = uvwS * brush.resolution;

    float sdf = DEFUALT_EMPTY_SPACE;
    if (any(texel < 0.0f) || any(texel >= brush.resolution))
    {

        sdf = DEFUALT_EMPTY_SPACE;
    }
    else
    {
        // The coordinate is valid, so perform the read.
        sdf = Read3D(brush.textureID, int3(texel)).x;
    }
    
    Write3DDist(brush.textureID2, voxelCoord, sdf);
}


/*
void DeformBrush(uint3 DTid : SV_DispatchThreadID)
{
    float3 controlPoints[8];

    const int chunkSize = DEFORMATION_CHUNK;
    int3 chunkOrigin = DTid * chunkSize;

    uint index = pc.triangleCount;
    Brush brush = Brushes[index];

    float scale = brush.aabbmax.w * 0.5f;

    // Identity cage corners in local [-1, 1] space
    controlPoints[0] = float3( 1,  1,  1);
    controlPoints[1] = float3(-1,  1,  1);
    controlPoints[2] = float3( 1, -1,  1);
    controlPoints[3] = float3(-1, -1,  1);
    controlPoints[4] = float3( 1,  1, -1);
    controlPoints[5] = float3(-1,  1, -1);
    controlPoints[6] = float3( 1, -1, -1);
    controlPoints[7] = float3(-1, -1, -1);

    for (int z = 0; z < chunkSize; ++z)
        for (int y = 0; y < chunkSize; ++y)
            for (int x = 0; x < chunkSize; ++x)
            {
                int3 voxelCoord = chunkOrigin + int3(x, y, z);
                if (all(voxelCoord < brush.resolution))
                {
                    float3 uvw = (float3(voxelCoord) + 0.5f) / brush.resolution;
                    float3 posLocal = uvw * 2.0f - 1.0f; // In [-1, 1]

                    float3 t = (posLocal + 1.0f) * 0.5f; // Map to [0,1] for interpolation

                    float3 p000 = lerp(controlPoints[0], controlPoints[1], t.x);
                    float3 p010 = lerp(controlPoints[2], controlPoints[3], t.x);
                    float3 p100 = lerp(controlPoints[4], controlPoints[5], t.x);
                    float3 p110 = lerp(controlPoints[6], controlPoints[7], t.x);

                    float3 p00 = lerp(p000, p010, t.y);
                    float3 p10 = lerp(p100, p110, t.y);

                    float3 p = lerp(p00, p10, t.z);


                    // Apply scale and center to get world space position
                    float3 posWS = p * scale + brush.center.xyz;

                    float3 sampleUVW = (posWS - brush.center.xyz) / scale;
                    sampleUVW = (sampleUVW + 1.0f) * 0.5f;
                    float3 sampleCoords = sampleUVW * brush.resolution;

                    float sdf = Read3D(brush.textureID, sampleCoords);
                    Write3D(brush.textureID2, voxelCoord, sdf);
                }
            }
}
*/

/*
void DeformBrush(uint3 DTid : SV_DispatchThreadID)
{
    const int chunkSize = DEFORMATION_CHUNK;
    int3 chunkOrigin = DTid * chunkSize;

    uint index = pc.triangleCount;
    Brush brush = Brushes[index];

    float scale = brush.aabbmax.w * 0.5f;

    float3 controlPoint = brush.center.xyz + float3(0, 0, scale * 0.5f); // above the shape
    float bulgeRadius = scale * 1.21f;
    float bulgeAmount = scale * 0.1f; // sin(time * 0.01);

    // Process 8x8x8 block
    for (int z = 0; z < chunkSize; ++z)
        for (int y = 0; y < chunkSize; ++y)
            for (int x = 0; x < chunkSize; ++x)
            {
                int3 voxelCoord = chunkOrigin + int3(x, y, z);
                if (all(voxelCoord < brush.resolution))
                {
                    // Convert voxel to world space
                    float3 uvw = (float3(voxelCoord) + 0.5f) / brush.resolution;
                    float3 posWS = (uvw * 2.0f - 1.0f) * scale + brush.center.xyz;

                    // Inverse bulge deformation
                    float3 dir = posWS - controlPoint;

                    float3 toControl = controlPoint - posWS;
                    float dist = length(toControl);
                    if (dist < bulgeRadius && dist > 1e-5f)
                    {
                        float t = saturate(1.0f - dist / bulgeRadius);
                        float falloff = t * t * t * (t * (t * 6 - 15) + 10); // smootherstep
                        float3 offset = normalize(toControl) * (bulgeAmount * falloff);
                        posWS -= offset;
                    }

                    // Sample original SDF at inverse-deformed position
                    float3 sampleUVW = (posWS - brush.center.xyz) / scale;
                    sampleUVW = (sampleUVW + 1.0f) * 0.5f;
                    float3 sampleCoords = sampleUVW * brush.resolution;

                    float sdf = Read3D(brush.textureID, sampleCoords);
                    Write3D(brush.textureID2, voxelCoord, sdf);
                }
            }
}
*/

float GetSolidAngle(float3 p, float3 a, float3 b, float3 c)
{
    float3 va = a - p;
    float3 vb = b - p;
    float3 vc = c - p;

    float len_a = length(va);
    float len_b = length(vb);
    float len_c = length(vc);

    if (len_a < 1e-6 || len_b < 1e-6 || len_c < 1e-6)
    {
        return 0.0f;
    }

    float triple_product = dot(normalize(va), cross(normalize(vb), normalize(vc)));

    float denominator = 1.0f + dot(normalize(va), normalize(vb)) + dot(normalize(va), normalize(vc)) + dot(normalize(vb), normalize(vc));

    return 2.0f * atan2(triple_product, denominator);
}

void CreateBrush(uint3 DTid : SV_DispatchThreadID)
{
    uint index = pc.triangleCount;
    Brush brush = Brushes[index];

    float3 minBounds, maxBounds;
    float3 extent = getAABB(brush.vertexOffset, brush.vertexCount, minBounds, maxBounds, brush);
    float3 center = (minBounds + maxBounds) * 0.5f;
    float maxExtent = max(extent.x, max(extent.y, extent.z));

    Brushes[index].aabbmax.w = maxExtent;
    Brushes[index].center.xyz = center;
    Brushes[index].isDirty = 1;
    Brushes[index].invModel = inverse(Brushes[index].model);
    Brushes[index].aabbmax.xyz = maxBounds;
    Brushes[index].aabbmin.xyz = minBounds;

    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution;
    float3 samplePos = uvw * 2.0f - 1.0f;
    float3 localPos = samplePos * (maxExtent * 0.5f) + center;

    float minDist = DEFUALT_EMPTY_SPACE;
    float windingSum = 0.0f;

    for (uint i = brush.vertexOffset; i < brush.vertexOffset + brush.vertexCount; i += 3)
    {
        uint3 idx = uint3(i, i + 1, i + 2);
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;

        minDist = min(minDist, DistanceToTriangle(localPos, a, b, c));
        windingSum += GetSolidAngle(localPos, a, b, c);
    }

    if (abs(windingSum) > 2.0f * 3.14159265f)
    {
        minDist = -minDist;
    }

    float sdf = minDist;
    
    float3 posLocal = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, uvw);
    float3 uvwS = (posLocal - brush.aabbmin.xyz) / (brush.aabbmax.xyz - brush.aabbmin.xyz);
    float3 texel = uvwS * brush.resolution;
    if (any(texel < 0.0f) || any(texel >= brush.resolution))
    {
        sdf = DEFUALT_EMPTY_SPACE;
    }

    Write3D(brush.textureID, int3(DTid), float2(sdf, NO_LABELF()));
    Write3D(brush.textureID2, int3(DTid), float2(sdf, NO_LABELF()));

    for (int i = 0; i < CAGE_VERTS; i++)
    {
        controlParticlesL1Out[index * CAGE_VERTS + i].position = float4(canonicalControlPoints[i].xyz, 0);
    }
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

    return min(u, voxelSize * 4.0f);
}

void WriteToWorldSDF(uint3 DTid : SV_DispatchThreadID)
{
    
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(1.0f);
    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;
    
    float3 center = ((float3) DTid + 0.5f) * voxelSize - halfScene;

    float3 voxelMin = center - voxelSize * 0.5f;
    float3 voxelMax = center + voxelSize * 0.5f;

    
    float minDist = DEFUALT_EMPTY_SPACE;
    int minId = 0;
    
    float tileWorldSize = TILE_SIZE * voxelSize;
    int numTilesPerAxis = WORLD_SDF_RESOLUTION / TILE_SIZE;

    int3 tileCoord = floor((center + halfScene) / tileWorldSize);

    tileCoord = clamp(tileCoord, int3(0, 0, 0), int3(numTilesPerAxis - 1, numTilesPerAxis - 1, numTilesPerAxis - 1));

    uint tileIndex = Flatten3D(tileCoord, numTilesPerAxis);
    

    //Find the distance field closes to this voxel.
    float blendFactor = 0;
    float smoothness = 10;
    uint brushCount = TileBrushCounts[tileIndex];
    if(brushCount == 0)
    {
        return;
    }

    for (uint i = 0; i < brushCount; i++)
    {
        uint offset = tileIndex * TILE_MAX_BRUSHES + i;
        uint index = BrushesIndices[offset];
        
        Brush brush = Brushes[index];
        

        float d = Read3DTransformed(brush, center).x;



        if (brush.opcode == 1)
            minDist = max(-d + 1, minDist);
        else if (brush.opcode == 0)
        {
            blendFactor += brush.blend;
            //This brush actually gets added.
            if(minDist > d)
            {
                minId = brush.id;

            }

            minDist = smin(minDist, d, blendFactor + 0.0001f);
            smoothness = smin(smoothness, brush.smoothness, blendFactor + 0.01f);

        }
    }
    
    int3 DTL1 = DTid / 2;
    float2 voxelSceneBoundsl1 = GetVoxelResolution(0.0f);
    voxelsL1Out[Flatten3DR(DTL1, voxelSceneBoundsl1.x)].brushId = minId;
    voxelsL1Out[Flatten3DR(DTL1, voxelSceneBoundsl1.x)].normalDistance.w = blendFactor;
    voxelsL1Out[Flatten3DR(DTL1, voxelSceneBoundsl1.x)].normalDistance.x = smoothness;

    Write3DDist(0, DTid, minDist);
}

void GenerateMIPS(uint3 DTid : SV_DispatchThreadID, float sampleLevel)
{
    /*
    //level.
    float sampleLevelL = pc.lod - 1;
    float powerLevel = pow(2, sampleLevelL);
    
    //Get the original value at the previous MIP level.
    
    int3 dtScaledId = DTid * powerLevel;
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(1.0f);
    
    voxelSceneBounds.x /= pow(2, sampleLevelL);
    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;
    
    float3 center = ((float3) dtScaledId + 0.5f) * voxelSize - halfScene;
    
    float2 sdfValue = Read3DMip(9, center);
*/
    uint sourceMipLevel = pc.lod - 1;
    int3 sourceBaseCoord = int3(DTid) * 2;
    
    float totalSDF = 0.0f;
    
    // Sample the 2x2x2 block from the source (finer) mip level
    [unroll]
    for (int z = 0; z < 2; ++z)
    {
        [unroll]
        for (int y = 0; y < 2; ++y)
        {
            [unroll]
            for (int x = 0; x < 2; ++x)
            {
                int3 sampleCoord = sourceBaseCoord + int3(x, y, z);
                totalSDF += Read3D(sourceMipLevel -1, sourceBaseCoord).x;
            }
        }
    }
    
    // Average the 8 samples
    float averageSDF = totalSDF / 8.0f;

    Write3D(sampleLevel - 1, DTid, float2(averageSDF, NO_LABELF()));
    
}

//Block-Based Union-Find
void InitLabels(uint3 DTid : SV_DispatchThreadID)
{
    float2 voxelSceneBounds = GetVoxelResolution(0.0f);
    uint3 blockCoord = DTid;
    uint3 blockOrigin = blockCoord * 2;

    // Scan entire 2×2×2 block
    bool hasFG = false;
    [unroll]
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
                hasFG |= Read3D(0, blockOrigin + int3(x, y, z)).x < 0.1f;

    float2 p = Read3D(0, blockOrigin);

    if (hasFG)                                  
    {
        uint width = WORLD_SDF_RESOLUTION / 2;
        uint height = width;
        uint label = blockCoord.x +
                      blockCoord.y * width +
                      blockCoord.z * width * height; // dense 0-based
        p.y = (float)(label);
    }
    else
    {
        p.y = NO_LABELF(); // sentinel (bit-pattern NaN)
    }

    voxelsL1Out[Flatten3DR(blockCoord, voxelSceneBounds.x)].id = p.y; //Write3D(0, blockOrigin, p);


}

int3 LabelToVoxel(float label)
{
    // Assuming you assigned labels in block-raster order during InitLabels
    // label = x + y * width + z * width * height
    float width = WORLD_SDF_RESOLUTION / 2;
    float height = width;

    float z = label / (width * height);
    float y = (label / width) % height;
    float x = label % width;

    return int3(x, y, z) * 2; // convert block ID to voxel coordinates
}

int3 LabelToVoxelR(float label)
{
    // Assuming you assigned labels in block-raster order during InitLabels
    // label = x + y * width + z * width * height
    float width = WORLD_SDF_RESOLUTION / 2;
    float height = width;

    float z = label / (width * height);
    float y = (label / width) % height;
    float x = label % width;

    return int3(x, y, z);
}

void SetLabel(uint oldLabel, uint newParent)
{
    float2 voxelSceneBounds = GetVoxelResolution(0.0f);
    int3 labelVoxel = LabelToVoxelR(oldLabel);
    float val = (float)(newParent);
    voxelsL1Out[Flatten3DR(labelVoxel, voxelSceneBounds.x)].id = val; //Write3D(0, labelVoxel, val);

}


// Find the root label for a voxel by following the label chain
uint Find(int3 voxel)
{
    float2 voxelSceneBounds = GetVoxelResolution(0.0f);
    float lbl = voxelsL1Out[Flatten3DR(voxel, voxelSceneBounds.x)].id; //Read3D(0, voxel).y;
    if (lbl == NO_LABELF())
        return NO_LABELF(); // background

    for (uint iter = 0; iter < 128; ++iter) 
    {
        float parent = voxelsL1Out[Flatten3DR(LabelToVoxelR(lbl), voxelSceneBounds.x)].id; //Read3D(0, LabelToVoxel(lbl)).y;
        if (parent == lbl || parent == NO_LABELF())
            return lbl; // root found
        lbl = parent;
    }
    return lbl; 
}


void Union(int3 voxelA, int3 voxelB)
{
    uint rootA = Find(voxelA);
    uint rootB = Find(voxelB);

    if (rootA == rootB)
        return;

    if (rootA < rootB)
        SetLabel(rootB, rootA);
    else
        SetLabel(rootA, rootB);
}


void MergeLabels(uint3 DTid : SV_DispatchThreadID)
{
    //Work on 2x2x2 block.
    uint3 blockCoord = DTid;
    uint3 blockOrigin = blockCoord * 2;
    
    //Exit early if entire block is background.
    bool anyForeground = false;
    [unroll]
    for (int z = 0; z < 2; ++z)
    for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 2; ++x)
    {
        int3 voxel = blockOrigin + int3(x, y, z);
        if (Read3D(0, voxel).x < 0.1f)
            anyForeground = true;
    }

    if (!anyForeground)
        return;


    int3 offsets[13] =
    {
        { -1, 0, 0 },
        { 0, -1, 0 },
        { 0, 0, -1 },
        { -1, -1, 0 },
        { -1, 0, -1 },
        { 0, -1, -1 },
        { -1, -1, -1 },
        { -1, 1, 0 },
        { -1, 0, 1 },
        { 0, -1, 1 },
        { 0, 1, -1 },
        { -1, 1, 1 },
        { -1, -1, 1 }
    };

    
    //Check neighbors.
    for (int i = 0; i < 13; i++)
    {
        int3 neighborBlock = blockCoord + offsets[i];
        int3 heightWidth = int3(WORLD_SDF_RESOLUTION/2, WORLD_SDF_RESOLUTION/2, WORLD_SDF_RESOLUTION/2);
        if (all(neighborBlock >= 0))
        {
            int3 neighborVoxel = neighborBlock * 2;

            bool3 inside = (neighborBlock < heightWidth);
            if (all(inside))
            {
                int3 neighborVoxel = neighborBlock * 2;
                if (Read3D(0, neighborVoxel).x < 0.1f)
                    Union(blockCoord, neighborBlock);
            }

        }
    }
}

void FlattenLabels(uint3 DTid : SV_DispatchThreadID)
{
    float2 voxelSceneBounds = GetVoxelResolution(0.0f);
    uint3 blockCoord = DTid;
    uint3 blockOrigin = blockCoord * 2;

    uint root = Find(blockCoord);
    float2 origin = Read3D(0, blockOrigin);
    origin.y = (float) root;
    
    float label = origin.y;
    int3 voxel = LabelToVoxelR(label);
    uint flatIndex = Flatten3DR(voxel, voxelSceneBounds.x);

    // If the ID is 0, a race to initialize it begins.
    if (voxelsL1Out[flatIndex].uniqueId == 0)
    {
        int valueBeforeWrite;
        InterlockedCompareExchange(voxelsL1Out[flatIndex].uniqueId, 0, 9999, valueBeforeWrite);

        // If the value was 0 before we tried to write, we are the WINNER.
        if (valueBeforeWrite == 0)
        {
            int reservedId;
            InterlockedAdd(GlobalIDCounter[0], 1, reservedId);
            
            voxelsL1Out[flatIndex].uniqueId = reservedId + 1;
            
            //Brushes[voxelsL1Out[flatIndex].uniqueId].opcode = 0;
        }
    }
    
    origin.y = (float) voxelsL1Out[flatIndex].uniqueId;
    
    Write3D(0, blockOrigin, origin);
}

void BroadcastLabels(uint3 DTid : SV_DispatchThreadID)
{
    int3 blockOrigin = DTid * 2;
    float2 v = Read3D(0, blockOrigin);
    
    float2 voxelSceneBounds = GetVoxelResolution(0.0f);
    float label = v.y;
    int3 voxel = DTid; ///LabelToVoxelR(label);
    
    uint brushID = voxelsL1Out[Flatten3DR(voxel, voxelSceneBounds.x)].brushId;
    uint labelID = asuint(label);
    uint finalID = (brushID << 24) | (labelID & 0xFFFFFF);
    
    float encodedFinalID = asfloat(finalID);
    
    
    v.y = encodedFinalID; // * MAX_BRUSHES;
    
    [unroll]
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x)
            {
                int3 voxel = blockOrigin + int3(x, y, z);
                if (v.x < 0.1f)
                {
                    Write3DID(0, voxel, v.y);
                }
            }
    Write3DID(0, blockOrigin, v.y);
}

//Cook changes into brushes.
void CookBrush(uint3 DTid : SV_DispatchThreadID)
{
    //Read the world SDF using the current brush coordinates.
    
    uint index = pc.triangleCount;
    Brush brush = Brushes[index];

    //Get localized position.
    // Get actual AABB from vertices
    float3 minBounds = brush.aabbmin.xyz;
    float3 maxBounds = brush.aabbmax.xyz;
    float3 center = brush.center.xyz;
    float maxExtent = brush.aabbmax.w;

    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution;
    float3 localPos = lerp(minBounds, maxBounds, uvw);
    
    //Get world position.
    float3 worldPos = mul(brush.model, float4(localPos, 1.0f)).xyz;


    
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(1.0f);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;
    
    float3 worldUVW = (worldPos + halfScene) / (2.0f * halfScene);
    
    if (any(worldUVW < 0.0f) || any(worldUVW > 1.0f))
    {
        return;
    }
    
    int3 worldSDFCoords = int3(worldUVW * (WORLD_SDF_RESOLUTION - 1));
    
    float2 sdfValue = Read3D(0, worldSDFCoords);
    
    //set this brush id.
    //InterlockedMin(Brushes[index].id, id);
    
    //Only process cuts.
    if(sdfValue.x > 0.1f)
        Write3DDist(brush.textureID, DTid, sdfValue.x);

}

float SampleSDF(float3 worldPos, int mipLevel)
{
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(mipLevel + 1);
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert world position to continuous texel coordinates
    float3 uvw = (worldPos + halfScene) / (2.0f * halfScene);
    float3 texelCoord = uvw * voxelSceneBounds.x;

    // Get the integer base coordinate and the fractional part for interpolation
    int3 baseTexel = int3(floor(texelCoord));
    float3 frac = texelCoord - baseTexel;

    // Fetch the 8 corner SDF values
    float d000 = Read3D(mipLevel, baseTexel + int3(0, 0, 0)).x;
    float d100 = Read3D(mipLevel, baseTexel + int3(1, 0, 0)).x;
    float d010 = Read3D(mipLevel, baseTexel + int3(0, 1, 0)).x;
    float d110 = Read3D(mipLevel, baseTexel + int3(1, 1, 0)).x;
    float d001 = Read3D(mipLevel, baseTexel + int3(0, 0, 1)).x;
    float d101 = Read3D(mipLevel, baseTexel + int3(1, 0, 1)).x;
    float d011 = Read3D(mipLevel, baseTexel + int3(0, 1, 1)).x;
    float d111 = Read3D(mipLevel, baseTexel + int3(1, 1, 1)).x;

    // Perform trilinear interpolation (7 lerps)
    float c00 = lerp(d000, d100, frac.x);
    float c10 = lerp(d010, d110, frac.x);
    float c01 = lerp(d001, d101, frac.x);
    float c11 = lerp(d011, d111, frac.x);
    float c0 = lerp(c00, c10, frac.y);
    float c1 = lerp(c01, c11, frac.y);
    return lerp(c0, c1, frac.z);
}


float3 GetNormal(float3 worldPos)
{
    const int mipLevel = 0;
    
    const float epsilon = 0.1f;

    // Use small offset vectors for sampling along each axis.
    float3 offsetX = float3(epsilon, 0, 0);
    float3 offsetY = float3(0, epsilon, 0);
    float3 offsetZ = float3(0, 0, epsilon);

    // Calculate the gradient using central differences on the interpolated SDF values.
    float dx = SampleSDF(worldPos + offsetX, mipLevel) - SampleSDF(worldPos - offsetX, mipLevel);
    float dy = SampleSDF(worldPos + offsetY, mipLevel) - SampleSDF(worldPos - offsetY, mipLevel);
    float dz = SampleSDF(worldPos + offsetZ, mipLevel) - SampleSDF(worldPos - offsetZ, mipLevel);
    
    // The gradient vector points in the direction of the normal.
    // Add a tiny value to prevent normalization of a zero vector.
    return normalize(float3(dx, dy, dz) + 1e-6);
}

float3 InterpolateVertexPos(float3 p1, float3 p2, float d1, float d2)
{
    if (abs(d1 - d2) < 0.00001f)
    {
        return p1;
    }
    float t = d1 / (d1 - d2);
    return lerp(p1, p2, t);
}


void FindActiveCells(uint3 DTid : SV_DispatchThreadID)
{
    //Need early exit strategies here.
    
    //Reduce to 64^3 by loading this voxel and load surrounding neighbors.
    uint index = pc.triangleCount;
    Brush brush = Brushes[index];
    
    //Get voxel to be used as main edge.
    float3 minBounds = brush.aabbmin.xyz;
    float3 maxBounds = brush.aabbmax.xyz;
    float3 center = brush.center.xyz;
    float maxExtent = brush.aabbmax.w;

    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution;
    float3 minLocal = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, uvw);
    
    //Read the second mip map.
    int mipLevel = 1;
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(mipLevel+1);//It substracts by 1 inside function.
    float halfScene = voxelSceneBounds.y * 0.5f;
    

    
    //Fetch the 8 corner values in the WORLD SDF. Because local brush does not contain deformation.
    //Integer coords of min corner texel
    float3 worldMin = mul(brush.model, float4(minLocal, 1)).xyz;
    float3 worldUVW0 = (worldMin + halfScene) / (2.0f * halfScene);
    int3 baseTexel = int3(worldUVW0 * voxelSceneBounds.x);
    
    // Fetch 8 SDF values from mip level 1
    float2 sdf000 = Read3D(mipLevel, baseTexel + int3(0, 0, 0));
    float2 sdf100 = Read3D(mipLevel, baseTexel + int3(1, 0, 0));
    float2 sdf010 = Read3D(mipLevel, baseTexel + int3(0, 1, 0));
    float2 sdf110 = Read3D(mipLevel, baseTexel + int3(1, 1, 0));
    float2 sdf001 = Read3D(mipLevel, baseTexel + int3(0, 0, 1));
    float2 sdf101 = Read3D(mipLevel, baseTexel + int3(1, 0, 1));
    float2 sdf011 = Read3D(mipLevel, baseTexel + int3(0, 1, 1));
    float2 sdf111 = Read3D(mipLevel, baseTexel + int3(1, 1, 1));
    float d000 = sdf000.x;
    float d100 = sdf100.x;
    float d010 = sdf010.x;
    float d110 = sdf110.x;
    float d001 = sdf001.x;
    float d101 = sdf101.x;
    float d011 = sdf011.x;
    float d111 = sdf111.x;

    // Active cell check
    float minVal = min(d000, min(d100, min(d010, min(d110, min(d001, min(d101, min(d011, d111)))))));
    float maxVal = max(d000, max(d100, max(d010, max(d110, max(d001, max(d101, max(d011, d111)))))));
    
    //Need to convert find the active cell for the voxelsBuffer which is lower resolution.
    //Currently the main one is baseTexel, convert that to voxel buffer index.
    int3 baseTexelForBuffer = baseTexel / pow(2, mipLevel - 1); //just lower it one resolution, 256 -> 128.
    uint flatIndex = Flatten3DR(baseTexelForBuffer, VOXEL_RESOLUTIONL1);
    
    if (!(minVal < 0.0f && maxVal > 0.0f))
    {
        return;
    }
    
    //Set Active cell.
    voxelsL1Out[flatIndex].normalDistance.y = 1;
}


float3 CalculateDualSurfaceNet(int3 cellCoord, float mipLevel)
{
    Brush brush = Brushes[0];
    float extent = brush.aabbmax.w;
    // Use the correct mip level passed into the function
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(mipLevel + 1);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    // CORRECTED: halfScene is half the total world size
    float halfScene = voxelSceneBounds.y * 0.5f;
    
    // Calculate the world-space position of the cell's center
    float3 worldPos = ((float3(cellCoord * mipLevel) + 0.5f) * voxelSize) - halfScene;
    
    float3 inversePosition = mul(brush.invModel, float4(worldPos, 1.0f)).xyz;
    return inversePosition;
}

float3 CalculateDualVertexGradient(int3 cellCoord, float mipLevel)
{
    //Get basic values of this scene.
    Brush brush = Brushes[0];
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(mipLevel + 1);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    //Need edges so we can average out particle among all edges.
    int edges[12][2] =
    {
        { 0, 1 },
        { 1, 3 },
        { 3, 2 },
        { 2, 0 }, // Bottom face
        { 4, 5 },
        { 5, 7 },
        { 7, 6 },
        { 6, 4 }, // Top face
        { 0, 4 },
        { 1, 5 },
        { 2, 6 },
        { 3, 7 } // Vertical edges
    };
    
    float3 cornerPositions[8];
    float cornerSDFs[8];
    for (int i = 0; i < 8; ++i)
    {
        // Get corner's integer offset (0,0,0), (1,0,0), (0,1,0), etc.
        int3 cornerOffset = int3((i & 1), (i & 2) >> 1, (i & 4) >> 2);
        int3 cornerTexel = (cellCoord * mipLevel) + cornerOffset;
        
        cornerPositions[i] = ((float3(cornerTexel)) * voxelSize) - halfScene;
        cornerSDFs[i] = Read3D(mipLevel, cornerTexel).x;
    }

    float3 averagePos = float3(0, 0, 0);
    int intersectionCount = 0;


    for (int i = 0; i < 12; i++)
    {
        int c1_idx = edges[i][0];
        int c2_idx = edges[i][1];

        float d1 = cornerSDFs[c1_idx];
        float d2 = cornerSDFs[c2_idx];

        // Check if the surface crosses this edge
        if ((d1 < 0) != (d2 < 0))
        {
            float3 p1 = cornerPositions[c1_idx];
            float3 p2 = cornerPositions[c2_idx];
            
            // Find the intersection point and add it to our running total
            averagePos += InterpolateVertexPos(p1, p2, d1, d2);
            intersectionCount++;
        }
    }

    //Initial Guess done.
    float3 massParticle = averagePos / intersectionCount;
    
    //Move it along its gradient.
    float3 particlePos = massParticle;
    const int iterations = 16;
    const float stepSize = 0.15f;

    [loop]
    for (int j = 0; j < iterations; ++j)
    {
        // Calculate the SDF value and normal at the particle's current position.
        float3 uvw = (particlePos + halfScene) / (2.0f * halfScene);
        int3 baseTexel = int3(uvw * voxelSceneBounds.x);
        float sdfAtParticle = Read3D(mipLevel, baseTexel).x;
        float3 normalAtParticle = GetNormal(particlePos);

        float3 force = -sdfAtParticle * normalAtParticle;

        // Move the particle along the force vector (Euler integration).
        particlePos += force * stepSize;
    }


    //Transform the final world-space vertex back into the brush's local space
    return mul(brush.invModel, float4(particlePos, 1.0f)).xyz;

}

float3 CalculateDualVertexCentroid(int3 cellCoord, float mipLevel)
{
    //Get basic values of this scene.
    Brush brush = Brushes[0];
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(mipLevel + 1);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    //Need edges so we can average out particle among all edges.
    int edges[12][2] =
    {
        { 0, 1 },
        { 1, 3 },
        { 3, 2 },
        { 2, 0 }, // Bottom face
        { 4, 5 },
        { 5, 7 },
        { 7, 6 },
        { 6, 4 }, // Top face
        { 0, 4 },
        { 1, 5 },
        { 2, 6 },
        { 3, 7 } // Vertical edges
    };
    
    float3 cornerPositions[8];
    float cornerSDFs[8];
    for (int i = 0; i < 8; ++i)
    {
        // Get corner's integer offset (0,0,0), (1,0,0), (0,1,0), etc.
        int3 cornerOffset = int3((i & 1), (i & 2) >> 1, (i & 4) >> 2);
        int3 cornerTexel = (cellCoord * mipLevel) + cornerOffset;
        
        cornerPositions[i] = ((float3(cornerTexel)) * voxelSize) - halfScene;
        cornerSDFs[i] = Read3D(mipLevel, cornerTexel).x;
    }

    float3 averagePos = float3(0, 0, 0);
    int intersectionCount = 0;


    for (int i = 0; i < 12; i++)
    {
        int c1_idx = edges[i][0];
        int c2_idx = edges[i][1];

        float d1 = cornerSDFs[c1_idx];
        float d2 = cornerSDFs[c2_idx];

        // Check if the surface crosses this edge
        if ((d1 < 0) != (d2 < 0))
        {
            float3 p1 = cornerPositions[c1_idx];
            float3 p2 = cornerPositions[c2_idx];
            
            // Find the intersection point and add it to our running total
            averagePos += InterpolateVertexPos(p1, p2, d1, d2);
            intersectionCount++;
        }
    }
    float3 massParticle = averagePos / intersectionCount;

    //Transform the final world-space vertex back into the brush's local space
    return mul(brush.invModel, float4(massParticle, 1.0f)).xyz;

}


float3 CalculateDualContour(int3 cellCoord, float mipLevel)
{
    return CalculateDualVertexCentroid(cellCoord, mipLevel);
}


void DualContour(uint3 DTid : SV_DispatchThreadID)
{
    Brush brush = Brushes[0];
    int index = Flatten3DR(DTid, VOXEL_RESOLUTIONL1);
    float activeValue = voxelsL1Out[index].normalDistance.y;
    int mipLevel = 1;
    int3 baseTexel = DTid * pow(2, mipLevel - 1); //128 -> 256.
    
    float sdfOrigin = Read3D(mipLevel, baseTexel).x;
    
    if(activeValue < 0.001f)
    {
        return;
    }

    //Check Every single face.
    int offSet = mipLevel;
    
    // Check edge along +X axis
    int3 xNeighbor = DTid * offSet + int3(offSet, 0, 0);
    float sdfx = Read3D(mipLevel, xNeighbor).x;
    if ((sdfOrigin < 0) != (sdfx < 0))
    {
        // Surface crosses this edge. Generate a quad.
        // The quad is formed by the dual vertices of the 4 cells sharing this edge.
        float3 v0 = CalculateDualContour(DTid + int3(0, 0, 0), mipLevel);
        float3 v1 = CalculateDualContour(DTid + int3(0, -1, 0), mipLevel);
        float3 v2 = CalculateDualContour(DTid + int3(0, -1, -1), mipLevel);
        float3 v3 = CalculateDualContour(DTid + int3(0, 0, -1), mipLevel);
        
        float3 n0 = GetNormal(mul(brush.model, float4(v0, 1)).xyz);
        float3 n1 = GetNormal(mul(brush.model, float4(v1, 1)).xyz);
        float3 n2 = GetNormal(mul(brush.model, float4(v2, 1)).xyz);
        float3 n3 = GetNormal(mul(brush.model, float4(v3, 1)).xyz);

        // Reserve space for 6 vertices (2 triangles)
        uint vertOffset;
        InterlockedAdd(GlobalIDCounter[1], 6, vertOffset);


        // Write 6 vertices for the two triangles
        meshingVertices[vertOffset + 0].position = float4(v0, 1);
        meshingVertices[vertOffset + 1].position = float4(v1, 1);
        meshingVertices[vertOffset + 2].position = float4(v2, 1);
    
        meshingVertices[vertOffset + 3].position = float4(v0, 1);
        meshingVertices[vertOffset + 4].position = float4(v2, 1); 
        meshingVertices[vertOffset + 5].position = float4(v3, 1); 
        
        meshingVertices[vertOffset + 0].normal = float4(n0, 1);
        meshingVertices[vertOffset + 1].normal = float4(n1, 1);
        meshingVertices[vertOffset + 2].normal = float4(n2, 1);
        
        meshingVertices[vertOffset + 3].normal = float4(n0, 1);
        meshingVertices[vertOffset + 4].normal = float4(n2, 1);
        meshingVertices[vertOffset + 5].normal = float4(n3, 1);

    }

    // Check edge along +Y axis
    int3 yNeighbor = DTid * offSet + int3(0, offSet, 0);
    float sdfY = Read3D(mipLevel, yNeighbor).x;
    if ((sdfOrigin < 0) != (sdfY < 0))
    {
        float3 v0 = CalculateDualContour(DTid + int3(0, 0, 0), mipLevel);
        float3 v1 = CalculateDualContour(DTid + int3(0, 0, -1), mipLevel);
        float3 v2 = CalculateDualContour(DTid + int3(-1, 0, -1), mipLevel);
        float3 v3 = CalculateDualContour(DTid + int3(-1, 0, 0), mipLevel);
        
        float3 n0 = GetNormal(mul(brush.model, float4(v0, 1)).xyz);
        float3 n1 = GetNormal(mul(brush.model, float4(v1, 1)).xyz);
        float3 n2 = GetNormal(mul(brush.model, float4(v2, 1)).xyz);
        float3 n3 = GetNormal(mul(brush.model, float4(v3, 1)).xyz);

        // Reserve space for 6 vertices (2 triangles)
        uint vertOffset;
        InterlockedAdd(GlobalIDCounter[1], 6, vertOffset);


        // Write 6 vertices for the two triangles
        meshingVertices[vertOffset + 0].position = float4(v0, 1);
        meshingVertices[vertOffset + 1].position = float4(v1, 1);
        meshingVertices[vertOffset + 2].position = float4(v2, 1);
    
        meshingVertices[vertOffset + 3].position = float4(v0, 1);
        meshingVertices[vertOffset + 4].position = float4(v2, 1);
        meshingVertices[vertOffset + 5].position = float4(v3, 1);
        
        meshingVertices[vertOffset + 0].normal = float4(n0, 1);
        meshingVertices[vertOffset + 1].normal = float4(n1, 1);
        meshingVertices[vertOffset + 2].normal = float4(n2, 1);
        
        meshingVertices[vertOffset + 3].normal = float4(n0, 1);
        meshingVertices[vertOffset + 4].normal = float4(n2, 1);
        meshingVertices[vertOffset + 5].normal = float4(n3, 1);
    }

    // Check edge along +Z axis
    int3 zNeighbor = DTid * offSet + int3(0, 0, offSet);
    float sdfZ = Read3D(mipLevel, zNeighbor).x;
    if ((sdfOrigin < 0) != (sdfZ < 0))
    {
        float3 v0 = CalculateDualContour(DTid + int3(0, 0, 0), mipLevel);
        float3 v1 = CalculateDualContour(DTid + int3(-1, 0, 0), mipLevel);
        float3 v2 = CalculateDualContour(DTid + int3(-1, -1, 0), mipLevel);
        float3 v3 = CalculateDualContour(DTid + int3(0, -1, 0), mipLevel);
        
        float3 n0 = GetNormal(mul(brush.model, float4(v0, 1)).xyz);
        float3 n1 = GetNormal(mul(brush.model, float4(v1, 1)).xyz);
        float3 n2 = GetNormal(mul(brush.model, float4(v2, 1)).xyz);
        float3 n3 = GetNormal(mul(brush.model, float4(v3, 1)).xyz);

        // Reserve space for 6 vertices (2 triangles)
        uint vertOffset;
        InterlockedAdd(GlobalIDCounter[1], 6, vertOffset);


        // Write 6 vertices for the two triangles
        meshingVertices[vertOffset + 0].position = float4(v0, 1);
        meshingVertices[vertOffset + 1].position = float4(v1, 1);
        meshingVertices[vertOffset + 2].position = float4(v2, 1);
    
        meshingVertices[vertOffset + 3].position = float4(v0, 1);
        meshingVertices[vertOffset + 4].position = float4(v2, 1);
        meshingVertices[vertOffset + 5].position = float4(v3, 1);
        
        meshingVertices[vertOffset + 0].normal = float4(n0, 1);
        meshingVertices[vertOffset + 1].normal = float4(n1, 1);
        meshingVertices[vertOffset + 2].normal = float4(n2, 1);
        
        meshingVertices[vertOffset + 3].normal = float4(n0, 1);
        meshingVertices[vertOffset + 4].normal = float4(n2, 1);
        meshingVertices[vertOffset + 5].normal = float4(n3, 1);
    }
}

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID, uint gIndex : SV_GroupIndex, uint3 lThreadID : SV_GroupThreadID)
{
    
    
    float sampleLevelL = pc.lod;
    uint triangleCount = pc.triangleCount;

    if (sampleLevelL == 0.0f)
    {
        InitVoxelData(DTid);
        return;
    }
    
    if(sampleLevelL == 1.0f)
    {
        GlobalIDCounter[1] = 0;
        WriteToWorldSDF(DTid);
        return;
    }
    
    if (sampleLevelL >= 2.0f && sampleLevelL < 8.0f)
    {
        GenerateMIPS(DTid, sampleLevelL);
        return;
    }
    
    if (sampleLevelL == 8.0f)
    {
        CreateBrush(DTid);
        return;
    }
    
    if (sampleLevelL == 14.0f)
    {
        DeformBrush(DTid, gIndex, lThreadID);
        return;
    }
    
    if(sampleLevelL == 20.0f)
    {
        InitLabels(DTid);
        return;
    }
    
    if (sampleLevelL == 21.0f)
    {
        MergeLabels(DTid);
        return;
    }
    
    if(sampleLevelL == 22.0f)
    {
        FlattenLabels(DTid);
        return;

    }
    
        
    if (sampleLevelL == 23.0f)
    {
        BroadcastLabels(DTid);
        return;

    }
    
    if (sampleLevelL == 24.0f)
    {
        ClearVoxelData(DTid);
        return;
    }
    
    if(sampleLevelL == 30.0f)
    {
        CookBrush(DTid);
        return;
    }
    
    //In the future, make this indirect dispatch for dirty brushes.
    if(sampleLevelL == 40.0f)
    {
        FindActiveCells(DTid);
        return;
    }
    
    if (sampleLevelL == 50.0f)
    {
        DualContour(DTid);
        return;
    }
    
    
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(sampleLevelL);
    uint3 gridSize = uint3(voxelSceneBounds.x, voxelSceneBounds.x, voxelSceneBounds.x);
    uint voxelIndex = DTid.x * gridSize.y * gridSize.z + DTid.y * gridSize.z + DTid.z;
        
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
        
        //float dl1Old = asfloat(voxelsL1In[voxelIndexL1].distance);
        
        //voxelsL1Out[voxelIndexL1].distance = asuint(min(dl1, dl1Old));

        return;
    }
    
    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert grid index to world-space center position. center = dtid * vs - hs => (center + hs) / vs = dtid
    float3 center = ((float3) DTid + 0.5f) * voxelSize - halfScene;

    float3 voxelMin = center - voxelSize * 0.5f;
    float3 voxelMax = center + voxelSize * 0.5f;

    
    float minDist = 64.0f;
    
    
    float tileWorldSize = TILE_SIZE * voxelSize;
    int numTilesPerAxis = WORLD_SDF_RESOLUTION / TILE_SIZE;

    int3 tileCoord = floor((center + halfScene) / tileWorldSize);

    tileCoord = clamp(tileCoord, int3(0, 0, 0), int3(numTilesPerAxis - 1, numTilesPerAxis - 1, numTilesPerAxis - 1));

    uint tileIndex = Flatten3D(tileCoord, numTilesPerAxis);
    
    //if(minDist > 63.0f)
        //return;

    if (sampleLevelL == 1.0f)
    {
        //Write3D(0, DTid, minDist);
        //voxelsL1Out[voxelIndex].distance = asuint(d);
        //voxelsL1Out[voxelIndex].normalDistance = float4(1, 0, 0, 1); //float4(normal, 0);
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