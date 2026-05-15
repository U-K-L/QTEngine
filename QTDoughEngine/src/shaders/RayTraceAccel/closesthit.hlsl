#include "rayTracingAccelHelper.hlsl"
#include "../Helpers/ShaderHelpers.hlsl"

StructuredBuffer<Vertex> Vertices : register(t3, space1);
StructuredBuffer<uint> brushVertexOffsets : register(t4, space1);

[shader("closesthit")]
void main(inout Photon photon : SV_RayPayload, in Attributes attr : SV_IntersectionAttributes)
{
    // meshingVertices and meshingPositions are per-brush-partitioned. BLAS prim index is
    // local to this brush's slice; resolve to a global Vertices index via the brush's base.
    uint brushIdx = InstanceID();
    uint base = brushVertexOffsets[brushIdx];
    uint prim = PrimitiveIndex();
    uint i0 = base + prim * 3 + 0;
    uint i1 = base + prim * 3 + 1;
    uint i2 = base + prim * 3 + 2;

    float3 p0 = Vertices[i0].position.xyz;
    float3 p1 = Vertices[i1].position.xyz;
    float3 p2 = Vertices[i2].position.xyz;

    float b1 = attr.bary.x;
    float b2 = attr.bary.y;
    float b0 = 1.0f - b1 - b2;
    
    float3 ws =
        Vertices[i0].position.xyz * b0 +
        Vertices[i1].position.xyz * b1 +
        Vertices[i2].position.xyz * b2;

    // Flat geometric normal.
    float3 Ng_obj = normalize(cross(p1 - p0, p2 - p0));

    // Smooth normals.
    float3 Ns_obj = normalize(
        Vertices[i0].normal.xyz * b0 +
        Vertices[i1].normal.xyz * b1 +
        Vertices[i2].normal.xyz * b2
    );


    photon.color = float4(Ns_obj, Vertices[i0].normal.w);
    photon.position = float4(ws, 1.0f);

}
