struct PSInput
{
    float4 position : SV_Position; // Output to fragment shader
    float3 color : COLOR0; // Location 0 output
    float2 uv : TEXCOORD0; // Location 1 output
    nointerpolation float3 normal : NORMAL; // Location 2, flat interpolation
};

Texture2D textures[] : register(t0, space0);
SamplerState samplerState : register(s0);

float4 main(PSInput i) : SV_Target
{
    float2 textureUVs = float2(i.uv.x, 1.0 - i.uv.y);
    float4 _BottomColor = float4(0.5896226, 0.88767844, 1, 1);
    float4 _TopColor = float4(0.98083025, 0.8509804, 0.99215686, 1);
    float4 gradientYcolor = float4(0.9, 0.85, 0.92, 1.0);
    //Make gradient lerp from color
    gradientYcolor = lerp(_BottomColor, _TopColor, 1.0 - i.uv.y);

    float blackWhiteGradientBandsMask1 = step(frac((1.0 * (100.1 * (1.0 - i.uv.y * 1.25)) - i.uv.y) * (i.uv.y * 2)), 0.31) * step(i.uv.y, 0.35);
    float blackWhiteGradientBandsMask2 = step(i.uv.y, 0.35) + step(0.75, i.uv.y);
    float4 DitherMask = blackWhiteGradientBandsMask1 * blackWhiteGradientBandsMask2;

    float blackWhiteGradientBandsMask3 = frac((1.0 - i.uv.y) * (i.uv.y * 2));

    //float2 screenPos = i.screenPosition.xy / i.screenPosition.w;
    //float2 ditherCoordinate = screenPos * _ScreenParams.xy * _DitherTexture_TexelSize.xy * 0.9;
                
    //float ditherValue = tex2D(_DitherTexture, ditherCoordinate).r;

    //float dith = step(ditherValue, blackWhiteGradientBandsMask3);
                
    //float ditherMask2 = dith + DitherMask;

    float4 testTexture = textures[NonUniformResourceIndex(0)].Sample(samplerState, textureUVs);
    
    float4 colorDither = (lerp(gradientYcolor, _TopColor, DitherMask) * 1.0) + testTexture;

    return colorDither;
}
