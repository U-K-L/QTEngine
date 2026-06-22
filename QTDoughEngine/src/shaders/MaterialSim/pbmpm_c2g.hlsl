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
    float dt;
    float pad1;
    float pad2;
} pc;

StructuredBuffer<MaterialGridAccumulator> accumulator : register(t21, space1);
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int3 gridRes = GetMaterialGridSize();

    if (any(DTid >= (uint3)gridRes))
        return;

    int3 node = int3(DTid);
    int nodeId = Flatten3D(node, gridRes);

    float4 momentumSum = 0.0f;
    float energySum = 0.0f;
    float sdSum = 0.0f;

    [unroll]
    for (int i = 0; i < 2; i++)
    {
        [unroll]
        for (int j = 0; j < 2; j++)
        {
            [unroll]
            for (int k = 0; k < 2; k++)
            {
                int3 centerCoord = node - int3(1, 1, 1) + int3(i, j, k);

                if (any(centerCoord < 0) || any(centerCoord >= gridRes))
                    continue;

                int cellId = Flatten3D(centerCoord, gridRes);

                momentumSum.x += (float) accumulator[cellId].massMomentum.x;
                momentumSum.y += (float) accumulator[cellId].massMomentum.y;
                momentumSum.z += (float) accumulator[cellId].massMomentum.z;
                momentumSum.w += (float) accumulator[cellId].massMomentum.w;
                energySum += (float) accumulator[cellId].fieldValues.y;
                sdSum += (float) accumulator[cellId].fieldValues.x;
            }
        }
    }

    materialGrid[nodeId].massMomentum = (momentumSum * 0.125f) / FIXED_POINT_SCALE;
    materialGrid[nodeId].fieldValues.y += (energySum * 0.125f / FIXED_POINT_SCALE) * deltaTime * 0.05f;
    materialGrid[nodeId].fieldValues.x = (momentumSum.w > 0.0f) ? (sdSum / momentumSum.w) : DEFUALT_EMPTY_SPACE;
}
