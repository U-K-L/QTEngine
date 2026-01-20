#include "rayTracingAccelHelper.hlsl"

RWTexture2D<float4> outImage : register(u1);
RaytracingAccelerationStructure tlas : register(t0);

Texture2D textures[] : register(t0, space0); //Global
SamplerState samplers[] : register(s0, space0); //Global

StructuredBuffer<uint> intArray : register(t1, space1);

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
    float4 rayAlbedoImage = textures[images.RayAlbedoImage].Sample(samplers[images.RayAlbedoImage], pix);
    
    // Just write something obvious
    rayAlbedoImage[pix] = float4(1, 0, 0, 1); // red = raygen executed
}
