struct LitParams {
	float4 MaterialPack[4];
	float4 PointLightPack2[4];
};

half4 main(float2 texcoord : TEXCOORD0,
	   half2 corner : TEXCOORD1,
	   half4 light : TEXCOORD2,
	   uniform sampler2D Tex,
	   uniform sampler2D CornerTex,
	   uniform float4 CornerSize,
	   uniform float BackSideOpacity,
	   uniform LitParams lit_params) : COLOR
{
	half4 lighting = light + (half4)lit_params.PointLightPack2[0];
	half4 color = tex2D(Tex, texcoord) * (half4)lit_params.MaterialPack[0];
	float2 cornerCoord = max(abs(corner) - CornerSize.xy, 0) * CornerSize.zw;
	half4 edge = tex2D(CornerTex, cornerCoord);

	color += (1 - color) * (half)BackSideOpacity;
	color = color * lighting + light.w;
	color.rgb += edge.y;
	color.a *= edge.x;
	return color;
}
