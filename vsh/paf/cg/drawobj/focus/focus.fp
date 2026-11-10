/*
 * The focus frame, blended in the shader: the base colour (the texture's blue) and the glow (its
 * green) over the frame buffer, then the colour (its red, brightened) added on top.
 */
__nativecolor void main(float2 texcoord : TEXCOORD0,
			__regformat unsigned char4 fragColor : FRAGCOLOR,
			uniform sampler2D Texture0,
			uniform half4 Color,
			uniform half4 BaseColor,
			uniform half4 GlowColor,
			out __regformat unsigned char4 outColor : COLOR0)
{
	half4 mask = tex2D(Texture0, texcoord);
	unsigned char4 base = BaseColor * half4(1, 1, 1, mask.z) * 255;
	unsigned char4 glow = GlowColor * half4(1, 1, 1, mask.y) * 255;
	unsigned char4 color;
	unsigned char4 result;
	half alpha;

	result = __fastblend(base, fragColor,
			     __BLEND_OP_ADD, __BLEND_FACTOR_SRC1_ALPHA,
			     __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
			     __BLEND_OP_ADD, __BLEND_FACTOR_ONE,
			     __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);
	outColor = __fastblend(glow, result,
			       __BLEND_OP_ADD, __BLEND_FACTOR_SRC1_ALPHA,
			       __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
			       __BLEND_OP_ADD, __BLEND_FACTOR_ONE,
			       __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);
	alpha = saturate(mask.x * 1.7h);
	color = half4(Color.rgb, Color.a * alpha) * 255;
	outColor = __fastblend(color, outColor,
			       __BLEND_OP_ADD, __BLEND_FACTOR_SRC1_ALPHA, __BLEND_FACTOR_ONE,
			       __BLEND_OP_ADD, __BLEND_FACTOR_ZERO, __BLEND_FACTOR_ONE);
}
