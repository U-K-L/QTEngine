RWStructuredBuffer<float4> dataBuffer : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    float4 val = dataBuffer[index];
    float4 a = float4(1.00001f, 0.99999f, 1.00002f, 0.99998f);
    float4 b = float4(0.00001f, 0.00002f, 0.00003f, 0.00004f);

    [loop]
    for (int i = 0; i < 1024; i++)
    {
        val = val * a + b;
    }

    dataBuffer[index] = val;
}
