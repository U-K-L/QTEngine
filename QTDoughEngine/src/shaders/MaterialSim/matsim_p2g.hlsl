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

RWStructuredBuffer<MaterialGridAccumulator> accumulator : register(u21, space1);

RWStructuredBuffer<BrushAccumulator> brushAccumulator : register(u24, space1);

[numthreads(8, 8, 8)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint localIndex = GTid.x + GTid.y * 8 + GTid.z * 64;
    uint globalIndex = Gid.x * 512 + localIndex;

    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta q = quantaIn[globalIndex];

    if (q.position.w < 1.0f)
        return;

    float mass = q.position.w;

    // --- Grid constants ---
    float3 sceneSize = GetSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridRes = GetMaterialGridSize();
    float3 cellSize = sceneSize / float3(gridRes);

    // --- World-space position ---
    float3 pos = q.position.xyz;
    int brushId = q.information.x - 1;
    if (brushId >= 0)
        pos = mul(Brushes[brushId].model, float4(pos, 1.0f)).xyz;

    // --- Quadratic B-spline base cell and weights ---
    float3 gs = (pos + halfScene) / cellSize;
    int3 base = int3(floor(gs - 0.5f));
    float3 fx = gs - float3(base);

    float wx[3], wy[3], wz[3];
    wx[0] = 0.5f * (1.5f - fx.x) * (1.5f - fx.x);
    wx[1] = 0.75f - (fx.x - 1.0f) * (fx.x - 1.0f);
    wx[2] = 0.5f * (fx.x - 0.5f) * (fx.x - 0.5f);

    wy[0] = 0.5f * (1.5f - fx.y) * (1.5f - fx.y);
    wy[1] = 0.75f - (fx.y - 1.0f) * (fx.y - 1.0f);
    wy[2] = 0.5f * (fx.y - 0.5f) * (fx.y - 0.5f);

    wz[0] = 0.5f * (1.5f - fx.z) * (1.5f - fx.z);
    wz[1] = 0.75f - (fx.z - 1.0f) * (fx.z - 1.0f);
    wz[2] = 0.5f * (fx.z - 0.5f) * (fx.z - 0.5f);
    
    //Averaged position (world space).
    int posX = (int) round(pos.x * FIXED_POINT_SCALE);
    int posY = (int) round(pos.y * FIXED_POINT_SCALE);
    int posZ = (int) round(pos.z * FIXED_POINT_SCALE);
    
    if (brushId >= 0 && brushId < MAX_BRUSHES)
    {
        int dummyVal;
        InterlockedAdd(brushAccumulator[brushId].count, 1, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumX, posX, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumY, posY, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumZ, posZ, dummyVal);
    }

    // --- 27-cell stencil: scatter mass onto accumulator ---
    [unroll]
    for (int i = 0; i < 3; i++)
    {
        [unroll]
        for (int j = 0; j < 3; j++)
        {
            [unroll]
            for (int k = 0; k < 3; k++)
            {
                int3 cellCoord = base + int3(i, j, k);

                if (any(cellCoord < 0) || any(cellCoord >= gridRes))
                    continue;

                float weight = wx[i] * wy[j] * wz[k];
                int idx = Flatten3D(cellCoord, gridRes);

                int massContribution = (int)round(weight * mass * FIXED_POINT_SCALE);
                int momX = (int)round(weight * mass * q.mana.x * FIXED_POINT_SCALE);
                int momY = (int)round(weight * mass * q.mana.y * FIXED_POINT_SCALE);
                int momZ = (int)round(weight * mass * q.mana.z * FIXED_POINT_SCALE);

                int dummy;
                InterlockedAdd(accumulator[idx].massMomentum.x, momX, dummy);
                InterlockedAdd(accumulator[idx].massMomentum.y, momY, dummy);
                InterlockedAdd(accumulator[idx].massMomentum.z, momZ, dummy);
                InterlockedAdd(accumulator[idx].massMomentum.w, massContribution, dummy);

            }
        }
    }
}
