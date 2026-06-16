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

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;

    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta quanta = quantaIn[globalIndex];

    if (quanta.position.w < 1.0f)
        return;

    // --- Grid constants ---
    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridResolution = GetMaterialGridSize();
    float3 cellSize = sceneSize / float3(gridResolution);

    float mass = 0.1f;//quanta.position.w;
    float3x3 AffineVelocity = deformIn[globalIndex].AffVel; //Particle affine velocity.
    float3x3 DefformationF = deformIn[globalIndex].DeffGrad;

    //modifiable ----------
    float E = 12888.0f;
    float nu = 0.25f;
    float particlesPerCell = 1.0f;

    float mu = E / (2.0f * (1.0f + nu));
    float lambda = E * nu / ((1.0f + nu) * (1.0f - 2.0f * nu));
    float cellVolume = cellSize.x * cellSize.y * cellSize.z;
    float volume0 = cellVolume / particlesPerCell;
    //----------------

    float3x3 stressTensor = ComputeStress(DefformationF, mu, lambda); //Changable models.

    // --- World-space position ---
    float3 quantaPosition = quanta.position.xyz;
    int brushId = quanta.information.x - 1;
    if (brushId >= 0)
        quantaPosition = mul(Brushes[brushId].model, float4(quantaPosition, 1.0f)).xyz;

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
    
    //Averaged position (world space).
    int posX = (int) round(quantaPosition.x * FIXED_POINT_SCALE);
    int posY = (int) round(quantaPosition.y * FIXED_POINT_SCALE);
    int posZ = (int) round(quantaPosition.z * FIXED_POINT_SCALE);
    
    if (brushId >= 0 && brushId < MAX_BRUSHES)
    {
        int dummyVal;
        InterlockedAdd(brushAccumulator[brushId].count, 1, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumX, posX, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumY, posY, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumZ, posZ, dummyVal);
    }

    // --- 27-cell stencil: scatter mass onto accumulator
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

                float3 elasticTerm = mul(INERTIA_TENSOR_INVERSE,mul(stressTensor,mul(transpose(DefformationF), dx)));
                
                float3 forceCell = -weight * volume0 * elasticTerm;

                float massCell = weight * mass;
                float3 momentumCell = massCell * velocityCell;

                float3 momentumCellStar = momentumCell + FIXED_DELTA_TIME * forceCell;
                
                int massContributionFixedPoint = (int)round(massCell * FIXED_POINT_SCALE);
                int momentumContributionFixedPointX = (int)round(momentumCellStar.x * FIXED_POINT_SCALE);
                int momentumContributionFixedPointY = (int)round(momentumCellStar.y * FIXED_POINT_SCALE);
                int momentumContributionFixedPointZ = (int)round(momentumCellStar.z * FIXED_POINT_SCALE);

                int dummy;
                InterlockedAdd(accumulator[cellId].massMomentum.x, momentumContributionFixedPointX, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.y, momentumContributionFixedPointY, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.z, momentumContributionFixedPointZ, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.w, massContributionFixedPoint, dummy);

            }
        }
    }
}
