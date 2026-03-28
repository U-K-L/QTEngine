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

// Binding 8: MaterialGrid In (read).
StructuredBuffer<MaterialGridPoint> materialGridIn : register(t8, space1);

// Binding 20: MaterialGrid Out (write).
RWStructuredBuffer<MaterialGridPoint> materialGridOut : register(u20, space1);

// Dispatched per grid cell: (materialGridSize / 8) per axis.
[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int3 gridRes = GetMaterialGridSize();

    if (any(DTid >= (uint3)gridRes))
        return;

    int3 c = int3(DTid);
    int cellIdx = Flatten3D(c, gridRes);

    MaterialGridPoint center = materialGridIn[cellIdx];
    

    
    float manaSum = 0;
    float weightSum = 0;

    // 3x3x3 B-spline weighted stencil.
    [unroll]
    for (int dz = -1; dz <= 1; dz++)
    {
        [unroll]
        for (int dy = -1; dy <= 1; dy++)
        {
            [unroll]
            for (int dx = -1; dx <= 1; dx++)
            {
                int3 n = c + int3(dx, dy, dz);

                if (any(n < 0) || any(n >= gridRes))
                    continue;

                float w = BSplineWeight(dx) * BSplineWeight(dy) * BSplineWeight(dz);
                int nIdx = Flatten3D(n, gridRes);
                MaterialGridPoint nb = materialGridIn[nIdx];
                manaSum += nb.fieldValues.y * w;
                weightSum += w;

            }
        }
    }
    
    float alpha = 1.0f - exp(-DIFFUSION_RATE * deltaTime);
    center.fieldValues.y = lerp(center.fieldValues.y, manaSum / weightSum, alpha);
    //Natural cooling, this is basically empty space, during G2P quanta retains heat.
    center.fieldValues.y *= exp(-0.25f * deltaTime);
    
    center.fieldValues.y = clamp(center.fieldValues.y, 0.0f, 1048576.0f);


    materialGridOut[cellIdx] = center;
}
