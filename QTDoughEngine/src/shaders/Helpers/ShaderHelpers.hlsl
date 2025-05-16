
#define PI 3.14159265359

#define IDENTITY_MATRIX float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)

#define NOISE_SIMPLEX_1_DIV_289 0.00346020761245674740484429065744f

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
