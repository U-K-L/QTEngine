#include "../Helpers/ShaderHelpers.hlsl"

[[vk::push_constant]]
struct PushConsts
{
    float particleSize;
    int tileGridX;
    int tileGridY;
    int tileGridZ;
} pc;

// Binding 17: LeptonTileCounts.
RWStructuredBuffer<uint> leptonTileCounts : register(u17, space1);
// Binding 18: LeptonTileOffsets.
RWStructuredBuffer<uint> leptonTileOffsets : register(u18, space1);
// Binding 19: LeptonTileCursor.
RWStructuredBuffer<uint> leptonTileCursor : register(u19, space1);

#define TILE_COUNT 128

groupshared uint shared_data[TILE_COUNT];

[numthreads(128, 1, 1)]
void main(uint3 GTid : SV_GroupThreadID)
{
    uint tid = GTid.x;

    shared_data[tid] = leptonTileCounts[tid];
    GroupMemoryBarrierWithGroupSync();

    // Up-sweep (reduce).
    [unroll]
    for (uint d = 0; d < 7; d++)
    {
        uint stride = 1u << (d + 1);
        uint half_stride = 1u << d;
        if (tid < (TILE_COUNT >> (d + 1)))
        {
            uint ai = stride * (tid + 1) - 1;
            uint bi = ai - half_stride;
            shared_data[ai] += shared_data[bi];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    if (tid == 0)
        shared_data[TILE_COUNT - 1] = 0;
    GroupMemoryBarrierWithGroupSync();

    // Down-sweep.
    [unroll]
    for (int dd = 6; dd >= 0; dd--)
    {
        uint stride = 1u << (dd + 1);
        uint half_stride = 1u << dd;
        if (tid < (TILE_COUNT >> (dd + 1)))
        {
            uint ai = stride * (tid + 1) - 1;
            uint bi = ai - half_stride;
            uint temp = shared_data[bi];
            shared_data[bi] = shared_data[ai];
            shared_data[ai] += temp;
        }
        GroupMemoryBarrierWithGroupSync();
    }

    leptonTileOffsets[tid] = shared_data[tid];
    leptonTileCursor[tid] = shared_data[tid];
}
