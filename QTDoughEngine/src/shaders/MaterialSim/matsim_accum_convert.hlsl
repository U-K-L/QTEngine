#include "../Helpers/ShaderHelpers.hlsl"

cbuffer Constants : register(b2, space0)
{
    float deltaTime;
    float time;
    float2 pad;
};

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

// Accumulator (read).
StructuredBuffer<MaterialGridAccumulator> accumulator : register(t21, space1);

// MaterialGrid (write).
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);

// Dispatched per grid cell: (materialGridSize / 8) per axis.
[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int3 gridRes = GetMaterialGridSize();

    if (any(DTid >= (uint3)gridRes))
        return;

    int3 c = int3(DTid);
    int idx = Flatten3D(c, gridRes);

    materialGrid[idx].fieldValues.y += ((float) accumulator[idx].fieldValues.y / FIXED_POINT_SCALE) * deltaTime *0.1f;
}
