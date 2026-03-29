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

// Lepton buffer.
StructuredBuffer<Lepton> leptonIn : register(t13, space1);

// Accumulator (atomic int scatter target).
RWStructuredBuffer<MaterialGridAccumulator> accumulator : register(u21, space1);

// Dispatched per lepton: (leptonMaxSize / 256, 1, 1).
[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint globalIndex = DTid.x;
    if (globalIndex >= LEPTON_COUNT)
        return;

    Lepton l = leptonIn[globalIndex];

    if (l.position.w <= 0 || l.mana.w <= 0)
        return;

    float radius = l.direction.w;
    if (radius <= 0)
        return;

    float3 pos = l.position.xyz;

    int3 gridRes = GetMaterialGridSize();
    float3 sceneSize = GetSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    float3 cellSize = sceneSize / float3(gridRes);

    int3 minVoxel = int3(floor((pos - radius + halfScene) / cellSize));
    int3 maxVoxel = int3(floor((pos + radius + halfScene) / cellSize));
    minVoxel = clamp(minVoxel, int3(0, 0, 0), gridRes - 1);
    maxVoxel = clamp(maxVoxel, int3(0, 0, 0), gridRes - 1);

    for (int z = minVoxel.z; z <= maxVoxel.z; z++)
        for (int y = minVoxel.y; y <= maxVoxel.y; y++)
            for (int x = minVoxel.x; x <= maxVoxel.x; x++)
            {
                int3 voxelCoord = int3(x, y, z);
                float3 cellCenter = (float3(voxelCoord) + 0.5f) * cellSize - halfScene;

                float dist = length(cellCenter - pos);
                if (dist > radius)
                    continue;

                float weight = 1.0f - (dist / radius);
                int contribution = (int)round(weight * l.mana.x * FIXED_POINT_SCALE);
                int idx = Flatten3D(voxelCoord, gridRes);

                int dummy;
                InterlockedAdd(accumulator[idx].fieldValues.y, contribution, dummy);
            }
}
