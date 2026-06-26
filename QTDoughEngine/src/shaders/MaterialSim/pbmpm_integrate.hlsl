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

    float3x3 F = deformIn[globalIndex].DeffGrad;
    float3x3 D = deformIn[globalIndex].CandidateDeff;

    float3x3 Fstar = mul(F, IDENTITY_MATRIX3_3 + D);

    if (determinant(Fstar) <= 0.0f)
        Fstar = IDENTITY_MATRIX3_3;


    int brushId = quanta.information.x - 1;


    if (brushId >= 0)
    {
        QuantaUnseal(quanta, Brushes[brushId]);

        float3 pos = quanta.position.xyz;
        
        int posX = (int) round(pos.x * FIXED_POINT_SCALE);
        int posY = (int) round(pos.y * FIXED_POINT_SCALE);
        int posZ = (int) round(pos.z * FIXED_POINT_SCALE);
        
        int dummyVal;
        InterlockedAdd(brushAccumulator[brushId].bcentroid.w, 1, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].bcentroid.x, posX, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].bcentroid.y, posY, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].bcentroid.z, posZ, dummyVal);
        
        float3 velocity = quanta.mana.xyz;
        int velX = (int) round(velocity.x * FIXED_POINT_SCALE);
        int velY = (int) round(velocity.y * FIXED_POINT_SCALE);
        int velZ = (int) round(velocity.z * FIXED_POINT_SCALE);
        
        InterlockedAdd(brushAccumulator[brushId].velocity.w, 1, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].velocity.x, velX, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].velocity.y, velY, dummyVal);
        InterlockedAdd(brushAccumulator[brushId].velocity.z, velZ, dummyVal);
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
