#include "rayTracingAccelHelper.hlsl"

[shader("miss")]
void main(inout Photon photon : SV_RayPayload)
{
    //Missed. Calcuulate current position.
    photon.position.xyz = photon.position + photon.direction * RayTCurrent();
    photon.color = float4(0, 0, 0, -1);
}
