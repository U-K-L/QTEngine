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

// Lepton buffers: In (read) and Out (write) follow ping-pong.
StructuredBuffer<Lepton>    leptonIn  : register(t13, space1);
RWStructuredBuffer<Lepton>  leptonOut : register(u14, space1);

// MaterialGrid (read SDF for gradient).
StructuredBuffer<MaterialGridPoint> materialGrid : register(t8, space1);

float SampleSDF(float3 pos, float3 sceneSize, int3 gridRes)
{
    float3 halfScene = sceneSize * 0.5f;
    float3 gridPos = ((pos + halfScene) / sceneSize) * float3(gridRes);
    int3 coord = clamp(int3(floor(gridPos)), int3(0, 0, 0), gridRes - 1);
    int idx = Flatten3D(coord, gridRes);
    return materialGrid[idx].fieldValues.x;
}

float SampleE(float3 pos, float3 sceneSize, int3 gridRes)
{
    float3 halfScene = sceneSize * 0.5f;
    float3 gridPos = ((pos + halfScene) / sceneSize) * float3(gridRes);
    int3 coord = clamp(int3(floor(gridPos)), int3(0, 0, 0), gridRes - 1);
    int idx = Flatten3D(coord, gridRes);
    return materialGrid[idx].fieldValues.y;
}

float3 EnergyGradient(float3 pos, float3 sceneSize, int3 gridRes)
{
    float3 cellSize = sceneSize / float3(gridRes);
    float eps = max(cellSize.x, max(cellSize.y, cellSize.z));

    float dx = SampleE(pos + float3(eps, 0, 0), sceneSize, gridRes) - SampleE(pos - float3(eps, 0, 0), sceneSize, gridRes);
    float dy = SampleE(pos + float3(0, eps, 0), sceneSize, gridRes) - SampleE(pos - float3(0, eps, 0), sceneSize, gridRes);
    float dz = SampleE(pos + float3(0, 0, eps), sceneSize, gridRes) - SampleE(pos - float3(0, 0, eps), sceneSize, gridRes);

    float3 grad = float3(dx, dy, dz);
    float len = length(grad);
    return (len > 1e-5f) ? grad / len : float3(0, 0, 1);
}

float3 SDFGradient(float3 pos, float3 sceneSize, int3 gridRes)
{
    float3 cellSize = sceneSize / float3(gridRes);
    float eps = max(cellSize.x, max(cellSize.y, cellSize.z));

    float dx = SampleSDF(pos + float3(eps, 0, 0), sceneSize, gridRes) - SampleSDF(pos - float3(eps, 0, 0), sceneSize, gridRes);
    float dy = SampleSDF(pos + float3(0, eps, 0), sceneSize, gridRes) - SampleSDF(pos - float3(0, eps, 0), sceneSize, gridRes);
    float dz = SampleSDF(pos + float3(0, 0, eps), sceneSize, gridRes) - SampleSDF(pos - float3(0, 0, eps), sceneSize, gridRes);

    float3 grad = float3(dx, dy, dz);
    float len = length(grad);
    return (len > 1e-5f) ? grad / len : float3(0, 0, 1);
}

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x;
    if (idx >= LEPTON_COUNT)
        return;

    Lepton lepton = leptonIn[idx];

    int3 gridRes = GetMaterialGridSize();
    float3 sceneSize = GetSceneSize();

    float3 direction = lepton.direction.xyz;

    float3 grad = EnergyGradient(lepton.position.xyz, sceneSize, gridRes);
    float sdf = SampleSDF(lepton.position.xyz, sceneSize, gridRes);
    sdf = clamp(sdf, 0.0, 1.0f);

    // Lightning-ish steering:
    // - descend along energy gradient
    // - add deterministic chaos
    // - preserve forward persistence
    uint stepSeed = idx ^ asuint(time * 1000.0f) ^ (asuint(lepton.position.x) * 73856093u) ^ (asuint(lepton.position.y) * 19349663u) ^ (asuint(lepton.position.z) * 83492791u);
    float3 noise = RandomUnitVector(lepton.position.xyz, stepSeed);

    float gradientBias = 0.75f * (1.0f - lepton.velocity.y);
    float noiseStrength = 0.35f * (1.0f - lepton.velocity.y);
    float forwardBias = lepton.velocity.y;
    float steerStrength = 0.45f * (1.0f - lepton.velocity.y);

    float3 targetDir = normalize(
        (-gradientBias * grad) +
        (noiseStrength * noise) +
        (forwardBias * direction)
    );
    
    direction = lerp(direction, normalize(targetDir), steerStrength * lepton.velocity.z);
    
    float3 normal = SDFGradient(lepton.position.xyz, sceneSize, gridRes);
    float3 refl = reflect(direction, normal);
    
    direction = lerp(direction, refl, (1.0f - sdf) * lepton.velocity.z);
    


    lepton.direction.xyz = direction;
    lepton.position.xyz += direction * deltaTime * (lepton.velocity.x);
    lepton.mana *= exp(-3.0f * deltaTime);
    lepton.velocity.w *= exp(-2.0f * deltaTime);

    if (lepton.velocity.w <= 0.01f)
        lepton.position.w = 0; //unclaimed.

    leptonOut[idx] = lepton;
}
