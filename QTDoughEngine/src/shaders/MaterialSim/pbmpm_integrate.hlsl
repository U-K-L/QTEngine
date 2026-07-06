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
StructuredBuffer<Brush> Brushes : register(t7, space1);
StructuredBuffer<QuantaDeformation> deformIn : register(t9, space1);
RWStructuredBuffer<QuantaDeformation> deformOut : register(u10, space1);
RWStructuredBuffer<BrushAccumulator> brushAccumulator : register(u24, space1);

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;
    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta quanta = quantaOut[globalIndex];

    float3x3 F = deformOut[globalIndex].DeffGrad;
    float3x3 D = deformOut[globalIndex].CandidateDeff;

    float3x3 Fstar = mul(F, IDENTITY_MATRIX3_3 + D);

    if (determinant(Fstar) <= 0.0f)
        Fstar = IDENTITY_MATRIX3_3;


    int brushId = quanta.information.x - 1;


    if (brushId >= 0)
    {
        QuantaUnseal(quanta, Brushes[brushId]);

        uint count = brushAccumulator[brushId].bcentroid.w;
        float invScale = 1.0f / (float)FIXED_POINT_SCALE;
        float invCount = 1.0f / (float)count;

        float3 bcentroid = (float3)brushAccumulator[brushId].bcentroid.xyz * invCount * invScale;

        float3 rs = quanta.position - bcentroid;
        int dummyVal;

        int RX = (int)round(rs.x * FIXED_POINT_SCALE);
        int RY = (int)round(rs.y * FIXED_POINT_SCALE);
        int RZ = (int)round(rs.z * FIXED_POINT_SCALE);

        float massQ = 0.1f; // change to per quanta property.
        InterlockedAdd(brushAccumulator[brushId].inertia.w, massQ, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].inertia.x, RX, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].inertia.y, RY, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].inertia.z, RZ, dummyVal);
    }

    //float3 posNew = pos + pc.dt * quanta.mana.xyz;


/*
    if (brushId >= 0)
        quanta.position.xyz = mul(Brushes[brushId].invModel, float4(posNew, 1.0f)).xyz;
    else
        quanta.position.xyz = posNew;

*/

    deformOut[globalIndex].DeffGrad = Fstar;
    deformOut[globalIndex].AffVel = deformIn[globalIndex].AffVel;
    deformOut[globalIndex].CandidateDeff = (float3x3)0;
}
