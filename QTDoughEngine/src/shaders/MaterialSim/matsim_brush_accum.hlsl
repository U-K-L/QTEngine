#include "../Helpers/ShaderHelpers.hlsl"

RWStructuredBuffer<BrushMatrix> brushMatricies : register(u23, space1);
RWStructuredBuffer<BrushAccumulator> brushAccumulator : register(u24, space1);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint b = DTid.x;
    if (b >= MAX_BRUSHES)
        return;

    uint count = brushAccumulator[b].count;
    if (count == 0)
    {
        brushMatricies[b].bCentroid = float4(0, 0, 0, 0);
        return;
    }

    float invScale = 1.0f / (float)FIXED_POINT_SCALE;
    float invCount = 1.0f / (float)count;
    float cx = (float)(int)brushAccumulator[b].posSumX * invCount * invScale;
    float cy = (float)(int)brushAccumulator[b].posSumY * invCount * invScale;
    float cz = (float)(int)brushAccumulator[b].posSumZ * invCount * invScale;

    brushMatricies[b].bCentroid = float4(cx, cy, cz, (float)count);
}
