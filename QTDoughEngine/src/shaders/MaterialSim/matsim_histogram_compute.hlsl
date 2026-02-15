#include "../Helpers/ShaderHelpers.hlsl"

[[vk::push_constant]]
struct PushConsts
{
    float particleSize;
    int tileGridX;
    int tileGridY;
    int tileGridZ;
} pc;

// Set 1 bindings.
StructuredBuffer<Quanta> quantaIn : register(t0, space1);
RWStructuredBuffer<uint> tileCounts : register(u4, space1);

uint ComputeTileIndex(float3 pos)
{
    // Field centered at origin. halfField = tileGrid * tileSize(8) * 0.5.
    float3 halfField = float3(pc.tileGridX, pc.tileGridY, pc.tileGridZ) * 4.0f;
    float3 local = pos + halfField;
    int3 tileCoord = int3(floor(local / 8.0f));
    tileCoord = clamp(tileCoord, int3(0, 0, 0), int3(pc.tileGridX - 1, pc.tileGridY - 1, pc.tileGridZ - 1));
    return (uint)Flatten3D(tileCoord, int3(pc.tileGridX, pc.tileGridY, pc.tileGridZ));
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;
    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta q = quantaIn[globalIndex];
    uint tileIdx = (q.position.w < 1) ? 0 : ComputeTileIndex(q.position.xyz);

    uint dummy;
    InterlockedAdd(tileCounts[tileIdx], 1, dummy);
}
