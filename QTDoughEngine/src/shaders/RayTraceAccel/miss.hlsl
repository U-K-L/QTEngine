#include "rayTracingAccelHelper.hlsl"

[shader("miss")]
void main(inout Payload payload : SV_RayPayload)
{
    payload.color = float4(0, 0, 0,0);
}
