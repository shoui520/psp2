half4 main(float2 texcoord : TEXCOORD0,
	   uniform float3 ShadowLevel,
	   uniform sampler2D ShadowTex) : COLOR
{
	return half4(tex2D(ShadowTex, texcoord).x * (half3)ShadowLevel, 0);
}
