#include "rayTracingAccelHelper.hlsl"

[shader("miss")]
void main(inout Photon photon : SV_RayPayload)
{
    photon.color = float4(0, 0, 0, -1);
}
