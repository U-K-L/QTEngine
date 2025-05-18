RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);

#include "../Helpers/ShaderHelpers.hlsl"

#define VOXEL_RESOLUTION 64
#define SCENE_BOUNDS 10.0f

struct Voxel
{
    float4 positionDistance;
    float4 normalDensity;
    float4 occuipiedInfo;
};

struct ComputeVertex
{
    float4 position;
    float4 texCoord;
    float4 normal;
};

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float2 texelSize;
}

StructuredBuffer<Voxel> voxelsIn : register(t2, space1); // readonly
RWStructuredBuffer<Voxel> voxelsOut : register(u3, space1); // write


StructuredBuffer<ComputeVertex> vertexBuffer : register(t4, space1);
StructuredBuffer<uint3> indexBuffer : register(t5, space1);


[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int index = DTid.x;
    /*
    Voxel voxelIn = voxelsIn[index];

    Voxel result;
    result.positionDistance = 1;
    result.normalDensity.xyz = float3(1, texelSize.x, texelSize.y);
    result.normalDensity.w = 23312.0f;
    voxelsOut[index] = result;
*/
}
