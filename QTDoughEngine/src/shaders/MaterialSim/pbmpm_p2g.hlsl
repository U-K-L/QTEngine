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

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;

    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta quanta = quantaOut[globalIndex];

    if (quanta.position.w < 1.0f)
        return;

    int brushId = quanta.information.x - 1;
    if (brushId < 0)
        return;

    QuantaUnseal(quanta, Brushes[brushId]);

    // --- Grid constants ---
    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridResolution = GetMaterialGridSize();
    float3 cellSize = sceneSize / float3(gridResolution);

    float mass = 0.1f;//quanta.position.w;

    // PB-MPM: affine comes from the candidate displacement being solved, C = D / dt.
    float3x3 AffineVelocity = deformOut[globalIndex].CandidateDeff / pc.dt;

    // --- World-space position ---
    float3 quantaPosition = quanta.position.xyz;

    // --- Quadratic B-spline base cell and weights ---
    float3 gs = (quantaPosition + halfScene) / cellSize;
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

    // --- 27-cell stencil: scatter mass/momentum onto accumulator (no explicit stress force) ---
    [unroll]
    for (int i = 0; i < 3; i++)
    {
        [unroll]
        for (int j = 0; j < 3; j++)
        {
            [unroll]
            for (int k = 0; k < 3; k++)
            {
                int3 cellCoordinate = base + int3(i, j, k); // node offset.

                if (any(cellCoordinate < 0) || any(cellCoordinate >= gridResolution))
                    continue;

                int cellId = Flatten3D(cellCoordinate, gridResolution);
                float weight = wx[i] * wy[j] * wz[k];

                float3 nodePos = float3(cellCoordinate) * cellSize - halfScene;
                float3 dx = nodePos - quantaPosition;

                float3 velocityCell = quanta.mana.xyz + mul(AffineVelocity, dx);

                float massCell = weight * mass;
                float3 momentumCell = massCell * velocityCell;

                int massContributionFixedPoint = (int)round(massCell * FIXED_POINT_SCALE_GRID);
                int momentumContributionFixedPointX = (int)round(momentumCell.x * FIXED_POINT_SCALE_GRID);
                int momentumContributionFixedPointY = (int)round(momentumCell.y * FIXED_POINT_SCALE_GRID);
                int momentumContributionFixedPointZ = (int)round(momentumCell.z * FIXED_POINT_SCALE_GRID);

                int dummy;
                InterlockedAdd(accumulator[cellId].massMomentum.x, momentumContributionFixedPointX, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.y, momentumContributionFixedPointY, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.z, momentumContributionFixedPointZ, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.w, massContributionFixedPoint, dummy);

                //SDF collision field.
                float h = cellSize.x;
                float radiusParticleSpacing = 2.0f * 0.35f;
                float sd = length(dx) - radiusParticleSpacing * h;
                int sdFixed = (int)round(sd * massCell * FIXED_POINT_SCALE_GRID);
                InterlockedAdd(accumulator[cellId].fieldValues.x, sdFixed, dummy);
            }
        }
    }
}
