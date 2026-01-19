#include "rayTracingAccelHelper.hlsl"

[shader("closesthit")]
void main(inout Payload payload : SV_RayPayload, in Attributes attr : SV_IntersectionAttributes)
{
    payload.color = float4(0, 1, 0, 1);
}