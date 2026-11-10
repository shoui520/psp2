half4 main(float2 texcoord : TEXCOORD0, uniform sampler2D LitTex) : COLOR
{
	return tex2D(LitTex, texcoord);
}
