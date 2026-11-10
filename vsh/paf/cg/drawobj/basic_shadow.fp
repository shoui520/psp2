half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Texture0,
	   uniform half4 Color,
	   uniform float2 Reciprocal0) : COLOR
{
	float2 near = texcoord - Reciprocal0 * 2;
	float2 far = texcoord - Reciprocal0 * 4;
	half alpha = tex2D(Texture0, far).a +
		     tex2D(Texture0, float2(near.x, far.y)).a +
		     tex2D(Texture0, float2(texcoord.x, far.y)).a +
		     tex2D(Texture0, float2(far.x, near.y)).a +
		     tex2D(Texture0, near).a +
		     tex2D(Texture0, float2(texcoord.x, near.y)).a +
		     tex2D(Texture0, float2(far.x, texcoord.y)).a +
		     tex2D(Texture0, float2(near.x, texcoord.y)).a +
		     tex2D(Texture0, texcoord).a;
	half4 shadow = 1;
	shadow.a = alpha * (1.0h / 9);
	return Color * shadow;
}
