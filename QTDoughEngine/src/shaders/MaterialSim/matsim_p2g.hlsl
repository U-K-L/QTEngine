#include "../Helpers/ShaderHelpers.hlsl"

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
StructuredBuffer<Quanta> quantaIn : register(t0, space1);
RWStructuredBuffer<Quanta> quantaOut : register(u1, space1);
StructuredBuffer<Quanta> quantaRead : register(t2, space1);
RWStructuredBuffer<uint> quantaIds : register(u3, space1);
RWStructuredBuffer<uint> tileCounts : register(u4, space1);
RWStructuredBuffer<uint> tileOffsets : register(u5, space1);
RWStructuredBuffer<uint> tileCursor : register(u6, space1);
StructuredBuffer<Brush> Brushes : register(t7, space1);
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);
StructuredBuffer<QuantaDeformation> deformIn : register(t9, space1);
RWStructuredBuffer<QuantaDeformation> deformOut : register(u10, space1);

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x + DTid.y * 8 + DTid.z * 64;
    if (globalIndex >= QUANTA_COUNT)
        return;

    // TODO: P2G scatter — read particle, splat mass/momentum onto materialGrid.
}
