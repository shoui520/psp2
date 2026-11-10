/*
 * A folder's bubble: blended over the frame buffer in the shader, then the specular and
 * environment light added on top.
 */
struct ColorArgs {
	float4 BackColor;
	float4 SpecularFactor;
	sampler2D ColorMap;
	sampler2D EnvMapDetail;
};

__nativecolor __msaa void main(half4 color : TEXCOORD0,
			       half4 envCoord : TEXCOORD1,
			       half4 texcoord : TEXCOORD2,
			       __regformat unsigned char4 fragColor : FRAGCOLOR,
			       uniform float Alpha,
			       uniform ColorArgs cargs,
			       out __regformat unsigned char4 outColor : COLOR0)
{
	float4 image = tex2D<float4>(cargs.ColorMap, texcoord.xy);
	float4 env = tex2D<float4>(cargs.EnvMapDetail, envCoord.xy);
	float4 base = lerp(cargs.BackColor, image * color, envCoord.w);
	float4 light;
	unsigned char4 blended;

	/* the image's alpha where it has one, the blend's elsewhere */
	base.a = (base.a * step(image.a, 0) + image.a * sign(image.a)) * Alpha;
	blended = __fastblend((unsigned char4)(base * 255), fragColor,
			             __BLEND_OP_ADD, __BLEND_FACTOR_SRC1_ALPHA,
			             __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
			             __BLEND_OP_ADD, __BLEND_FACTOR_ONE,
			             __BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA);
	light = (cargs.SpecularFactor * texcoord.z + env * texcoord.w) * Alpha;
	outColor = ((float4)blended / 255 + light) * 255;
}
