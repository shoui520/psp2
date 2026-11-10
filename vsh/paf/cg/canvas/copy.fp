half4 main(float2 texCoord : TEXCOORD0, uniform sampler2D testTex) : COLOR
{
	return tex2D(testTex, texCoord);
}
