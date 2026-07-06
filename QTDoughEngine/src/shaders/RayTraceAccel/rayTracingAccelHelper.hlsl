struct Photon
{
    float4 mana;
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

#define Propagation_Step_Length 0.25f;

/*
float3 HyperSurfacePositionManifold(float3 candidatePosition)
{
    //TODO: Derive hypersurface from 4D manifold produced by gravitons.
    return candidatePosition;
}
*/

float3 ManifoldToHyperSurface(float3 candidatePosition)
{
    //TODO: Derive hypersurface from 4D manifold produced by gravitons.
    return candidatePosition;
}

//Fun fuunctions to play with:
float3 ShiftManifold(float3 candidatePosition)
{
    
    float3 shift = float3(0.001f, 0.0f, 0.0f);

    float4x4 manifoldTransform = float4x4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        shift.x, shift.y, shift.z, 1
    );

    return mul(float4(candidatePosition, 1.0f), manifoldTransform).xyz;
}

//This is an effect for a fun little swirl, test for future curvature, lens bending.
float3 SwirlManifold(float3 candidatePosition, float3 effectCenter)
{
    float3 p = candidatePosition - effectCenter;

    // Full 3D spherical falloff from effect center.
    float effectRadius = 2.25f;
    float d = length(p);

    float t = saturate(1.0f - d / effectRadius);

    // Smooth spherical falloff: 1 at center, 0 at radius.
    float influence = t * t * (3.0f - 2.0f * t);

    // Barely noticeable swirl.
    float swirlStrength = 0.005f;
    float waveFreq = 2.0f;

    // Swirl around local Y axis, but fade by spherical influence.
    float rXZ = length(p.xz);
    float angle = swirlStrength * influence * sin(rXZ * waveFreq + p.y * 0.25f);

    float c = cos(angle);
    float s = sin(angle);

    float2 xz;
    xz.x = c * p.x - s * p.z;
    xz.y = s * p.x + c * p.z;

    p.xz = xz;

    return p + effectCenter;
}


//This is a test that connectivity works.
float3 PortalManifold(float3 candidatePosition, float4 viewOrb, float4 peekOrb)
{
    float3 viewCenter = viewOrb.xyz;
    float viewRadius = viewOrb.w;

    float3 peekCenter = peekOrb.xyz;
    float peekRadius = peekOrb.w;

    float3 viewLocal = candidatePosition - viewCenter;
    float d = length(viewLocal);

    float3 normalizedLocal = viewLocal / viewRadius;
    float3 peekPosition = peekCenter + normalizedLocal * peekRadius;

    float inside = step(d, viewRadius);

    return lerp(candidatePosition, peekPosition, inside);
}

//Default placeholder, used to approximate compute needed.
float3 IdentityManifold(float3 candidatePosition)
{
    float4x4 I = float4x4(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    
    float4 identityPos = mul(I, float4(candidatePosition.xyz, 1.0f));

    return identityPos;
}

float3 HyperSurfacePositionMapper(float3 candidatePosition)
{
    //First get the true manifold position.
    float3 manifoldPosMapped = ManifoldToHyperSurface(candidatePosition);
    
    //Now chain effects:
    
    //For orb:
    float4 viewOrb = float4(0.0f, 0.0f, 1.20f, 1.0f); // portal orb here
    float4 peekOrb = float4(1.0f, 4.0f, 0.0f, 2.0f); // see this remote orb
    
    float3 chainedPositions = IdentityManifold(manifoldPosMapped); //No effect.
    //float3 chainedPositions = PortalManifold(candidatePosition, viewOrb, peekOrb);
    return chainedPositions;
}

void TraverseGeodesic(inout Photon p, float3 oldResolvedPosition)
{
    //Derive from hypersurface from 4D manifold. 
    //Photon p is in a new position given by Acceleration Structure.
    //The manifold is non-euclidean, but each step is a small straight line, local jacobian.
    //This produces direction as well as the new position of the point.
    //New position is required because, although the 4D manifold is garunteed to be smooth, the embedded hypersurface
    //might have discontunities, thus sudden jumps in position.
    
    float3 candidatePosition = p.position.xyz; //What AS gave.
    float3 indeterminatePosition = oldResolvedPosition; //The position still to be decided, position before AS. It's already mapped.
    
    float3 resolvedPosition = HyperSurfacePositionMapper(candidatePosition);
    
    float3 jacobian = normalize(resolvedPosition - indeterminatePosition); //Note ds is already calculated given AS trace.
    
    //Small trick, we are always a step behind direction here, but this happens recursively so smooths out.
    p.position.xyz = resolvedPosition; //This will become the new indeterminatePosition.
    p.direction.xyz = jacobian;
}