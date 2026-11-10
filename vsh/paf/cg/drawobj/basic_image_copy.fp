half4 main(float2 texcoord : TEXCOORD0, uniform sampler2D Texture0) : COLOR
{
	return tex2D(Texture0, texcoord);
}
