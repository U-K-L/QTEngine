#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

RWStructuredBuffer<GameObjectShaderData> globalObjMaterials : register(u6, space0);

cbuffer Constants : register(b2, space0)
{
    float deltaTime;
    float time;
    float2 pad;
    GPULight light[32];
};

struct PSInput
{
    float4 position    : SV_Position;
    float3 center      : TEXCOORD0;
    float3 rayOrigin   : TEXCOORD1;
    float3 worldCorner : TEXCOORD2;
    float  radius      : TEXCOORD3;
    nointerpolation uint matId : TEXCOORD4;
};

struct PSOutput
{
    float4 color : SV_Target;
    float  depth : SV_Depth;
};

PSOutput main(PSInput i)
{
    PSOutput o;

    float3 rd = normalize(i.worldCorner - i.rayOrigin);
    float3 oc = i.rayOrigin - i.center;
    float  b  = dot(oc, rd);
    float  c  = dot(oc, oc) - i.radius * i.radius;
    float  h  = b * b - c;
    if (h < 0.0f) discard;

    float  t    = -b - sqrt(h);
    float3 hitP = i.rayOrigin + rd * t;
    float3 N    = normalize(hitP - i.center);

    float  NdotL  = saturate(dot(N, -light[0].direction));
    float3 albedo = globalObjMaterials[i.matId].Midtone.rgb;
    float3 col    = albedo * NdotL * light[0].emission.rgb;

    float4 clipHit = mul(proj, mul(view, float4(hitP, 1.0f)));
    o.color = float4(col, 1.0f);
    o.depth = clipHit.z / clipHit.w;
    return o;
}
