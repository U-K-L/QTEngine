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
RWStructuredBuffer<uint> globalCursor : register(u6, space1); // tileCursor[0] used as atomic counter.
StructuredBuffer<Brush> Brushes : register(t7, space1);

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Brush brush = Brushes[pc.brushIndex];

    int blockSize = brush.density;
    if (!all((DTid.xyz % blockSize) == 0))
        return;

    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution;
    float3 localPos = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, uvw);
    float sdf = gBindless3D[brush.textureID2].Load(int4(DTid, 0));

    if (sdf >= 0.0f)
        return;

    float3 worldPos = mul(brush.model, float4(localPos, 1.0f)).xyz;

    // Atomic counter: grab sequential quanta slots until we find a free one.
    while (true)
    {
        uint slot;
        InterlockedAdd(globalCursor[0], 1, slot);

        if (slot >= QUANTA_COUNT)
            return;

        if (quantaOut[slot].information.x != 0)
            continue; // Already claimed, try next.

        quantaOut[slot].position.xyz = worldPos;
        quantaOut[slot].information.x = (int) brush.id + 1;
        return;
    }
}
