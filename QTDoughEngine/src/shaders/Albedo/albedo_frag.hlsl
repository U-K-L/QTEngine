#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/UniformBuffers.hlsl"

struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float4 normal : NORMAL; // Location 2, flat interpolation
};

struct PSOutput
{
    float4 color0 : SV_Target0; // Will map to color attachment #0
    float4 color1 : SV_Target1; // Will map to color attachment #1
    float4 color2 : SV_Target2; // Will map to color attachment #2
    float4 color3 : SV_Target3; // Will map to color attachment #3
};

// Define an unbounded array of textures and samplers
// Using register space0 to match Vulkan descriptor set 0
Texture2D textures[] : register(t0, space0); //Global
SamplerState samplers[] : register(s0, space0); //Global
RWStructuredBuffer<GameObjectShaderData> globalObjMaterials : register(u6, space0);

// Define a structured buffer for unsigned int array
// Binding slot t1, space0 to match Vulkan descriptor set 1, binding 1
StructuredBuffer<uint> intArray : register(t1, space1);

struct Images
{
    uint AnimeGirl;
};

Images InitImages()
{
    Images image;
    image.AnimeGirl = intArray[0];
    
    return image;
}

cbuffer Constants : register(b2, space0)
{
    float deltaTime; // offset 0
    float time; // offset 4
    float2 pad; // offset 8..15, total 16 bytes
    GPULight light[32];
};

PSOutput main(PSInput input)
{
    PSOutput output;
    Images images = InitImages();
    float2 textureUVs = float2(input.uv.x, 1.0 - input.uv.y);
    float4 animeGirl = textures[images.AnimeGirl].Sample(samplers[images.AnimeGirl], textureUVs);
    float thresholdX = 0.2;
    float thresholdY = 0.6;
    float thresholdZ = 0.8;
    
    float4 vertexColor = float4(input.color, 1.0);
    // Normalize light direction
    float3 normLightDir = light[0].direction; //normalize(float3(0.5, 0.5, 0.0));

    // Dot product for lighting intensity, adjusted for [-1, 1] to [0, 1] range
    float NdotL = dot(input.normal, normLightDir) * 0.5 + 0.5;
    
    /*
    float4 midTones = baseAlbedo * step(thresholdX, NdotL);
    float4 shadows = (sideAlbedo) * step(NdotL, thresholdY);
    float4 highlights = (topAlbedo) * step(thresholdZ, NdotL);
    
    
    float4 finalColor = max(midTones, shadows);
    finalColor = max(finalColor, highlights);
    */
    
    //Pick based on normals.
    float3 normal = input.normal;
    float4 front = rand4(float4(input.normal.w, input.normal.w + 1, input.normal.w + 2, 1.0)); //globalObjMaterials[0].BaseAlbedo; //float4(1, globalObjMaterials[1], 0, 1);
    float4 sides = globalObjMaterials[0].SideAlbedo;
    float4 top = globalObjMaterials[0].TopAlbedo;
    
    float3 forward = float3(0, 1, 0);
    float3 up = float3(0, 0, 1);
    float3 right = float3(1, 0, 0);
    
    float weightFront = abs(normal.y);
    float weightSides = abs(normal.x);
    float weightTop = abs(normal.z);
    
    float total = weightFront + weightSides + weightTop;
    weightFront /= total;
    weightSides /= total;
    weightTop /= total;

    float4 finalColor = front * weightFront + sides * weightSides + top * weightTop;
    
    finalColor.a = 1.0;
    //return animeGirl;
    output.color0 = lerp(finalColor, animeGirl, 0);
    output.color1 = outerOutlineColor;
    output.color2 = innerOutlineColor;
    
    //w component should not need to be 1, there is some incorrect blending here.
    output.color3 = float4(textureUVs.x, textureUVs.y, texelSize.x, 1);
    return output;
}
