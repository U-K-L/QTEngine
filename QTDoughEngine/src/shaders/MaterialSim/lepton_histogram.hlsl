#include "../Helpers/ShaderHelpers.hlsl"

[[vk::push_constant]]
struct PushConsts
{
    float particleSize;
    int tileGridX;
    int tileGridY;
    int tileGridZ;
} pc;

// Binding 13: Lepton In.
StructuredBuffer<Lepton> leptonIn : register(t13, space1);
// Binding 17: LeptonTileCounts.
RWStructuredBuffer<uint> leptonTileCounts : register(u17, space1);

uint ComputeTileIndex(float3 pos)
{
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
    if (globalIndex >= LEPTON_COUNT)
        return;

    Lepton l = leptonIn[globalIndex];
    // Unclaimed leptons (claimer ID < 0) go to tile 0.
    uint tileIdx = (l.position.w < 0) ? 0 : ComputeTileIndex(l.position.xyz);

    uint dummy;
    InterlockedAdd(leptonTileCounts[tileIdx], 1, dummy);
}
