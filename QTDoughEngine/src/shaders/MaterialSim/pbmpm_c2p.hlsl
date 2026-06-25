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

#define GROUP_SIZE 512

[numthreads(8, 8, 8)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint localIndex  = GTid.x + GTid.y * 8 + GTid.z * 64;
    uint globalIndex = Gid.x * GROUP_SIZE + localIndex;

    if (globalIndex >= QUANTA_COUNT)
        return;

    uint qIdx = quantaIds[globalIndex];
    Quanta quanta = quantaOut[qIdx];
    int brushId = quanta.information.x - 1;
    //if (brushId >= 0)
    //    pos = mul(Brushes[brushId].model, float4(pos, 1.0f)).xyz;

    if (brushId < 0)
    {
        quantaOut[qIdx] = quanta;
        deformOut[qIdx] = deformIn[qIdx];
        return;
    }

    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridRes = GetMaterialGridSize();
    float3 cellSize  = sceneSize / float3(gridRes);

    float3 pos = quanta.position.xyz;

        

    float3 gsc = (pos + halfScene) / cellSize - 0.5f;
    int3 base = int3(floor(gsc));
    float3 fx = gsc - float3(base);

    float wx[2], wy[2], wz[2];
    wx[0] = 1.0f - fx.x; wx[1] = fx.x;
    wy[0] = 1.0f - fx.y; wy[1] = fx.y;
    wz[0] = 1.0f - fx.z; wz[1] = fx.z;

    float3 velocitySum = 0;
    float3x3 B = 0.0f;
    float3x3 D = 0.0f;
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
                float weight = wx[i] * wy[j] * wz[k];
                float3 centerPos = (float3(centerCoord) + 0.5f) * cellSize - halfScene;
                float3 dx = centerPos - pos;

                float3 centerVel = 0;
                [unroll]
                for (int a = 0; a < 2; a++)
                {
                    [unroll]
                    for (int b = 0; b < 2; b++)
                    {
                        [unroll]
                        for (int d = 0; d < 2; d++)
                        {
                            int3 nodeCoord = centerCoord + int3(a, b, d);
                            if (any(nodeCoord < 0) || any(nodeCoord >= gridRes))
                                continue;
                            int idx = Flatten3D(nodeCoord, gridRes);
                            float nodeMass = materialGrid[idx].massMomentum.w;
                            if (nodeMass <= 0.0f)
                                continue;
                            centerVel += 0.125f * (materialGrid[idx].massMomentum.xyz / nodeMass);
                        }
                    }
                }

                velocitySum += weight * centerVel;
                B += weight * Outer(centerVel, dx);
                D += weight * Outer(dx, dx);
            }
        }
    }
    float3x3 C = mul(B, inverse(D + IDENTITY_MATRIX3_3 * 1e-6f));

    deformOut[qIdx].DeffGrad = deformIn[qIdx].DeffGrad;
    deformOut[qIdx].AffVel = C;
    deformOut[qIdx].CandidateDeff = pc.dt * C;

    quanta.mana.xyz = velocitySum;

    quantaOut[qIdx] = quanta;
}
