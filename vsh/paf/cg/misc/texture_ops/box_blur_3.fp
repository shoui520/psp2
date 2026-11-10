#pragma argument (nofastmath)

half4 main(half2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform float2 Step) : COLOR
{
	half4 color = tex2D(Source, texcoord + Step * 3.0f) * 0.5h + tex2D(Source, texcoord + Step * -2.5f);
	color.rgb += tex2D(Source, texcoord + Step * -0.5f).rgb;
	color.rgb += tex2D(Source, texcoord + Step * 1.5f).rgb;
	return half4(color.rgb * (1.0h / 3.5), 1);
}
