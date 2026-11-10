half4 main(float2 texcoord : TEXCOORD0,
	   half signal : TEXCOORD1,
	   uniform sampler2D Texture0,
	   uniform half4 Color) : COLOR
{
	half alpha = signal * sqrt(1 - signal * signal);

	return Color * half4(1, 1, 1, alpha * 2) * tex2D(Texture0, texcoord);
}
