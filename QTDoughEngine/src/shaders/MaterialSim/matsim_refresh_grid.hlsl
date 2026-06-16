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
    float pad0;
    float pad1;
    float pad2;
} pc;

RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);
RWStructuredBuffer<MaterialGridPoint> materialGridOut : register(u20, space1);

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int3 gridResolution = GetMaterialGridSize();
    int cellId = Flatten3D(int3(DTid), gridResolution);

    MaterialGridPoint p = (MaterialGridPoint)0;

    materialGrid[cellId] = p;
    materialGridOut[cellId] = p;
}
