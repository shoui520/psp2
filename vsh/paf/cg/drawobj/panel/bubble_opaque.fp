struct ColorArgs {
	float4 BackColor;
	float4 SpecularFactor;
	sampler2D ColorMap;
	sampler2D EnvMapDetail;
};

half4 main(half4 color : TEXCOORD0,
	   half4 envCoord : TEXCOORD1,
	   half4 texcoord : TEXCOORD2,
	   uniform float Alpha,
	   uniform ColorArgs cargs) : COLOR
{
	half4 image = tex2D(cargs.ColorMap, texcoord.xy);
	half4 env = tex2D(cargs.EnvMapDetail, envCoord.xy);
	half3 back = (half3)cargs.BackColor.rgb;
	half4 result;

	image.rgb *= color.rgb;
	result.rgb = lerp(back, image.rgb, envCoord.w) + texcoord.z * (half3)cargs.SpecularFactor.rgb +
		     texcoord.w * env.rgb;
	result.a = (half)Alpha;
	return result;
}
