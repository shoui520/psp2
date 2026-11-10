half4 main(float2 coord : SPRITECOORD, uniform float4 fColor, uniform sampler2D testTex) : COLOR
{
	return tex2D(testTex, coord) * fColor;
}
