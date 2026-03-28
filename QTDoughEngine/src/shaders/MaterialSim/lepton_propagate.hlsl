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

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x;
    if (idx >= LEPTON_COUNT)
        return;

    Lepton lepton = leptonIn[idx];

    // For now, pass through In -> Out.
    // TODO: March lepton through field (position += direction * deltaTime).
    // TODO: Decrease lifespan (mana.w -= deltaTime).
    // TODO: Reset to unclaimed if lifespan <= 0.
    lepton.position.xyz += (lepton.direction.xyz * deltaTime * lepton.velocity.x).xyz;
    lepton.mana *= exp(-3.0f * deltaTime);
    lepton.velocity.w *= exp(-2.0f * deltaTime);
    
    if(lepton.velocity.w <= 0.01f)
        lepton.position.w = 0; //unclaimed.
    

    leptonOut[idx] = lepton;
}
