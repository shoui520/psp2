half4 main(half4 color : COLOR0,
	   float2 texcoord : TEXCOORD0,
	   uniform sampler2D uColorTex,
	   uniform float4 uColor) : COLOR
{
	return tex2D(uColorTex, texcoord) * (half4)uColor * color;
}
