RWTexture2D<float4> gBindlessStorage[] : register(u3, space0);

#include "../Helpers/ShaderHelpers.hlsl"


cbuffer UniformBufferObject : register(b0, space1)
{
    float4x4 model; // Model matrix
    float4x4 view; // View matrix
    float4x4 proj; // Projection matrix
    float4 texelSize;
    float isOrtho;
}

cbuffer Constants : register(b2, space0)
{
    float deltaTime; // offset 0
    float time; // offset 4
    float2 pad; // offset 8..15, total 16 bytes
};

struct PushConsts
{
    float lod;
    uint triangleCount;
    int3 voxelResolution;
};

[[vk::push_constant]]
PushConsts pc;


StructuredBuffer<VoxelL1> voxelsL1In : register(t2, space1); // readonly
RWStructuredBuffer<VoxelL1> voxelsL1Out : register(u3, space1); // write

StructuredBuffer<Voxel> voxelsL2In : register(t4, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL2Out : register(u5, space1); // write

StructuredBuffer<Voxel> voxelsL3In : register(t6, space1); // readonly
RWStructuredBuffer<Voxel> voxelsL3Out : register(u7, space1); // write


StructuredBuffer<ComputeVertex> vertexBuffer : register(t8, space1);
RWStructuredBuffer<Brush> Brushes : register(u9, space1);

RWTexture3D<float> gBindless3DStorage[] : register(u5, space0);

// For reading
Texture3D<float> gBindless3D[] : register(t4, space0);

RWStructuredBuffer<uint> BrushesIndices : register(u10, space1);

RWStructuredBuffer<uint> TileBrushCounts : register(u11, space1);

StructuredBuffer<Particle> particlesL1In : register(t12, space1); // readonly
RWStructuredBuffer<Particle> particlesL1Out : register(u13, space1); // write

StructuredBuffer<ControlParticle> controlParticlesL1In : register(t14, space1); // readonly
RWStructuredBuffer<ControlParticle> controlParticlesL1Out : register(u15, space1); // write

// Filtered read using normalized coordinates and mipmaps
float Read3D(uint textureIndex, int3 coord)
{
    return gBindless3D[textureIndex].Load(int4(coord, 0)).x;
}

float ReadWorldSDF(float3 worldPos)
{
    // Constants defining the world SDF volume (assuming these are defined in a helper)
    float worldHalfExtent = WORLD_SDF_BOUNDS * 0.5f;
    float voxelSize = WORLD_SDF_BOUNDS / pc.voxelResolution.x;

    // Convert world position to integer texture coordinates
    int3 texCoord = int3(floor((worldPos + worldHalfExtent) / voxelSize));

    // Bounds check to ensure we don't sample outside the volume
    if (any(texCoord < 0) || any(texCoord >= pc.voxelResolution.x))
    {
        return 64.0f; // Return a large distance if outside the world volume
    }

    // Sample the world SDF texture (assuming it's at bindless index 0)
    return gBindless3D[0].Load(int4(texCoord, 0)).x;
}

void Write3D(uint textureIndex, int3 coord, float value)
{
    gBindless3DStorage[textureIndex][coord] = value;
}

void Write3D(uint textureIndex, int3 coord, float2 value)
{
    gBindless3DStorage[textureIndex][coord] = value;
}

float3 getAABB(uint vertexOffset, uint vertexCount, out float3 minBounds, out float3 maxBounds)
{
    minBounds = float3(1e30, 1e30, 1e30);
    maxBounds = float3(-1e30, -1e30, -1e30);
    
    for (uint i = 0; i < vertexCount; i += 3) // Skip by 3 for triangles
    {
        for (uint j = 0; j < 3; j++) // Process each vertex of the triangle
        {
            float3 pos = vertexBuffer[vertexOffset + i + j].position.xyz;
            minBounds = min(minBounds, pos);
            maxBounds = max(maxBounds, pos);
        }
    }
    
    return abs(maxBounds - minBounds);
}



void ClearTileCount(uint3 DTid : SV_DispatchThreadID)
{
    TileBrushCounts[DTid.x] = 0;
}


float3 getAABBWorld(uint vertexOffset, uint vertexCount,
               out float3 minBounds, out float3 maxBounds, in Brush brush)
{
    
    minBounds = float3(1e30, 1e30, 1e30);
    maxBounds = float3(-1e30, -1e30, -1e30);

    for (uint i = 0; i < vertexCount; i += 3)
        for (uint j = 0; j < 3; ++j)
        {
            float4 lp = float4(vertexBuffer[vertexOffset + i + j].position.xyz, 1.0);
            float3 pos = mul(brush.model, lp).xyz;
            minBounds = min(minBounds, pos);
            maxBounds = max(maxBounds, pos);
        }
    
    //Create an enlarged container to fit deformations.
    float3 extent = abs(maxBounds - minBounds);
    float maxExtent = max(extent.x, max(extent.y, extent.z)) * 0.25f;
    minBounds -= maxExtent;
    maxBounds += maxExtent;
    
    //Additional padding based on blending values. As blending more, AABB expands.
    float blendingPadding = brush.blend * 2 * maxExtent;
    minBounds -= blendingPadding;
    maxBounds += blendingPadding;
    
    //Pad container by 4 voxels
    float voxelPadding = 0.03125f * 4;
    minBounds -= voxelPadding;
    maxBounds += voxelPadding;
    


    return abs(maxBounds - minBounds);
}


float SampleSDF(float3 worldPos, int mipLevel)
{
    float4 voxelRes = GetVoxelResolutionWorldSDFArbitrary(mipLevel + 1, pc.voxelResolution);
    float3 sceneSize = GetSceneSize();
    float3 halfScene = sceneSize.xyz * 0.5f;

    // Convert world position to continuous texel coordinates
    float3 uvw = (worldPos + halfScene) / (2.0f * halfScene);
    float3 texelCoord = uvw * voxelRes.xyz;

    // Get the integer base coordinate and the fractional part for interpolation
    int3 baseTexel = int3(floor(texelCoord));
    float3 frac = texelCoord - baseTexel;

    // Fetch the 8 corner SDF values
    float d000 = Read3D(mipLevel, baseTexel + int3(0, 0, 0)).x;
    float d100 = Read3D(mipLevel, baseTexel + int3(1, 0, 0)).x;
    float d010 = Read3D(mipLevel, baseTexel + int3(0, 1, 0)).x;
    float d110 = Read3D(mipLevel, baseTexel + int3(1, 1, 0)).x;
    float d001 = Read3D(mipLevel, baseTexel + int3(0, 0, 1)).x;
    float d101 = Read3D(mipLevel, baseTexel + int3(1, 0, 1)).x;
    float d011 = Read3D(mipLevel, baseTexel + int3(0, 1, 1)).x;
    float d111 = Read3D(mipLevel, baseTexel + int3(1, 1, 1)).x;

    // Perform trilinear interpolation (7 lerps)
    float c00 = lerp(d000, d100, frac.x);
    float c10 = lerp(d010, d110, frac.x);
    float c01 = lerp(d001, d101, frac.x);
    float c11 = lerp(d011, d111, frac.x);
    float c0 = lerp(c00, c10, frac.y);
    float c1 = lerp(c01, c11, frac.y);
    return lerp(c0, c1, frac.z);
}

float3 GetNormal(float3 worldPos)
{
    const int mipLevel = 0;
    
    const float epsilon = 0.025f;

    // Use small offset vectors for sampling along each axis.
    float3 offsetX = float3(epsilon, 0, 0);
    float3 offsetY = float3(0, epsilon, 0);
    float3 offsetZ = float3(0, 0, epsilon);

    // Calculate the gradient using central differences on the interpolated SDF values.
    float dx = SampleSDF(worldPos + offsetX, mipLevel) - SampleSDF(worldPos - offsetX, mipLevel);
    float dy = SampleSDF(worldPos + offsetY, mipLevel) - SampleSDF(worldPos - offsetY, mipLevel);
    float dz = SampleSDF(worldPos + offsetZ, mipLevel) - SampleSDF(worldPos - offsetZ, mipLevel);
    
    // The gradient vector points in the direction of the normal.
    // Add a tiny value to prevent normalization of a zero vector.
    return normalize(float3(dx, dy, dz) + 1e-6);
}


// Tiny smooth clamp helpers
float saturate01(float x)
{
    return saturate(x);
}

// Cheap pseudo-noise (continuous) for extra "dance"
float3 SinNoise3(float3 p, float t)
{
    // Continuous, no texture, deterministic.
    float n1 = sin(dot(p, float3(12.9898, 78.233, 37.719)) + t * 1.13);
    float n2 = sin(dot(p, float3(39.3467, 11.135, 83.155)) + t * 0.97);
    float n3 = sin(dot(p, float3(73.156, 52.235, 9.151)) + t * 1.21);
    return float3(n1, n2, n3);
}

// Main effect: swirling sphere + radial breathing + slight axial bob
float3 SwirlSphereDanceWS(
    float3 pWS,
    float3 centerWS,
    float t,
    float dt,
    float radius,
    float swirlStrength, // tangential speed
    float radialStrength, // in/out breathing
    float noiseStrength // small chaotic wobble
)
{
    float3 v = pWS - centerWS;
    float r = length(v);
    if (r < 1e-6f)
        return pWS;

    float3 dir = v / r;

    // A slowly precessing axis so it doesn't look like a rigid turntable.
    float3 axis = normalize(float3(
        sin(t * 0.73f) * 0.6f + 0.2f,
        cos(t * 0.91f) * 0.7f + 0.1f,
        sin(t * 0.57f) * 0.8f + 0.3f
    ));

    // Tangential direction around the axis
    float3 tangent = cross(axis, dir);
    float tangLen = length(tangent);
    tangent = (tangLen > 1e-6f) ? (tangent / tangLen) : float3(0, 0, 0);

    // Falloff: strongest near center, fades to 0 at/beyond radius.
    // smoothstep(radius, 0, r) => 1 at r=0, 0 at r>=radius
    float falloff = smoothstep(radius, 0.0f, r);

    // Radial breathing: phase depends on radius for nice layered waves.
    float pulse = sin(t * 2.4f + r * 6.0f);

    // Add a gentle axial bob so the sphere "stirs" in 3D.
    float axial = sin(t * 1.7f + r * 2.5f);

    // Small continuous wobble (kept subtle to avoid exploding motion).
    float3 wobble = SinNoise3(pWS * 1.3f, t) * noiseStrength;

    // Compose velocity
    float3 vel =
        tangent * (swirlStrength * falloff * (0.65f + 0.35f * pulse)) +
        dir * (radialStrength * falloff * pulse) +
        axis * (0.25f * radialStrength * falloff * axial) +
        wobble * falloff;

    return pWS + vel * dt;
}



void ParticlesSDF(uint3 DTid : SV_DispatchThreadID)
{
    //Move to world space if connected to a brush.
    Particle particle = particlesL1In[DTid.x];
    Brush brush = Brushes[particle.particleIDs.x];
    
    if (particle.position.w < 1)
    {
        
        particlesL1Out[DTid.x].position = particle.position;
        particlesL1Out[DTid.x].initPosition = particle.initPosition;

        return;
    }

    
    float sigma = 0.6325f * 0.1f;//brush.smoothness; // Controls the spread of the Gaussian
    float amplitude = 1.0f; // Can be a particle attribute
    float radiusParticleSpacing = brush.particleRadius;
    
    float supportWS = sigma * 1.75f;
    
    float3 position = particle.position.xyz;
    
    


    
    float speed = 0.001f;
    float timeX = time.x * speed;
    
    float3 direction = normalize(position - float3(0, 0, 0));

    position = mul(brush.model, float4(position, 1.0f)).xyz;
    float3 positionOld = position;


    
    float distFromHeat = 1 / pow(length(position - float3(1.5, 0, 0)), 2);
    
    float t = time.x * 0.001f; // your existing speed scaling
    float3 centerWS = mul(brush.model, float4(0, 0, 0, 1)).xyz; // or any world-space pivot

    float danceRadius = 20.0f;
    /*
    position = SwirlSphereDanceWS(
    position,
    centerWS,
    t,
    deltaTime,
    danceRadius,
    23.5f,
    20.0f,
    1.6f
);

    if(distFromHeat < 2.125f)
        position += 0.096885f * (direction + float3(0, 0, -9.9)) * deltaTime * distFromHeat;

    
    */
    
    float3 voxelRes = GetVoxelResolutionL1().xyz; ///GetVoxelResolutionWorldSDFArbitrary(1.0f, pc.voxelResolution).xyz;
    float3 sceneSize = GetSceneSize();
    
    float3 voxelSize = sceneSize / voxelRes;
    float3 halfScene = sceneSize * 0.5f;

    float3 minPos = position - supportWS;
    float3 maxPos = position + supportWS;

    int3 minVoxel = floor((minPos + halfScene) / voxelSize);
    int3 maxVoxel = floor((maxPos + halfScene) / voxelSize);
    
    //float3 n = GetNormal(position);

    float3 initialPosition = mul(brush.model, float4(particle.initPosition.xyz, 1.0f)).xyz;
    
    for (int z = minVoxel.z; z <= maxVoxel.z; ++z)
        for (int y = minVoxel.y; y <= maxVoxel.y; ++y)
            for (int x = minVoxel.x; x <= maxVoxel.x; ++x)
            {
                int3 voxelIndex = int3(x, y, z);
                
                int3 res = int3(voxelRes);
                if (x < 0 || y < 0 || z < 0 || x >= res.x || y >= res.y || z >= res.z)
                    continue;
                  
                // worldPos = (VoxelIndex + 0.5) * VoxelSize - HalfScene
                float3 worldPos = (float3(voxelIndex) + 0.5f) * voxelSize - halfScene;
                
                float3 diffWS = worldPos - position;
                float squaredDist = dot(diffWS, diffWS);

                float gaussianValue = amplitude * exp(-squaredDist / (2.0f * sigma * sigma));

                uint guassContribution = (uint) round(gaussianValue * DENSITY_SCALE);
                
                uint flatIndex = Flatten3D(voxelIndex, voxelRes);

                    
                float3 velocity = ((position.xyz - initialPosition.xyz) + (positionOld - position)) * deltaTime;
                float mag = length(velocity);
                particle.initPosition.w += mag;
                    
                //Special flag.
                if (particle.initPosition.w > 0.005f)
                {
                    voxelsL1Out[flatIndex].jacobian = 0.1;

                }
                
                float sd = length(worldPos - position) - radiusParticleSpacing;
                
                int distanceContribution = (int) round(sd * (float) guassContribution);
                
                InterlockedAdd(voxelsL1Out[flatIndex].density, guassContribution);
                InterlockedAdd(voxelsL1Out[flatIndex].distance, distanceContribution);

            }
    
    if(particle.initPosition.w > 0.05f)
        Brushes[particle.particleIDs.x].isDeformed = true;
    
    particle.initPosition.w *= 0.95f;
    
    particlesL1Out[DTid.x].position.xyz = mul(brush.invModel, float4(position, 1.0f)).xyz;
    particlesL1Out[DTid.x].initPosition = particle.initPosition;

}

void UpdateControlPoints(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    
    // Determine which brush this control point belongs to
    // Assumes CAGE_VERTS is defined (e.g., as 26) in a helper header
    uint brushID = index / CAGE_VERTS;
    Brush brush = Brushes[brushID];

    // Read the CURRENT local position of the control point from the previous frame
    float3 local_pos = controlParticlesL1In[index].position.xyz;

    // --- Apply physics/animation ---
    // Applying simple gravity as an example
    float3 gravity = float3(0.0f, 0.0, 0.0098f);
    float3 new_local_pos = local_pos + (gravity * (1.0f - brush.stiffness));
    
    // --- Collision Detection ---

    float3 new_world_pos = mul(brush.model, float4(new_local_pos, 1.0)).xyz;

    float sdf_dist = ReadWorldSDF(new_world_pos);

    if (sdf_dist <= 0.2f)
    {
        // Collision detected! The point is inside or on a surface.
        // For a simple response, we just stop its movement by reverting to the previous position.
        new_local_pos = local_pos + (gravity * (1.0f - max(brush.stiffness, 0.9f) ));
    }
    
    //new_local_pos = local_pos;
    // Write the final, validated position back to the output buffer
    controlParticlesL1Out[index].position.xyz = new_local_pos;
}

[numthreads(8, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    
    float level = pc.lod;
    if(level == 5.0f)
    {
        ClearTileCount(DTid);
        return;
    }
    
    if (level == 2.0f)
    {
        ParticlesSDF(DTid);
        return;
    }
    
    if(level == 8.0f)
    {
        UpdateControlPoints(DTid);
    }
    
   //Do this per brush.
    uint brushID = DTid.x;
    Brush brush = Brushes[brushID];
    
    //This happens when aabbmax roughly equals aabbmin.
    /*
    if(brush.isDirty == 2)
        return;
    if(brush.id > MAX_BRUSHES)
        return;
    */
    /*
    bool needUpdate = brush.isDirty;
    if (needUpdate == false)
        return;
    */

    // Get actual AABB from vertices
    float3 minBounds, maxBounds;
    float3 extent = getAABBWorld(brush.vertexOffset, brush.vertexCount, minBounds, maxBounds, brush);
    
    float3 brushMin = minBounds;
    float3 brushMax = maxBounds;
    
    Brushes[brushID].invModel = inverse(brush.model);
    //brush.aabbmax = maxBounds;
    //brush.aabbmin = minBounds;
    
    float3 worldHalfExtent = GetSceneSize() * 0.5f;
    float3 voxelSize = GetSceneSize() / pc.voxelResolution;
    float3 tileWorldSize = GetTileSize(pc.voxelResolution) * voxelSize;
    int3 numOfTilesDim = (pc.voxelResolution / GetTileSize(pc.voxelResolution));
    
    int3 minTile = floor((brushMin + worldHalfExtent) / tileWorldSize);
    int3 maxTile = floor((brushMax + worldHalfExtent) / tileWorldSize);
    

    for (int z = minTile.z; z <= maxTile.z; ++z)
        for (int y = minTile.y; y <= maxTile.y; ++y)
            for (int x = minTile.x; x <= maxTile.x; ++x)
            {
                int3 tileCoord = int3(x, y, z);
                uint tileIndex = Flatten3D(tileCoord, numOfTilesDim);
                
                // Atomically reserve a slot in this tile's brush list
                uint writeIndex;
                InterlockedAdd(TileBrushCounts[tileIndex], 1, writeIndex);

                if (writeIndex < TILE_MAX_BRUSHES)
                {
                    uint offset = tileIndex * TILE_MAX_BRUSHES + writeIndex;
                    BrushesIndices[offset] = brushID;
                }

            }
}
