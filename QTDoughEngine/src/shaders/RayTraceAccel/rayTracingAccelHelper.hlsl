struct Photon
{
    float4 color;
    float4 direction;
    float4 position;
};

struct Attributes
{
    float2 bary : SV_Barycentrics;
};

struct Surface
{
    float4 normal;
    float4 position;
};