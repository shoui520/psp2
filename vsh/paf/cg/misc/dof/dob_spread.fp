half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform float2 Step) : COLOR
{
	half3 dob = 0;
	for (int i = -3; i <= 3; i++)
		dob = max(dob, tex2D(Source, texcoord + Step * i).rgb);
	return half4(dob, 1);
}
