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
    float4 smoothRefract;
    float4 absorption;
};

struct GPULight
{
    float4 emission;
    float3 direction;
};

float4 absorptionFunc(float4 absorptionRate)
{
    float k = 1.155f;
    return exp2(-k * absorptionRate);

}

float4 UnigmaBRDFDirectionalLight(in GameObjectShaderData material, inout Photon photon, in Surface surface, in GPULight light)
{
    float diffuseLobeEnergy = 1.0f - material.smoothRefract.z;
    float specularLobeEnergy = material.smoothRefract.z;
    
    //Pick based on normals.
    float3 normal = surface.normal.xyz;
    
    float3 toLight = -light.direction.xyz; //light.direction - surface.position.xyz;
    float4 Le = light.emission;
    float3 viewDir = -photon.direction.xyz;
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
    
    float4 finalSpecular = 1.0f - (specular + rim + rimBloom);
    
    float4 mid = material.Midtone * 1.0725f;
    float4 shad = material.Shadow * 1.0725f;
    float4 high = material.Highlight * 1.0725f;
    
    float4 midTones = mid * step(0.2, NdotL);
    float4 shadows = shad * step(NdotL, 0.4);
    float4 highlights = high * step(0.6, NdotL);

    float4 BRDF = max(midTones, shadows);
    BRDF = max(BRDF, highlights);
    
    //Refractance.
    float3 I = normalize(photon.direction.xyz);
    float3 N = normalize(normal);

    if (dot(I, N) > 0.0f)
        N = -N;

    float n1 = photon.direction.w;
    float n2 = material.smoothRefract.w;
    
    float eta = n1 / n2;
    float3 refractedDir = refract(I, N, eta);
    
    float ndotv = saturate(dot(normalize(N), normalize(-I)));

    float iridescence = pow(1.0f - ndotv, 2.0f);

    float phase = 0.2f * 40.0f + iridescence * 6.28318f;

    float3 filmColor = 0.5f + 0.5f * cos(phase + float3(0.0f, 2.094f, 4.188f));
    
    //float fresnel = pow(1.0f - ndotv, 5.0f);

    float3 iridescentSpecular = lerp(1.0f, filmColor, material.smoothRefract.y);
    
    float4 specularColor = float4(iridescentSpecular, 1.0f);
    //Reflectance.
    //float roughness = 0.05f; // Add to material struct.
    float3 specularReflect = reflect(photon.direction.xyz, normal);
    //float3 smoothReflect = lerp(specularReflect, normal, roughness); //Replace with diffuse direction.
    
    //Probability of it reflecting or going into the material.
    //Use this when full probability is inserted with denoiser.
    //float fresenelCoeff = 0.5f;
    
    photon.direction.xyz = lerp(refractedDir, specularReflect, material.smoothRefract.x); //Hack for now.

    
    //Calculate energy loss.
    photon.mana = photon.mana * absorptionFunc(material.absorption);
    
    float4 diffuseLobe = BRDF * diffuseLobeEnergy;
    float4 specularLobe = specularColor * specularLobeEnergy;
    
    return BRDF.w * (diffuseLobe + specularLobe) * photon.mana;

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