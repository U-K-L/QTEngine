#include "../Helpers/ShaderHelpers.hlsl"

RWStructuredBuffer<BrushMatrix> brushMatricies : register(u23, space1);
RWStructuredBuffer<BrushAccumulator> brushAccumulator : register(u24, space1);
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);

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
        brushMatricies[b].angularMomentum = float4(0, 0, 0, 0);
        return;
    }

    float invScale = 1.0f / (float)FIXED_POINT_SCALE;
    float invCount = 1.0f / (float)count;
        
    float3 bcentroid = (float3)brushAccumulator[b].bcentroid.xyz * invCount * invScale;
    float3 velocity = (float3) brushAccumulator[b].velocity.xyz * invCount * invScale;
    float3 angularMomentum = (float3) brushAccumulator[b].angularMomentum.xyz * invCount * invScale;

    brushMatricies[b].bCentroid = float4(bcentroid.xyz, count);
    //brushMatricies[b].velocity = float4(velocity.xyz, brushMatricies[b].velocity.w);

    // Read the material grid at the brush centroid.
    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    int3 gridRes = GetMaterialGridSize();
    float3 cellSize = sceneSize / float3(gridRes);
    int3 centerCell = clamp(int3(floor((bcentroid + halfScene) / cellSize)), int3(0, 0, 0), gridRes - 1);

    //Kernel:
    int kernelSize = 1;
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
  MaterialGridPoint centroidPoint = materialGrid[Flatten3D(centerCell, gridRes)];
  massMomentumSum = centroidPoint.massMomentum;

  float3 inertiaDiagonal = (float3)brushAccumulator[b].inertiaDiag.xyz * invCount * invScale;
  float3 inertiaOffDiagonal = (float3)brushAccumulator[b].inertiaOffDiag.xyz * invCount * invScale;

  float3x3 inertiaTensor = float3x3(
      inertiaDiagonal.x, inertiaOffDiagonal.x, inertiaOffDiagonal.y,
      inertiaOffDiagonal.x, inertiaDiagonal.y, inertiaOffDiagonal.z,
      inertiaOffDiagonal.y, inertiaOffDiagonal.z, inertiaDiagonal.z);

  inertiaTensor += (1e-4f * max(inertiaDiagonal.x + inertiaDiagonal.y + inertiaDiagonal.z, 1e-6f)) * IDENTITY_MATRIX3_3;

  float3 angularVelocity = mul(inverse(inertiaTensor), angularMomentum);

  float angularSpeed = length(angularVelocity);
  float maxAngularSpeed = 10.0f;
  if (angularSpeed > maxAngularSpeed)
      angularVelocity *= maxAngularSpeed / angularSpeed;

  float gm = massMomentumSum.w;

  //Snap to rest after sustained stillness; counter lives in angularMomentum.w.
 /*
  float restFrames = brushMatricies[b].angularMomentum.w;
  bool still = dot(velocity, velocity) < 0.0025f && dot(angularVelocity, angularVelocity) < 0.0025f;
  restFrames = still ? restFrames + 1.0f : 0.0f;

  if (restFrames > 10.0f)
  {
      velocity = 0.0f;
      angularVelocity = 0.0f;
  }
*/
  brushMatricies[b].velocity = float4(velocity, gm);
  brushMatricies[b].angularMomentum = float4(angularVelocity, 0);
}