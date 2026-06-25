#include "../Helpers/ShaderHelpers.hlsl"

RWStructuredBuffer<BrushMatrix> brushMatricies : register(u23, space1);
RWStructuredBuffer<BrushAccumulator> brushAccumulator : register(u24, space1);
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u20, space1);

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint b = DTid.x;
    if (b >= MAX_BRUSHES)
        return;

    uint count = brushAccumulator[b].bcentroid.w;
    if (count == 0)
    {
        brushMatricies[b].bCentroid = float4(0, 0, 0, 0);
        return;
    }

    float invScale = 1.0f / (float)FIXED_POINT_SCALE;
    float invCount = 1.0f / (float)count;
        
    float3 bcentroid = (float3)brushAccumulator[b].bcentroid.xyz * invCount * invScale;
    float3 velocity = (float3) brushAccumulator[b].velocity.xyz * invCount * invScale;

    brushMatricies[b].bCentroid = float4(bcentroid.xyz, count);
    brushMatricies[b].velocity = float4(velocity.xyz, count);

        
    /*
    // Read the material grid at the brush centroid.
    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridRes = GetMaterialGridSize();
    float3 cellSize = sceneSize / float3(gridRes);
    int3 centerCell = clamp(int3(floor((float3(cx, cy, cz) + halfScene) / cellSize)), int3(0, 0, 0), gridRes - 1);

    //Kernel:
    int kernelSize = 3;
    float4 massMomentumSum = 0;
    
    for(int i = -kernelSize; i < kernelSize; i++)
    {
        for(int j = -kernelSize; j < kernelSize; j++)
        {
            for(int k = -kernelSize; k < kernelSize; k++)
            {
                int3 centerCoord = centerCell + int3(i, j, k);

                if (any(centerCoord < 0) || any(centerCoord >= gridRes))
                    continue;

                MaterialGridPoint centroidPoint = materialGrid[Flatten3D(centerCoord, gridRes)];
                massMomentumSum += centroidPoint.massMomentum;
            }
        }
    }

  
  massMomentumSum /= pow(kernelSize, 3);

  float gm = massMomentumSum.w;
  
  float3 gv = (gm > 0.0f) ? (massMomentumSum.xyz / gm) : float3(0.0f, 0.0f, 0.0f);

  //brushMatricies[b].velocity = float4(gv, gm);
    */
}