struct ColorArgs {
	float4 Color;
};

half4 main(half4 color : TEXCOORD0, uniform ColorArgs cargs) : COLOR
{
	color.a = cargs.Color.a;
	return color;
}
