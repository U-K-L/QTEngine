#include "rayTracingAccelHelper.hlsl"

[shader("anyhit")]
void main(inout Payload payload : SV_RayPayload, in Attributes attr : SV_IntersectionAttributes)
{
    // do nothing; accept hit
}