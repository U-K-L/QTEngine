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
RWStructuredBuffer<uint> tileCounts : register(u4, space1);
RWStructuredBuffer<uint> tileOffsets : register(u5, space1);
RWStructuredBuffer<uint> tileCursor : register(u6, space1);

#define TILE_COUNT 128

groupshared uint shared_data[TILE_COUNT];

// Blelloch exclusive prefix sum over 128 tiles in a single workgroup.
[numthreads(128, 1, 1)]
void main(uint3 GTid : SV_GroupThreadID)
{
    uint tid = GTid.x;

    // Load tile counts into shared memory.
    shared_data[tid] = tileCounts[tid];
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

    // Clear the last element for exclusive scan.
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

    // Write exclusive prefix sums to tileOffsets and initialize tileCursor to same values.
    tileOffsets[tid] = shared_data[tid];
    tileCursor[tid] = shared_data[tid];
}
