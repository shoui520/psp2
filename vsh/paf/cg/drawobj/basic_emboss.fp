half4 Premultiplied(sampler2D source, float2 texcoord)
{
	half4 color = tex2D(source, texcoord);
	return color * color.a;
}

half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Texture0,
	   uniform half4 Color,
	   uniform float2 Reciprocal0) : COLOR
{
	float2 lo = texcoord - Reciprocal0;
	float2 hi = texcoord + Reciprocal0;
	half4 center = tex2D(Texture0, texcoord);
	half4 emboss = Premultiplied(Texture0, lo) +
		       Premultiplied(Texture0, float2(texcoord.x, lo.y)) +
		       Premultiplied(Texture0, float2(hi.x, lo.y)) -
		       Premultiplied(Texture0, float2(lo.x, hi.y)) -
		       Premultiplied(Texture0, float2(texcoord.x, hi.y)) -
		       Premultiplied(Texture0, hi);
	half4 color = emboss * 0.1667h + center;
	return Color * half4(color.rgb, center.a);
}
