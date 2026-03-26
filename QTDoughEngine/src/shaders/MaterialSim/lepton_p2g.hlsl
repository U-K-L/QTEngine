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

// Lepton buffers.
StructuredBuffer<Lepton>              leptonIn          : register(t13, space1);
RWStructuredBuffer<uint>              leptonIds         : register(u16, space1);
RWStructuredBuffer<uint>              leptonTileCounts  : register(u17, space1);
RWStructuredBuffer<uint>              leptonTileOffsets : register(u18, space1);

// Material grid (accumulation target).
RWStructuredBuffer<MaterialGridPoint> materialGrid      : register(u8, space1);

// Dispatched per grid cell: (materialGridSize / 8) per axis.
[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int3 gridRes = GetMaterialGridSize();

    if (any(DTid >= (uint3)gridRes))
        return;

    int3 cellCoord = int3(DTid);
    int cellIdx = Flatten3D(cellCoord, gridRes);

    // World position of this grid cell center.
    float3 sceneSize = GetSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    float3 cellSize  = sceneSize / float3(gridRes);
    float3 cellCenter = (float3(cellCoord) + 0.5f) * cellSize - halfScene;

    // Which tile does this cell belong to?
    int3 tileGrid = int3(pc.tileGridX, pc.tileGridY, pc.tileGridZ);
    int3 cellTile = int3(floor((cellCenter + halfScene) / TILE_SIZE));
    cellTile = clamp(cellTile, int3(0, 0, 0), tileGrid - 1);
    materialGrid[cellIdx].fieldValues.y = 0.0f;
    // Walk neighboring tiles to gather all nearby leptons.
    [loop]
    for (int ti = -1; ti <= 1; ti++)
    {
        [loop]
        for (int tj = -1; tj <= 1; tj++)
        {
            [loop]
            for (int tk = -1; tk <= 1; tk++)
            {
                int3 neighborTile = cellTile + int3(ti, tj, tk);

                if (any(neighborTile < 0) || any(neighborTile >= tileGrid))
                    continue;

                uint tileIdx = (uint)Flatten3D(neighborTile, tileGrid);
                uint tileStart = leptonTileOffsets[tileIdx];
                uint tileEnd   = tileStart + leptonTileCounts[tileIdx];

                for (uint s = tileStart; s < tileEnd; s++)
                {
                    uint lIdx = leptonIds[s];
                    Lepton l = leptonIn[lIdx];

                    // Skip unclaimed or dead.
                    if (l.position.w < 0 || l.mana.w <= 0)
                        continue;

                    // TODO: Compute weight from lepton position to cellCenter and accumulate into materialGrid[cellIdx].
                    materialGrid[cellIdx].fieldValues.y = 1.1f;
                }
            }
        }
    }
}
