struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
};

cbuffer ParameterUBO : register(b0)
{
    float deltaTime;
};

StructuredBuffer<Particle> particlesIn : register(t1); // readonly
RWStructuredBuffer<Particle> particlesOut : register(u2); // write

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;

    Particle particleIn = particlesIn[index];

    Particle result;
    result.position = particleIn.position + particleIn.velocity * deltaTime;
    result.velocity = particleIn.velocity;
    result.color = particleIn.color;

    // Flip movement at window border
    if (result.position.x <= -1.0 || result.position.x >= 1.0)
    {
        result.velocity.x = -result.velocity.x;
    }
    if (result.position.y <= -1.0 || result.position.y >= 1.0)
    {
        result.velocity.y = -result.velocity.y;
    }

    result.velocity = 1.245f;
    particlesOut[index] = result;
}
