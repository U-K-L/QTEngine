#include "../Helpers/ShaderHelpers.hlsl"


// HLSL Shader Code

// Define an unbounded array of textures and samplers
// Using register space0 to match Vulkan descriptor set 0
Texture2D textures[] : register(t0, space0); //Global
SamplerState samplers[] : register(s0, space0); //Global

// Define a structured buffer for unsigned int array
// Binding slot t1, space0 to match Vulkan descriptor set 1, binding 1
StructuredBuffer<uint> intArray : register(t1, space1);

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float2 texelSize;
}

struct Images
{
    uint NormalImage;
    uint PositionImage;
    uint DepthImage;
    uint AlbedoImage;
    uint OutlineColorsImage;
    uint InnerColorsImage;
};

Images InitImages()
{
    Images image;

    image.NormalImage = intArray[0];
    image.PositionImage = intArray[1];
    image.DepthImage = intArray[2];
    image.AlbedoImage = intArray[3];
    image.OutlineColorsImage = intArray[4];
    image.InnerColorsImage = intArray[5];
    
    return image;
}

// Define any constants or uniforms you need
cbuffer Constants : register(b0, space0)
{
    uint offscreenImageIndex;
    // Add any other constants you might need
}

struct VSOutput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};
// Main pixel shader function
float4 main(VSOutput i) : SV_Target
{
    Images images = InitImages();
    float2 textureUVs = float2(i.uv.x, 1.0 - i.uv.y);
    
    float4 normalImage = textures[images.NormalImage].Sample(samplers[images.NormalImage], textureUVs);
    float4 positionImage = textures[images.PositionImage].Sample(samplers[images.PositionImage], textureUVs);
    float4 depthImage = textures[images.DepthImage].Sample(samplers[images.DepthImage], textureUVs);
    float4 albedoImage = textures[images.AlbedoImage].Sample(samplers[images.AlbedoImage], textureUVs);
    float4 outlineColorsImage = textures[images.OutlineColorsImage].Sample(samplers[images.OutlineColorsImage], textureUVs);
    float4 innerColorsImage = textures[images.InnerColorsImage].Sample(samplers[images.InnerColorsImage], textureUVs);

    float depth = depthImage.r;
    float linearDepth = LinearizeDepth(depth);
    // near and far plane CHANGE TO UNIFORMS
    float near_plane = 0.1;
    float far_plane = 100.0;
    // Visualize linear depth
    float4 outColor = linearDepth / far_plane;
    outColor = float4(outColor.xyz, 1.0);
    
    float _DepthThreshold = 0.4;
    float _NormalThreshold = 0.2;
    float _PosThreshold = 0.01;
    float _ScaleOuter = 3.0;
    float _ScaleInner = 1.0;
    float _ScaleOuterOuterLines = 10.0;
    float OuterScale = _ScaleOuter;
    
    float scaleFloor = floor(OuterScale * 0.5);
    float scaleCeil = ceil(OuterScale * 0.5);

    float2 _MainTex_TexelSize = texelSize;
    
    float2 bottomLeft = textureUVs - float2(_MainTex_TexelSize.x, _MainTex_TexelSize.y) * scaleFloor;
    float2 topRight = textureUVs + float2(_MainTex_TexelSize.x, _MainTex_TexelSize.y) * scaleCeil;
    float2 bottomRight = textureUVs + float2(_MainTex_TexelSize.x * scaleCeil, -_MainTex_TexelSize.y * scaleFloor);
    float2 topLeft = textureUVs + float2(-_MainTex_TexelSize.x * scaleFloor, _MainTex_TexelSize.y * scaleCeil);
    
    
    //Get ID uniqueness. ID is contained in the w component.
    float pos0 = textures[images.PositionImage].Sample(samplers[images.PositionImage], bottomLeft).w;
    float pos1 = textures[images.PositionImage].Sample(samplers[images.PositionImage], topRight).w;
    float pos2 = textures[images.PositionImage].Sample(samplers[images.PositionImage], bottomRight).w;
    float pos3 = textures[images.PositionImage].Sample(samplers[images.PositionImage], topLeft).w;


    float posFiniteDifference3 = abs(pos1 - pos0); //length(pos1 - pos0);
    float posFiniteDifference4 = abs(pos3 - pos2); //length(pos3 - pos2);
    float edgePos = sqrt(pow(posFiniteDifference3, 2) + pow(posFiniteDifference4, 2)) * 100;
    float posThreshold = _PosThreshold;
    edgePos = edgePos > posThreshold ? 1 : 0;
    
    //------------------------------------------------------
    
    scaleFloor = floor(_ScaleInner * 0.5);
    scaleCeil = ceil(_ScaleInner * 0.5);

    bottomLeft = textureUVs - float2(_MainTex_TexelSize.x, _MainTex_TexelSize.y) * scaleFloor;
    topRight = textureUVs + float2(_MainTex_TexelSize.x, _MainTex_TexelSize.y) * scaleCeil;
    bottomRight = textureUVs + float2(_MainTex_TexelSize.x * scaleCeil, -_MainTex_TexelSize.y * scaleFloor);
    topLeft = textureUVs + float2(-_MainTex_TexelSize.x * scaleFloor, _MainTex_TexelSize.y * scaleCeil);
    
    //Get Normal Edge Detection
    float3 normal0 = textures[images.NormalImage].Sample(samplers[images.NormalImage], bottomLeft).xyz;
    float3 normal1 = textures[images.NormalImage].Sample(samplers[images.NormalImage], topRight).xyz;
    float3 normal2 = textures[images.NormalImage].Sample(samplers[images.NormalImage], bottomRight).xyz;
    float3 normal3 = textures[images.NormalImage].Sample(samplers[images.NormalImage], topLeft).xyz;
    
    float3 normalFiniteDifference0 = abs(normal1 - normal0);
    float3 normalFiniteDifference1 = abs(normal3 - normal2);
    
    float edgeNormal = sqrt(dot(normalFiniteDifference0, normalFiniteDifference0) + dot(normalFiniteDifference1, normalFiniteDifference1));
    edgeNormal = edgeNormal > _NormalThreshold ? 1 : 0;
    
     //------------------------------------------------------
    
    //Depth based WIDE outlines.
    
    scaleFloor = floor(_ScaleOuterOuterLines * 0.5);
    scaleCeil = ceil(_ScaleOuterOuterLines * 0.5);

    bottomLeft = textureUVs - float2(_MainTex_TexelSize.x, _MainTex_TexelSize.y) * scaleFloor;
    topRight = textureUVs + float2(_MainTex_TexelSize.x, _MainTex_TexelSize.y) * scaleCeil;
    bottomRight = textureUVs + float2(_MainTex_TexelSize.x * scaleCeil, -_MainTex_TexelSize.y * scaleFloor);
    topLeft = textureUVs + float2(-_MainTex_TexelSize.x * scaleFloor, _MainTex_TexelSize.y * scaleCeil);
    
    //Get depth textures.
    float depth0 = LinearizeDepth(textures[images.DepthImage].Sample(samplers[images.DepthImage], bottomLeft).r) / far_plane;
    float depth1 = LinearizeDepth(textures[images.DepthImage].Sample(samplers[images.DepthImage], topRight).r) / far_plane;
    float depth2 = LinearizeDepth(textures[images.DepthImage].Sample(samplers[images.DepthImage], bottomRight).r) / far_plane;
    float depth3 = LinearizeDepth(textures[images.DepthImage].Sample(samplers[images.DepthImage], topLeft).r) / far_plane;
    
    
    float depthFiniteDifference0 = abs(depth1 - depth0);
    float depthFiniteDifference1 = abs(depth3 - depth2);
    
    float edgeDepth = sqrt(pow(depthFiniteDifference0, 2) + pow(depthFiniteDifference1, 2)) * 1;
    edgeDepth = edgeDepth > _DepthThreshold ? 1 : 0;
    
    return innerColorsImage;
    //return edgePos;
    return albedoImage;
    return edgeDepth + edgeNormal + edgePos;
    //return float4(GammaEncode(albedoImage.xyz, 0.32875), 1);
    //return float4(GammaEncode(color.xyz, 0.32875), color.w);

}
