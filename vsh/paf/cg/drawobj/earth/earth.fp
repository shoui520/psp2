half4 main(half4 texcoord : TEXCOORD0,
	   half3 light : TEXCOORD1,
	   half4 factor : TEXCOORD2,
	   uniform float4 StretchRangeScale,
	   uniform float4 StretchRangeOffset,
	   uniform float Transparency,
	   uniform sampler2D SpecularMap,
	   uniform sampler2D EdgeIris,
	   uniform sampler2D EnvMapDetail) : COLOR
{
	half4 specSample = tex2D(SpecularMap, texcoord.xy);
	half4 env = tex2D(EnvMapDetail, texcoord.zw);
	half4 iris = tex2D(EdgeIris, float2(factor.y, 0.5));
	half4 reflection = env * factor.z + half4(iris.rgb * factor.x, 0);
	half4 stretch = specSample.a * (half4)StretchRangeScale + (half4)StretchRangeOffset;
	half3 surface = light * stretch.x + stretch.y * factor.w + stretch.w;
	half luma = dot(reflection.rgb, half3(0.299, 0.587, 0.114));
	half4 result;

	result.rgb = (1 - saturate(factor.z)) * surface + reflection.rgb;
	result.a = (half)Transparency * (stretch.z + luma + stretch.y * factor.w);
	return result;
}
