__nativecolor void main(float4 texcoord : TEXCOORD0,
			__regformat unsigned char4 fragColor : FRAGCOLOR,
			uniform sampler2D Texture0,
			uniform sampler2D Texture1,
			uniform sampler2D Texture2,
			uniform half4 Color,
			out __regformat unsigned char4 outColor : COLOR0)
{
	half4 frame = tex2D(Texture0, texcoord.xy);
	half4 image = tex2D(Texture1, texcoord.zw);
	half4 imageMask = tex2D(Texture2, texcoord.zw);
	half alpha = image.a * imageMask.a;
	half3 blended = lerp(Color.rgb, image.rgb, alpha);
	unsigned char4 shadow = unsigned char4(0, 0, 0, Color.a * frame.y * 255);
	unsigned char4 body = half4(lerp(blended, half3(1, 1, 1), frame.x), Color.a * frame.a) * 255;

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
