#include "rayTracingAccelHelper.hlsl"
#include "../Helpers/ShaderHelpers.hlsl"

RaytracingAccelerationStructure tlas : register(t0);

RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 texelSize; // xy = 1/width, 1/height
    float isOrtho;
}

[shader("raygeneration")]
void main()
{
    uint2 pix = DispatchRaysIndex().xy;

    // NDC in [-1,1]
    float2 uv = ((float2(pix) + 0.5f) * texelSize.xy) * 2.0f - 1.0f;
    uv.y = -uv.y;

    float4x4 invProj = inverse(proj);
    float4x4 invView = inverse(view);

    // Camera origin in world
    float3 ro = invView[3].xyz;

    // Ray direction in world (perspective)
    float4 viewPos = mul(invProj, float4(uv, 0.0f, 1.0f));
    viewPos.xyz /= viewPos.w;
    float3 rd = normalize(mul((float3x3) invView, normalize(viewPos.xyz)));

    Payload p;
    p.color = float4(0, 0, 0, 1);

    RayDesc ray;
    ray.Origin = ro;
    ray.Direction = rd;
    ray.TMin = 0.001f;
    ray.TMax = 50;
    
    /*
    // Trace ray
    TraceRay(
    tlas,
    0, // RayFlags
    0xFF, // cull mask
    0, // sbtRecordOffset (RayContribution)
    1, // sbtRecordStride
    0, // missIndex
    ray,
    p
);

*/
    uint outHandle = NonUniformResourceIndex(intArray[0]);
    gBindlessStorage[outHandle][pix] = float4(p.color.xyz, 1.0f);
}
