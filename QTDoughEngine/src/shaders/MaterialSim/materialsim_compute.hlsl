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

#define GROUP_SIZE 512 // 8 * 8 * 8
#define BROWNIAN_STRENGTH 1.5f

[numthreads(8, 8, 8)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint localIndex = GTid.x + GTid.y * 8 + GTid.z * 64;
    uint globalIndex = Gid.x * GROUP_SIZE + localIndex;

    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta q = quantaIn[globalIndex];

    if (q.position.w < 1)
    {
        quantaOut[globalIndex] = q;
        return;
    }

    // Brownian motion — random kick each frame.
    float3 kick = float3(
        randNegative1to1(q.position.xyz, time),
        randNegative1to1(q.position.xyz, time + 3.7f),
        randNegative1to1(q.position.xyz, time + 7.3f)
    );

    q.position.xyz += kick * BROWNIAN_STRENGTH * deltaTime;

    // Melt: only x >= 0 half, strength falls off with distance from origin.
    if (q.position.x >= 0)
    {
        float dist = length(q.position.xyz);
        float meltStrength = 1.0f / (1.0f + dist * dist); // Inverse square falloff.
        float3 meltDir = float3(0, 0, -1); // Drip downward.
        q.position.xyz += meltDir * meltStrength * 5400.0f * deltaTime;
    }

    quantaOut[globalIndex] = q;
}
