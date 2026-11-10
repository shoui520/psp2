half4 main(float2 tex : TEXCOORD0, uniform sampler2D Source) : COLOR
{
	return tex2D(Source, tex);
}
