half4 main(half4 color : COLOR0,
	   float2 texcoord : TEXCOORD0,
	   uniform sampler2D Tex) : COLOR
{
	return color * tex2D(Tex, texcoord);
}
