/*
 * The empty space under the turning page, with a rounded corner: the background in the empty
 * colour, shadowed towards the fold.
 */
half4 main(half4 texcoord : TEXCOORD0,
	   half2 corner : TEXCOORD1,
	   uniform sampler2D CornerTex,
	   uniform sampler2D EmptyBackground,
	   uniform float4 CornerSize,
	   uniform float EmptyShadow,
	   uniform float4 EmptyColor,
	   uniform float4 EmptyShadowColor,
	   uniform float2 EmptyBackgroundUVFactor) : COLOR
{
	float2 cornerCoord = max(abs(corner) - CornerSize.xy, 0) * CornerSize.zw;
	half4 edge = tex2D(CornerTex, cornerCoord);
	half shadow = 1 - texcoord.z * lerp((half)EmptyShadow, 1, edge.z);
	half4 color = tex2D(EmptyBackground, EmptyBackgroundUVFactor * texcoord.xy);

	color.rgb = max(color.rgb * (half3)EmptyColor.rgb - shadow * (half3)EmptyShadowColor.rgb, 0) +
		    edge.y;
	color.a = (half)EmptyColor.a * edge.x;
	return color;
}
