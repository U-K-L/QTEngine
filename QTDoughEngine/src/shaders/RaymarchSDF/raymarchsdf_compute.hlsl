RWTexture2D<float4> OutputImage;
RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

#include "../Helpers/ShaderHelpers.hlsl"

struct Voxel
{
    float4 positionDistance;
    float4 normalDensity;
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
    for (int i = 0; i < 64; i++)
    {
        float hitPoint = SDFRoundBox(pos, float3(0,0,0), 5.0f, 0.05f);   
        //Remove this when not debugging.
        if (hitPoint < intersection)
        {
            float4 randPos = rand4(voxelsIn[i].positionDistance);
            resultOutput.xyz = normalize(randPos.xyz);
            resultOutput.w = 1.0f;
        }
        
        intersection = min(intersection, hitPoint);

        
    }
    
    return intersection;
}

float SphereMarch(float3 ro, float3 rd, inout float4 resultOutput)
{
    float maxDistance = 100.0f;
    float minDistance = 0.01f;
    float3 pos = ro;
    int maxSteps = 32;
    
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


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    uint2 pixel = DTid.xy;
    uint2 outputImageIndex = uint2(DTid.x, DTid.y);
    uint outputImageHandle = NonUniformResourceIndex(intArray[0]);

    float2 uv = (float2(pixel) + 0.5) * texelSize * 2.0 - 1.0;
    uv.y = -uv.y; // Flip Y

    float4 clip = float4(uv, 1.0, 1.0); 

    float4 viewPos = mul(inverse(proj), clip);
    viewPos /= viewPos.w;

    float3 dirView = normalize(viewPos.xyz); 
    float3 camPos = mul(inverse(view), float4(0, 0, 0, 1)).xyz;
    float3 dirWorld = normalize(mul((float3x3) inverse(view), dirView));


    float4 result = 0;
    float hit = SphereMarch(camPos, dirWorld, result);
    float4 col = (hit > 0) ? float4(1, 1, 1, 1) : float4(0, 0, 0, 1);
    gBindlessStorage[outputImageHandle][pixel] = saturate(result * col);
    
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
