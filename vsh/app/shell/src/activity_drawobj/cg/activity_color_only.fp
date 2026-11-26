__nativecolor void main(float2 texcoord : TEXCOORD0,
			__regformat unsigned char4 fragColor : FRAGCOLOR,
			uniform sampler2D Texture0,
			uniform half4 Color,
			out __regformat unsigned char4 outColor : COLOR0)
{
	half4 mask = tex2D(Texture0, texcoord);
	unsigned char4 shadow = unsigned char4(0, 0, 0, Color.a * mask.y * 255);
	unsigned char4 body = half4(lerp(Color.rgb, half3(1, 1, 1), mask.x), Color.a * mask.a) * 255;

	outColor = __fastblend(shadow, fragColor,
			       __BLEND_OP_ADD, __BLEND_FACTOR_SRC1_ALPHA,
			       __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
			       __BLEND_OP_ADD, __BLEND_FACTOR_ONE,
			       __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);
	outColor = __fastblend(body, outColor,
			       __BLEND_OP_ADD, __BLEND_FACTOR_SRC1_ALPHA,
			       __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
			       __BLEND_OP_ADD, __BLEND_FACTOR_ONE,
			       __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);
}
