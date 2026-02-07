RWStructuredBuffer<float4> srcBuffer : register(u0);
RWStructuredBuffer<float4> dstBuffer : register(u1);

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    float4 val = srcBuffer[index];
    val.x += 1.0f;
    dstBuffer[index] = val;
}
