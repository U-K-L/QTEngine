#include "rayTracingAccelHelper.hlsl"

RWTexture2D<float4> outImage : register(u1);
RaytracingAccelerationStructure tlas : register(t0);

RWTexture2D<float4> OutputImage;
RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

SamplerState samplers[] : register(s1, space0); //Global

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 texelSize;
    float isOrtho;
}

struct Images
{
    uint RayAlbedoImage;
    uint RayNormalImage;
    uint RayDepthImage;
};

Images InitImages()
{
    Images image;
    
    image.RayAlbedoImage = intArray[0];
    image.RayNormalImage = intArray[1];
    image.RayDepthImage = intArray[2];
    
    return image;
}


[shader("raygeneration")]
void main()
{
    Images images = InitImages();
    uint2 pix = DispatchRaysIndex().xy;
    uint outputImageHandle = NonUniformResourceIndex(images.RayAlbedoImage);
    // Just write something obvious
    gBindlessStorage[0][pix] = float4(1, 1, 0, 1); // red = raygen executed
}
