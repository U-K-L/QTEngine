#include "rayTracingAccelHelper.hlsl"
#include "../Helpers/ShaderHelpers.hlsl"
StructuredBuffer<Brush> Brushes : register(t9, space1);
// For reading
Texture3D<float> gBindless3D[] : register(t4, space0);

RaytracingAccelerationStructure tlas : register(t2, space1);


RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
    float4 texelSize; // xy = 1/width, 1/height
    float isOrtho;
}
struct PushConsts
{
    float lod;
    uint triangleCount;
    int3 voxelResolution;
};

[[vk::push_constant]]
PushConsts pc;

float Read3D(uint textureIndex, int3 coord)
{
    return gBindless3D[textureIndex].Load(int4(coord, 0));
}

float Read3DMip(uint textureIndex, int3 coord, int level)
{
    return gBindless3D[textureIndex].Load(int4(coord, level));
}

float2 GetVoxelValueTexture(int textureId, int3 coord, float sampleLevel)
{
    //float3 uvw = (float3(coord) + 0.5f) / WORLD_SDF_RESOLUTION;
    //uvw = clamp(uvw, 0.001f, 0.999f); // avoid edge bleeding
    return Read3D(textureId, coord);
}

float2 TrilinearSampleSDFTexture(float3 pos, float sampleLevel)
{
    float4 voxelSceneBounds = GetVoxelResolutionWorldSDFArbitrary(sampleLevel, pc.voxelResolution);
    float3 voxelGridRes = voxelSceneBounds.xyz;
    float3 sceneSize = GetSceneSize(); //voxelSceneBounds.w;
    
    float3 gridPos = ((pos + sceneSize * 0.5f) / sceneSize) * voxelGridRes;
    
    int3 base = int3(floor(gridPos));
    float3 fracVal = frac(gridPos); // interpolation weights

    base = clamp(base, int3(0, 0, 0), voxelGridRes - 2);

    return GetVoxelValueTexture(sampleLevel - 1, base, sampleLevel);
}

float2 TrilinearSampleSDFTextureNormals(float3 pos, float sampleLevel)
{
    float4 voxelSceneBounds = GetVoxelResolutionWorldSDFArbitrary(sampleLevel, pc.voxelResolution);
    float3 voxelGridRes = voxelSceneBounds.xyz;
    float3 sceneSize = GetSceneSize(); //voxelSceneBounds.w;
    
    float3 gridPos = ((pos + sceneSize * 0.5f) / sceneSize) * voxelGridRes;
    
    int3 base = int3(floor(gridPos));
    float3 fracVal = frac(gridPos); // interpolation weights

    base = clamp(base, int3(0, 0, 0), voxelGridRes - 2);
    
    return GetVoxelValueTexture(sampleLevel - 1, base, sampleLevel);
}


float2 SampleNormalSDFTexture(float3 pos, float sampleLevel)
{
    float4 voxelSceneBounds = GetVoxelResolutionWorldSDFArbitrary(sampleLevel, pc.voxelResolution);
    float3 voxelGridRes = voxelSceneBounds.xyz;
    float3 sceneSize = GetSceneSize();
    
    float3 halfScene = sceneSize * 0.5f;
    
    float3 voxelSize = sceneSize / voxelGridRes;

    if (any(pos < -halfScene) || any(pos > halfScene))
        return DEFUALT_EMPTY_SPACE;
    
    return TrilinearSampleSDFTexture(pos, sampleLevel);
}

float3 CentralDifferenceNormalTexture(float3 p, float sampleLevel)
{
    
    float blendFactor = 0.1f; //voxelsL1In[index].normalDistance.w;
    float smoothness = 0.1f; //voxelsL1In[index].normalDistance.x;
    
    blendFactor = clamp(blendFactor, 0.0f, 0.05f);
    smoothness = clamp(smoothness, 0.0f, 1.0f);
    //if (blendFactor < 0.00425f) //No blend, return triangle normals
    //        return 0;
    
    float eps = 0.022127f * pow(2.0f, 1.0f + (smoothness * 8.0f) + blendFactor);

    float dx = TrilinearSampleSDFTextureNormals(p + float3(eps, 0, 0), sampleLevel).x - TrilinearSampleSDFTextureNormals(p - float3(eps, 0, 0), sampleLevel).x;
    float dy = TrilinearSampleSDFTextureNormals(p + float3(0, eps, 0), sampleLevel).x - TrilinearSampleSDFTextureNormals(p - float3(0, eps, 0), sampleLevel).x;
    float dz = TrilinearSampleSDFTextureNormals(p + float3(0, 0, eps), sampleLevel).x - TrilinearSampleSDFTextureNormals(p - float3(0, 0, eps), sampleLevel).x;

    float3 n = float3(dx, dy, dz);
    return (length(n) > 1e-5f) ? normalize(n) : float3(0, 0, 0);
}


float4 FullMarch(float3 ro, float3 rd, float3 camPos, inout float4 surface, inout float4 visibility, inout float4 specular, inout float4 positionId)
{
    visibility = 1;
    float3 direction = rd;
    float3 light = normalize(float3(-0.85f, 1.0, 0.5f));
    
    float3 pos = ro;
    int maxSteps = 1024;
    float4 closesSDF = 1.0f;
    float4 currentSDF = 1.0f;
    float accumaltor = 0;
    int bounces = 0;
    float voxelSizeL1 = 0.03125f;
    
    float minDistanceL0 = voxelSizeL1 * 0.5;
    float minDistReturn = voxelSizeL1 * 0.05f;
    
    float4 hitSample = float4(100.0f, 0, 0, 100.0f);

    for (int i = 0; i < maxSteps; i++)
    {
        float sampleLevel = GetSampleLevelCone(pos, camPos);
        float4 currentSDF = 0;
        float2 sampleId = SampleNormalSDFTexture(pos, sampleLevel);
        currentSDF.x = sampleId.x;
        closesSDF = min(closesSDF, currentSDF);

                
        bool inAABB = PointInAABB(pos, -GetDCAABBSize() * 0.5, GetDCAABBSize() * 0.5);
        
        bool canTerminate =
        (closesSDF.x < minDistReturn) && !inAABB;
        
        if (closesSDF.x > 1)
            return hitSample;

        if (canTerminate)
        {
            if (bounces == 0)
            {
                //surface = float4(1, 1, 1, 1);
                //return float4(1, 1, 1, 1);
                closesSDF.yzw = CentralDifferenceNormalTexture(pos, 1);
                //int index = GetVoxelIndexFromPosition(pos, 1.0f);
                //float smoothness = voxelsL1In[index].normalDistance.x;
                surface.xyz = normalize(closesSDF.yzw);
                surface.w = length(pos - ro);
                positionId.xyz = pos;
                positionId.w = sampleId.y;
                hitSample = closesSDF;
                
                bounces = 1;
                
                direction = light;
                
                //putrude out.
                pos += surface.xyz * voxelSizeL1 * 4.0f;
                
                closesSDF.xy = SampleNormalSDFTexture(pos, sampleLevel);
                specular.w = i; //store count.
                
                return hitSample;

            }
            else if (bounces == 1)
            {
                visibility = 0;
                return hitSample;

            }
        }
     
        //update position to the nearest point. effectively a sphere trace.

        float stepSize = clamp(closesSDF.x, voxelSizeL1 * 0.05f, voxelSizeL1 * (sampleLevel + 1));
        /*
        if (bounces == 0)43423454
        {
            if (closesSDF.x < minDistanceL0)
                stepSize = voxelSizeL1 * 0.075f;
        }


        */

            
        
        accumaltor += abs((voxelSizeL1 * 4.0f * sampleLevel) - currentSDF.x) * 0.001f;
        pos += direction * stepSize;


    }
    
    
    surface.w = accumaltor;
    return hitSample;

}



[shader("raygeneration")]
void main()
{
    uint2 pixel = DispatchRaysIndex().xy;

    float2 dim = texelSize.xy;
    float2 uv = (float2(pixel) + 0.5) * dim * 2.0 - 1.0;
    uv.y = -uv.y;

    float4x4 invProj = inverse(proj);
    float4x4 invView = inverse(view);

    float4 viewPos = mul(invProj, float4(uv.x, uv.y, 0, 1));
    float3 camPos = float3(0, 0, 0); //For now player is center of the world.

    float3 perspectiveRayDir = normalize(mul((float3x3) invView, normalize(viewPos.xyz)));
    float3 perspectiveRayOrigin = mul(invView, float4(0, 0, 0, 1)).xyz;

    // If you truly need ortho too, keep your blend:
    float3 orthoRayOrigin = mul(invView, float4(viewPos.xyz, 1.0)).xyz;
    float3 orthoRayDir = normalize(mul((float3x3) invView, float3(0, 0, -1)));

    float3 ro = lerp(perspectiveRayOrigin, orthoRayOrigin, isOrtho);
    float3 rd = normalize(lerp(perspectiveRayDir, orthoRayDir, isOrtho));

    RayDesc ray;
    ray.Origin = ro;
    ray.Direction = rd;
    ray.TMin = 0.001f;
    ray.TMax = 1e6f; // use something big while debugging

    Payload p;
    p.color = float4(0, 0, 0, 0);
    


    float4 surface = p.color;
    float4 visibility = 0;
    float4 specular = 0;
    float4 depth = 0;
    float4 positionId = 0;
    float4 surfaceFull = float4(0, 0, 0, 0);
    float4 hit = 0;
    float4 col = 0;
    
    float3 bmin = -GetDCAABBSize() * 0.5f;
    float3 bmax = GetDCAABBSize() * 0.5f;
    float tHit;
    
    bool rayHitAABB = RayAABB(ro, rd, bmin, bmax, tHit);
    
    if (rayHitAABB)
    {
        TraceRay(tlas, 0, 0xFF, 0, 1, 0, ray, p);
        surface.xyz = p.color.xyz;
    }
    else
        hit = FullMarch(ro, rd, camPos, surface, visibility, specular, positionId);
    
    col = (hit.x < 1.0f) ? float4(1, 1, 1, 1) : float4(0, 0, 0, 0);
    
    
    p.color.xyz = surface.xyz;

    uint outHandle = NonUniformResourceIndex(intArray[0]);
    gBindlessStorage[outHandle][pixel] = float4(p.color.xyz, 1);
}
