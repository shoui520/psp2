half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Texture0,
	   uniform half4 Color) : COLOR
{
	return Color * tex2D(Texture0, texcoord);
}
