/*
 * The lit wave, drawn from its position and normal textures: half-Lambert lighting from the
 * point light, coloured from the centre to the edge by how squarely it faces the eye, fogged
 * per channel with distance towards a fog colour that turns to the sun's near the light, and
 * faded to the fog colour at the strip's edges.
 */
half4 main(float2 texcoord : TEXCOORD0,
	   uniform float4x4 ModelMatrix,
	   uniform float2 HalfPixel,
	   uniform float3 EyePos,
	   uniform float4 MaterialPack[4],
	   uniform float4 PointLightPack2[4],
	   uniform float4 PointLightPack1[4],
	   uniform float4 EdgeColor,
	   uniform float4 CenterColor,
	   uniform float4 FogPack[4],
	   uniform float2 EdgeFade,
	   uniform sampler2D PosTex : TEXUNIT0,
	   uniform sampler2D NorTex : TEXUNIT1) : COLOR
{
	float2 uv = float2(texcoord.x, 1 - texcoord.y);
	float3 pos = tex2D<float4>(PosTex, uv - HalfPixel).xyz;
	half3 nor = normalize(tex2D<half4>(NorTex, uv - HalfPixel).xyz * 2 - 1);
	float3 normal = normalize(mul((float3)nor, (float3x3)ModelMatrix));
	float3 world = mul(float4(pos, 1), ModelMatrix).xyz;
	float3 toEye = EyePos - world;
	float3 eye = normalize(toEye);
	float facing = saturate(dot(normal, eye));
	float3 light = normalize(PointLightPack1[0].xyz - world);
	float2 edge = saturate((abs(uv * 2 - 1) - EdgeFade) / (1 - EdgeFade));
	float3 fog = saturate((length(toEye) - FogPack[3].y) / (FogPack[1].xyz * FogPack[3].x));
	float sun = max(dot(light, eye), 0);
	float diffuse = dot(normal, light) * 0.5 + 0.5;
	float3 color;
	float3 fogColor;
	float fade;

	edge = edge * edge * (3 - 2 * edge);
	color = MaterialPack[0].xyz * (PointLightPack2[2].xyz * MaterialPack[1].x *
		(diffuse * diffuse) + PointLightPack2[0].xyz) +
		(EdgeColor.xyz * pow(abs(1 - facing), EdgeColor.w) +
		 CenterColor.xyz * pow(facing, CenterColor.w));
	fade = (1 - edge.x) * (1 - edge.y);
	fogColor = lerp(FogPack[0].xyz, FogPack[2].xyz, pow(sun, 8));
	color = lerp(fogColor, color, 1 - fog * fog * (3 - 2 * fog));
	return half4(FogPack[0].xyz * (1 - fade) + color * fade, 1);
}
