#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

[[vk::push_constant]]
struct PushConsts
{
    float radius;
    uint  flags;
    uint  quantaCount;
    uint  pad0;
} pc;

StructuredBuffer<Quanta> quantaBuffer : register(t12, space1);
StructuredBuffer<Brush>  Brushes      : register(t7,  space1);

struct VSOutput
{
    float4 position    : SV_Position;
    float3 center      : TEXCOORD0;
    float3 rayOrigin   : TEXCOORD1;
    float3 worldCorner : TEXCOORD2;
    float  radius      : TEXCOORD3;
    nointerpolation uint matId : TEXCOORD4;
};

VSOutput main(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    VSOutput o;
    Quanta q = quantaBuffer[iid];

    if (q.position.w < 1.0f || q.information.x == 0)
    {
        o.position = float4(2, 2, 2, 1);
        o.center = 0; o.rayOrigin = 0; o.worldCorner = 0; o.radius = 0; o.matId = 0;
        return o;
    }

    float3 worldPos = q.position.xyz;
    int brushId = q.information.x - 1;
    if (brushId >= 0)
        worldPos = mul(Brushes[brushId].model, float4(worldPos, 1.0f)).xyz;
    o.matId = (uint)Brushes[brushId].materialId;

    float4x4 invView = inverse(view);
    float3 camRight  = normalize(mul(invView, float4(1, 0, 0, 0)).xyz);
    float3 camUp     = normalize(mul(invView, float4(0, 1, 0, 0)).xyz);
    float3 camPos    = mul(invView, float4(0, 0, 0, 1)).xyz;

    float2 offs = float2(((vid & 1) ? 1.0f : -1.0f), ((vid & 2) ? 1.0f : -1.0f));
    float3 corner = worldPos + (camRight * offs.x + camUp * offs.y) * pc.radius;

    o.position    = mul(proj, mul(view, float4(corner, 1.0f)));
    o.center      = worldPos;
    o.rayOrigin   = camPos;
    o.worldCorner = corner;
    o.radius      = pc.radius;
    return o;
}
