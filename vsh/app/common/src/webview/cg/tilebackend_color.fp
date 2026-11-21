float4 main(float2 position : WPOS,
	    uniform float4 color,
	    uniform float4 aabb) : COLOR
{
	float2 inside = float2(aabb.xy <= position) * float2(position <= aabb.zw);

	return color * (inside.x * inside.y);
}
