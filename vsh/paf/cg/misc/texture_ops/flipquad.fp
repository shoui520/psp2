half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform sampler2D FlipTex,
	   uniform float2 Offset0,
	   uniform float2 Offset1,
	   uniform float2 Offset2,
	   uniform float2 Offset3,
	   uniform float2 FlipTexSize) : COLOR
{
	float2 flip = tex2D(FlipTex, texcoord * FlipTexSize).xy * 2.0h - 1.0f;
	half4 color = tex2D(Source, texcoord + flip * Offset0);
	color += tex2D(Source, texcoord + flip * Offset1);
	color += tex2D(Source, texcoord + flip * Offset2);
	color += tex2D(Source, texcoord + flip * Offset3);
	return color * 0.25h;
}
