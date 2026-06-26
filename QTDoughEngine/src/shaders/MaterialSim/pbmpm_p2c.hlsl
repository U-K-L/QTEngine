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

    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridResolution = GetMaterialGridSize();
    float3 cellSize = sceneSize / float3(gridResolution);

    float mass = 0.1f;

    float3x3 AffineVelocity = deformIn[globalIndex].CandidateDeff / pc.dt;

    int brushId = quanta.information.x - 1;

    if (brushId < 0)
        return;

    QuantaUnseal(quanta, Brushes[brushId]);

    float3 quantaPosition = quanta.position.xyz;


    float3 gsc = (quantaPosition + halfScene) / cellSize - 0.5f;
    int3 base = int3(floor(gsc));
    float3 fx = gsc - float3(base);

    float wx[2], wy[2], wz[2];
    wx[0] = 1.0f - fx.x; wx[1] = fx.x;
    wy[0] = 1.0f - fx.y; wy[1] = fx.y;
    wz[0] = 1.0f - fx.z; wz[1] = fx.z;

    [unroll]
    for (int i = 0; i < 2; i++)
    {
        [unroll]
        for (int j = 0; j < 2; j++)
        {
            [unroll]
            for (int k = 0; k < 2; k++)
            {
                int3 centerCoord = base + int3(i, j, k);

                if (any(centerCoord < 0) || any(centerCoord >= gridResolution))
                    continue;

                int cellId = Flatten3D(centerCoord, gridResolution);
                float weight = wx[i] * wy[j] * wz[k];

                float3 centerPos = (float3(centerCoord) + 0.5f) * cellSize - halfScene;
                float3 dx = centerPos - quantaPosition;

                float3 velocityCell = quanta.mana.xyz + mul(AffineVelocity, dx);

                float massCell = weight * mass;
                float3 momentumCell = massCell * velocityCell;

                int massFixed = (int) round(massCell * FIXED_POINT_SCALE);
                int momX = (int) round(momentumCell.x * FIXED_POINT_SCALE);
                int momY = (int) round(momentumCell.y * FIXED_POINT_SCALE);
                int momZ = (int) round(momentumCell.z * FIXED_POINT_SCALE);

                int dummy;
                InterlockedAdd(accumulator[cellId].massMomentum.x, momX, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.y, momY, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.z, momZ, dummy);
                InterlockedAdd(accumulator[cellId].massMomentum.w, massFixed, dummy);

                //SDF collision field.
                float h = cellSize.x;
                float radiusParticleSpacing = 2.0f * 0.35f;
                float sd = length(dx) - radiusParticleSpacing * h;
                int sdFixed = (int) round(sd * massCell * FIXED_POINT_SCALE);
                InterlockedAdd(accumulator[cellId].fieldValues.x, sdFixed, dummy);
            }
        }
    }
}
