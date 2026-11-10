struct ColorArgs {
	float4 SpecularFactor;
	float4 Color;
	sampler2D Background;
	sampler2D EnvMapDetail;
};

half4 main(half4 color : TEXCOORD0,
	   half4 factor : TEXCOORD1,
	   half4 texcoord : TEXCOORD2,
	   uniform ColorArgs cargs) : COLOR
{
	half4 background = tex2D(cargs.Background, texcoord.xy);
	half4 result;

	background.rgb *= color.rgb;
	result = tex2D(cargs.EnvMapDetail, texcoord.zw) * factor.y + background;
	result.rgb += factor.x * (half3)cargs.SpecularFactor.rgb;
	result.rgb += factor.z * half3(0, 1, 1);
	result.a = (half)cargs.Color.a;
	return result;
}
