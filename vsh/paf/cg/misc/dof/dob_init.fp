half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Depth,
	   uniform float2 NearFar,
	   uniform float4 Curve1,
	   uniform float4 Curve2) : COLOR
{
	float z = tex2D<float4>(Depth, texcoord).x * 2 - 1;
	float depth = -2 * -NearFar.x * NearFar.y / (NearFar.y + NearFar.x - z * (NearFar.y - NearFar.x));
	float t = saturate(depth / Curve2.z);
	float coc = ((Curve1.x * t + Curve1.y) * t + Curve1.z) * t + Curve1.w;
	return (coc - Curve2.x) * Curve2.y * Curve2.w;
}
