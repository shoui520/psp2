half4 main(float2 texcoord : TEXCOORD0, uniform sampler2D ColorMap) : COLOR
{
	return tex2D(ColorMap, texcoord);
}
