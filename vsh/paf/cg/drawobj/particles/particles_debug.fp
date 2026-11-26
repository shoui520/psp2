half4 main(half4 texcoord : TEXCOORD0,
	   half4 color : TEXCOORD1,
	   uniform sampler2D RadiusTex) : COLOR
{
	float r = tex2D<float4>(RadiusTex, texcoord.xy).x;
	float blur = saturate((float)texcoord.z);
	float halfWidth = blur * 0.6345 + 0.05;
	float lo = 0.5 - halfWidth;
	float hi = 0.5 + halfWidth;
	float t = saturate((r - lo) / (hi - lo));
	float shape = 1 - t * t * (3 - 2 * t);
	float soft = 1 - exp(-(float)texcoord.w * shape);
	float mask = lerp(shape, soft, blur);

	return (half4)((float4)color * mask);
}
