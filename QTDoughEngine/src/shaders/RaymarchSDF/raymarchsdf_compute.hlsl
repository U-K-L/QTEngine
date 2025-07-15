﻿
RWTexture2D<float4> OutputImage;
RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

#include "../Helpers/ShaderHelpers.hlsl"

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 texelSize;
    float isOrtho;
}

StructuredBuffer<Voxel> voxelsL1In : register(t2, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL1Out : register(u3, space1); // write

StructuredBuffer<Voxel> voxelsL2In : register(t4, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL2Out : register(u5, space1); // write

StructuredBuffer<Voxel> voxelsL3In : register(t6, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL3Out : register(u7, space1); // write


StructuredBuffer<ComputeVertex> vertexBuffer : register(t8, space1);
// For reading
Texture3D<float2> gBindless3D[] : register(t4, space0);


float2 Read3D(uint textureIndex, int3 coord)
{
    return gBindless3D[textureIndex].Load(int4(coord, 0));
}


float2 GetVoxelValueTexture(int textureId, int3 coord, float sampleLevel)
{
    return Read3D(textureId, coord);
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


float SDFSphere(float3 position, float3 spherePos, float radius)
{
    return length(position - spherePos) - radius;
}

float SDFRoundBox(float3 position, float3 boxPosition, float3 halfSize, float radius)
{
    float3 q = abs(position - boxPosition) - halfSize;
    return length(max(q, 0.0f)) + min(max(q.x, max(q.y, q.z)), 0.0f) - radius;
}


float SDFTriangle(float3 p, float3 a, float3 b, float3 c)
{
    float3 ba = b - a;
    float3 pa = p - a;
    float3 cb = c - b;
    float3 pb = p - b;
    float3 ac = a - c;
    float3 pc = p - c;
    float3 nor = cross(ba, ac);

    float inside = sign(dot(cross(ba, nor), pa)) +
                   sign(dot(cross(cb, nor), pb)) +
                   sign(dot(cross(ac, nor), pc));

    if (inside < 2.0f)
    {
        float d1 = dot2(ba * clamp(dot(ba, pa) / dot2(ba), 0.0f, 1.0f) - pa);
        float d2 = dot2(cb * clamp(dot(cb, pb) / dot2(cb), 0.0f, 1.0f) - pb);
        float d3 = dot2(ac * clamp(dot(ac, pc) / dot2(ac), 0.0f, 1.0f) - pc);
        return sqrt(min(min(d1, d2), d3));
    }
    else
    {
        return abs(dot(nor, pa)) / length(nor);
    }
}


float2 TrilinearSampleSDFTexture(float3 pos)
{
    float sampleLevel = 1.0f;
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(sampleLevel);
    
    float3 gridPos = ((pos + voxelSceneBounds.y * 0.5f) / voxelSceneBounds.y) * voxelSceneBounds.x;
    int3 base = int3(floor(gridPos));
    float3 fracVal = frac(gridPos); // interpolation weights

    base = clamp(base, int3(0, 0, 0), int3(voxelSceneBounds.x - 2, voxelSceneBounds.x - 2, voxelSceneBounds.x - 2));

    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert voxel grid coords back to world positions for sampling
    float3 p000 = (float3(base + int3(0, 0, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    
    /*
    float3 p100 = (float3(base + int3(1, 0, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p010 = (float3(base + int3(0, 1, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p110 = (float3(base + int3(1, 1, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p001 = (float3(base + int3(0, 0, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p101 = (float3(base + int3(1, 0, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p011 = (float3(base + int3(0, 1, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p111 = (float3(base + int3(1, 1, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;

    float4 c000 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p000, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p000, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c100 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p100, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p100, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c010 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p010, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p010, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c110 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p110, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p110, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c001 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p001, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p001, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c101 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p101, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p101, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c011 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p011, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p011, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c111 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p111, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p111, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );



    float4 c00 = lerp(c000, c100, fracVal.x);
    float4 c10 = lerp(c010, c110, fracVal.x);
    float4 c01 = lerp(c001, c101, fracVal.x);
    float4 c11 = lerp(c011, c111, fracVal.x);

    float4 c0 = lerp(c00, c10, fracVal.y);
    float4 c1 = lerp(c01, c11, fracVal.y);

    return lerp(c0, c1, fracVal.z);
    */
    
    return GetVoxelValueTexture(0, base, 0);
}


float4 TrilinearSampleSDF(float3 pos)
{
    float sampleLevel = GetSampleLevel(pos, 0);
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    
    float3 gridPos = ((pos + voxelSceneBounds.y * 0.5f) / voxelSceneBounds.y) * voxelSceneBounds.x;
    int3 base = int3(floor(gridPos));
    float3 fracVal = frac(gridPos); // interpolation weights

    base = clamp(base, int3(0, 0, 0), int3(voxelSceneBounds.x - 2, voxelSceneBounds.x - 2, voxelSceneBounds.x - 2));

    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float halfScene = voxelSceneBounds.y * 0.5f;

    // Convert voxel grid coords back to world positions for sampling
    float3 p000 = (float3(base + int3(0, 0, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p100 = (float3(base + int3(1, 0, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p010 = (float3(base + int3(0, 1, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p110 = (float3(base + int3(1, 1, 0)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p001 = (float3(base + int3(0, 0, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p101 = (float3(base + int3(1, 0, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p011 = (float3(base + int3(0, 1, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;
    float3 p111 = (float3(base + int3(1, 1, 1)) / voxelSceneBounds.x) * voxelSceneBounds.y - halfScene;

    float4 c000 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p000, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p000, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c100 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p100, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p100, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c010 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p010, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p010, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c110 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p110, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p110, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c001 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p001, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p001, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c101 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p101, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p101, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c011 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p011, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p011, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );

    float4 c111 = float4(
    GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p111, voxelSceneBounds.y, voxelSceneBounds.x)).w,
    normalize(GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(p111, voxelSceneBounds.y, voxelSceneBounds.x)).xyz)
    );



    float4 c00 = lerp(c000, c100, fracVal.x);
    float4 c10 = lerp(c010, c110, fracVal.x);
    float4 c01 = lerp(c001, c101, fracVal.x);
    float4 c11 = lerp(c011, c111, fracVal.x);

    float4 c0 = lerp(c00, c10, fracVal.y);
    float4 c1 = lerp(c01, c11, fracVal.y);

    return lerp(c0, c1, fracVal.z);
}


float3 CentralDifferenceNormalTexture(float3 p)
{
    float eps = 0.1851755f;

    float dx = TrilinearSampleSDFTexture(p + float3(eps, 0, 0)).x - TrilinearSampleSDFTexture(p - float3(eps, 0, 0)).x;
    float dy = TrilinearSampleSDFTexture(p + float3(0, eps, 0)).x - TrilinearSampleSDFTexture(p - float3(0, eps, 0)).x;
    float dz = TrilinearSampleSDFTexture(p + float3(0, 0, eps)).x - TrilinearSampleSDFTexture(p - float3(0, 0, eps)).x;

    float3 n = float3(dx, dy, dz);
    return (length(n) > 1e-5f) ? normalize(n) : float3(0, 0, 0);
}

float3 CentralDifferenceNormal(float3 p)
{
    float eps = 0.055f;

    float dx = TrilinearSampleSDF(p + float3(eps, 0, 0)).x - TrilinearSampleSDF(p - float3(eps, 0, 0)).x;
    float dy = TrilinearSampleSDF(p + float3(0, eps, 0)).x - TrilinearSampleSDF(p - float3(0, eps, 0)).x;
    float dz = TrilinearSampleSDF(p + float3(0, 0, eps)).x - TrilinearSampleSDF(p - float3(0, 0, eps)).x;

    float3 n = float3(dx, dy, dz);
    return (length(n) > 1e-5f) ? normalize(n) : float3(0, 0, 0);
}

float3 CentralDifferenceNormalBrush(float3 p, float3 center, float size)
{
    float eps = 0.075f;

    float dx = sdSphere(p + float3(eps, 0, 0), center, size).x - sdSphere(p - float3(eps, 0, 0), center, size).x;
    float dy = sdSphere(p + float3(0, eps, 0), center, size).x - sdSphere(p - float3(0, eps, 0), center, size).x;
    float dz = sdSphere(p + float3(0, 0, eps), center, size).x - sdSphere(p - float3(0, 0, eps), center, size).x;

    float3 n = float3(dx, dy, dz);
    return (length(n) > 1e-5f) ? normalize(n) : float3(0, 0, 0);
}

float3 Normal4Point(float3 p, float eps)
{
    float dx = (-TrilinearSampleSDF(p + 2 * float3(eps, 0, 0)).x +
                 8 * TrilinearSampleSDF(p + float3(eps, 0, 0)).x -
                 8 * TrilinearSampleSDF(p - float3(eps, 0, 0)).x +
                 TrilinearSampleSDF(p - 2 * float3(eps, 0, 0)).x) / (12.0 * eps);

    float dy = (-TrilinearSampleSDF(p + 2 * float3(0, eps, 0)).x +
                 8 * TrilinearSampleSDF(p + float3(0, eps, 0)).x -
                 8 * TrilinearSampleSDF(p - float3(0, eps, 0)).x +
                 TrilinearSampleSDF(p - 2 * float3(0, eps, 0)).x) / (12.0 * eps);

    float dz = (-TrilinearSampleSDF(p + 2 * float3(0, 0, eps)).x +
                 8 * TrilinearSampleSDF(p + float3(0, 0, eps)).x -
                 8 * TrilinearSampleSDF(p - float3(0, 0, eps)).x +
                 TrilinearSampleSDF(p - 2 * float3(0, 0, eps)).x) / (12.0 * eps);

    return normalize(float3(dx, dy, dz));
}


float3 GetHighQualityNormal(float3 p)
{
    float sampleLevel = GetSampleLevel(p, 0);
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float eps = max(1e-4f, voxelSize * 0.5f);
    return Normal4Point(p, eps);
}


float4 SampleSDF(float3 pos)
{
    float sampleLevel = GetSampleLevel(pos, 0);
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    float halfScene = voxelSceneBounds.y * 0.5f;
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    
    if (any(pos < -halfScene) || any(pos > halfScene))
        return voxelSize * 2.0f; //Let's move carefully until we approach the actual scene bounding. Move one voxel at a time.
    
    int3 fakeNormal = HashPositionToVoxelIndex3(pos, voxelSceneBounds.y, voxelSceneBounds.x);
    float4 result = GetVoxelValue(sampleLevel, HashPositionToVoxelIndex(pos, voxelSceneBounds.y, voxelSceneBounds.x)).wxyz;
    result.yzw = fakeNormal / 256.0f;
    
    return result;

}

//Kills anything outside of bounds.
float SafeSampleSDF(float3 pos)
{
    float sampleLevel = GetSampleLevel(pos, 0);
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    float halfScene = voxelSceneBounds.y * 0.5f;
    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;

    if (any(pos < -halfScene) || any(pos > halfScene))
        return voxelSize * 2.0f; //Let's move carefully until we approach the actual scene bounding. Move one voxel at a time.
    
    return TrilinearSampleSDF(pos).x;
}

float4 SampleNormalSDF(float3 pos)
{
    float sampleLevel = GetSampleLevel(pos, 0);
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    float halfScene = voxelSceneBounds.y * 0.5f;
    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;

    if (any(pos < -halfScene) || any(pos > halfScene))
        return voxelSize;
    
    return TrilinearSampleSDF(pos);
}

float2 SampleNormalSDFTexture(float3 pos)
{
    float sampleLevel = GetSampleLevel(pos, 0);
    float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(sampleLevel);
    float halfScene = voxelSceneBounds.y * 0.5f;
    
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;

    if (any(pos < -halfScene) || any(pos > halfScene))
        return voxelSize;
    
    return TrilinearSampleSDFTexture(pos);
}


float IntersectionPoint(float3 pos, float3 dir, inout float4 resultOutput)
{
    float maxDistance = 100.0f;
    /*
    float3 spheres[3];
    spheres[0] = float3(0, 0, 0);
    spheres[1] = float3(8, 0, 0);
    spheres[2] = float3(0, 8, 0);
    
    //Find the smallest intersection to return.
    float intersection = maxDistance;
    for (int i = 0; i < 3; i++)
    {
        float hitPoint = SDFSphere(pos, spheres[i], 1.0f);
        intersection = min(intersection, hitPoint);
        
    }
    */
    
    //Voxels.
    //Find the smallest intersection to return.
    float intersection = maxDistance;
    int minIndex = 0;
    for (int i = 0; i < 512; i++)
    {
        /*
        if (voxelsIn[i].positionDistance.w >= 0.5)
            continue;
        float hitPoint = SDFRoundBox(pos, voxelsIn[i].positionDistance.xyz, voxelsIn[i].normalDensity.w*0.5f, 0.05f);
        */
        float hitPoint = SampleSDF(pos).x;
        //Remove this when not debugging.
        /*
        if (hitPoint < intersection)
        {
            float index = i;
            float4 randPos = rand4(voxelsIn[i].positionDistance);
            float4 randIndex = float4(rand(index), rand(index + 1.0f), rand(index + 2.0f), index);
            resultOutput.xyz = normalize(randPos.xyz + randIndex.xyz);
            resultOutput.w = 1.0f;
            minIndex = i;

        }

        */
        
        intersection = min(intersection, hitPoint);
    }
    
    float keepAlive = 0;
    //Check if box contains a vertex. Is vertex position in box?
    /*
    for (int j = 0; j < 6661; j++)
    {
        Voxel voxel = voxelsIn[minIndex];
        ComputeVertex vert = vertexBuffer[j];

        float3 halfExtent = voxel.normalDensity.w * 0.5f;
        float3 voxelMin = voxel.positionDistance.xyz - halfExtent;
        float3 voxelMax = voxel.positionDistance.xyz + halfExtent;

        bool isInside = all(vert.position.xyz >= voxelMin) && all(vert.position.xyz <= voxelMax);

        if (isInside)
        {
            keepAlive = 1;
            break;
        }
    }
    
    resultOutput *= keepAlive;
    */
    return intersection;
}




float minFunction(float a, float b)
{
    float minimum = 0;
    //Functions list.
    
    //minimum = min(a, b);
    minimum = smin(a, b, 0.09f);
    
    return minimum;
}

float4 SphereMarch(float3 ro, float3 rd, inout float4 resultOutput)
{
    float sampleLevelL1 = 1.0f;
    float2 voxelSceneBoundsL1 = GetVoxelResolutionWorldSDF(sampleLevelL1);
    float voxelSizeL1 = voxelSceneBoundsL1.y / voxelSceneBoundsL1.x;
    float minDistanceL1 = voxelSizeL1 * 0.5f;
    
    //L2
    float sampleLevelL2 = 2.0f;
    float2 voxelSceneBoundsL2 = GetVoxelResolution(sampleLevelL2);
    float voxelSizeL2 = voxelSceneBoundsL2.y / voxelSceneBoundsL2.x;
    float minDistanceL2 = voxelSizeL2 * 2.0f;
    
    //L3
    float sampleLevelL3 = 3.0f;
    float2 voxelSceneBoundsL3 = GetVoxelResolution(sampleLevelL3);
    float voxelSizeL3 = voxelSceneBoundsL3.y / voxelSceneBoundsL3.x;
    float minDistanceL3 = voxelSizeL3 * 0.5;

    float maxDistance = 100.0f;

    float3 pos = ro;
    int maxSteps = 1024;
    float4 closesSDF = maxDistance;
    
    float sampleLevel = GetSampleLevel(pos, 0);
    for (int i = 0; i < maxSteps; i++)
    {
        
        //Find the smallest distance

        float4 currentSDF = 0;
        float2 sampleId = SampleNormalSDFTexture(pos);
        currentSDF.x = sampleId.x;
        currentSDF.yzw = CentralDifferenceNormalTexture(pos);
        //closesSDF = currentSDF;
        
        float newSampleLevel = GetSampleLevel(pos, 0);
        bool sameLevel = sampleLevel == newSampleLevel;
        sampleLevel = newSampleLevel;
        bool inL1 = sampleLevel == 1.0f;
        bool inL2 = sampleLevel == 2.0f;
        bool inL3 = sampleLevel == 3.0f;
        
        float2 voxelSceneBounds = GetVoxelResolutionWorldSDF(sampleLevel);
        float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;

        //float4 mixedSDF = smin(closesSDF, currentSDF, minDistanceL1 * 0.025f);
        //mixedSDF = smin(mixedSDF, brushSDF, 0.524f);
        //mixedSDF = smin(sdf2, mixedSDF, 0.024f);
        closesSDF = smin(closesSDF, currentSDF, 0.0025f); //lerp(currentSDF, mixedSDF, sameLevel);
        
        
        bool canTerminate =
        (currentSDF.x < minDistanceL1);

        if (canTerminate)
        {
            float len = length(closesSDF.yzw);
            closesSDF.yzw = (len > 1e-4f) ? normalize(closesSDF.yzw) : float3(0, 0, 1); // fallback normal
            resultOutput.xy = sampleId;
            return closesSDF;
        }


        //update position to the nearest point. effectively a sphere trace.
        float stepSize = clamp(currentSDF.x, 0, voxelSizeL1 * 4.0f);
        pos += rd * stepSize;

    }
    
    
    resultOutput.y = NO_LABELF();
    return float4(0, 0, 0, -1.0f); //Nothing hit.

}


float4 FullMarch(float3 ro, float3 rd, inout float4 resultOutput)
{
    float sampleLevelL1 = 1.0f;
    float2 voxelSceneBoundsL1 = GetVoxelResolution(sampleLevelL1);
    float voxelSizeL1 = voxelSceneBoundsL1.y / voxelSceneBoundsL1.x;
    float minDistanceL1 = voxelSizeL1 * 2.0f;
    
    //L2
    float sampleLevelL2 = 2.0f;
    float2 voxelSceneBoundsL2 = GetVoxelResolution(sampleLevelL2);
    float voxelSizeL2 = voxelSceneBoundsL2.y / voxelSceneBoundsL2.x;
    float minDistanceL2 = voxelSizeL2 * 2.0f;
    
    //L3
    float sampleLevelL3 = 3.0f;
    float2 voxelSceneBoundsL3 = GetVoxelResolution(sampleLevelL3);
    float voxelSizeL3 = voxelSceneBoundsL3.y / voxelSceneBoundsL3.x;
    float minDistanceL3 = voxelSizeL3 * 0.5;


    
    float maxDistance = 32.0f;

    float3 pos = ro;
    int maxSteps = 128;
    float4 closesSDF = maxDistance;
    
    float sampleLevel = GetSampleLevel(pos, 0);
    for (int i = 0; i < maxSteps; i++)
    {
        
        //Find the smallest distance
        //float distance = IntersectionPoint(pos, rd, resultOutput);
        //minDistance = voxelsIn[HashPositionToVoxelIndex(pos, SCENE_BOUNDSL2, voxelSceneBounds.x)].normalDistance.w;
        
        //float3 sdf = sdSphere(pos, 0, 2.0f);
        //float4 sdf2 = 0;
        //sdf2.xyz = sdSphere(pos, vertexBuffer[4855].position.xyz, 2.0f);
        //sdf2.yzw = CentralDifferenceNormalBrush(pos, vertexBuffer[4855].position.xyz, 2.0f);
        //float4 brushSDF = float4(sdf, 1.0);
        float4 currentSDF = SampleSDF(pos);
        currentSDF.yzw = CentralDifferenceNormal(pos);
        
        float newSampleLevel = GetSampleLevel(pos, 0);
        bool sameLevel = sampleLevel == newSampleLevel;
        sampleLevel = newSampleLevel;
        bool inL1 = sampleLevel == 1.0f;

        if (abs(currentSDF.x) < abs(closesSDF.x))
            closesSDF = currentSDF;

        float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
        float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
        /*
        if(sameLevel == true)
            closesSDF = smin(closesSDF, currentSDF, minDistanceL1 * 0.25f);
        else
            closesSDF = currentSDF;
        */
        
        bool canTerminate =
        (inL1 && abs(closesSDF.x) < minDistanceL1);

        if (canTerminate)
        {
            float len = length(closesSDF.yzw);
            closesSDF.yzw = (len > 1e-4f) ? normalize(closesSDF.yzw) : float3(0, 0, 1); // fallback normal
            return closesSDF;
        }


        
        //See if it is close enough otherwise continue.
        /*
        if (abs(closesSDF.x) < minDistanceL1)
        {
                    
            closesSDF.yzw = normalize(closesSDF.yzw);
            return closesSDF; //Something was hit.
        }
        */
        //update position to the nearest point. effectively a sphere trace.
        float stepSize = clamp(abs(closesSDF.x), voxelSize * 0.25f, voxelSize * 10.0f);
        pos += rd * stepSize;

    }
    
    /*    
    float sampleLevel = GetSampleLevel(pos, 0);
    float2 voxelSceneBounds = GetVoxelResolution(sampleLevel);
    float voxelSize = voxelSceneBounds.y / voxelSceneBounds.x;
    float minDistance = voxelSize * 0.5f;
    
    if (closesSDF.x < minDistance)
    {
                    
        closesSDF.yzw = normalize(closesSDF.yzw);
        return closesSDF; //Something was hit.
    }
    */
    return closesSDF; //Nothing hit.

}

bool RayIntersectsSphere(float3 ro, float3 rd, float3 center, float radius)
{
    float3 oc = ro - center;
    float a = dot(rd, rd);
    float b = 2.0 * dot(oc, rd);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b * b - 4.0 * a * c;
    return discriminant > 0.0f;
}

/*
float DebugMarchVertices(float3 ro, float3 rd)
{
    const float radius = 0.05f;
    const int maxVerts = 6661;

    for (uint i = 0; i < maxVerts; ++i)
    {
        float3 center = vertexBuffer[i].position.xyz;

        if (RayIntersectsSphere(ro, rd, center, radius))
        {
            return 1.0f;
        }
    }

    return 0.0f;
}
*/
float3 turboColor(float t)
{
    t = saturate(t);
    return float3(
        saturate(1.0 - abs(1.0 - 4.0 * t)),
        saturate(1.0 - abs(2.0 - 4.0 * t)),
        saturate(1.0 - abs(3.0 - 4.0 * t))
    );
}


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float4x4 invProj = inverse(proj);
    float4x4 invView = inverse(view);
    
    uint2 pixel = DTid.xy;
    uint2 outputImageIndex = uint2(DTid.x, DTid.y);
    uint outputImageHandle = NonUniformResourceIndex(intArray[0]);
    gBindlessStorage[outputImageHandle][pixel] = 0; //CLEAR IMAGE.

    //Construct a ray shooting from the camera projection plane.
    uint3 id = DTid;
    float2 dim = texelSize.xy;
    float2 uv = (float2(pixel) + 0.5) * dim * 2.0 - 1.0;
    uv.y = -uv.y; // Flip Y
    
    //Convert to camera space.
    float4 viewPos = mul(invProj, float4(uv.x, uv.y, 0, 1));
    
    //Perspective divide.
    float4 perspectiveViewPos = viewPos / viewPos.w;
    float3 perspectiveRayDir = normalize(mul((float3x3) invView, normalize(viewPos.xyz)));
    float3 perspectiveRayOrigin = mul(invView, float4(0, 0, 0, 1)).xyz;
    
    //Orthogonal.
    float3 orthoRayOrigin = mul(invView, float4(viewPos.xyz, 1.0)).xyz;
    float3 orthoRayDir = mul((float3x3) invView, float3(0.0, 0.0, -1.0));
    orthoRayDir = normalize(orthoRayDir);
    
    //Interpolate.
    float3 interpRayOrigin = lerp(perspectiveRayOrigin, orthoRayOrigin, isOrtho);
    float3 interpRayDir = lerp(perspectiveRayDir, orthoRayDir, isOrtho);
    
    float4 result = 0;
    float4 hit = SphereMarch(interpRayOrigin, interpRayDir, result);
    float4 col = (hit.w > 0) ? float4(1, 1, 1, 1) : float4(0, 0, 0, 1);
    
    //gBindlessStorage[outputImageHandle][pixel] = voxelsIn[4002].positionDistance; //float4(hit.xyz, 1.0); //float4(1, 0, 0, 1) * col; //saturate(result * col);
    //gBindlessStorage[outputImageHandle][pixel] = float4(hit.xyz, 1.0);
    float4 light = saturate(dot(hit.yzw, normalize(float3(0.25f, 0.0, 1.0f))));
    gBindlessStorage[outputImageHandle][pixel] = light; //float4(hit.yzw, 1.0); // * col; // + col*0.25;
    
    //int idHash = floor(result.y / MAX_BRUSHES);
    
    uint finalID = asuint(result.y);
    uint brushID = finalID >> 24;
    uint labelID = finalID & 0xFFFFFF;

    
    if (result.y < NO_LABELF())
        gBindlessStorage[outputImageHandle][pixel] = float4(normalize(float3(abs(rand(labelID / 16000.0f)), abs(rand(labelID / 16000.0f + 3)), abs(rand(labelID / 16000.0f + 1)))), 1.0f);
    
    /*
    if (brushID == 1)
    {
        float4 baseColor = float4(normalize(float3(abs(rand(labelID / 16000.0f)), abs(rand(labelID / 16000.0f + 3)), abs(rand(labelID / 16000.0f + 1)))), 1.0f);
        gBindlessStorage[outputImageHandle][pixel] = float4(1.0f, 0.0f, 0.55f, 1.0f) + baseColor*0.5f;

    }
*/


}