#include "../Helpers/ShaderHelpers.hlsl"

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
    
        
    //Reset
    if (q.information.x > 0 && q.information.z > 0 && q.mana.w < 0.01f && q.information.x != 23)
    {
        //q.information.x = 0;
        //q.mana.w = 0;
        quantaOut[globalIndex] = q;
        return;
    }
    
    //Skip.
    if ((q.position.w < 1 || q.mana.w < 0.01) && q.information.x != 23)
    {
        quantaOut[globalIndex] = q;
        return;
    }




    float3 worldPos = q.position.xyz;
    
    if (brushId >= 0)
        worldPos = mul(Brushes[brushId].model, float4(q.position.xyz, 1.0f)).xyz;
    
    float3 gravity = float3(0, 0, -9.8f) * q.mana.w;
    worldPos += gravity * deltaTime * 0.01f;

    if (q.information.x == 23)
    {
        uint seed = globalIndex;
        float p1 = float((seed * 7u)  % 997u) * 0.00628f + time * 1.5f;
        float p2 = float((seed * 13u) % 991u) * 0.00628f + time * 2.1f;
        float3 waveVel = float3(0.023f * sin(p1),
                                0.023f * sin(p2),
                                0.015f * cos(p1 + p2));
        //worldPos += waveVel * deltaTime;

    }

    if (brushId >= 0)
    {
        q.position.xyz = mul(Brushes[brushId].invModel, float4(worldPos, 1.0f)).xyz;

        // If quanta left its brush AABB, unassign it.
        float3 qUvw = (q.position.xyz - Brushes[brushId].aabbmin.xyz) / (Brushes[brushId].aabbmax.xyz - Brushes[brushId].aabbmin.xyz);
        if (any(qUvw < 0.0f) || any(qUvw > 1.0f))
            q.information.x = 0;
    }
    else
        q.position.xyz = worldPos;
    quantaOut[globalIndex] = q;
}
