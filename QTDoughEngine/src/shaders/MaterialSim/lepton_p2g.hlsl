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

// Lepton buffer.
StructuredBuffer<Lepton> leptonIn : register(t13, space1);

// Accumulator (atomic int scatter target).
RWStructuredBuffer<MaterialGridAccumulator> accumulator : register(u21, space1);

// Dispatched per lepton: (leptonMaxSize / 256, 1, 1).
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;
    if (globalIndex >= LEPTON_COUNT)
        return;

    Lepton l = leptonIn[globalIndex];

    if (l.position.w <= 0 || l.mana.w <= 0)
        return;

    float3 pos = l.position.xyz;

    int3 gridRes = GetMaterialGridSize();
    float3 sceneSize = GetSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    float3 cellSize = sceneSize / float3(gridRes);

    // Grid-space position and base cell (cubic B-spline scaled to 7x7x7, support 3.0 cells).
    float3 gs = (pos + halfScene) / cellSize;
    int3 base = int3(floor(gs - 2.5f));
    float3 fx = gs - float3(base);

    // Cubic B-spline scaled: evaluate at d/1.5 so support stretches to ±3 cells.
    float wx[7], wy[7], wz[7];
    [unroll]
    for (int n = 0; n < 7; n++)
    {
        float dx = abs(fx.x - (float)n) / 1.5f;
        float dy = abs(fx.y - (float)n) / 1.5f;
        float dz = abs(fx.z - (float)n) / 1.5f;

        wx[n] = (dx < 1.0f) ? (2.0f/3.0f - dx*dx + 0.5f*dx*dx*dx) :
                (dx < 2.0f) ? (1.0f/6.0f * (2.0f - dx)*(2.0f - dx)*(2.0f - dx)) : 0.0f;
        wy[n] = (dy < 1.0f) ? (2.0f/3.0f - dy*dy + 0.5f*dy*dy*dy) :
                (dy < 2.0f) ? (1.0f/6.0f * (2.0f - dy)*(2.0f - dy)*(2.0f - dy)) : 0.0f;
        wz[n] = (dz < 1.0f) ? (2.0f/3.0f - dz*dz + 0.5f*dz*dz*dz) :
                (dz < 2.0f) ? (1.0f/6.0f * (2.0f - dz)*(2.0f - dz)*(2.0f - dz)) : 0.0f;
    }

    // 7x7x7 cubic B-spline scatter.
    [unroll]
    for (int i = 0; i < 7; i++)
    {
        [unroll]
        for (int j = 0; j < 7; j++)
        {
            [unroll]
            for (int k = 0; k < 7; k++)
            {
                int3 cellCoord = base + int3(i, j, k);

                if (any(cellCoord < 0) || any(cellCoord >= gridRes))
                    continue;

                float weight = wx[i] * wy[j] * wz[k];
                int contribution = (int)round(weight * l.mana.x * FIXED_POINT_SCALE);
                int idx = Flatten3D(cellCoord, gridRes);

                int dummy;
                InterlockedAdd(accumulator[idx].fieldValues.y, contribution, dummy);
            }
        }
    }
}
