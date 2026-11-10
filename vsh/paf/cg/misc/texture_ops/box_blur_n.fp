half4 main(half2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform float2 Step,
	   uniform float Spread) : COLOR
{
	short spread = Spread;
	float3 sum = 0;
	for (short i = -spread; i <= spread; i++)
		sum += tex2D<float4>(Source, texcoord + i * Step).rgb;
	float scale = 1.0 / (2 * spread + 1);
	return half4(sum, 1) * half4(scale, scale, scale, 1);
}
