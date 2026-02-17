#include "../Helpers/ShaderHelpers.hlsl"

Texture3D<float> gBindless3D[] : register(t4, space0);

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

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // 1:1 load from the matching SDF mip (texture ID passed via pc.brushIndex)
    float sdf = gBindless3D[pc.brushIndex].Load(int4(DTid, 0));
    uint idx = DTid.x + DTid.y * 256 + DTid.z * 256 * 256;
    materialGrid[idx].fieldValues.x = sdf;
}
