struct LitParams {
	float4 MaterialPack[4];
	float4 PointLightPack2[4];
};

half4 main(float2 texcoord : TEXCOORD0,
	   half4 light : TEXCOORD2,
	   uniform sampler2D Tex,
	   uniform float BackSideOpacity,
	   uniform LitParams lit_params) : COLOR
{
	half4 lighting = light + (half4)lit_params.PointLightPack2[0];
	half4 color = tex2D(Tex, texcoord) * (half4)lit_params.MaterialPack[0];

	color += (1 - color) * (half)BackSideOpacity;
	return color * lighting + light.w;
}
