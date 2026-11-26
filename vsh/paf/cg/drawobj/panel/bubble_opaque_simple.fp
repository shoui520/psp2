struct ColorArgs {
	float4 BackColor;
	float4 SpecularFactor;
	sampler2D ColorMap;
};

half4 main(half3 color : TEXCOORD0,
	   half4 envCoord : TEXCOORD1,
	   half4 texcoord : TEXCOORD2,
	   uniform float Alpha,
	   uniform ColorArgs cargs) : COLOR
{
	half4 image = tex2D(cargs.ColorMap, texcoord.xy);
	half3 back = (half3)cargs.BackColor.rgb;
	half4 result;

	image.rgb *= color.rgb;
	result.rgb = lerp(back, image.rgb, envCoord.w) + texcoord.z * (half3)cargs.SpecularFactor.rgb +
		     texcoord.w;
	result.a = (half)Alpha;
	return result;
}
