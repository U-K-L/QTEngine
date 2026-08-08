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

        float3 relativePosition = quanta.position - bcentroid;
        int dummyVal;

        if (count > 0)
        {
            float3 meanVelocity = (float3)brushAccumulator[brushId].velocity.xyz * invCount * invScale;
            float3 relativeVelocity = quanta.mana.xyz - meanVelocity;

            //Affine velocity already contains the angular velocity, just extract it and multiply by interia tensor.
            //NEEDS MASS.
            float mass = 1.0f;
            
            float3x3 affineVelocity = deformOut[globalIndex].AffVel;
            float3 spin = 0.5f * float3(
                affineVelocity[2][1] - affineVelocity[1][2],
                affineVelocity[0][2] - affineVelocity[2][0],
                affineVelocity[1][0] - affineVelocity[0][1]);
            
            float particleInertia = 2.0f / INERTIA_TENSOR_INVERSE[0][0];

            float3 angularMomentum = mass * cross(relativePosition, relativeVelocity) + mass * particleInertia * spin;

            InterlockedAdd(brushAccumulator[brushId].angularMomentum.x, (int) round(angularMomentum.x * FIXED_POINT_SCALE), dummyVal);
            InterlockedAdd(brushAccumulator[brushId].angularMomentum.y, (int) round(angularMomentum.y * FIXED_POINT_SCALE), dummyVal);
            InterlockedAdd(brushAccumulator[brushId].angularMomentum.z, (int) round(angularMomentum.z * FIXED_POINT_SCALE), dummyVal);

            float radiusSquared = dot(relativePosition, relativePosition);
            InterlockedAdd(brushAccumulator[brushId].inertiaDiag.x, (int)round((radiusSquared - relativePosition.x * relativePosition.x + particleInertia) * FIXED_POINT_SCALE), dummyVal);
            InterlockedAdd(brushAccumulator[brushId].inertiaDiag.y, (int)round((radiusSquared - relativePosition.y * relativePosition.y + particleInertia) * FIXED_POINT_SCALE), dummyVal);
            InterlockedAdd(brushAccumulator[brushId].inertiaDiag.z, (int)round((radiusSquared - relativePosition.z * relativePosition.z + particleInertia) * FIXED_POINT_SCALE), dummyVal);
            InterlockedAdd(brushAccumulator[brushId].inertiaOffDiag.x, (int)round((-relativePosition.x * relativePosition.y) * FIXED_POINT_SCALE), dummyVal);
            InterlockedAdd(brushAccumulator[brushId].inertiaOffDiag.y, (int)round((-relativePosition.x * relativePosition.z) * FIXED_POINT_SCALE), dummyVal);
            InterlockedAdd(brushAccumulator[brushId].inertiaOffDiag.z, (int)round((-relativePosition.y * relativePosition.z) * FIXED_POINT_SCALE), dummyVal);
        }
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
