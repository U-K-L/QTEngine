#include "../Helpers/ShaderHelpers.hlsl"

[[vk::push_constant]]
struct PushConsts
{
    int brushIndex;
    int tileGridX;
    int tileGridY;
    int tileGridZ;
} pc;

RWStructuredBuffer<Brush> Brushes : register(u9, space1);
RWStructuredBuffer<MaterialBrushPoint> materialBrushPoints : register(u23, space1);

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid >= MATERIAL_BRUSH_GRID_RES))
        return;

    Brush brush = Brushes[pc.brushIndex];

    int3 cellCoord = int3(DTid);
    int gridSize = MATERIAL_BRUSH_GRID_RES * MATERIAL_BRUSH_GRID_RES * MATERIAL_BRUSH_GRID_RES;
    int flatIdx = Flatten3D(cellCoord, int3(MATERIAL_BRUSH_GRID_RES, MATERIAL_BRUSH_GRID_RES, MATERIAL_BRUSH_GRID_RES));
    int mbpIdx = pc.brushIndex * gridSize + flatIdx;

    // Check neighbors (kernel size 2) — if any neighbor has occupancy, decrement slower.
    int3 gridRes = int3(MATERIAL_BRUSH_GRID_RES, MATERIAL_BRUSH_GRID_RES, MATERIAL_BRUSH_GRID_RES);
    int kernelSize = 1;
    int neighborOccupied = 0;

    for (int dz = -kernelSize; dz <= kernelSize && !neighborOccupied; dz++)
        for (int dy = -kernelSize; dy <= kernelSize && !neighborOccupied; dy++)
            for (int dx = -kernelSize; dx <= kernelSize && !neighborOccupied; dx++)
            {
                if (dx == 0 && dy == 0 && dz == 0)
                    continue;
                int3 nc = cellCoord + int3(dx, dy, dz);
                if (any(nc < 0) || any(nc >= MATERIAL_BRUSH_GRID_RES))
                    continue;
                int nIdx = pc.brushIndex * gridSize + Flatten3D(nc, gridRes);
                //neighborOccupied += materialBrushPoints[nIdx].information.z;
            }

    // Slowly decay. Scatter tops it back up each frame quanta are present.
    int val = materialBrushPoints[mbpIdx].information.y;
    neighborOccupied += materialBrushPoints[mbpIdx].information.x;
    if (brush.isDeformed)
        materialBrushPoints[mbpIdx].information.y = val - 5;
}
