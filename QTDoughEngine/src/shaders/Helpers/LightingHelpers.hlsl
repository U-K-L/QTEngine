#include "../RayTraceAccel/rayTracingAccelHelper.hlsl"

#define _Glossiness 0.0125f //Move this to per material.
#define _RimAmount 0.46f //Move this to per material.
#define _RimThreshold 0.99f //Move this to per material.

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

float4 UnigmaBRDFDirectionalLight(in GameObjectShaderData material, in Photon photon, in Surface surface, in GPULight light)
{
    //Pick based on normals.
    float3 normal = surface.normal.xyz;
    
    float3 toLight = -light.direction.xyz; //light.direction - surface.position.xyz;
    float4 Le = light.emission;
    float3 viewDir = -photon.direction;
    float3 betweenVec = normalize(toLight + viewDir);
    
    float NdotL = saturate(dot(normal, toLight));
    float BdotN = saturate(dot(betweenVec, normal));
    
    float specularIntensity = pow(BdotN * Le.w, _Glossiness * _Glossiness) * 0.01f;
    float specularSmoothed = smoothstep(0.005, 0.01, specularIntensity) * 0.01f;
    float4 specular = specularSmoothed;// * _SpecularColor;

    float4 rimLight = 1 - dot(normal, betweenVec);
    float4 rimBloom = 1.0f - _RimAmount * 0.4456 * dot(normal, betweenVec);
    rimBloom *= 0.1f;
    
    float rimIntensity = rimLight * pow(BdotN, _RimThreshold);
    rimIntensity = smoothstep(_RimAmount - 0.05, _RimAmount + 0.05, rimIntensity);
	

    float rim = rimIntensity; // * _RimLightColor;
    
    float4 finalSpecular = 1.0f - (specular + rim + rimBloom)*10;
    
    float4 mid = material.Midtone * 1.0725f;
    float4 shad = material.Shadow * 1.0725f;
    float4 high = material.Highlight * 1.0725f;
    
    float4 midTones = mid * step(0.2, NdotL);
    float4 shadows = shad * step(NdotL, 0.4);
    float4 highlights = high * step(0.6, NdotL);

    float4 BRDF = max(midTones, shadows);
    BRDF = max(BRDF, highlights);
    
    return BRDF + finalSpecular;
}

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