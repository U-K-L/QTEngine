#include "rayTracingAccelHelper.hlsl"

RWTexture2D<float4> outImage : register(u1);
RaytracingAccelerationStructure tlas : register(t0);

[shader("raygeneration")]
void main()
{
    uint2 pix = DispatchRaysIndex().xy;

    // Just write something obvious
    outImage[pix] = float4(1, 0, 0, 1); // red = raygen executed
}
