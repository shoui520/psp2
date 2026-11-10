half4 main(float2 texcoord : TEXCOORD0,
	   uniform sampler2D Source,
	   uniform float4 SourceDuDv) : COLOR
{
	float3 du = tex2D<float4>(Source, texcoord + SourceDuDv.xy).xyz -
		    tex2D<float4>(Source, texcoord - SourceDuDv.xy).xyz;
	float3 dv = tex2D<float4>(Source, texcoord + SourceDuDv.zw).xyz -
		    tex2D<float4>(Source, texcoord - SourceDuDv.zw).xyz;
	float3 normal = normalize(cross(du, dv));
	float4 color;
	color.xyz = normal * 0.5 + 0.5;
	return color;
}
