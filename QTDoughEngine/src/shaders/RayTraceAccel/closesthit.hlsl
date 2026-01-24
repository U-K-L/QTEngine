#include "rayTracingAccelHelper.hlsl"
#include "../Helpers/ShaderHelpers.hlsl"

StructuredBuffer<Vertex> Vertices : register(t3, space1); // example binding

[shader("closesthit")]
void main(inout Payload payload : SV_RayPayload, in Attributes attr : SV_IntersectionAttributes)
{
    uint prim = PrimitiveIndex();
    uint i0 = prim * 3 + 0;
    uint i1 = prim * 3 + 1;
    uint i2 = prim * 3 + 2;

    float3 p0 = Vertices[i0].position.xyz;
    float3 p1 = Vertices[i1].position.xyz;
    float3 p2 = Vertices[i2].position.xyz;

    // Barycentrics: most helpers store (b1,b2). b0 = 1 - b1 - b2.
    float b1 = attr.bary.x;
    float b2 = attr.bary.y;
    float b0 = 1.0f - b1 - b2;

    // Flat geometric normal (object space)
    float3 Ng_obj = normalize(cross(p1 - p0, p2 - p0));

    // Optional smooth normal if you have per-vertex normals:
    float3 Ns_obj = normalize(
        Vertices[i0].normal.xyz * b0 +
        Vertices[i1].normal.xyz * b1 +
        Vertices[i2].normal.xyz * b2
    );

    // World-space (ok for uniform scale / rigid)
    float3x3 O2W = (float3x3) ObjectToWorld3x4();
    float3 Ns_ws = normalize(mul(O2W, Ns_obj));
    float3 Ng_ws = normalize(mul(O2W, Ng_obj));

    payload.color = float4(Ns_ws, 1); // visualize
}
