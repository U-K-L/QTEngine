struct Voxel
{
    float4 positionDistance;
    float4 normalDensity;
};

StructuredBuffer<Voxel> voxelsIn : register(t2); // readonly
RWStructuredBuffer<Voxel> voxelsOut : register(u3); // write

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{

    uint index = DTid.x;
    
    if (index >= 64)
    {
        return;
    }
    
    Voxel voxelIn = voxelsIn[index];

    Voxel result;
    result.positionDistance.xyz = float3(1, 1, 1);
    result.positionDistance.w = 20.0f;
    result.normalDensity.xyz = float3(1, 0, 0);
    result.normalDensity.w = 23312.0f;

    voxelsOut[index] = result;
}
