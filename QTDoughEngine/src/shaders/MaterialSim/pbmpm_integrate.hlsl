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

    Quanta quanta = quantaIn[globalIndex];

    if (quanta.position.w < 1.0f)
    {
        quantaOut[globalIndex] = quanta;
        deformOut[globalIndex] = deformIn[globalIndex];
        return;
    }

    float3x3 F = deformIn[globalIndex].DeffGrad;
    float3x3 D = deformIn[globalIndex].CandidateDeff;

    float3x3 Fstar = mul(F, IDENTITY_MATRIX3_3 + D);

    if (determinant(Fstar) <= 0.0f)
        Fstar = IDENTITY_MATRIX3_3;

    float3 pos = quanta.position.xyz;
    int brushId = quanta.information.x - 1;
    if (brushId >= 0)
        pos = mul(Brushes[brushId].model, float4(pos, 1.0f)).xyz;

    float3 posNew = pos + pc.dt * quanta.mana.xyz;

    //posNew.x = QuantizeDown(posNew.x, 0.01f);
    //posNew.y = QuantizeDown(posNew.y, 0.01f);
    //posNew.z = QuantizeDown(posNew.z, 0.01f);


        
    //Averaged position (world space).
    int posX = (int) round(posNew.x * FIXED_POINT_SCALE);
    int posY = (int) round(posNew.y * FIXED_POINT_SCALE);
    int posZ = (int) round(posNew.z * FIXED_POINT_SCALE);

    if (brushId >= 0 && brushId < MAX_BRUSHES)
    {
        int dummyVal;
        InterlockedAdd(brushAccumulator[brushId].count, 1, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumX, posX, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumY, posY, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].posSumZ, posZ, dummyVal);
    }


/*
    if (brushId >= 0)
        quanta.position.xyz = mul(Brushes[brushId].invModel, float4(posNew, 1.0f)).xyz;
    else
        quanta.position.xyz = posNew;

*/

    deformOut[globalIndex].DeffGrad = Fstar;
    deformOut[globalIndex].AffVel = deformIn[globalIndex].AffVel;
    deformOut[globalIndex].CandidateDeff = (float3x3)0;

    quantaOut[globalIndex] = quanta;
}
