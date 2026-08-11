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

StructuredBuffer<Quanta>              quantaIn    : register(t0,  space1);
RWStructuredBuffer<Quanta>            quantaOut   : register(u1,  space1);
StructuredBuffer<Quanta>              quantaRead  : register(t2,  space1);
RWStructuredBuffer<uint>              quantaIds   : register(u3,  space1);
RWStructuredBuffer<uint>              tileCounts  : register(u4,  space1);
RWStructuredBuffer<uint>              tileOffsets : register(u5,  space1);
RWStructuredBuffer<uint>              tileCursor  : register(u6,  space1);
StructuredBuffer<Brush>               Brushes     : register(t7,  space1);
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);
StructuredBuffer<QuantaDeformation>   deformIn    : register(t9,  space1);
RWStructuredBuffer<QuantaDeformation> deformOut   : register(u10, space1);
RWStructuredBuffer<float>             materialGridSDF : register(u11, space1);
StructuredBuffer<VoxelL1>             voxelsL1    : register(t12, space1);
RWStructuredBuffer<BrushAccumulator>  brushAccumulator : register(u24, space1);

#define GROUP_SIZE 512

[numthreads(8, 8, 8)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint localIndex  = GTid.x + GTid.y * 8 + GTid.z * 64;
    uint globalIndex = Gid.x * GROUP_SIZE + localIndex;

    if (globalIndex >= QUANTA_COUNT)
        return;

    uint qIdx = quantaIds[globalIndex];
    Quanta quanta = quantaIn[qIdx];

    int brushId = quanta.information.x - 1;

    // Skipped quanta must still be copied through: with the In/Out ping-pong,
    // any slot not written this frame would carry two-frame-old state forward.
    if (quanta.position.w < 1.0f || brushId < 0)
    {
        quantaOut[qIdx] = quanta;
        deformOut[qIdx] = deformIn[qIdx];
        return;
    }

    QuantaUnseal(quanta, Brushes[brushId]);

    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridRes = GetMaterialGridSize();
    float3 cellSize  = sceneSize / float3(gridRes);

    float3 pos = quanta.position.xyz;

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

    float3 velocitySum = 0;
    float3x3 B = 0.0f;
    float wsum = 0.0f;
    float3 m1 = 0.0f;
    float3x3 M2 = 0.0f;
    [unroll]
    for (int i = 0; i < 3; i++)
    {
        [unroll]
        for (int j = 0; j < 3; j++)
        {
            [unroll]
            for (int k = 0; k < 3; k++)
            {
                int3 cellCoordinate = base + int3(i, j, k);

                if (any(cellCoordinate < 0) || any(cellCoordinate >= gridRes))
                    continue;

                float weight = wx[i] * wy[j] * wz[k];
                int idx = Flatten3D(cellCoordinate, gridRes);
                float3 nodePos = float3(cellCoordinate) * cellSize - halfScene;
                float3 dx = nodePos - pos;

                float nodeMass = materialGrid[idx].massMomentum.w;
                if (nodeMass <= 0.0f)
                    continue;

                float3 velocity = materialGrid[idx].massMomentum.xyz / nodeMass;

                velocitySum += weight * velocity;
                B += weight * Outer(velocity, dx);
                wsum += weight;
                m1 += weight * dx;
                M2 += weight * Outer(dx, dx);
            }
        }
    }
    wsum = max(wsum, 1e-6f);
    velocitySum /= wsum;

    //Solve C against the stencil's actual moments: translation-invariant and exact
    //for linear fields even on truncated boundary stencils.
    float3x3 M = M2 - Outer(m1, m1) / wsum + (1e-4f * cellSize.x * cellSize.x) * IDENTITY_MATRIX3_3;
    float3x3 C = mul(B - Outer(velocitySum, m1), inverse(M));

    //Expand Quanta include center mass.
    int posX = (int)round(pos.x * FIXED_POINT_SCALE);
    int posY = (int)round(pos.y * FIXED_POINT_SCALE);
    int posZ = (int)round(pos.z * FIXED_POINT_SCALE);

    int dummyVal;
    InterlockedAdd(brushAccumulator[brushId].bcentroid.w, 1, dummyVal);
    InterlockedAdd(brushAccumulator[brushId].bcentroid.x, posX, dummyVal);
    InterlockedAdd(brushAccumulator[brushId].bcentroid.y, posY, dummyVal);
    InterlockedAdd(brushAccumulator[brushId].bcentroid.z, posZ, dummyVal);

    int velX = (int)round(velocitySum.x * FIXED_POINT_SCALE);
    int velY = (int)round(velocitySum.y * FIXED_POINT_SCALE);
    int velZ = (int)round(velocitySum.z * FIXED_POINT_SCALE);

    float massQ = 0.1f; // change to per quanta property.
    InterlockedAdd(brushAccumulator[brushId].velocity.w, massQ, dummyVal);
    InterlockedAdd(brushAccumulator[brushId].velocity.x, velX, dummyVal);
    InterlockedAdd(brushAccumulator[brushId].velocity.y, velY, dummyVal);
    InterlockedAdd(brushAccumulator[brushId].velocity.z, velZ, dummyVal);

    deformOut[qIdx].DeffGrad = deformIn[qIdx].DeffGrad;
    deformOut[qIdx].AffVel = C;
    deformOut[qIdx].CandidateDeff = pc.dt * C;

    quanta.mana.xyz = velocitySum;

    QuantaSeal(quanta, Brushes[brushId]);

    quantaOut[qIdx] = quanta;
}
