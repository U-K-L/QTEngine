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
    
    float voxelMini = 0.03125;
    minBounds -= voxelMini * 12;
    maxBounds += voxelMini * 12;
    
    return abs(maxBounds - minBounds);
}



float Read3DTransformed(in Brush brush, float3 worldPos)
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
        return 64.0f;
    
    return gBindless3D[brush.textureID2].Load(int4(voxelCoord, 0));
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
    Write3D(0, DTid, 0.03125 * 4.0f);
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
    const int chunkSize = DEFORMATION_CHUNK;
    int3 chunkOrigin = DTid * chunkSize;

    uint index = pc.triangleCount;
    Brush brush = Brushes[index];

    float scale = brush.aabbmax.w * 0.5f;

    float3 controlPoints[8] =
    {
        float3(1, 1, 1), // 0
        float3(-1, 1, 1), // 1
        float3(1, -1, 1), // 2
        float3(-1, -1, 1), // 3
        float3(1, 1, -1), // 4
        float3(-1, 1, -1), // 5
        float3(1, -1, -1), // 6
        float3(-1, -1, -1) // 7
    };
    
    float3 min = brush.aabbmin.xyz;
    float3 max = brush.aabbmax.xyz;

    controlPoints[0] = float3(max.x, max.y, max.z);
    controlPoints[1] = float3(min.x, max.y, max.z);
    controlPoints[2] = float3(max.x, min.y, max.z);
    controlPoints[3] = float3(min.x, min.y, max.z);
    controlPoints[4] = float3(max.x, max.y, min.z);
    controlPoints[5] = float3(min.x, max.y, min.z);
    controlPoints[6] = float3(max.x, min.y, min.z);
    controlPoints[7] = float3(min.x, min.y, min.z);
    



    static const int3 triangles[12] =
    {
        int3(0, 2, 1), int3(1, 2, 3), // Front
    int3(4, 5, 6), int3(5, 7, 6), // Back
    int3(0, 4, 2), int3(2, 4, 6), // Right
    int3(1, 3, 5), int3(3, 7, 5), // Left
    int3(0, 1, 4), int3(1, 5, 4), // Top
    int3(2, 6, 3), int3(3, 6, 7) // Bottom
    };

    // Define canonical cube control points in [-1, 1]
    float3 canonicalControlPoints[8] =
    {
        float3(1, 1, 1),
        float3(-1, 1, 1),
        float3(1, -1, 1),
        float3(-1, -1, 1),
        float3(1, 1, -1),
        float3(-1, 1, -1),
        float3(1, -1, -1),
        float3(-1, -1, -1)
    };


    //First past pre compute weights.
    for (int z = 0; z < chunkSize; ++z)
        for (int y = 0; y < chunkSize; ++y)
            for (int x = 0; x < chunkSize; ++x)
            {
                int3 voxelCoord = chunkOrigin + int3(x, y, z);
                if (all(voxelCoord < brush.resolution))
                {
                    float3 uvw = (float3(voxelCoord) + 0.5f) / brush.resolution;
                    float3 posLocal = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, uvw);

                    // Remap input voxel position into normalized cube space [-1, 1]
                    float3 posNormalized = (posLocal - brush.aabbmin.xyz) / (brush.aabbmax.xyz - brush.aabbmin.xyz);
                    posNormalized = posNormalized * 2.0f - 1.0f;



                    float3 p = posNormalized; // point in canonical space
                    float3 numerator = float3(0, 0, 0);
                    float denominator = 0;

                    float weights[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
                    
                    for (int i = 0; i < 12; ++i)
                    {
                        int3 tri = triangles[i];
                        float3 v0 = canonicalControlPoints[tri.x];
                        float3 v1 = canonicalControlPoints[tri.y];
                        float3 v2 = canonicalControlPoints[tri.z];

                        float3 r0 = v0 - p;
                        float3 r1 = v1 - p;
                        float3 r2 = v2 - p;


                        float l0 = length(r0);
                        float l1 = length(r1);
                        float l2 = length(r2);

                        float3 n0 = normalize(r0);
                        float3 n1 = normalize(r1);
                        float3 n2 = normalize(r2);

                        float alpha0 = acos(dot(n1, n2));
                        float alpha1 = acos(dot(n2, n0));
                        float alpha2 = acos(dot(n0, n1));

                        float w0 = (tan(alpha1 * 0.5f) + tan(alpha2 * 0.5f)) / l0;
                        float w1 = (tan(alpha2 * 0.5f) + tan(alpha0 * 0.5f)) / l1;
                        float w2 = (tan(alpha0 * 0.5f) + tan(alpha1 * 0.5f)) / l2;
                        
                        weights[tri.x] += w0;
                        weights[tri.y] += w1;
                        weights[tri.z] += w2;

                    }

                    for (int j = 0; j < 8; ++j)
                        denominator += weights[j];
                    
                    float invDen = rcp(denominator); // safe: denom > 0 inside the cage
                    [unroll]
                    for (int k = 0; k < 8; ++k)
                        weights[k] *= invDen;
                    
                    
                    float3 newControlPoints[8] =
                    {
                        float3(1, 1, 1),
                        float3(-1, 1, 1),
                        float3(1, -1, 1),
                        float3(-1, -1, 1),
                        float3(1, 1, -1),
                        float3(-1, 1, -1),
                        float3(1, -1, -1),
                        float3(-1, -1, -1)
                    };
                    
                    /*
                    //Breathing or melting a bit.
                    float a = abs(sin(time * 0.0006f));
                    for (int c = 0; c < 4; ++c)          // front face  (z = +1)
                        newControlPoints[c] += float3(0, 0, a);
                    for (int c = 4; c < 8; ++c)          // back face   (z = –1)
                        newControlPoints[c] -= float3(0, 0, a);
                    */
                    //Test identity
                    
                        float3 V0 = canonicalControlPoints[0];
                    float3 V1 = canonicalControlPoints[1];
                    float3 V2 = canonicalControlPoints[2];
                    float3 V3 = canonicalControlPoints[3];
                    float3 V4 = canonicalControlPoints[4];
                    float3 V5 = canonicalControlPoints[5];
                    float3 V6 = canonicalControlPoints[6];
                    float3 V7 = canonicalControlPoints[7];
                    
                    float3 V0n = newControlPoints[0];
                    float3 V1n = newControlPoints[1];
                    float3 V2n = newControlPoints[2];
                    float3 V3n = newControlPoints[3];
                    float3 V4n = newControlPoints[4];
                    float3 V5n = newControlPoints[5];
                    float3 V6n = newControlPoints[6];
                    float3 V7n = newControlPoints[7];
                    
                    float4 w0 = float4(0, 0, 0, 0);
                    w0.x = weights[0];
                    w0.y = weights[1];
                    w0.z = weights[2];
                    w0.w = weights[3];
                    
                    float4 w1 = float4(0, 0, 0, 0);
                    w1.x = weights[4];
                    w1.y = weights[5];
                    w1.z = weights[6];
                    w1.w = weights[7];

                    float3 delta =
                      w0.x * (V0n - V0) +
                      w0.y * (V1n - V1) +
                      w0.z * (V2n - V2) +
                      w0.w * (V3n - V3) +
                      w1.x * (V4n - V4) +
                      w1.y * (V5n - V5) +
                      w1.z * (V6n - V6) +
                      w1.w * (V7n - V7);

                    float3 posSample = posLocal + delta;
                    
                    float3 sampleUVW = (posSample - min) / (max - min);
                    float3 tex = sampleUVW * brush.resolution;
                    float sdf = Read3D(brush.textureID, tex);

                    Write3D(brush.textureID2, voxelCoord, sdf);

                }
            }
    
    

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

void CreateBrush(uint3 DTid : SV_DispatchThreadID)
{
    uint index = pc.triangleCount;
    
    Brush brush = Brushes[index];
    
    // Get actual AABB from vertices
    float3 minBounds, maxBounds; //The min and max bounds. For an object from [-8, 8] this is that value.
    float3 extent = getAABB(brush.vertexOffset, brush.vertexCount, minBounds, maxBounds); //The extent, which is 16 now.
    float3 center = (minBounds + maxBounds) * 0.5f; //Need to find the center, which should be 0.
    float maxExtent = max(extent.x, max(extent.y, extent.z)); //Finally the maximum size of the box, which is 16.
    
    Brushes[index].aabbmax.w = maxExtent;
    Brushes[index].center.xyz = center;
    Brushes[index].isDirty = 1;
    Brushes[index].invModel = inverse(Brushes[index].model);
    Brushes[index].aabbmax.xyz = maxBounds;
    Brushes[index].aabbmin.xyz = minBounds;


    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution; //This is the center of a voxel.
    float3 samplePos = uvw * 2.0f - 1.0f; // [-1, 1] in texture space
    
    // Transform from normalized [-1,1] space to vertex space
    float3 worldPos = samplePos * (maxExtent * 0.5f) + center;


    float minDist = 100.0f;
    
    float4 isHitChecks = 0;
    float4 isHitChecks2 = 0;

    for (uint i = brush.vertexOffset; i < brush.vertexOffset + brush.vertexCount; i += 3)
    {
        uint3 idx = uint3(i, i + 1, i + 2);
        float3 a = vertexBuffer[idx.x].position.xyz;
        float3 b = vertexBuffer[idx.y].position.xyz;
        float3 c = vertexBuffer[idx.z].position.xyz;

        // Distance to triangle
        float d = DistanceToTriangle(worldPos, a, b, c);
        minDist = min(minDist, d);
        
        //ray intersection test.
        //Ray origin is the voxel position
        float3 ro = worldPos;
        float t, u, v;
        
        //8 cardinal directions test. If at any
        /*
        bool isHit = 0;
        isHit = TriangleTrace(ro, float3(0, 0, 1), a, b, c, t,u,v);
        isHit = min(TriangleTrace(ro, float3(0, 1, 0), a, b, c, t, u, v), isHit);
        isHit = min(TriangleTrace(ro, float3(0, -1, 0), a, b, c, t, u, v), isHit);
        
        isHit = min(TriangleTrace(ro, float3(1, 0, 0), a, b, c, t, u, v), isHit);
        isHit = min(TriangleTrace(ro, float3(-1, 0, 0), a, b, c, t, u, v), isHit);
        
        isHit = min(TriangleTrace(ro, float3(0, 0, -1), a, b, c, t, u, v), isHit);
        isHit = min(TriangleTrace(ro, float3(1, 0, 1), a, b, c, t, u, v), isHit);
*/
        //random direction.
        
        isHitChecks.x += TriangleTrace(ro, float3(1, 0, 0), a, b, c, t, u, v);
        isHitChecks.z += TriangleTrace(ro, float3(0, 1, 0), a, b, c, t, u, v);
        isHitChecks.y += TriangleTrace(ro, float3(0, 0, 1), a, b, c, t, u, v);
        isHitChecks.w += TriangleTrace(ro, float3(1, 0, 1), a, b, c, t, u, v);
        
        isHitChecks2.x += TriangleTrace(ro, float3(-1, 0, 0), a, b, c, t, u, v);
        isHitChecks2.z += TriangleTrace(ro, float3(0, -1, 0), a, b, c, t, u, v);
        isHitChecks2.y += TriangleTrace(ro, float3(0, 0, -1), a, b, c, t, u, v);
        isHitChecks2.w += TriangleTrace(ro, float3(-1, 0, -1), a, b, c, t, u, v);

    }
    
    if (all(isHitChecks) > 0 && all(isHitChecks2) > 0)
        minDist = 0;

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

    return min(u, voxelSize * 4.0f);
}

void WriteToWorldSDF(uint3 DTid : SV_DispatchThreadID)
{
    
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(1.0f);
    
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
    

    //Find the distance field closes to this voxel.
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
                
        float d = Read3DTransformed(brush, center);

        //if(index == 1)
         //   minDist = max(-minDist, d);
        //else
        minDist = smin(minDist, d, 0);

    }
    
    //if(minDist > 63.0f)
        //return;

    Write3D(0, DTid, minDist);
}

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    
    float sampleLevelL = pc.lod;
    uint triangleCount = pc.triangleCount;

    if(sampleLevelL == 1.0f)
    {
        WriteToWorldSDF(DTid);
        return;
    }
    
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

        //if(index == 1)
         //   minDist = max(-minDist, d);
        //else
            minDist = smin(minDist, d, 0);

    }
    
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