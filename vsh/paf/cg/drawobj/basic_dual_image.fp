half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Texture0,
	   uniform sampler2D Texture1,
	   uniform half4 Color) : COLOR
{
	return tex2D(Texture0, texcoord) * tex2D(Texture1, texcoord) * Color;
}
