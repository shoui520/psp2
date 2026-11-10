#pragma argument (fastprecision)
/*
 * The wave's positions, drawn into a texture: each texel of the strip (u across, v along the
 * cross section) is the extruded cross section's point, widened by the perspective hack, and
 * pushed along the section's normal by the height map, which is sampled rotated and scaled.
 * The alpha keeps the displacement.
 */
struct ExtrudeArgs {
	float4 CrossSection;
	float3 CrossSectionScale;
	float4x4 VM;
	float4 PerspHack;
};

float2 CrossSectionPoint(ExtrudeArgs args, float v)
{
	float curve = ((args.CrossSection.x * v + args.CrossSection.y) * v +
		       args.CrossSection.z) * v + args.CrossSection.w;

	return args.CrossSectionScale.yz * float2(v * 2 - 1, curve * 2);
}

half4 main(float2 uv : TEXCOORD0,
	   uniform float DistortionScale,
	   uniform float2 DistortionUVScaleRotate,
	   uniform ExtrudeArgs extrude_args,
	   uniform sampler2D HeightMap : TEXUNIT7) : COLOR
{
	float2 section = CrossSectionPoint(extrude_args, uv.y);
	float3 pos = float3(extrude_args.CrossSectionScale.x * (uv.x * 2 - 1), section);
	float persp = (mul(float4(pos, 1), extrude_args.VM).z * extrude_args.PerspHack.z +
		       extrude_args.PerspHack.w) * extrude_args.PerspHack.x + 1;
	float2 centered = float2((uv.x - 0.5) * persp + 0.5, uv.y) - 0.5;
	float s, c;
	half height;
	float2 along;
	half3 normal;

	sincos(DistortionUVScaleRotate.y, s, c);
	height = (half)DistortionScale *
		 (tex2D(HeightMap, float2(c * centered.x - s * centered.y, s * centered.x + c * centered.y) *
			DistortionUVScaleRotate.x + 0.5).w * 2 - 1);
	along = CrossSectionPoint(extrude_args, uv.y + 0.01) - pos.yz;
	normal = half3(0, normalize(float2(-along.y, along.x)));
	pos.x *= persp;
	return half4(pos + normal * height, height);
}
