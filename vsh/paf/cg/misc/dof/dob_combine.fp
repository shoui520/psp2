half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Dob,
	   uniform sampler2D DobBlurred) : COLOR
{
	half4 dob = tex2D(Dob, texcoord);
	half4 blurred = max(dob, tex2D(DobBlurred, texcoord));
	return blurred * 2 - dob;
}
