#include "../Helpers/ShaderHelpers.hlsl"

cbuffer Constants : register(b2, space0)
{
    float deltaTime;
    float time;
    float2 pad;
};

[[vk::push_constant]]
struct EmitterPushConsts
{
    uint activeEventCount;
    uint threadsPerEvent;
    float pad0;
    float pad1;
} pc;

#define PARTICLE_TYPE_QUARK  0
#define PARTICLE_TYPE_LEPTON 1

#define SHAPE_POINT  0
#define SHAPE_SPHERE 1

// Set 1 bindings — Emitter system.
struct EmitterEvent
{
    int4   information; // x = owner, y = type, z = texture id.
    float4 position;    // w = count.
    float4 shape;       // x = radius, w = shape type.
    float4 direction;
    float4 velocity;    // w = lifetime.
    float4 mana;        // albedo for graphical.
};

StructuredBuffer<EmitterEvent>  emitterEvents : register(t0, space1);
StructuredBuffer<Quanta>        quantaRead    : register(t1, space1);
RWStructuredBuffer<Quanta>      quantaIn      : register(u2, space1);
StructuredBuffer<Lepton>        leptonRead    : register(t3, space1);
RWStructuredBuffer<Lepton>      leptonIn      : register(u4, space1);

struct SpawnResult
{
    float3 position;
    float3 direction;
};

SpawnResult Spawn(EmitterEvent ev, uint threadIndex)
{
    SpawnResult result;
    result.position = ev.position.xyz;
    result.direction = ev.direction.xyz;

    int shapeType = (int) ev.shape.w;

    if (shapeType == SHAPE_SPHERE)
    {
        float radius = ev.shape.x;
        float seed = float(threadIndex * 73856093u ^ (uint)(time * 1000.0f)) * 0.00000001f;
        float3 rnd = float3(
            frac(sin(seed * 127.1f) * 43758.5453f),
            frac(sin(seed * 269.5f) * 43758.5453f),
            frac(sin(seed * 419.2f) * 43758.5453f)
        );
        float theta = rnd.x * 2.0f * PI;
        float phi = acos(2.0f * rnd.y - 1.0f);
        float r = radius * pow(rnd.z, 1.0f / 3.0f);
        float3 offset = float3(r * sin(phi) * cos(theta), r * sin(phi) * sin(theta), r * cos(phi));
        result.position += offset;
        result.direction = normalize(offset);
    }

    return result;
}

[numthreads(64, 1, 1)]
void main(uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint eventIndex = Gid.y;
    uint threadIndex = Gid.x * 64 + GTid.x;

    if (eventIndex >= pc.activeEventCount)
        return;

    EmitterEvent ev = emitterEvents[eventIndex];

    uint particleCount = (uint) ev.position.w;
    if (threadIndex >= particleCount)
        return;

    int particleType = ev.information.y;
    SpawnResult spawn = Spawn(ev, threadIndex);

    if (particleType == PARTICLE_TYPE_LEPTON)
    {
        // Scan leptonRead for unclaimed slot (position.w <= 0).
        for (uint i = threadIndex; i < LEPTON_COUNT; i += pc.threadsPerEvent)
        {
            if (leptonRead[i].position.w <= 0)
            {
                Lepton claimed;
                claimed.position = float4(spawn.position, float(ev.information.x+1));
                claimed.direction = float4(spawn.direction, ev.direction.w);
                claimed.velocity = ev.velocity;
                claimed.mana = float4(ev.mana.xyz, ev.velocity.w);
                leptonIn[i] = claimed;
                return;
            }
        }
    }
    else if (particleType == PARTICLE_TYPE_QUARK)
    {
        // TODO: Scan quantaRead for free quanta and claim in quantaIn.
    }
}
