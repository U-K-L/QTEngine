#include "rayTracingAccelHelper.hlsl"


[shader("closesthit")]
void main(inout Payload p)
{
    p.color = float4(1, 1, 0, 1);
}
