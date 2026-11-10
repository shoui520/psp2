half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D BlurLevels,
	   uniform float Alpha) : COLOR
{
	half4 color = tex2D(BlurLevels, texcoord);
	color.a = Alpha;
	return color;
}
