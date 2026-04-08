#include "../Helpers/ShaderHelpers.hlsl"

// Global bindless 3D textures.
Texture3D<float> gBindless3D[] : register(t4, space0);

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
RWStructuredBuffer<Quanta> quantaOut : register(u1, space1);
RWStructuredBuffer<uint> globalCursor : register(u6, space1); // tileCursor[0] used as atomic counter.
StructuredBuffer<Brush> Brushes : register(t7, space1);

float2 Load3D(uint texId, int3 p)
{
    return gBindless3D[texId].Load(int4(p, 0));
}

float2 Read3DTrilinearManual(uint texId, float3 uvw, int res)
{
    // clamp uvw to [0,1] so we don’t read outside
    uvw = saturate(uvw);

    // texel space coordinate where texel centers are at i+0.5
    float3 t = uvw * res - 0.5f;

    int3 p0 = (int3) floor(t);
    float3 f = t - (float3) p0;

    int3 p1 = p0 + 1;

    // clamp integer coords
    int3 lo = int3(0, 0, 0);
    int3 hi = int3(res - 1, res - 1, res - 1);
    p0 = clamp(p0, lo, hi);
    p1 = clamp(p1, lo, hi);

    // 8 corners
    float2 c000 = Load3D(texId, int3(p0.x, p0.y, p0.z));
    float2 c100 = Load3D(texId, int3(p1.x, p0.y, p0.z));
    float2 c010 = Load3D(texId, int3(p0.x, p1.y, p0.z));
    float2 c110 = Load3D(texId, int3(p1.x, p1.y, p0.z));
    float2 c001 = Load3D(texId, int3(p0.x, p0.y, p1.z));
    float2 c101 = Load3D(texId, int3(p1.x, p0.y, p1.z));
    float2 c011 = Load3D(texId, int3(p0.x, p1.y, p1.z));
    float2 c111 = Load3D(texId, int3(p1.x, p1.y, p1.z));

    // trilinear
    float2 c00 = lerp(c000, c100, f.x);
    float2 c10 = lerp(c010, c110, f.x);
    float2 c01 = lerp(c001, c101, f.x);
    float2 c11 = lerp(c011, c111, f.x);

    float2 c0 = lerp(c00, c10, f.y);
    float2 c1 = lerp(c01, c11, f.y);

    return lerp(c0, c1, f.z);
}

float2 Read3DTransformed(in Brush brush, float3 worldPos)
{
    float3 localPos = mul(brush.invModel, float4(worldPos, 1.0f)).xyz;

    // map localPos into [0,1] using the SAME bounds used to author the volume
    float3 uvw = (localPos - brush.aabbmin.xyz) / (brush.aabbmax.xyz - brush.aabbmin.xyz);

    // outside = empty
    if (any(uvw < 0.0f) || any(uvw > 1.0f))
        return DEFUALT_EMPTY_SPACE;

    return Read3DTrilinearManual(brush.textureID2, uvw, (int) brush.resolution);
}



[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    Brush brush = Brushes[pc.brushIndex];

    if (any(DTid >= (uint3)brush.resolution))
        return;

    int blockSize = brush.density;
    if (!all((DTid.xyz % blockSize) == 0))
        return;


    float3 uvw = ((float3) DTid + 0.5f) / brush.resolution;
    float3 localPos = lerp(brush.aabbmin.xyz, brush.aabbmax.xyz, uvw);
    float3 worldPos = mul(brush.model, float4(localPos, 1.0f)).xyz;
    float sdf = gBindless3D[brush.textureID].Load(int4(DTid, 0));

    if (sdf >= 0.0f)
        return;

    int slot = DTid.x + DTid.y * DTid.x + DTid.x * DTid.y * DTid.z;

    // Atomic counter: grab sequential quanta slots until we find a free one.
    while (true)
    {
        uint slot;
        InterlockedAdd(globalCursor[0], 1, slot);

        if (slot >= QUANTA_COUNT)
            return;

        if (quantaOut[slot].information.x != 0)
            continue; // Already claimed, try next.

        quantaOut[slot].position.xyz = localPos;
        quantaOut[slot].information.x = (int) brush.id;
        return;
    }
}
