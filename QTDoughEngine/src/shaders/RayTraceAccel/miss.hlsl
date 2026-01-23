#include "rayTracingAccelHelper.hlsl"

[shader("miss")]
void main(inout Payload p)
{
    p.color = float4(0, 0, 0, 1);
}
