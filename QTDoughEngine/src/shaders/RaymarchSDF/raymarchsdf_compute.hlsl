RWTexture2D<float4> OutputImage;
RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

#include "../Helpers/ShaderHelpers.hlsl"

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
    for (int i = 0; i <4096; i++)
    {
        if(voxelsIn[i].occuipiedInfo.w <= 0)
            continue;
        float hitPoint = SDFRoundBox(pos, voxelsIn[i].positionDistance.xyz, voxelsIn[i].normalDensity.w*0.5f, 0.05f);
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

float SphereMarch(float3 ro, float3 rd, inout float4 resultOutput)
{
    float maxDistance = 100.0f;
    float minDistance = 0.01f;
    float3 pos = ro;
    int maxSteps = 64;
    
    for (int i = 0; i < maxSteps; i++)
    {
        //Find the smallest distance
        float distance = IntersectionPoint(pos, rd, resultOutput);
        
        //See if it is close enough otherwise continue.
        if(distance < minDistance)
        {
            return distance; //Something was hit.
        }
        
        //update position to the nearest point. effectively a sphere trace.
        pos = pos + rd * distance;
    }
    
    return -1; //Nothing hit.

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
    float hit = SphereMarch(camPos, dirWorld, result);
    float4 col = (hit > 0) ? float4(1, 1, 1, 1) : float4(0, 0, 0, 1);
    
    gBindlessStorage[outputImageHandle][pixel] += saturate(result * col);

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
