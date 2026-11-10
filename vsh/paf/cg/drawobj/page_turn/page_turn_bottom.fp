/* the page under the turning one: its texture, darkened by the shade in texcoord.z */
half4 main(half4 texcoord : TEXCOORD0, uniform sampler2D Tex) : COLOR
{
	return half4(texcoord.zzz, 1) * tex2D(Tex, texcoord.xy);
}
