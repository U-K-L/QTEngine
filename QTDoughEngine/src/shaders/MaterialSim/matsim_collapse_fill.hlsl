#include "../Helpers/ShaderHelpers.hlsl"

// Global bindless 3D textures.
Texture3D<float> gBindless3D[] : register(t4, space0);

[[vk::push_constant]]
struct PushConsts
{
    float particleSize;
    int tileGridX;
    int tileGridY;
    int tileGridZ;
    int brushIndex;
    float pad0;
    float pad1;
    float pad2;
} pc;

// Set 1 bindings — MaterialSimulation buffers.
RWStructuredBuffer<Quanta> quantaOut : register(u1, space1);
RWStructuredBuffer<uint> quantaIds : register(u3, space1);
RWStructuredBuffer<uint> tileCounts : register(u4, space1);
RWStructuredBuffer<uint> tileOffsets : register(u5, space1);
RWStructuredBuffer<uint> tileCursor : register(u6, space1);
StructuredBuffer<Brush> Brushes : register(t7, space1);

#define MAX_CLAIM_ATTEMPTS 32

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Brush brush = Brushes[pc.brushIndex];

    // Density grid — same as CreateBrush.
    int blockSize = brush.density;
    if (any((DTid.xyz % blockSize) != 0))
        return;

    // Compute UVW and local position — same as CreateBrush.
    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution;
    float3 localPos = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, uvw);
    float sdf = gBindless3D[brush.textureID2].Load(int4(DTid, 0));

    if (sdf >= 0.0f)
        return;

    float3 worldPos = mul(brush.model, float4(localPos, 1.0f)).xyz;

    // Now within this tile we will get the Quantas, fast acceleration structure.
    uint tileIdx = ComputeTileIndex(worldPos, int3(pc.tileGridX, pc.tileGridY, pc.tileGridZ));
    uint tileEnd = tileOffsets[tileIdx] + tileCounts[tileIdx];

    // In this tile we will go over MAX_CLAIM of quantas or until we exhaust all quanta.
    for (int attempt = 0; attempt < MAX_CLAIM_ATTEMPTS; attempt++)
    {
        uint slot;
        InterlockedAdd(tileCursor[tileIdx], 1, slot); //Critical as many threads could claim this quanta.

        if (slot >= tileEnd)
            break; // Tile exhausted.

        uint qIdx = quantaIds[slot];
        Quanta q = quantaOut[qIdx];

        // Only claim free quanta.
        if (q.information.x == 0)
        {
            q.position.xyz = worldPos;
            q.information.x = (int) brush.id; //New definitive claim.
            quantaOut[qIdx] = q;
            return;
        }
    }
}
