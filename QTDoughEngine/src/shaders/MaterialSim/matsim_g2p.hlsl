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

// Set 1 bindings MaterialSimulation buffers.
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

#define GROUP_SIZE 512

[numthreads(8, 8, 8)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint localIndex  = GTid.x + GTid.y * 8 + GTid.z * 64;
    uint globalIndex = Gid.x * GROUP_SIZE + localIndex;

    if (globalIndex >= QUANTA_COUNT)
        return;

    // Use tile-sorted index for cache coherence.
    uint qIdx = quantaIds[globalIndex];
    Quanta quanta = quantaIn[qIdx];

    // Skip inactive particles.
    if (quanta.position.w < 1.0f)
    {
        quantaOut[qIdx] = quanta;
        return;
    }

    // --- Grid constants ---
    float3 sceneSize = GetMaterialSceneSize(); 
    float3 halfScene = sceneSize * 0.5f;
    int3 gridRes = GetMaterialGridSize();
    float3 cellSize  = sceneSize / float3(gridRes);

    // --- World-space position ---
    float3 pos = quanta.position.xyz;
    int brushId = quanta.information.x - 1;
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

    // --- 27-cell stencil loop ---
    float weightSum = 0;
    float3 velocitySum = 0;
    float3x3 B = 0.0f;
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
                weightSum += weight;
            }
        }
    }
    float3x3 C = mul(B, INERTIA_TENSOR_INVERSE);


    float3x3 F = deformIn[qIdx].DeffGrad;
    float3x3 Fnew = mul(IDENTITY_MATRIX3_3 + pc.dt * C, F);
    deformOut[qIdx].AffVel = C;
    deformOut[qIdx].DeffGrad = Fnew;

    quanta.mana.xyz = velocitySum;

    float3 posNew = pos + pc.dt * velocitySum;

    float4 localPos = mul(Brushes[brushId].invModel, float4(posNew, 1.0f));
    quanta.position.xyz = localPos.xyz;

    quanta.mana.w = clamp(length(quanta.mana.xyz), 0.0f, 10000.0f);
    //if(quanta.mana.w > 0.01f) // excited!
        quanta.information.z += 1; //Ledger.
/*
    q.mana.w += ((manaSum / weightSum) - q.mana.w) * deltaTime * 0.1f;
    q.mana.w = clamp(q.mana.w, 0.0f, 10000.0f);
    if(q.mana.w > 0.01f) // excited!
        q.information.z += 1; //Ledger.
*/
    quantaOut[qIdx] = quanta;
}
