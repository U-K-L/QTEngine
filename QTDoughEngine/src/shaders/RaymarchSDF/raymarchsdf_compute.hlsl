RWTexture2D<float4> OutputImage;
RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

#include "../Helpers/ShaderHelpers.hlsl"

#define VOXEL_RESOLUTION 256
#define SCENE_BOUNDS 10.0f

struct Voxel
{
    float4 positionDistance;
    float4 normalDensity;
    float4 occuipiedInfo;
};

struct ComputeVertex
{
    float4 position;
    float4 texCoord;
    float4 normal;
};

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float2 texelSize;
}

StructuredBuffer<Voxel> voxelsIn : register(t2, space1); // readonly
RWStructuredBuffer<Voxel> voxelsOut : register(u3, space1); // write


StructuredBuffer<ComputeVertex> vertexBuffer : register(t4, space1);
StructuredBuffer<uint3> indexBuffer : register(t5, space1);


float SDFSphere(float3 position, float3 spherePos, float radius)
{
    return length(position - spherePos) -  radius;
}

float SDFRoundBox(float3 position, float3 boxPosition, float3 halfSize, float radius)
{
    float3 q = abs(position - boxPosition) - halfSize;
    return length(max(q, 0.0f)) + min(max(q.x, max(q.y, q.z)), 0.0f) - radius;
}

float dot2(float3 v)
{
    return dot(v, v);
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

int HashPositionToVoxelIndex(float3 pos, float sceneBounds, int voxelResolution)
{
    float halfScene = sceneBounds * 0.5f;
    float3 gridPos = (pos + halfScene) / sceneBounds;
    int3 voxelCoord = int3(floor(gridPos * voxelResolution));

    //Clamp to avoid invalid access
    voxelCoord = clamp(voxelCoord, int3(0, 0, 0), int3(voxelResolution - 1, voxelResolution - 1, voxelResolution - 1));

    return voxelCoord.x * voxelResolution * voxelResolution + voxelCoord.y * voxelResolution + voxelCoord.z;
}


float4 TrilinearSampleSDF(float3 pos)
{
    float3 gridPos = ((pos + SCENE_BOUNDS * 0.5f) / SCENE_BOUNDS) * VOXEL_RESOLUTION;
    int3 base = int3(floor(gridPos));
    float3 fracVal = frac(gridPos); // interpolation weights

    base = clamp(base, int3(0, 0, 0), int3(VOXEL_RESOLUTION - 2, VOXEL_RESOLUTION - 2, VOXEL_RESOLUTION - 2));

    float voxelSize = SCENE_BOUNDS / VOXEL_RESOLUTION;
    float halfScene = SCENE_BOUNDS * 0.5f;

    // Convert voxel grid coords back to world positions for sampling
    float3 p000 = (float3(base + int3(0, 0, 0)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;
    float3 p100 = (float3(base + int3(1, 0, 0)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;
    float3 p010 = (float3(base + int3(0, 1, 0)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;
    float3 p110 = (float3(base + int3(1, 1, 0)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;
    float3 p001 = (float3(base + int3(0, 0, 1)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;
    float3 p101 = (float3(base + int3(1, 0, 1)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;
    float3 p011 = (float3(base + int3(0, 1, 1)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;
    float3 p111 = (float3(base + int3(1, 1, 1)) / VOXEL_RESOLUTION) * SCENE_BOUNDS - halfScene;

    float4 c000 = float4(
    voxelsIn[HashPositionToVoxelIndex(p000, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p000, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);
    float4 c100 = float4(
    voxelsIn[HashPositionToVoxelIndex(p100, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p100, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);
    float4 c010 = float4(
    voxelsIn[HashPositionToVoxelIndex(p010, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p010, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);
    float4 c110 = float4(
    voxelsIn[HashPositionToVoxelIndex(p110, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p110, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);
    float4 c001 = float4(
    voxelsIn[HashPositionToVoxelIndex(p001, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p001, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);
    float4 c101 = float4(
    voxelsIn[HashPositionToVoxelIndex(p101, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p101, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);
    float4 c011 = float4(
    voxelsIn[HashPositionToVoxelIndex(p011, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p011, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);
    float4 c111 = float4(
    voxelsIn[HashPositionToVoxelIndex(p111, SCENE_BOUNDS, VOXEL_RESOLUTION)].positionDistance.w,
    voxelsIn[HashPositionToVoxelIndex(p111, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.xyz);


    float4 c00 = lerp(c000, c100, fracVal.x);
    float4 c10 = lerp(c010, c110, fracVal.x);
    float4 c01 = lerp(c001, c101, fracVal.x);
    float4 c11 = lerp(c011, c111, fracVal.x);

    float4 c0 = lerp(c00, c10, fracVal.y);
    float4 c1 = lerp(c01, c11, fracVal.y);

    return lerp(c0, c1, fracVal.z);
}

float3 CentralDifferenceNormal(float3 p)
{
    float voxelSize = SCENE_BOUNDS / VOXEL_RESOLUTION;
    float eps = voxelSize *0.5f;

    float dx = TrilinearSampleSDF(p + float3(eps, 0, 0)).x - TrilinearSampleSDF(p - float3(eps, 0, 0)).x;
    float dy = TrilinearSampleSDF(p + float3(0, eps, 0)).x - TrilinearSampleSDF(p - float3(0, eps, 0)).x;
    float dz = TrilinearSampleSDF(p + float3(0, 0, eps)).x - TrilinearSampleSDF(p - float3(0, 0, eps)).x;

    float3 n = float3(dx, dy, dz);
    return (length(n) > 1e-5f) ? normalize(n) : float3(0, 0, 0);
}


float SampleSDF(float3 pos)
{
    int index = HashPositionToVoxelIndex(pos, SCENE_BOUNDS, VOXEL_RESOLUTION);
    float sdf = voxelsIn[index].positionDistance.w;

    return sdf;
}

//Kills anything outside of bounds.
float SafeSampleSDF(float3 pos)
{
    float halfScene = SCENE_BOUNDS * 0.5f;
    
    float voxelSize = SCENE_BOUNDS / VOXEL_RESOLUTION;

    if (any(pos < -halfScene) || any(pos > halfScene))
        return voxelSize*10.0f; //Let's move carefully until we approach the actual scene bounding. Move one voxel at a time.
    
    return TrilinearSampleSDF(pos).x;
}

float4 SampleNormalSDF(float3 pos)
{
    float halfScene = SCENE_BOUNDS * 0.5f;
    
    float voxelSize = SCENE_BOUNDS / VOXEL_RESOLUTION;

    if (any(pos < -halfScene) || any(pos > halfScene))
        return voxelSize * 10.0f; //Let's move carefully until we approach the actual scene bounding. Move one voxel at a time.
    
    return TrilinearSampleSDF(pos);
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
    for (int i = 0; i <512; i++)
    {
        /*
        if (voxelsIn[i].positionDistance.w >= 0.5)
            continue;
        float hitPoint = SDFRoundBox(pos, voxelsIn[i].positionDistance.xyz, voxelsIn[i].normalDensity.w*0.5f, 0.05f);
        */
        float hitPoint = SampleSDF(pos);
        //Remove this when not debugging.
        if (hitPoint < intersection)
        {
            float index = i;
            float4 randPos = rand4(voxelsIn[i].positionDistance);
            float4 randIndex = float4(rand(index), rand(index + 1.0f), rand(index + 2.0f), index);
            resultOutput.xyz = normalize(randPos.xyz + randIndex.xyz);
            resultOutput.w = 1.0f;
            minIndex = i;

        }

        
        
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

float smin(float a, float b, float k)
{
    k *= 16.0 / 3.0;
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * (4.0 - h) * k * (1.0 / 16.0);
}

float4 smin(float4 a, float4 b, float k)
{
    k *= 4.0f;
    float h = max(k - abs(a.x - b.x), 0.0f) / (2.0f * k);

    float resultDist = min(a.x, b.x) - h * h * k;
    float3 resultGrad = (a.x < b.x) ? lerp(a.yzw, b.yzw, h)
                                    : lerp(a.yzw, b.yzw, 1.0f - h);

    return float4(resultDist, resultGrad);
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
    float voxelSize = SCENE_BOUNDS / VOXEL_RESOLUTION;
    float maxDistance = 100.0f;
    float minDistance = voxelSize*0.5f;
    float3 pos = ro;
    int maxSteps = 1024;
    float4 closesSDF = maxDistance;
    
    for (int i = 0; i < maxSteps; i++)
    {
        //Find the smallest distance
        //float distance = IntersectionPoint(pos, rd, resultOutput);
        minDistance = voxelsIn[HashPositionToVoxelIndex(pos, SCENE_BOUNDS, VOXEL_RESOLUTION)].normalDensity.w;
        
        float4 currentSDF = SampleNormalSDF(pos);
        //currentSDF.yzw = CentralDifferenceNormal(pos);
        
        closesSDF = smin(closesSDF, currentSDF, minDistance*0.25f);
        
        
        //See if it is close enough otherwise continue.
        if (closesSDF.x < minDistance)
        {
                    
            closesSDF.yzw = normalize(closesSDF.yzw);
            return closesSDF; //Something was hit.
        }
        
        //update position to the nearest point. effectively a sphere trace.
        pos = pos + rd * closesSDF.x;
    }
    
    return float4(0, 0, 0, -1.0f); //Nothing hit.

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


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    uint2 pixel = DTid.xy;
    uint2 outputImageIndex = uint2(DTid.x, DTid.y);
    uint outputImageHandle = NonUniformResourceIndex(intArray[0]);
    gBindlessStorage[outputImageHandle][pixel] = 0; //CLEAR IMAGE.
    bool ortho = abs(proj[3][3] - 1.0) < 1e-5;

    float4x4 invProj = inverse(proj);
    float4x4 invView = inverse(view);
    
    float2 uv = (float2(pixel) + 0.5) * texelSize * 2.0 - 1.0;
    uv.y = -uv.y; // Flip Y

    float3 camPos, dirWorld;

    if (ortho)
    {
        float4 clip = float4(uv, 0.0, 1.0); // no extra scale
        camPos = mul(invView, mul(invProj, clip)).xyz;
        dirWorld = normalize(-invView[2].xyz);
    }
    else
    {
        float4 clip = float4(uv, 1.0, 1.0);
        float4 viewP = mul(invProj, clip);
        viewP /= viewP.w;
        dirWorld = normalize(mul((float3x3) invView, normalize(viewP.xyz)));
        camPos = mul(invView, float4(0, 0, 0, 1)).xyz;
    }

    float4 result = 0;
    float4 hit = SphereMarch(camPos, dirWorld, result);
    float4 col = (hit.w > 0) ? float4(1, 1, 1, 1) : float4(0, 0, 0, 1);
    
    //gBindlessStorage[outputImageHandle][pixel] = voxelsIn[4002].positionDistance; //float4(hit.xyz, 1.0); //float4(1, 0, 0, 1) * col; //saturate(result * col);
    //gBindlessStorage[outputImageHandle][pixel] = float4(hit.xyz, 1.0);
    gBindlessStorage[outputImageHandle][pixel] = float4(hit.yzw, 1.0);// + col*0.25;
    /*
    //SDF sphere trace each vertex point.
    float debugHit = DebugMarchVertices(camPos, dirWorld);
    
    gBindlessStorage[outputImageHandle][pixel] += debugHit * float4(1, 0, 0, 1);
    */
    
    /*
    uint2 pix = DTid.xy;
    uint imgId = intArray[0];
    uint handle = NonUniformResourceIndex(imgId);

    // Shader‑model 6.6 (or later) – texel operator
    gBindlessStorage[handle][pix] = float4(1, 1, 0, 1);

    // Pre‑6.6 form – Write method
    // gBindlessStorage[handle].Write(pix, float4(1, 0, 0, 1));

    float4 col = gBindlessStorage[handle][pix]; // read
    
    uint index = DTid.x;
    
    uint2 imageIndex = uint2(DTid.x, DTid.y);
    //gBindlessStorage[imageIndex] = 1;
    
    if (index >= 64)
    {
        return;
    }
    
    Voxel voxelIn = voxelsIn[index];

    Voxel result;
    result.positionDistance.xyz = col.xyz;
    result.positionDistance.w = 20.0f;
    result.normalDensity.xyz = float3(1, texelSize.x, texelSize.y);
    result.normalDensity.w = 23312.0f;

    voxelsOut[index] = result;
*/
    

}
