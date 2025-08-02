
#define PI 3.14159265359

#define IDENTITY_MATRIX float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)

#define NOISE_SIMPLEX_1_DIV_289 0.00346020761245674740484429065744f

#define WORLD_SDF_RESOLUTION 512.0f
#define WORLD_SDF_BOUNDS 16.0f

#define VOXEL_RESOLUTIONL1 256.0f
#define SCENE_BOUNDSL1 16.0f

#define VOXEL_RESOLUTIONL2 128.0f
#define SCENE_BOUNDSL2 16.0f

#define VOXEL_RESOLUTIONL3 64.0f
#define SCENE_BOUNDSL3 16.0f

#define TILE_MAX_BRUSHES 32.0f
#define TILE_SIZE 8.0f

#define DEFORMATION_CHUNK 8.0f

#define DEFUALT_EMPTY_SPACE 1.75f

#define CAGE_VERTS 26

#define MAX_BRUSHES 8192

#define NO_LABEL 16777215  // safe max exact int
float NO_LABELF()
{
    return 16777215.0f;
}

// cheap length (dot*rsqrt)
inline float lenFast(float3 v)
{
    float s = dot(v, v) + 1e-6f;
    return s * rsqrt(s);
}
#define TAN_HALF(a,b,la,lb) (lenFast(cross(a,b)) / (la*lb + dot(a,b) + 1e-6f))

struct Voxel
{
    uint distance;
    float id;
    uint brushId;
    uint uniqueId;
    float4 normalDistance;
};

struct Particle
{
    float4 position;
    float4 velocity;
    float4 force;
    float4 tbd;
    float4 tbd2;
    float4 tbd3;
    float4 tbd4;
};

struct ControlParticle
{
    float4 position;
};

struct Brush
{
    uint type;
    uint vertexCount;
    uint vertexOffset;
    uint textureID;
    uint textureID2;
    uint resolution;
    float4x4 model;
    float4x4 invModel;
    float4 aabbmin;
    float4 aabbmax;
    float4 center;
    uint isDirty;
    float stiffness;
    float blend;
    uint opcode;
    uint id;
    //Make some of this materials.
    float smoothness;
};

struct Vertex
{
    float3 position;
    float3 color;
    float2 texCoord;
    float3 normal;
};

struct ComputeVertex
{
    float4 position; // 16 bytes
    float4 normal; // 16 bytes
    float4 texCoord; // 16 bytes
}; // Total: 48 bytes

float2 GetVoxelResolutionWorldSDF(float sampleLevel)
{
    float2 result = float2(WORLD_SDF_RESOLUTION, WORLD_SDF_BOUNDS);
    
    result.x /= pow(2, sampleLevel - 1);
    
    return result;

}

float2 GetVoxelResolution(float sampleLevel)
{
    float2 v1 = float2(VOXEL_RESOLUTIONL1, SCENE_BOUNDSL1);
    float2 v2 = float2(VOXEL_RESOLUTIONL2, SCENE_BOUNDSL2);
    float2 v3 = float2(VOXEL_RESOLUTIONL3, SCENE_BOUNDSL3);

    float s1 = step(sampleLevel, 1.01f);
    float s2 = step(sampleLevel, 2.01f) - s1;
    float s3 = 1.0f - s1 - s2;

    return v1 * s1 + v2 * s2 + v3 * s3;
}


float GetSampleLevel(float3 pos, float3 camPos)
{

    float2 voxelSceneBounds = GetVoxelResolution(1.0f);
    float halfScene = voxelSceneBounds.y * 0.5f;

    if (all(pos >= -halfScene) && all(pos <= halfScene))
        return 1.0f;
    
    voxelSceneBounds = GetVoxelResolution(2.0f);
    halfScene = voxelSceneBounds.y * 0.5f;
    
    if (all(pos >= -halfScene) && all(pos <= halfScene))
        return 2.0f;
    
    voxelSceneBounds = GetVoxelResolution(3.0f);
    halfScene = voxelSceneBounds.y * 0.5f;
    
    if (all(pos >= -halfScene) && all(pos <= halfScene))
        return 3.0f;
    
    return 3.0f;
    
    /*
    float d = length(pos);
    float level1 = step(d, SCENE_BOUNDSL1 * 0.5f);
    float level2 = step(d, SCENE_BOUNDSL2 * 0.5f);
    return 3.0f - level2 - level1;
    */
}

#define CONE_SPREAD_FACTOR 0.275f 
#define MAX_MIP_LEVEL 5.0f

float GetSampleLevelCone(float3 pos, float3 camPos)
{
    float dist = length(pos - camPos);
    float lod = log2(dist * CONE_SPREAD_FACTOR);
    float snapped_lod = round(lod);
    return clamp(snapped_lod, 1.0f, MAX_MIP_LEVEL); //Always do at least 2 for that smoothness.
}


float dot2(in float3 v)
{
    return dot(v, v);
}

float udTriangle(in float3 v1, in float3 v2, in float3 v3, in float3 p)
{
    float3 v21 = v2 - v1;
    float3 p1 = p - v1;
    float3 v32 = v3 - v2;
    float3 p2 = p - v2;
    float3 v13 = v1 - v3;
    float3 p3 = p - v3;
    float3 nor = cross(v21, v13);

    return sqrt( // inside/outside test    
                 (sign(dot(cross(v21, nor), p1)) +
                  sign(dot(cross(v32, nor), p2)) +
                  sign(dot(cross(v13, nor), p3)) < 2.0)
                  ?
                  // 3 edges    
                  min(min(
                  dot2(v21 * clamp(dot(v21, p1) / dot2(v21), 0.0, 1.0) - p1),
                  dot2(v32 * clamp(dot(v32, p2) / dot2(v32), 0.0, 1.0) - p2)),
                  dot2(v13 * clamp(dot(v13, p3) / dot2(v13), 0.0, 1.0) - p3))
                  :
                  // 1 face    
                  dot(nor, p1) * dot(nor, p1) / dot2(nor));
}

float udTrianglePlaneOnly(float3 v1, float3 v2, float3 v3, float3 p)
{
    float3 n = normalize(cross(v2 - v1, v3 - v1));
    return abs(dot(n, p - v1)); // unsigned distance to triangle's plane
}

int HashPositionToVoxelIndex(float3 pos, float sceneBounds, int voxelResolution)
{
    float halfScene = sceneBounds * 0.5f;
    float3 gridPos = (pos + halfScene) / sceneBounds;
    int3 voxelCoord = int3(floor(gridPos * voxelResolution));

    //Clamp to avoid invalid access
    voxelCoord = clamp(voxelCoord, int3(0, 0, 0), int3(voxelResolution - 1, voxelResolution - 1, voxelResolution - 1));

    return voxelCoord.x * voxelResolution * voxelResolution + voxelCoord.y * voxelResolution + voxelCoord.z;
}


int HashPositionToVoxelIndexR(float3 pos, float sceneBounds, int voxelResolution)
{
    float halfScene = sceneBounds * 0.5f;
    float3 gridPos = (pos + halfScene) / sceneBounds;
    int3 voxelCoord = int3(floor(gridPos * voxelResolution));

    //Clamp to avoid invalid access
    voxelCoord = clamp(voxelCoord, int3(0, 0, 0), int3(voxelResolution - 1, voxelResolution - 1, voxelResolution - 1));

    return voxelCoord.x + voxelCoord.y * voxelResolution + voxelCoord.z * voxelResolution * voxelResolution;
}

int3 HashPositionToVoxelIndex3(float3 pos, float sceneBounds, int voxelResolution)
{
    float halfScene = sceneBounds * 0.5f;
    float3 gridPos = (pos + halfScene) / sceneBounds;
    int3 voxelCoord = int3(floor(gridPos * voxelResolution));

    //Clamp to avoid invalid access
    voxelCoord = clamp(voxelCoord, int3(0, 0, 0), int3(voxelResolution - 1, voxelResolution - 1, voxelResolution - 1));

    return voxelCoord;
}

int Flatten3D(int3 voxelCoord, int voxelResolution)
{
    return voxelCoord.x * voxelResolution * voxelResolution + voxelCoord.y * voxelResolution + voxelCoord.z;
}

int Flatten3DR(int3 voxelCoord, int voxelResolution)
{
    return voxelCoord.x + voxelCoord.y * voxelResolution + voxelCoord.z * voxelResolution * voxelResolution;
}

float3 VoxelCenter(int3 v, float3 gridOrigin, float voxelSize)
{
    return gridOrigin + (float3(v) + 0.5f) * voxelSize;
}

int3 WorldToVoxel(float3 p, float3 gridOrigin, float voxelSize, int voxelResolution)
{
    // offset into the grid, then convert to voxel index
    float3 fp = (p - gridOrigin) / voxelSize;
    int3 ip = (int3) floor(fp);

    // clamp so we never read/write out of bounds
    ip = clamp(ip, 0, voxelResolution - 1);
    return ip;
}

bool TriangleAABBOverlap(float3 a, float3 b, float3 c, float3 voxelMin, float3 voxelMax)
{
    float3 triMin = min(a, min(b, c));
    float3 triMax = max(a, max(b, c));
    return !(triMin.x > voxelMax.x || triMax.x < voxelMin.x ||
             triMin.y > voxelMax.y || triMax.y < voxelMin.y ||
             triMin.z > voxelMax.z || triMax.z < voxelMin.z);
}



//Moller method.
bool TriangleTrace(float3 ro, float3 rd, float3 a, float3 b, float3 c, inout float t, inout float u, inout float v)
{
    float epsilon = 0.00001;
    float3 edge1 = b - a;
    float3 edge2 = c - a;

    float3 pvec = cross(rd, edge2);

    float determinate = dot(edge1, pvec);

    if (determinate < epsilon)
        return false;

    float3 tvec = ro - a;
    float inv_det = 1.0 / determinate;

    u = dot(tvec, pvec) * inv_det;
    if (u < 0.0 || u > 1.0)
    {
        return false;
    }

    float3 qvec = cross(tvec, edge1);
    v = dot(rd, qvec) * inv_det;
    if (v < 0.0 || u + v > 1.0)
    {
        return false;
    }

    //Final calculate of T after crammer's rule.
    t = dot(edge2, qvec) * inv_det;

    return true;
}


// Calculates the signed distance from point p to the triangle (v1, v2, v3)
float sdTriangle(float3 v1, float3 v2, float3 v3, float3 p)
{
    float3 e0 = v2 - v1;
    float3 e1 = v3 - v2;
    float3 e2 = v1 - v3;
    float3 n = cross(e0, -e2);
    
    float3 p0 = p - v1;
    float3 p1 = p - v2;
    float3 p2 = p - v3;

    float3 n0 = cross(e0, n);
    float3 n1 = cross(e1, n);
    float3 n2 = cross(e2, n);

    // Winding-aware inside-triangle test
    float inside = (dot(n0, p0) >= 0.0 && dot(n1, p1) >= 0.0 && dot(n2, p2) >= 0.0) ? 1.0 : 0.0;

    // Distance to triangle plane (if inside)
    float distPlane = dot(n, p0) / length(n);

    // Distance to edges (if outside)
    float edgeDist = min(
        length(p0 - e0 * clamp(dot(e0, p0) / dot(e0, e0), 0.0, 1.0)),
        min(
            length(p1 - e1 * clamp(dot(e1, p1) / dot(e1, e1), 0.0, 1.0)),
            length(p2 - e2 * clamp(dot(e2, p2) / dot(e2, e2), 0.0, 1.0))
        )
    );

    float unsignedDist = lerp(edgeDist, abs(distPlane), inside);
    float signDist = sign(dot(n, p0));

    return signDist * unsignedDist;
}


float DistanceToTriangle(float3 p, float3 a, float3 b, float3 c)
{
    /*
    float3 ba = b - a, pa = p - a;
    float3 cb = c - b, pb = p - b;
    float3 ac = a - c, pc = p - c;
    float3 nor = cross(ba, ac);

    float signTri =
        sign(dot(cross(ba, nor), pa)) +
        sign(dot(cross(cb, nor), pb)) +
        sign(dot(cross(ac, nor), pc));

    float d = min(min(
        length(ba * clamp(dot(ba, pa) / dot(ba, ba), 0.0, 1.0) - pa),
        length(cb * clamp(dot(cb, pb) / dot(cb, cb), 0.0, 1.0) - pb)),
        length(ac * clamp(dot(ac, pc) / dot(ac, ac), 0.0, 1.0) - pc));

    float distToPlane = abs(dot(nor, pa)) / length(nor);
    float distResult = (signTri >= 2.0) ? distToPlane : d;
    
    return distResult - 0.025f;
*/
    return udTriangle(a, b, c, p);
}

float mod289(float x)
{
    return x - floor(x * NOISE_SIMPLEX_1_DIV_289) * 289.0;
}

float2 mod289(float2 x)
{
    return x - floor(x * NOISE_SIMPLEX_1_DIV_289) * 289.0;
}

float3 mod289(float3 x)
{
    return x - floor(x * NOISE_SIMPLEX_1_DIV_289) * 289.0;
}

float4 mod289(float4 x)
{
    return x - floor(x * NOISE_SIMPLEX_1_DIV_289) * 289.0;
}

float2 hash2(float2 p)
{ // procedural white noise	
    return frac(sin(float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3)))) * 43758.5453);
}

float4x4 inverse(float4x4 m)
{
    float n11 = m[0][0], n12 = m[1][0], n13 = m[2][0], n14 = m[3][0];
    float n21 = m[0][1], n22 = m[1][1], n23 = m[2][1], n24 = m[3][1];
    float n31 = m[0][2], n32 = m[1][2], n33 = m[2][2], n34 = m[3][2];
    float n41 = m[0][3], n42 = m[1][3], n43 = m[2][3], n44 = m[3][3];

    float t11 = n23 * n34 * n42 - n24 * n33 * n42 + n24 * n32 * n43 - n22 * n34 * n43 - n23 * n32 * n44 + n22 * n33 * n44;
    float t12 = n14 * n33 * n42 - n13 * n34 * n42 - n14 * n32 * n43 + n12 * n34 * n43 + n13 * n32 * n44 - n12 * n33 * n44;
    float t13 = n13 * n24 * n42 - n14 * n23 * n42 + n14 * n22 * n43 - n12 * n24 * n43 - n13 * n22 * n44 + n12 * n23 * n44;
    float t14 = n14 * n23 * n32 - n13 * n24 * n32 - n14 * n22 * n33 + n12 * n24 * n33 + n13 * n22 * n34 - n12 * n23 * n34;

    float det = n11 * t11 + n21 * t12 + n31 * t13 + n41 * t14;
    float idet = 1.0f / det;

    float4x4 ret;

    ret[0][0] = t11 * idet;
    ret[0][1] = (n24 * n33 * n41 - n23 * n34 * n41 - n24 * n31 * n43 + n21 * n34 * n43 + n23 * n31 * n44 - n21 * n33 * n44) * idet;
    ret[0][2] = (n22 * n34 * n41 - n24 * n32 * n41 + n24 * n31 * n42 - n21 * n34 * n42 - n22 * n31 * n44 + n21 * n32 * n44) * idet;
    ret[0][3] = (n23 * n32 * n41 - n22 * n33 * n41 - n23 * n31 * n42 + n21 * n33 * n42 + n22 * n31 * n43 - n21 * n32 * n43) * idet;

    ret[1][0] = t12 * idet;
    ret[1][1] = (n13 * n34 * n41 - n14 * n33 * n41 + n14 * n31 * n43 - n11 * n34 * n43 - n13 * n31 * n44 + n11 * n33 * n44) * idet;
    ret[1][2] = (n14 * n32 * n41 - n12 * n34 * n41 - n14 * n31 * n42 + n11 * n34 * n42 + n12 * n31 * n44 - n11 * n32 * n44) * idet;
    ret[1][3] = (n12 * n33 * n41 - n13 * n32 * n41 + n13 * n31 * n42 - n11 * n33 * n42 - n12 * n31 * n43 + n11 * n32 * n43) * idet;

    ret[2][0] = t13 * idet;
    ret[2][1] = (n14 * n23 * n41 - n13 * n24 * n41 - n14 * n21 * n43 + n11 * n24 * n43 + n13 * n21 * n44 - n11 * n23 * n44) * idet;
    ret[2][2] = (n12 * n24 * n41 - n14 * n22 * n41 + n14 * n21 * n42 - n11 * n24 * n42 - n12 * n21 * n44 + n11 * n22 * n44) * idet;
    ret[2][3] = (n13 * n22 * n41 - n12 * n23 * n41 - n13 * n21 * n42 + n11 * n23 * n42 + n12 * n21 * n43 - n11 * n22 * n43) * idet;

    ret[3][0] = t14 * idet;
    ret[3][1] = (n13 * n24 * n31 - n14 * n23 * n31 + n14 * n21 * n33 - n11 * n24 * n33 - n13 * n21 * n34 + n11 * n23 * n34) * idet;
    ret[3][2] = (n14 * n22 * n31 - n12 * n24 * n31 - n14 * n21 * n32 + n11 * n24 * n32 + n12 * n21 * n34 - n11 * n22 * n34) * idet;
    ret[3][3] = (n12 * n23 * n31 - n13 * n22 * n31 + n13 * n21 * n32 - n11 * n23 * n32 - n12 * n21 * n33 + n11 * n22 * n33) * idet;

    return ret;
}



float LinearizeDepth(float depth)
{
    // near and far plane CHANGE TO UNIFORMS
    float near_plane = 0.1;
    float far_plane = 100.0;
    float z = depth * 2.0 - 1.0; // transform to NDC coordinates
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

//Iquilezles, raises value slightly, m = threshold (anything above m stays unchanged).
// n = value when 0
// x = value input.
float almostIdentity(float x, float m, float n)
{
    if (x > m)
        return x;

    const float a = 2.0 * n - m;
    const float b = 2.0 * m - 3.0 * n;
    const float t = x / m;
    return (a * t + b) * t * t + n;
}

// ( x*34.0 + 1.0 )*x =
// x*x*34.0 + x
float permute(float x)
{
    return mod289(
        x * x * 34.0 + x
    );
}

float3 permute(float3 x)
{
    return mod289(
        x * x * 34.0 + x
    );
}

float4 permute(float4 x)
{
    return mod289(
        x * x * 34.0 + x
    );
}



float taylorInvSqrt(float r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

float4 taylorInvSqrt(float4 r)
{
    return 1.79284291400159 - 0.85373472095314 * r;
}

uint clz(uint b)
{
    uint len = b ? (31 - floor(log2(b))) : 32;
    return len;
}

uint setBit(uint input, uint index)
{
    uint mask = 1U << index;
    input = input | mask;
    return input;
}

uint clearBit(uint input, uint index)
{
    uint mask = 1U << index;
    input = input & ~mask;
    return input;
}

float4 grad4(float j, float4 ip)
{
    const float4 ones = float4(1.0, 1.0, 1.0, -1.0);
    float4 p, s;
    p.xyz = floor(frac(j * ip.xyz) * 7.0) * ip.z - 1.0;
    p.w = 1.5 - dot(abs(p.xyz), ones.xyz);

    // GLSL: lessThan(x, y) = x < y
    // HLSL: 1 - step(y, x) = x < y
    s = float4(
        1 - step(0.0, p)
        );

    // Optimization hint Dolkar
    // p.xyz = p.xyz + (s.xyz * 2 - 1) * s.www;
    p.xyz -= sign(p.xyz) * (p.w < 0);

    return p;
}

float hash1(float n)
{
    return frac(sin(n) * 43758.5453);
}


float rand(float val)
{
    float3 co = float3(val, val, val);
    return frac(sin(dot(co.xyz, float3(12.9898, 78.233, 53.539))) * 43758.5453);
}


float rand(float2 co)
{
    return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

float rand(float3 co)
{
    return frac(sin(dot(co.xyz, float3(12.9898, 78.233, 53.539))) * 43758.5453);
}

float4 rand4(float4 co)
{
    return float4(rand(co.x), rand(co.y), rand(co.z), rand(co.w));
}

float3 randPos(float seed)
{
    return float3(rand(seed), rand(seed + 1), rand(seed + 2));
}

float3 randPos(float seed, float3 pos)
{
    return float3(rand(seed + pos.x), rand(seed + pos.y + 1 ), rand(seed + pos.z + 2));
}

// Returns a pseudorandom number. By Ronja Böhringer
float rand(float4 value)
{
    float4 smallValue = sin(value);
    float random = dot(smallValue, float4(12.9898, 78.233, 37.719, 09.151));
    random = frac(sin(random) * 143758.5453);
    return random;
}

float rand(float3 pos, float offset)
{
    return rand(float4(pos, offset));
    
}

float randNegative1to1(float3 pos, float offset)
{
    return rand(pos, offset) * 2 - 1;
}

//Box–Muller transform: https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-37-efficient-random-number-generation-and-application
float2 randGaussian(float3 pos, float offset)
{
    float u1 = rand(pos, offset);
    float u2 = rand(pos, offset + 1);
    float theta = 2 * PI * u1;
    float rho = 0.164955 * sqrt(-2 * log(abs(u2) + 0.01));
    float z0 = rho * cos(theta) + 0.5;
    float z1 = rho * sin(theta) + 0.5;
    z0 = max(z0, 0);
    z0 = min(z0, 1);
    z1 = max(z1, 0);
    z1 = min(z1, 1);
    return float2(z0, z1);
}

float rand2dTo1d(float2 value, float2 dotDir = float2(12.9898, 78.233))
{
    float2 smallValue = sin(value);
    float random = dot(smallValue, dotDir);
    random = frac(sin(random) * 143758.5453);
    return random;
}

float2 rand2dTo2d(float2 value)
{
    return float2(
        rand2dTo1d(value, float2(12.989, 78.233)),
        rand2dTo1d(value, float2(39.346, 11.135))
        );
}

float smin(float a, float b, float k)
{
    k *= 16.0 / 3.0;
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * (4.0 - h) * k * (1.0 / 16.0);
}

float4 smin(float4 a, float4 b, float k)
{
    k *= 4.0f;
    float h = max(k - abs(a.x - b.x), 0.0f) / (2.0f * k);

    float resultDist = min(a.x, b.x) - h * h * k;
    float3 resultGrad = (a.x < b.x) ? lerp(a.yzw, b.yzw, h)
                                    : lerp(a.yzw, b.yzw, 1.0f - h);

    return float4(resultDist, resultGrad);
}

float2 voronoiNoise(float2 value)
{

    //From ronja Tutorials. Gets the distance of each point to generate voronoi Noise.
    float2 baseCell = floor(value);
    float minDistToCell = 10;
    float2 closestCell;
    [unroll]
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float2 cell = baseCell + float2(x, y);
            float2 cellPosition = cell + rand2dTo2d(cell);
            float2 toCell = cellPosition - value;
            float distToCell = length(toCell);
            if (distToCell < minDistToCell)
            {
                minDistToCell = distToCell;
                closestCell = cell;
            }

        }
    }
    float random = rand2dTo1d(closestCell);
    return float2(minDistToCell, random);
}

float3 GammaEncode(float3 color, float gamma)
{
    float3 gammaCorrected = 1.0 / gamma;
    return pow(color, gammaCorrected);
}

float3 Barycentric(float3 a, float3 b, float3 c, float3 p)
{
    float3 v0 = b - a, v1 = c - a, v2 = p - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    float3 bary;
    bary.y = (d11 * d20 - d01 * d21) / denom;
    bary.z = (d00 * d21 - d01 * d20) / denom;
    bary.x = 1.0 - bary.y - bary.z;
    
    if (abs(denom) < 1e-6f || !all(isfinite(bary)))
    {
        bary = float3(1.0, 0.0, 0.0); // fallback to a
    }
    
    return bary;
}

float sdSphere(float3 p, float3 center, float radius)
{
    return length(p - center) - radius;
}


float opUnion(float d1, float d2)
{
    return min(d1, d2);
}
float opSubtraction(float d1, float d2)
{
    return max(-d1, d2);
}
float opIntersection(float d1, float d2)
{
    return max(d1, d2);
}
float opXor(float d1, float d2)
{
    return max(min(d1, d2), -max(d1, d2));
}


float opSmoothUnion(float d1, float d2, float k)
{
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) - k * h * (1.0 - h);
}

float opSmoothSubtraction(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return lerp(d2, -d1, h) + k * h * (1.0 - h);
}

float opSmoothIntersection(float d1, float d2, float k)
{
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return lerp(d2, d1, h) + k * h * (1.0 - h);
}



        //-----------------------------------
    // 26-VERTEX CANONICAL CAGE
    //-----------------------------------
static const float3 canonicalControlPoints[CAGE_VERTS] =
{
        // 0-7  corners  (+X first, then –X)
        float3(1, 1, 1), // 0
        float3(-1, 1, 1), // 1
        float3(1, -1, 1), // 2
        float3(-1, -1, 1), // 3
        float3(1, 1, -1), // 4
        float3(-1, 1, -1), // 5
        float3(1, -1, -1), // 6
        float3(-1, -1, -1), // 7

        // 8-19  edge mid-points   (Z-front edges, then back, then X-edges, then Y-edges)
        float3(0, 1, 1), //  8  top front
        float3(0, -1, 1), //  9  bottom front
        float3(0, 1, -1), // 10  top back
        float3(0, -1, -1), // 11  bottom back
        float3(1, 0, 1), // 12  right front
        float3(-1, 0, 1), // 13  left  front
        float3(1, 0, -1), // 14  right back
        float3(-1, 0, -1), // 15  left  back
        float3(1, 1, 0), // 16  right top
        float3(-1, 1, 0), // 17  left  top
        float3(1, -1, 0), // 18  right bottom
        float3(-1, -1, 0), // 19  left  bottom

        // 20-26  face centres
        float3(0, 0, 1), // 20
        float3(0, 0, -1), // 21
        float3(1, 0, 0), // 22
        float3(-1, 0, 0), // 23
        float3(0, 1, 0), // 24
        float3(0, -1, 0) // 25
};

    // ------------------------------------------
    // 48 TRIANGLES (6 faces × 8 tris)
    // ------------------------------------------
static const uint3 triangles[] =
{
        // FRONT (+Z)
    uint3(0, 8, 20), uint3(8, 1, 20),
        uint3(1, 13, 20), uint3(13, 3, 20),
        uint3(3, 9, 20), uint3(9, 2, 20),
        uint3(2, 12, 20), uint3(12, 0, 20),

        // BACK (–Z)
        uint3(4, 10, 21), uint3(10, 5, 21),
        uint3(5, 15, 21), uint3(15, 7, 21),
        uint3(7, 11, 21), uint3(11, 6, 21),
        uint3(6, 14, 21), uint3(14, 4, 21),

        // RIGHT (+X)
        uint3(0, 16, 22), uint3(16, 4, 22),
        uint3(4, 14, 22), uint3(14, 6, 22),
        uint3(6, 18, 22), uint3(18, 2, 22),
        uint3(2, 12, 22), uint3(12, 0, 22),

        // LEFT (–X)
        uint3(1, 17, 23), uint3(17, 5, 23),
        uint3(5, 15, 23), uint3(15, 7, 23),
        uint3(7, 19, 23), uint3(19, 3, 23),
        uint3(3, 13, 23), uint3(13, 1, 23),

        // TOP (+Y)
        uint3(0, 8, 24), uint3(8, 1, 24),
        uint3(1, 17, 24), uint3(17, 5, 24),
        uint3(5, 10, 24), uint3(10, 4, 24),
        uint3(4, 16, 24), uint3(16, 0, 24),

        // BOTTOM (–Y)
        uint3(2, 9, 25), uint3(9, 3, 25),
        uint3(3, 19, 25), uint3(19, 7, 25),
        uint3(7, 11, 25), uint3(11, 6, 25),
        uint3(6, 18, 25), uint3(18, 2, 25)
};