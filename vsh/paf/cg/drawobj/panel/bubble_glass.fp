struct ColorArgs {
	float4 SpecularFactor;
	sampler2D Background;
	sampler2D EnvMapDetail;
};

half4 main(half4 color : TEXCOORD0,
	   half3 factor : TEXCOORD1,
	   half4 texcoord : TEXCOORD2,
	   uniform ColorArgs cargs) : COLOR
{
	half4 background = tex2D(cargs.Background, texcoord.xy);
	half4 env = tex2D(cargs.EnvMapDetail, texcoord.zw);

	return background * color + env * factor.y + factor.x * (half4)cargs.SpecularFactor;
}
