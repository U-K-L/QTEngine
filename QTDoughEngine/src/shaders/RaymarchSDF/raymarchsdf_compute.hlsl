struct Voxel
{
    float3 position;
    float distance;
    float3  normal;
    float density;
};

StructuredBuffer<Voxel> voxelsIn : register(t1); // readonly
RWStructuredBuffer<Voxel> voxelsOut : register(u2); // write

[numthreads(32, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{

    uint index = DTid.x;

    /*
    Voxel voxelIn = voxelsIn[index];

    Voxel result;
    result.position = float3(1, 1, 1);
    result.distance = 2.0f;
    result.normal = float3(1, 0, 0);
    result.density = 2.0f;

    voxelsOut[index] = result;
*/
}
