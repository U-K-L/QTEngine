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

// Binding 8: MaterialGrid (read/write).
RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);

// Dispatched per grid cell: (materialGridSize / 8) per axis.
[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int3 gridRes = GetMaterialGridSize();

    if (any(DTid >= (uint3)gridRes))
        return;

    int3 c = int3(DTid);
    int idx = Flatten3D(c, gridRes);

    MaterialGridPoint cell = materialGrid[idx];

    float mass = cell.massMomentum.w;
    if (mass < 1e-6f)
    {
        materialGrid[idx] = cell;
        return;
    }

    // --- Compute velocity from momentum ---
    float3 velocity = cell.massMomentum.xyz / mass;

    // --- External forces ---
    // TODO: gravity, user forces, etc.
    float3 gravity = float3(0, -9.8f, 0);
    velocity += gravity * deltaTime;

    // --- Internal forces (stress divergence) ---
    // TODO: compute stress divergence from neighboring cells
    // TODO: pressure, viscosity, elasticity, etc.

    // --- Boundary conditions ---
    // TODO: enforce domain boundaries, collision with SDF, etc.

    // --- Write back ---
    cell.velocity = float4(velocity, 0);
    materialGrid[idx] = cell;
}
