half4 main(half2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform sampler2D Rect2SphereMap,
	   uniform sampler2D Previous,
	   uniform half2 PreviousOffset,
	   uniform float4 PreviousBlend) : COLOR
{
	half4 previous = tex2D(Previous, texcoord + PreviousOffset);
	float2 coord = tex2D<float4>(Rect2SphereMap, texcoord).xy;
	half4 color = half4(tex2D(Source, coord).rgb, 0);
	return lerp(previous, color, (half4)PreviousBlend);
}
