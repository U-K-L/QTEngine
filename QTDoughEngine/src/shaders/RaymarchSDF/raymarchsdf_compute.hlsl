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


float SDFSphere(float3 position, float radius)
{
    return length(position) - radius;
}

float RayMarch(float3 ro, float3 rd)
{
    float dist = 0.0f;
    for (uint i = 0; i < 128; ++i)
    {
        float3 pos = ro + rd * dist;
        float d = SDFSphere(pos, 1.0f); // <- test with small radius
        if (d < 0.001f)
            return dist;
        dist += d;
        if (dist > 50.0f)
            break;
    }
    return -1.0f;
}


[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    uint2 pixel = DTid.xy;
    uint2 outputImageIndex = uint2(DTid.x, DTid.y);
    uint outputImageHandle = NonUniformResourceIndex(intArray[0]);
    
    /* ---- build the ray --------------------------------------------------- */

    float2 uv = (float2(pixel) + 0.5) * texelSize * 2.0 - 1.0; // –1 .. 1
    float4 clip = float4(uv, 0.0, 1.0); // z = 0  (near)

    float3 dirView = normalize(mul(inverse(proj), clip).xyz);

    float3 camPos = mul(inverse(view), float4(0, 0, 0, 1)).xyz; // correct
    float3 dirWorld = normalize(mul((float3x3) inverse(view), dirView));

    /* ---- march ----------------------------------------------------------- */

    float hit = RayMarch(camPos, dirWorld);
    float4 col = (hit > 0) ? float4(1, 1, 1, 1) : float4(0, 0, 0, 1);
    gBindlessStorage[outputImageHandle][pixel] = col;
    
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
