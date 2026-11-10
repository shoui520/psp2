struct ColorArgs {
	float4 SpecularFactor;
	float4 Color;
	sampler2D ColorMap;
	sampler2D EnvMapDetail;
};

half4 main(half4 color : TEXCOORD0,
	   float2 envCoord : TEXCOORD1,
	   half4 texcoord : TEXCOORD2,
	   uniform ColorArgs cargs) : COLOR
{
	half4 image = tex2D(cargs.ColorMap, texcoord.xy);
	half4 env = tex2D(cargs.EnvMapDetail, envCoord);
	half4 result;

	env.rgb *= texcoord.w;
	result = image * color + env;
	result.rgb += texcoord.z * (half3)cargs.SpecularFactor.rgb;
	result.a = image.a * (half)cargs.Color.a;
	return result;
}
