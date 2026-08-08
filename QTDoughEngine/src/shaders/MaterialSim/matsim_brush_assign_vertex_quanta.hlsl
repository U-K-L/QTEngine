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

RWStructuredBuffer<Quanta> quantaOut : register(u1, space1);
RWStructuredBuffer<uint> quantaIds : register(u3, space1);
RWStructuredBuffer<uint> tileCounts : register(u4, space1);
RWStructuredBuffer<uint> tileOffsets : register(u5, space1);
StructuredBuffer<Brush> Brushes : register(t7, space1);
RWStructuredBuffer<Vertex> vertexBuffer : register(u25, space1);

[numthreads(8, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int nearestIds[4] = { -1, -1, -1, -1 };
    float nearestDist[4] = { 1e30f, 1e30f, 1e30f, 1e30f };
    
    Brush brush = Brushes[pc.brushIndex];

    if (DTid.x >= brush.vertexCount)
        return;

    uint vertexId = brush.vertexOffset + DTid.x;
    float3 localVertexPosition = vertexBuffer[vertexId].position.xyz;
    
    //Need the center of this tile the vertex is in the search this bin acceelration struct, make it go brr.
    int3 tileGridSize = int3(pc.tileGridX, pc.tileGridY, pc.tileGridZ);
    float3 halfField = float3(tileGridSize) * (QUANTA_TILE_SIZE * 0.5f); //Shift to find center.
    
    int3 centerTile = clamp((int3) floor((localVertexPosition + halfField) / QUANTA_TILE_SIZE), int3(0, 0, 0), tileGridSize - 1);
    
    int kernelSize = 1;
    
    for (int i = -kernelSize; i < kernelSize; i++)
    {
        for (int j = -kernelSize; j < kernelSize; j++)
        {
            for (int k = -kernelSize; k < kernelSize; k++)
            {
                int3 tile = centerTile + int3(i, j, k);
                if (any(tile < 0) || any(tile >= tileGridSize))
                    continue;
                
                uint TileIndex = (uint) Flatten3D(tile, tileGridSize);
                
                //Start and end for the quanta in this bin.
                uint start = tileOffsets[TileIndex];
                uint end = start + tileCounts[TileIndex];
                
                for (uint s = start; s < end; s++)
                {
                    uint quantaID = quantaIds[s];
                    
                    Quanta quanta = quantaOut[quantaID];

                    if (quanta.information.x != (int) brush.id)
                        continue;
                    
                    
                    float difference = distance(quanta.position.xyz, localVertexPosition);

                    if (difference >= nearestDist[3])
                        continue;
                    
                    int slot = 3;
                    while (slot > 0 && nearestDist[slot - 1] > difference)
                    {
                        nearestDist[slot] = nearestDist[slot - 1];
                        nearestIds[slot] = nearestIds[slot - 1];
                        slot--;
                    }
                    nearestDist[slot] = difference;
                    nearestIds[slot] = (int) quantaID;
                }

            }
        }
    }
    vertexBuffer[vertexId].quantaIDs = int4(nearestIds[0], nearestIds[1], nearestIds[2], nearestIds[3]);
}
