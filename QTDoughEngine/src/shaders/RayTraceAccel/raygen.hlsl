#include "../Helpers/ShaderHelpers.hlsl"
#include "../Helpers/LightingHelpers.hlsl"

StructuredBuffer<Brush> Brushes : register(t9, space1);
// For reading
Texture3D<float> gBindless3D[] : register(t4, space0);

RaytracingAccelerationStructure tlas : register(t2, space1);


RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);
RWStructuredBuffer<GameObjectShaderData> globalObjMaterials : register(u6, space0);
StructuredBuffer<uint> intArray : register(t1, space1);

struct Images
{
    uint AlbedoImage;
    uint NormalImage;
    uint PositionImage;
};

Images InitImages()
{
    Images image;
    image.AlbedoImage = intArray[0];
    image.NormalImage = intArray[1];
    image.PositionImage = intArray[2];
    return image;
}

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

cbuffer Constants : register(b2, space0)
{
    float deltaTime; // offset 0
    float time; // offset 4
    float2 pad; // offset 8..15
    GPULight light[32]; // each: float4 emission + float3 direction
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


float4 FullMarch(float3 ro, float3 rd, float3 camPos, inout float4 surface)
{
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
        (closesSDF.x < minDistReturn);// && !inAABB;
        
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
                hitSample = closesSDF;
                
                bounces = 1;
                
                direction = light;
                
                //putrude out.
                pos += surface.xyz * voxelSizeL1 * 4.0f;
                
                closesSDF.xy = SampleNormalSDFTexture(pos, sampleLevel);
                
                return hitSample;

            }
            else if (bounces == 1)
            {
                return hitSample;

            }
        }
     
        //update position to the nearest point. effectively a sphere trace.

        float stepSize = clamp(closesSDF.x, voxelSizeL1 * 0.05f, voxelSizeL1 * (sampleLevel + 1));
        
        accumaltor += abs((voxelSizeL1 * 4.0f * sampleLevel) - currentSDF.x) * 0.001f;
        pos += direction * stepSize;


    }
    
    
    surface.w = accumaltor;
    return hitSample;

}

float2 samplePotentialField(float3 position)
{
    float2 sampleSDF = SampleNormalSDFTexture(position, 1.0f);
    return sampleSDF;
        
}

void photonMarch(inout Photon p, inout Surface surface, int mask = 0xFF, int maxTries = 64)
{
    RayDesc ray;
    ray.Origin = p.position;
    ray.Direction = p.direction;
    ray.TMin = 0.000001f;
    ray.TMax = Propagation_Step_Length;
    
    for (int i = 0; i < maxTries; i++)
    {
        TraceRay(tlas, 0, mask, 0, 1, 0, ray, p);
        if (p.color.w > -1)
        {
           
            surface.normal = p.color;
            return;
        }
            


        TraverseGeodesic(p, ray.Origin);
            
        //Incremental step.
        float ds = Propagation_Step_Length;
        ds *= 0.125f;
        //take larger steps in empty space.
        float dist = samplePotentialField(p.position.xyz).x;
        float tol = 0.025f;
            
        float3 bmin = -GetDCAABBSize() * 0.25f;
        float3 bmax = GetDCAABBSize() * 0.25f;
        float tHit;
        bool centerOfInterest = RayAABB(p.position.xyz, p.direction.xyz, bmin, bmax, tHit);
            
        if (dist >= abs(DEFUALT_EMPTY_SPACE - tol) || centerOfInterest == false)
            ds *= 16.0f;


        ray.Origin = p.position;
        ray.Direction = p.direction;
        ray.TMax = ds;

    }
    p.direction.w = 1.0f;

}

//Energy comes from a light source emitter. Get the light and initalize the energy.
float4 ExcitePhoton(in Photon photon)
{
    Photon probingPhoton;
    probingPhoton.direction = photon.direction;
    probingPhoton.position = photon.position;
    probingPhoton.color = -1;
    probingPhoton.mana = 1.0f;
    
    //Dummy surface
    Surface dummySurface;
    dummySurface.normal = -1;
    dummySurface.position = 0;
    
    //Only one direct light for now.
    GPULight l = light[0];
    float3 toLight = -l.direction.xyz;
    float4 Le = l.emission; //How much mana is in this light.
    
    probingPhoton.direction.xyz = toLight;

    
    //Can I reach light? In otherwords, will this miss?
    photonMarch(probingPhoton, dummySurface, 0xFF, 16);
    
    float4 absorbed = 1.0f;
    if (dummySurface.normal.w > -1)
        absorbed = absorptionFunc( 0.2285f * globalObjMaterials[(uint) (dummySurface.normal.w)].absorption);

    return Le * absorbed;
    
}

float4 accumulateLight(inout Photon p, in Surface surface, float3 camPos, bool rayHitAABB = true)
{
    GPULight l = light[0];
    float4 finalColor = 0;
    p.mana = ExcitePhoton(p);
    
    //Fire away! Multiple shots.
    int samples = 64;
    float norm = 1.0f / (1.0f - exp2(-(float) samples));
    float pdfWeight = p.mana.w;
    for (int i = 0; i < samples; i++)
    {   
        //Initial Pass.
        if(i == 0)
        {
            if (surface.normal.w >= 0)
            {
                GameObjectShaderData material = globalObjMaterials[(uint) (surface.normal.w)];
                finalColor += UnigmaBRDFDirectionalLight(material, p, surface, l) * pdfWeight;
            }
            continue;
        }

                
        if (p.mana.w < 0.001f) //DEAD.
            break;
        
        p.mana *= ExcitePhoton(p);
        
        //Ensures it always sums to the mana availible, can go above 1, which causes bloom in HDR.
        //(exp2(-(float) (i + 1)) * norm)
        pdfWeight =  p.mana.w;
        
        photonMarch(p, surface, 0xFF, 8);
        if (surface.normal.w >= 0)
        {
            GameObjectShaderData material = globalObjMaterials[(uint) (surface.normal.w)];
            finalColor += UnigmaBRDFDirectionalLight(material, p, surface, l) * pdfWeight;
        }

        
    }

    return finalColor;
}



[shader("raygeneration")]
void main()
{
    
    
    Images image = InitImages();
    uint albedoHandle = NonUniformResourceIndex(image.AlbedoImage);
    uint normalHandle = NonUniformResourceIndex(image.NormalImage);
    uint positionHandle = NonUniformResourceIndex(image.PositionImage);
    
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

    float3 orthoRayOrigin = mul(invView, float4(viewPos.xyz, 1.0)).xyz;
    float3 orthoRayDir = normalize(mul((float3x3) invView, float3(0, 0, -1)));

    float3 ro = lerp(perspectiveRayOrigin, orthoRayOrigin, isOrtho);
    float3 rd = normalize(lerp(perspectiveRayDir, orthoRayDir, isOrtho));

    RayDesc ray;
    ray.Origin = ro;
    ray.Direction = rd;
    ray.TMin = 0.001f;
    ray.TMax = 1e6f; 

    Photon p;
    p.mana = 0.0f;
    p.position = float4(ro, 0);
    p.color = float4(0, 0, 0, 0);
    p.direction = float4(rd, 1.0f);
    
    GameObjectShaderData material;
    material.Midtone = float4(0.90, 0.9, 0.78, 1.0);
    material.Highlight = float4(1.0, 0.92, 0.928, 1.0);
    material.Shadow = float4(0.9, 0.63, 0.61, 1.0);

    Surface surface;
    surface.normal = -1;
    surface.position = 0;
    float4 visibility = 0;
    float4 specular = 0;
    float4 depth = 0;
    float4 positionId = 0;
    float4 surfaceFull = float4(0, 0, 0, 0);
    float4 hit = 0;
    float4 col = 0;
    float tHit;
    float4 finalColor = 0;
    
    //Get the main center of the world by which triangle based ray tracing is done.
    float3 bmin = -GetDCAABBSize() * 0.5f;
    float3 bmax = GetDCAABBSize() * 0.5f;
    bool rayHitAABB = true; //RayAABB(ro, rd, bmin, bmax, tHit); TEMP OFF.
    
    
    photonMarch(p, surface, 0x01);
    

    
    //Store initial surface.
    Surface firstHitSurface;
    firstHitSurface.normal = surface.normal;
    firstHitSurface.position = p.position;
    
    bool surfaceHit = firstHitSurface.normal.w > -1;

    //Initial depth.
    float linearDepth = distance(p.position.xyz, ro);
    float maxRenderDistance = 64.0f;
    float normalizedDepth = linearDepth / maxRenderDistance;
    float depthMapped = surfaceHit * normalizedDepth;
    
    if (surfaceHit)
        finalColor = accumulateLight(p, surface, camPos);
    
    /*
    //Replace with phase it's in. Which can be per voxel/"triangle"
    if(firstHitSurface.normal.w == 23)
        finalColor = depthMapped;
    else
        finalColor = 0;
*/

    
    gBindlessStorage[albedoHandle][pixel] = float4(finalColor.xyz, 1.0f-visibility.x);
    gBindlessStorage[normalHandle][pixel] = float4(firstHitSurface.normal.xyz, depthMapped);
    gBindlessStorage[positionHandle][pixel] = float4(p.position.xyz, firstHitSurface.normal.w);

}
