/* the cubic B-spline: 1/6 (P(x + 2)^3 - 4 P(x + 1)^3 + 6 P(x)^3 - 4 P(x - 1)^3), P(x) = max(x, 0) */
float4 P(float4 x)
{
	return x > 0 ? x : 0;
}

half4 R(float4 x)
{
	half4 p0 = P(x + 2);
	half4 p1 = P(x + 1);
	half4 p2 = P(x);
	half4 p3 = P(x - 1);
	return (p0 * p0 * p0 - 4 * p1 * p1 * p1 + 6 * p2 * p2 * p2 - 4 * p3 * p3 * p3) * (1.0h / 6);
}

half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform float4 SourceSize) : COLOR
{
	float2 base = floor(texcoord * SourceSize.xy);
	float2 f = texcoord * SourceSize.xy - base;
	float4 offset = float4(-1, 0, 1, 2);
	half4 wy = R(f.y - offset);
	half4 wx = R(offset - f.x);
	half4 sum = 0;
	sum += tex2D(Source, (base + offset.xx) * SourceSize.zw) * wx[0] * wy[0];
	sum += tex2D(Source, (base + offset.xy) * SourceSize.zw) * wx[0] * wy[1];
	sum += tex2D(Source, (base + offset.xz) * SourceSize.zw) * wx[0] * wy[2];
	sum += tex2D(Source, (base + offset.xw) * SourceSize.zw) * wx[0] * wy[3];
	sum += tex2D(Source, (base + offset.yx) * SourceSize.zw) * wx[1] * wy[0];
	sum += tex2D(Source, (base + offset.yy) * SourceSize.zw) * wx[1] * wy[1];
	sum += tex2D(Source, (base + offset.yz) * SourceSize.zw) * wx[1] * wy[2];
	sum += tex2D(Source, (base + offset.yw) * SourceSize.zw) * wx[1] * wy[3];
	sum += tex2D(Source, (base + offset.zx) * SourceSize.zw) * wx[2] * wy[0];
	sum += tex2D(Source, (base + offset.zy) * SourceSize.zw) * wx[2] * wy[1];
	sum += tex2D(Source, (base + offset.zz) * SourceSize.zw) * wx[2] * wy[2];
	sum += tex2D(Source, (base + offset.zw) * SourceSize.zw) * wx[2] * wy[3];
	sum += tex2D(Source, (base + offset.wx) * SourceSize.zw) * wx[3] * wy[0];
	sum += tex2D(Source, (base + offset.wy) * SourceSize.zw) * wx[3] * wy[1];
	sum += tex2D(Source, (base + offset.wz) * SourceSize.zw) * wx[3] * wy[2];
	sum += tex2D(Source, (base + offset.ww) * SourceSize.zw) * wx[3] * wy[3];
	return half4(sum.rgb, 1);
}
