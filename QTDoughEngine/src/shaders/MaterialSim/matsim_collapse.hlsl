#include "../Helpers/ShaderHelpers.hlsl"

cbuffer Constants : register(b2, space0)
{
    float deltaTime;
    float time;
    float2 pad;
};

// Global bindless 3D textures (same as voxelizer).
Texture3D<float> gBindless3D[] : register(t4, space0);

// Push constants.
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

// Set 1 bindings — MaterialSimulation buffers.
StructuredBuffer<Quanta> quantaIn : register(t0, space1);
RWStructuredBuffer<Quanta> quantaOut : register(u1, space1);

// Binding 7 — Brushes (read-only).
StructuredBuffer<Brush> Brushes : register(t7, space1);

#define GROUP_SIZE 512

// How far outside the AABB (world-space) quanta begin to be attracted.
#define GATHER_RANGE    4.0f
// Force magnitude for attraction toward AABB.
#define GATHER_STRENGTH 100.0f
// Force magnitude pushing free quanta out of AABB.
#define PUSH_STRENGTH   1500.0f

// Trilinear SDF sample — mirrors Read3DTrilinearManual in voxelizer.
float2 SampleBrushSDF(uint texId, float3 uvw, int res)
{
    uvw = saturate(uvw);
    float3 t = uvw * res - 0.5f;
    int3 p0 = (int3) floor(t);
    float3 f = t - (float3) p0;
    int3 p1 = p0 + 1;
    int3 lo = int3(0, 0, 0);
    int3 hi = int3(res - 1, res - 1, res - 1);
    p0 = clamp(p0, lo, hi);
    p1 = clamp(p1, lo, hi);

    float2 c000 = gBindless3D[texId].Load(int4(p0.x, p0.y, p0.z, 0));
    float2 c100 = gBindless3D[texId].Load(int4(p1.x, p0.y, p0.z, 0));
    float2 c010 = gBindless3D[texId].Load(int4(p0.x, p1.y, p0.z, 0));
    float2 c110 = gBindless3D[texId].Load(int4(p1.x, p1.y, p0.z, 0));
    float2 c001 = gBindless3D[texId].Load(int4(p0.x, p0.y, p1.z, 0));
    float2 c101 = gBindless3D[texId].Load(int4(p1.x, p0.y, p1.z, 0));
    float2 c011 = gBindless3D[texId].Load(int4(p0.x, p1.y, p1.z, 0));
    float2 c111 = gBindless3D[texId].Load(int4(p1.x, p1.y, p1.z, 0));

    float2 c00 = lerp(c000, c100, f.x);
    float2 c10 = lerp(c010, c110, f.x);
    float2 c01 = lerp(c001, c101, f.x);
    float2 c11 = lerp(c011, c111, f.x);
    float2 c0 = lerp(c00, c10, f.y);
    float2 c1 = lerp(c01, c11, f.y);
    return lerp(c0, c1, f.z);
}

[numthreads(8, 8, 8)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint localIndex = GTid.x + GTid.y * 8 + GTid.z * 64;
    uint globalIndex = Gid.x * GROUP_SIZE + localIndex;

    if (globalIndex >= QUANTA_COUNT)
        return;

    Quanta q = quantaIn[globalIndex];

    // Inactive quanta — pass through.
    if (q.position.w < 1)
    {
        quantaOut[globalIndex] = q;
        return;
    }

    Brush brush = Brushes[pc.brushIndex];

    // World-space AABB (already in world space on the brush).
    float3 aabbMin = mul(brush.model, float4(brush.aabbmin.xyz, 1.0f)).xyz;
    float3 aabbMax = mul(brush.model, float4(brush.aabbmax.xyz, 1.0f)).xyz;
    // Ensure min < max after transform.
    float3 wMin = min(aabbMin, aabbMax);
    float3 wMax = max(aabbMin, aabbMax);

    float3 pos = q.position.xyz;

    // Already collapsed to this brush — hold position, skip all forces.
    if (q.information.x == (int)brush.id)
    {
        //quantaOut[globalIndex] = q;
        //return;
    }

    // Already collapsed to a different brush — don't touch.
    if (q.information.x > 0)
    {
        quantaOut[globalIndex] = q;
        return;
    }

    // --- Free quanta (information.x == 0) ---

    // Signed distance to the AABB (negative = inside).
    float3 closest = clamp(pos, wMin, wMax);
    float3 diff = pos - closest;
    float distToAABB = length(diff);
    bool insideAABB = all(pos >= wMin) && all(pos <= wMax);


    if (insideAABB)
    {
        // Transform to brush local space.
        float3 localPos = mul(brush.invModel, float4(pos, 1.0f)).xyz;

        // UVW in brush volume [0,1].
        float3 uvw = (localPos - brush.aabbmin.xyz) / (brush.aabbmax.xyz - brush.aabbmin.xyz);

        // Snap to density grid — same logic as CreateBrush.
        int blockSize = brush.density;
        int3 texelCoord = (int3) floor(uvw * brush.resolution);
        texelCoord = clamp(texelCoord, int3(0, 0, 0), int3((int)brush.resolution - 1, (int)brush.resolution - 1, (int)brush.resolution - 1));

        // Align to density grid.
        int3 snappedCoord = (texelCoord / blockSize) * blockSize;

        // Sample SDF at snapped position.
        float3 snappedUvw = ((float3) snappedCoord + 0.5f) / brush.resolution;
        float2 sdfSample = SampleBrushSDF(brush.textureID2, snappedUvw, (int) brush.resolution);
        float sdf = sdfSample.x;

        if (sdf < 0.0f)
        {
            // Collapse — snap to the grid position on the SDF interior.
            float3 snappedLocal = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, snappedUvw);
            float3 snappedWorld = mul(brush.model, float4(snappedLocal, 1.0f)).xyz;

            q.position.xyz = snappedWorld;
            //q.information.x = (int) brush.id;
        }
        else
        {
            // Inside AABB but not in a valid SDF slot — push outward.
            float3 center = (wMin + wMax) * 0.5f;
            float3 pushDir = pos - center;
            float pushLen = length(pushDir);
            if (pushLen > 0.001f)
                pushDir /= pushLen;
            else
                pushDir = float3(0, 1, 0);

            q.position.xyz += pushDir * PUSH_STRENGTH * deltaTime;
        }
    }
    else if (distToAABB < GATHER_RANGE)
    {
        // Within gather range — attract toward AABB center.
        float3 center = (wMin + wMax) * 0.5f;
        float3 toCenter = center - pos;
        float dist = length(toCenter);
        if (dist > 0.001f)
        {
            float3 dir = toCenter / dist;
            // Strength increases as quanta get closer.
            float falloff = 1.0f - saturate(distToAABB / GATHER_RANGE);
            q.position.xyz += dir * GATHER_STRENGTH * falloff * deltaTime;
        }
    }

    quantaOut[globalIndex] = q;
}
