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
    float dt;
    float pad1;
    float pad2;
} pc;

RWStructuredBuffer<MaterialGridPoint> materialGrid : register(u8, space1);

float SampleGridSDF(int3 coord, int3 gridRes)
{
    coord = clamp(coord, int3(0, 0, 0), gridRes - 1);
    return materialGrid[Flatten3D(coord, gridRes)].fieldValues.x;
}

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int3 gridRes = GetMaterialGridSize();
    int cellId = Flatten3D(int3(DTid), gridRes);

    float mass = materialGrid[cellId].massMomentum.w;
    if (mass <= 0.0f)
        return;

    int3 ci = int3(DTid);
    float sdf = materialGrid[cellId].fieldValues.x;

    float dX = SampleGridSDF(ci + int3(1, 0, 0), gridRes) - SampleGridSDF(ci - int3(1, 0, 0), gridRes);
    float dY = SampleGridSDF(ci + int3(0, 1, 0), gridRes) - SampleGridSDF(ci - int3(0, 1, 0), gridRes);
    float dZ = SampleGridSDF(ci + int3(0, 0, 1), gridRes) - SampleGridSDF(ci - int3(0, 0, 1), gridRes);
    float3 sdfNormal = normalize(float3(dX, dY, dZ) + 1e-6);
    materialGrid[cellId].normal.xyz = sdfNormal;

    float3 sceneSize = GetMaterialSceneSize();
    float3 halfScene = sceneSize * 0.5f;
    float3 cellSize = sceneSize / float3(gridRes);
    float3 nodePos = float3(DTid) * cellSize - halfScene;

    float3 velocity = materialGrid[cellId].massMomentum.xyz / mass;

    velocity += pc.dt * float3(0.0f, 0.0f, -9.8f);

    float floorZ = 0.0f;
    float collisionBand = cellSize.z * 4.0f;

    float3 n = float3(0.0f, 0.0f, 1.0f);

    float phi = nodePos.z - floorZ;
    float vn = dot(velocity, n);

    if (phi <= collisionBand && vn < 0.0f)
    {
        float contactBand = saturate(1.0f - phi / collisionBand);

        float normalDamping = 0.15f;
        float damp = lerp(1.0f, normalDamping, contactBand);

        float vnNew = vn * damp;

        velocity += (vnNew - vn) * n;
    }


    // General case: collide against the implicit SDF surface.
    float phiSDF = sdf;
    float vnSDF = dot(velocity, sdfNormal);

    if (phiSDF <= collisionBand && vnSDF < 0.0f)
    {
        float contactBand = saturate(1.0f - phiSDF / collisionBand);

        float normalDamping = 0.15f;
        float damp = lerp(1.0f, normalDamping, contactBand);

        float vnNew = vnSDF * damp;

        velocity += (vnNew - vnSDF) * sdfNormal;
    }

    //Velocity clamp.
    velocity = clamp(velocity, -15.0f, 15.0f);
    materialGrid[cellId].massMomentum.xyz = velocity * mass;
}
