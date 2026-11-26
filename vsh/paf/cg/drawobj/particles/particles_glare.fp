half4 main(half4 texcoord : TEXCOORD0,
	   half4 color : TEXCOORD1,
	   uniform sampler2D RadiusTex,
	   uniform sampler2D GlareTex) : COLOR
{
	float r = tex2D<float4>(RadiusTex, texcoord.xy).x;
	half glare = tex2D(GlareTex, float2(r, 0.5)).x;

	return color * glare;
}
