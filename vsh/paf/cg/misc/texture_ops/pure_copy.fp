half4 main(float2 texcoord : TEXCOORD0, uniform sampler2D Source) : COLOR
{
	return tex2D(Source, texcoord);
}
