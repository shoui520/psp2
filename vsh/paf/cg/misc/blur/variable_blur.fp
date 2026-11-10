#pragma argument (fastprecision)

half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D BlurLevels,
	   uniform sampler2D Mask,
	   uniform float Nmips) : COLOR
{
	float mask = tex2D<float4>(Mask, texcoord).x;
	float lod = mask * Nmips;
	half4 color = tex2Dlod(BlurLevels, float4(texcoord, 0, lod));
	half t = saturate((half)(mask + lod) * 0.5h);
	color.a = t * t * (t * -2 + 3);
	return color;
}
