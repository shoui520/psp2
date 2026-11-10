half4 main(half2 texcoord : TEXCOORD0,
	   uniform sampler2D Tex,
	   uniform half4 Border,
	   uniform half Level,
	   uniform float4 Color) : COLOR
{
	half2 coord = saturate((abs(texcoord) - Border.xy) * Border.zw);

	return lerp(Level * (half4)Color, 1, tex2D(Tex, coord).x);
}
