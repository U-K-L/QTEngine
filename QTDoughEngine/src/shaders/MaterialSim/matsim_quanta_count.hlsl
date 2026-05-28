#include "../Helpers/ShaderHelpers.hlsl"

[[vk::push_constant]]
struct PushConsts
{
    float particleSize;
    int tileGridX;
    int tileGridY;
    int tileGridZ;
    int brushIndex;
    float pad0;
    float pad1;
    float pad2;
} pc;

StructuredBuffer<Quanta> quantaIn : register(t2, space1);
RWStructuredBuffer<uint> brushQuantaCounts : register(u22, space1);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x;
    if (idx >= QUANTA_COUNT)
        return;

    Quanta q = quantaIn[idx];

    if (q.position.w < 1)
        return;

    int brushId = q.information.x;
    if (brushId > 0 && brushId < 256)
    {
        InterlockedAdd(brushQuantaCounts[brushId], 1);
    }
}
