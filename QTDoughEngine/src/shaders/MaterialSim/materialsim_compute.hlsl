#include "../Helpers/ShaderHelpers.hlsl"

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 texelSize; // xy = 1/width, 1/height
    float isOrtho;
}


cbuffer Constants : register(b2, space0)
{
    float deltaTime;
    float time;
    float2 pad;
};

// Push constants.
[[vk::push_constant]]
struct PushConsts
{
    float particleSize;
    int tileGridX;
    int tileGridY;
    int tileGridZ;
} pc;

// Set 1 bindings — MaterialSimulation buffers.
StructuredBuffer<Quanta> quantaIn : register(t0, space1);       // Ping-pong read.
RWStructuredBuffer<Quanta> quantaOut : register(u1, space1);    // Ping-pong write.
StructuredBuffer<Quanta> quantaRead : register(t2, space1);     // READ buffer (other passes read this).

RWStructuredBuffer<uint> quantaIds : register(u3, space1);
RWStructuredBuffer<uint> tileCounts : register(u4, space1);
RWStructuredBuffer<uint> tileOffsets : register(u5, space1);
RWStructuredBuffer<uint> tileCursor : register(u6, space1);

StructuredBuffer<QuantaDeformation> deformIn : register(t9, space1);
RWStructuredBuffer<QuantaDeformation> deformOut : register(u10, space1);
StructuredBuffer<Brush> Brushes : register(t7, space1);

#define GROUP_SIZE 512 // 8 * 8 * 8
#define BROWNIAN_STRENGTH 25.5f

[numthreads(8, 8, 8)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint localIndex = GTid.x + GTid.y * 8 + GTid.z * 64;
    uint globalIndex = Gid.x * GROUP_SIZE + localIndex;

    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta q = quantaIn[globalIndex];
    int brushId = q.information.x - 1;
    
    /*
    //Reset
    if (q.information.x > 0 && q.information.z > 0 && q.mana.w < 0.01f)
    {
        //q.information.x = 0;
        //q.mana.w = 0;
        quantaOut[globalIndex] = q;
        return;
    }
    
    //Skip.
    if ((q.position.w < 1 || q.mana.w < 0.01))
    {
        quantaOut[globalIndex] = q;
        return;
    }
    */

    
    if (brushId >= 0)
        QuantaUnseal(q, Brushes[brushId]);

    float3 worldPos = q.position.xyz;
    float4 clip = mul(view, float4(worldPos, 1.0));

    float3 ndc = clip.xyz / clip.w;

    float2 uv = ndc.xy * 0.5 + 0.5;
    uv.y = 1.0 - uv.y; 

    //Distance from observer:
    float4x4 invProj = inverse(proj);
    float4x4 invView = inverse(view);

    float4 viewPos = mul(invProj, float4(uv.x, uv.y, 0, 1));

    float3 perspectiveRayDir = normalize(mul((float3x3) invView, normalize(viewPos.xyz)));
    float3 perspectiveRayOrigin = mul(invView, float4(0, 0, 0, 1)).xyz;

    float3 orthoRayOrigin = mul(invView, float4(viewPos.xyz, 1.0)).xyz;
    float3 orthoRayDir = normalize(mul((float3x3) invView, float3(0, 0, -1)));

    float3 ro = lerp(perspectiveRayOrigin, orthoRayOrigin, isOrtho);

    float linearDepth = distance(worldPos.xyz, ro);

    q.resonance.w = linearDepth;

    if (ndc.x < -1.0 || ndc.x > 1.0 ||
        ndc.y < -1.0 || ndc.y > 1.0 ||
        ndc.z <  0.0 || ndc.z > 1.0)
    {
        q.resonance.w = 99999;
    }

    if (clip.w <= 0.0)
        q.resonance.w = 99999; // behind camera

    if (brushId >= 0)
        QuantaSeal(q, Brushes[brushId]);


    quantaOut[globalIndex] = q;
}
