float4 main(float2 texcoord : TEXCOORD0,
	    float2 position : WPOS,
	    uniform sampler2D testTex,
	    uniform float opacity,
	    uniform float4 aabb) : COLOR
{
	float2 inside = float2(aabb.xy <= position) * float2(position <= aabb.zw);

	return tex2D(testTex, texcoord) * (opacity * inside.x * inside.y);
}
