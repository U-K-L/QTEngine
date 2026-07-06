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

StructuredBuffer<Quanta> quantaIn                  : register(t0,  space1);
RWStructuredBuffer<Quanta> quantaOut               : register(u1,  space1);
StructuredBuffer<Quanta> quantaRead                : register(t2,  space1);
RWStructuredBuffer<uint> quantaIds                 : register(u3,  space1);
RWStructuredBuffer<uint> tileCounts                : register(u4,  space1);
RWStructuredBuffer<uint> tileOffsets               : register(u5,  space1);
RWStructuredBuffer<uint> tileCursor                : register(u6,  space1);
StructuredBuffer<Brush> Brushes                    : register(t7,  space1);
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8,  space1);
StructuredBuffer<QuantaDeformation> deformIn       : register(t9,  space1);
RWStructuredBuffer<QuantaDeformation> deformOut    : register(u10, space1);
RWStructuredBuffer<float> materialGridSDF          : register(u11, space1);
StructuredBuffer<VoxelL1> voxelL1                  : register(t12, space1);
RWStructuredBuffer<MaterialGridPoint> materialGridOut : register(u20, space1);
RWStructuredBuffer<MaterialGridAccumulator> accumulator : register(u21, space1);
RWStructuredBuffer<BrushMatrix> brushMatricies     : register(u23, space1);
RWStructuredBuffer<BrushAccumulator> brushAccumulator : register(u24, space1);

[numthreads(512, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;

    if (globalIndex >= QUANTA_COUNT)
        return;


    Quanta quanta = quantaOut[globalIndex];

    int brushId = quanta.information.x - 1;

    if (brushId < 0)
        return;

    Brush brush = Brushes[brushId];

    QuantaUnseal(quanta, brush);

    if (brush.interactiveType == 1)
        return;

    float3 pos = quanta.position.xyz;

    float3 posNew = pos + pc.dt * quanta.mana.xyz;

    float k = -0.25f;
    float3 displacement = (posNew - quanta.canonicalPosition.xyz);
    float3 springForce = k * displacement;

    float l = distance(posNew, quanta.canonicalPosition.xyz);

    quanta.position.xyz = lerp(posNew, quanta.canonicalPosition.xyz, saturate(l));

    QuantaSeal(quanta, brush);

    quantaOut[globalIndex] = quanta;
}
