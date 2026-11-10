half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform float Alpha) : COLOR
{
	half4 color = tex2D(Source, texcoord);
	color.a = Alpha;
	return color;
}
