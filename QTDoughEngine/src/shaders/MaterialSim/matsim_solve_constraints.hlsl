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
StructuredBuffer<QuantaDeformation> deformIn : register(t9, space1);
RWStructuredBuffer<QuantaDeformation> deformOut : register(u10, space1);

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;
    if (globalIndex >= QUANTA_COUNT)
        return;

    QuantaDeformation deform = deformIn[globalIndex];

    float beta = 0.125f;

    float3x3 Fquanta = deform.DeffGrad;
    float3x3 Dquanta = deform.CandidateDeff;

    //Elastic only for now.
    float3x3 Fstar = mul(Fquanta, IDENTITY_MATRIX3_3 + Dquanta);

    float3x3 Ashape = PolarRotation(Fstar);

    float Fdeterminant = max(0.001f, determinant(Fstar));

    float3x3 Avol = Fstar / pow(Fdeterminant, 1.0f / 3.0f);

    Dquanta = mul(inverse(Fquanta), mul(Avol, beta) + mul((1.0f - beta), Ashape)) - IDENTITY_MATRIX3_3;

    deform.CandidateDeff = Dquanta;

    deformOut[globalIndex] = deform;
}
