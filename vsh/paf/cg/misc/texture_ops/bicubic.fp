half4 main(half2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform sampler2D Table,
	   uniform float4 SourceSize) : COLOR
{
	half4 size = SourceSize;
	half2 coord = texcoord * size.xy - 0.5h;
	half4 hgx = tex2D(Table, coord.xx);
	half4 hgy = tex2D(Table, coord.yy);
	half2 ex = half2(size.z, 0);
	half2 ey = half2(0, size.w);
	half2 coord10 = texcoord + hgx.y * ex;
	half2 coord00 = texcoord - hgx.x * ex;
	half4 col0 = tex2D(Source, coord00 + hgy.y * ey) * hgy.w +
		     tex2D(Source, coord00 - hgy.x * ey) * hgy.z;
	half4 col1 = tex2D(Source, coord10 + hgy.y * ey) * hgy.w +
		     tex2D(Source, coord10 - hgy.x * ey) * hgy.z;
	return half4((col1 * hgx.w + col0 * hgx.z).rgb, 1);
}
