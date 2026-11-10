struct ColorArgs {
	float4 SpecularFactor;
	sampler2D ColorMap;
};

half4 main(half4 color : TEXCOORD0,
	   half4 texcoord : TEXCOORD1,
	   uniform ColorArgs cargs) : COLOR
{
	half4 image = tex2D(cargs.ColorMap, texcoord.xy);

	return half4(color.rgb * image.rgb + texcoord.z * (half3)cargs.SpecularFactor.rgb, image.a);
}
