#include "../RayTraceAccel/rayTracingAccelHelper.hlsl"

struct GameObjectShaderData
{
    float4x4 transform;
    float4 Midtone;
    float4 Highlight;
    float4 Shadow;
};

struct GPULight
{
    float4 emission;
    float3 direction;
};

float4 UnigmaBRDF(in GameObjectShaderData material, in Photon photon, in Surface surface, in GPULight light)
{
    //Pick based on normals.
    float3 normal = surface.normal.xyz;
    
    float3 toLight = light.direction.xyz; //light.direction - surface.position.xyz;
    float4 Le = light.emission;
    
    float NdotL = saturate(dot(normal, toLight));

    float4 mid = material.Midtone * 1.0725f;
    float4 shad = material.Shadow * 1.0725f;
    float4 high = material.Highlight * 1.0725f;
    
    float4 midTones = mid * step(0.2, NdotL);
    float4 shadows = shad * step(NdotL, 0.4);
    float4 highlights = high * step(0.6, NdotL);

    float4 BRDF = max(midTones, shadows);
    BRDF = max(BRDF, highlights);
    
    float distSquared = distance(float3(5, 5, 5), surface.position.xyz) * distance(float3(5, 5, 5), surface.position.xyz);
    float Gx = min(50000, 1.0f / distSquared);

    
    //return BRDF * Le * Gx;
    
    
    return BRDF;
}