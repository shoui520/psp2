/*
 * The page under the turning one, with a rounded corner: the corner texture gives the corner's
 * alpha (x) and a highlight that also bends the page's texture (y).
 */
half4 main(half4 texcoord : TEXCOORD0,
	   half2 corner : TEXCOORD1,
	   uniform sampler2D Tex,
	   uniform sampler2D CornerTex,
	   uniform float4 CornerSize,
	   uniform float2 CornerRefract) : COLOR
{
	float2 cornerCoord = max(abs(corner) - CornerSize.xy, 0) * CornerSize.zw;
	float4 edge = tex2D<float4>(CornerTex, cornerCoord);
	half4 color = tex2D<half4>(Tex, texcoord.xy + edge.y * CornerRefract);
	half4 result = color * texcoord.z + edge.y;

	result.a = (half)edge.x * color.a;
	return result;
}
