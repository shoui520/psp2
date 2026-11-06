/*
 * The panel renderer: the glass panels and bubbles of the panel and bubble objects, drawn with
 * the shaders of paf/cg/drawobj/panel, lit by its parameter sets and picked by the touch ray.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/effects/panel_renderer.h>
#include <graphics/surface/surface_display.h>
#include <graphics/surface/surface_screen.h>
#include <widget/core/environment.h>
#include <graphics/effects/fft_wave.h>
#include <graphics/gutil.h>
#include <src/common/frame_count.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

/* a vertex of a corner's border: the outline from corner to corner, its edge turning from
   angle to angle, in quarter turns (names ours) */
class PanelBorderVertex : public VertexGenerator<PanelVertex> {
public:
	PanelBorderVertex(int32_t corner0, int32_t corner1, int32_t angle0, int32_t angle1)
		: m_corner0(corner0), m_corner1(corner1), m_angle0(angle0), m_angle1(angle1)
	{
	}

	virtual PanelVertex operator()(const math::v2 &uv) const;

	int32_t m_corner0;
	int32_t m_corner1;
	int32_t m_angle0;
	int32_t m_angle1;
};

/* a vertex of the face: where it is in the grid (name ours) */
class PanelFaceVertex : public VertexGenerator<PanelVertex> {
public:
	virtual PanelVertex operator()(const math::v2 &uv) const;
};

/* a profile's vertex: its point, the point's normal, where it is on the grid and the turn
   around (name ours) */
class ProfileVertexGenerator : public GridVertexGenerator<ProfileVertex> {
public:
	ProfileVertexGenerator(PointArray points, uint32_t flags) : m_points(points), m_flags(flags)
	{
	}

	virtual ProfileVertex operator()(GridPosition position, arl::SurfaceSize size) const;

	/* the point at the index, those past the ends mirrored back when asked */
	math::v2 GetPoint(int32_t index) const
	{
		if (index == -1) {
			if (m_flags & ProfileMesh::MIRROR_START)
				return m_points.m_points[1] * math::v2(-1.0f, 1.0f);
			return m_points.m_points[0];
		}
		if (index == (int32_t)m_points.m_count) {
			if (m_flags & ProfileMesh::MIRROR_END) {
				int32_t mirrored = -1 % (int32_t)m_points.m_count;

				if (mirrored < 0)
					mirrored += m_points.m_count;
				return m_points.m_points[mirrored] * math::v2(-1.0f, 1.0f);
			}
			return m_points.m_points[m_points.m_count - 1];
		}
		return m_points.m_points[index];
	}

	PointArray m_points;
	uint32_t m_flags;
};

/* a vertex's four halves at the offset, after it the next (name ours) */
template<class Vertex>
void AddHalf4Attribute(common::SharedPtr<arl::VertexArraySpec> spec, int32_t slot,
		       uint32_t &offset)
{
	common::SharedPtr<arl::VertexArraySpec> target = spec;

	target->AddAttribute(slot, 4, SCE_GXM_ATTRIBUTE_FORMAT_F16, sizeof(Vertex), offset);
	offset += 4 * sizeof(Half);
}

/* where the ray meets the panel's plane, its centre moved along its normal by its depth, kept in
   the ray when nearer than what it hit before; 0 when it hits nothing (name ours) */
__attribute__((used)) static math::v4 HitPanel(const PanelParam &param, Ray &ray)
{
	math::matrix rot(math::quaternion(sce::Vectormath::Simd::Aos::Quat(param.m_rot)));
	math::v4 normal = rot.z();
	math::v4 center = math::v4(sce::Vectormath::Simd::Aos::Vector4(param.m_pos)) +
			  normal * vgetq_lane_f32(param.m_scale, 2);
	sce::Vectormath::Simd::floatInVec facing = normal.dot(ray.m_direction);

	if (facing != sce::Vectormath::Simd::floatInVec(0.0f)) {
		sce::Vectormath::Simd::floatInVec distance = normal.dot(center - ray.m_origin) / facing;

		if (!(distance < sce::Vectormath::Simd::floatInVec(0.0f)) &&
		    (!ray.m_hit || !(distance > ray.m_distance))) {
			ray.m_distance = distance;
			ray.m_hit = true;
			ray.m_hit_point = ray.m_origin + ray.m_direction * distance;
		}
	}
	if (!ray.m_hit)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	rot.inverse1();
	return ray.m_hit_point;
}

void PanelMeshes::Init(int32_t border_columns, int32_t rows, int32_t columns)
{
	arl::ArlInstance *gl = arl::Gl();
	uint32_t offset = 0;

	m_spec = common::SharedPtr<arl::VertexArraySpec>(
		new arl::VertexArraySpec(SCE_GXM_INDEX_SOURCE_INDEX_16BIT));
	AddHalf4Attribute<PanelVertex>(m_spec, 0, offset);
	AddHalf4Attribute<PanelVertex>(m_spec, 3, offset);

	for (int32_t i = 0; i < 4; i++) {
		m_borders[i] = WaveMesh<PanelVertex>(arl::SurfaceSize(border_columns, rows), m_spec,
						     gl->m_default_allocator);
		m_tops[i] = WaveMesh<PanelVertex>(arl::SurfaceSize(columns, rows), m_spec,
						  gl->m_default_allocator);

		if (i == 0)
			m_borders[i].Fill(PanelBorderVertex(0, 0, 0, 1));
		if (i == 1)
			m_borders[i].Fill(PanelBorderVertex(1, 1, 1, 2));
		if (i == 2)
			m_borders[i].Fill(PanelBorderVertex(2, 2, 2, 3));
		if (i == 3)
			m_borders[i].Fill(PanelBorderVertex(3, 3, 3, 4));

		if (i == 0)
			m_tops[i].Fill(PanelBorderVertex(0, 1, 1, 1));
		if (i == 1)
			m_tops[i].Fill(PanelBorderVertex(1, 2, 2, 2));
		if (i == 2)
			m_tops[i].Fill(PanelBorderVertex(2, 3, 3, 3));
		if (i == 3)
			m_tops[i].Fill(PanelBorderVertex(3, 0, 0, 0));
	}

	m_face = WaveMesh<PanelVertex>(arl::SurfaceSize(columns, columns), m_spec,
				       gl->m_default_allocator);
	m_face.Fill(PanelFaceVertex());
}

void ProfileMesh::Init(PointArray points, int32_t columns, uint32_t flags)
{
	arl::ArlInstance *gl = arl::Gl();

	m_points.clear();
	for (uint32_t i = 0; i < points.m_count; i++)
		m_points.push_back(points.m_points[i]);

	m_bottom = m_top = points.m_points[0].extract_y();
	for (uint32_t i = 0; i < points.m_count; i++) {
		float y = points.m_points[i].extract_y();

		m_top = m_top < y ? y : m_top;
		m_bottom = y < m_bottom ? y : m_bottom;
	}

	m_mesh = WaveMesh<ProfileVertex>(arl::SurfaceSize(columns, points.m_count),
					 ScePafGraphics_4D1E289A(gl,
								 arl::VertexArraySpec::ATTRIB0_F4_ATTRIB3_F4,
								 SCE_GXM_INDEX_SOURCE_INDEX_16BIT),
					 gl->m_default_allocator);
	FillGrid(m_mesh, ProfileVertexGenerator(points, flags));
}

MaterialParamSet *PanelRenderer::GetMaterial(uint32_t material)
{
	switch (material) {
	case 0:
		return &m_bubble_material.m_set;
	case 1:
		return &m_panel_material.m_set;
	case 2:
		return &m_panel_opaque_material.m_set;
	case 3:
		return &m_color_chip_material.m_set;
	default:
		return &m_panel_material.m_set;
	}
}

void PanelRenderer::CreateScreen()
{
	DisplaySurface *root = ui::Screen::s_root_surface;
	void *page = root->GetPagePtr(0);
	ScreenSurface::Param param;

	root->GetParam(&param);
	param.scale_mode = 2;
	param.full_size = true;
	param.depth_format = ImageMode_NONE;
	param.depth_order = (ImageOrder)0;
	m_screen = new ui::Screen(intrusive_ptr<Surface>(new ScreenSurface(&param, page,
									    Surface::TypeRenderTarget)));
}

void PanelRenderer::SetRenderStates(const int32_t &kind, bool blend)
{
	arl::ArlInstance *gl = arl::Gl();

	gl->CullMode(arl::CULL_CW);
	gl->ColorMask(true, true, true, true);
	gl->Disable(arl::DEPTH_TEST);
	gl->DepthFunc(arl::DEPTH_ALWAYS, arl::FRONT_AND_BACK);
	gl->DepthMask(true, arl::FRONT_AND_BACK);
	switch (kind) {
	case PanelKind_Glass:
	case PanelKind_GlassGlowing:
	case PanelKind_Opaque:
	case PanelKind_OpaqueWave:
	case PanelKind_Bubble:
	case PanelKind_BubbleFolder:
		if (blend) {
			gl->BlendFunc(arl::SRC_ALPHA, arl::ONE_MINUS_SRC_ALPHA, arl::ONE,
				      arl::ONE_MINUS_SRC_ALPHA);
			gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
			gl->Enable(arl::BLEND);
		} else {
			gl->Disable(arl::BLEND);
		}
		break;
	}
}

/* an empty member nothing calls (name ours) */
void PanelRenderer::Unused0()
{
}

PanelFragShaderPair PanelShaders::GetPanelFrag(int32_t kind)
{
	switch (kind) {
	case PanelKind_Glass:
		return PanelFragShaderPair(&m_glass_frag.m_interface, &m_glass_frag.m_object);
	case PanelKind_GlassGlowing:
		return PanelFragShaderPair(&m_glass_glowing_frag.m_interface,
					   &m_glass_glowing_frag.m_object);
	case PanelKind_Opaque:
		return PanelFragShaderPair(&m_opaque_frag.m_interface, &m_opaque_frag.m_object);
	case PanelKind_OpaqueWave:
		return PanelFragShaderPair(&m_opaque_wave_frag.m_interface,
					   &m_opaque_wave_frag.m_object);
	default:
		return PanelFragShaderPair(NULL, NULL);
	}
}

PanelVertShaderPair PanelShaders::GetPanelVert(int32_t kind, bool top)
{
	switch (kind) {
	case PanelKind_Glass:
		if (top)
			return PanelVertShaderPair(&m_glass_top_vert.m_interface,
						   &m_glass_top_vert.m_object);
		return PanelVertShaderPair(&m_glass_border_vert.m_interface,
					   &m_glass_border_vert.m_object);
	case PanelKind_GlassGlowing:
		if (top)
			return PanelVertShaderPair(&m_glass_top_vert.m_interface,
						   &m_glass_top_vert.m_object);
		return PanelVertShaderPair(&m_glass_border_glowing_vert.m_interface,
					   &m_glass_border_glowing_vert.m_object);
	case PanelKind_Opaque:
		if (top)
			return PanelVertShaderPair(&m_opaque_top_vert.m_interface,
						   &m_opaque_top_vert.m_object);
		return PanelVertShaderPair(&m_opaque_border_vert.m_interface,
					   &m_opaque_border_vert.m_object);
	case PanelKind_OpaqueWave:
		if (top)
			return PanelVertShaderPair(&m_opaque_wave_top_vert.m_interface,
						   &m_opaque_wave_top_vert.m_object);
		return PanelVertShaderPair(&m_opaque_wave_border_vert.m_interface,
					   &m_opaque_wave_border_vert.m_object);
	default:
		return PanelVertShaderPair(NULL, NULL);
	}
}

void PanelRenderer::DrawSolid3x3Panel(const int32_t &kind, const uint32_t &material,
				      const int32_t &flags)
{
	bool top = flags & DRAW_TOP;
	bool blend = (flags & DRAW_BLEND) != 0;
	arl::ArlInstance *gl = arl::Gl();

	if (m_screen != NULL) {
		m_screen->GetSurface()->ActivatePage(ui::Screen::s_root_surface->GetPageCounter());
		m_screen->OnTarget();
	}
	gl->PushMarker("draw_solid_3x3_panel");
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_LINE, arl::FRONT_AND_BACK);

	PanelVertShaderPair vert = m_shaders->GetPanelVert(kind, top);
	PanelFragShaderPair frag = m_shaders->GetPanelFrag(kind);

	SetRenderStates(kind, blend);

	math::v4 specular(Vector4(GetMaterial(material)->m_values.m_specular_coeff.m_value));

	vert.m_params->SetMVP(m_camera->GetViewProjection());
	vert.m_params->SetWorldScaleHack(1.0f);
	vert.m_params->SetEyePos(m_camera->GetEyePos());
	vert.m_params->SetMaterialPack(GetMaterial(material)->m_values.GetPack());
	vert.m_params->SetPointLightPack2(GetLight(material)->m_values.GetPack2());
	vert.m_params->SetPointLightPack1(GetLight(material)->m_values.MakePack1());
	vert.m_params->SetBackgroundSize(GetTextureSize(*m_background->GetSharedTexture()));
	vert.m_params->SetBackgroundRatio(GetTextureRatio(*m_background->GetSharedTexture()));
	vert.m_params->SetView(m_camera->m_transform.m_inverse);
	vert.m_params->SetSpecularFactor(specular);
	vert.m_object->begin();

	frag.m_params->SetEyePos(m_camera->GetEyePos());
	frag.m_params->SetMaterialPack(GetMaterial(material)->m_values.GetPack());
	frag.m_params->SetPointLightPack2(GetLight(material)->m_values.GetPack2());
	frag.m_params->SetPointLightPack1(GetLight(material)->m_values.MakePack1());
	frag.m_params->SetEnvMapDetail(m_background->GetReflectionTexture());
	{
		arl::DefaultTextures *textures = gl;
		common::SharedPtr<arl::TextureObject> texture = textures->m_white;
		arl::TextureSource source;

		source = texture;
		frag.m_params->SetColorMap(source);
	}
	{
		common::SharedPtr<arl::Texture2D> texture = m_background->GetSharedTexture();
		arl::TextureSource source;

		source.m_texture = texture;
		source.m_surface.clear();
		frag.m_params->SetBackground(source);
	}
	frag.m_params->SetBackgroundSize(GetTextureSize(*m_background->GetSharedTexture()));
	frag.m_params->SetBackgroundRatio(GetTextureRatio(*m_background->GetSharedTexture()));
	frag.m_params->SetView(m_camera->m_transform.m_inverse);
	frag.m_params->SetSpecularFactor(specular);
	frag.m_params->SetGlowColor(s_color_cyan + (s_color_white - s_color_cyan) * 0.7f);
	frag.m_object->begin();
}

void PanelRenderer::DrawPanel(const int32_t &kind, const PanelParam &param, const bool &top)
{
	arl::ArlInstance *gl = arl::Gl();
	PanelVertShaderPair vert = m_shaders->GetPanelVert(kind, top);
	PanelFragShaderPair frag = m_shaders->GetPanelFrag(kind);
	Vector2 size(vget_low_f32(param.m_scale));

	vert.m_params->SetSize(math::v4(Vector4(size, recipPerElem(size))));
	vert.m_params->SetRoundness(math::v2(param.m_pair_y, param.m_pair_x));

	Vector2 border = minPerElem(size * 0.5f, Vector2(param.m_edge));
	math::v3 border_size(Vector4(border, border).getXYZ());

	border_size.set_z(vgetq_lane_f32(param.m_scale, 2));
	vert.m_params->SetBorderSize(border_size);

	math::matrix transform(math::quaternion(sce::Vectormath::Simd::Aos::Quat(param.m_rot)));

	transform.set_w(math::v4(Vector4(param.m_pos)));
	vert.m_params->SetTransform(transform);
	vert.m_params->SetColor(param.GetColor());
	if (param.m_revolution.m_axis[1] > 0.0f) {
		float angle = param.m_revolution.m_axis[0];

		vert.m_params->SetBorderGlow(math::v4(sce_paf_cosf(angle), sce_paf_sinf(angle),
						      param.m_revolution.m_axis[1], 0.0f));
		vert.m_params->SetBorderPow(math::v2(param.m_revolution.m_axis[2],
						     param.m_revolution.m_angle));
	} else {
		vert.m_params->SetBorderGlow(math::v4(0.0f, 0.0f, 1.0f, 0.0f));
		vert.m_params->SetBorderPow(math::v2(1.0f, 1.0f));
	}
	if (kind == PanelKind_OpaqueWave && param.GetName()[0] != '\0' &&
	    m_wave_renderer.get() != NULL) {
		WaveInstanceParamSet::Values &instance =
			m_wave_renderer->m_wave_instance.m_keys.GetKey(string(param.GetName()), false)
				->m_values;

		vert.m_params->SetWaveCenterColor(math::v4(instance.m_center_color.m_value,
							   instance.m_center_power.m_value));
		vert.m_params->SetWaveEdgeColor(math::v4(instance.m_edge_color.m_value,
							 instance.m_edge_power.m_value));
		vert.m_params->SetColor(s_color_white);
		MaterialParamSet::Values &material =
			m_wave_renderer->m_material.m_keys.GetKey(string(param.GetName()), false)
				->m_values;

		vert.m_params->SetMaterialPack(material.GetPack());

		PointLightSphereParamSet::Values &light =
			m_wave_renderer->m_point_light.m_keys.GetKey(string(param.GetName()), false)
				->m_values;

		vert.m_params->SetPointLightPack2(light.GetPack2());
		vert.m_params->SetPointLightPack1(light.MakePack1());

		FogParamSet::Values &fog =
			m_wave_renderer->m_fog.m_keys.GetKey(string(param.GetName()), false)->m_values;

		vert.m_params->SetWaveFogPack(fog.MakePack());
	}

	arl::DefaultTextures *textures = gl;

	frag.m_params->SetColorMap(
		param.m_texture.m_texture.get() != NULL || param.m_texture.m_surface.get() != NULL ?
			param.m_texture :
			arl::TextureSource(common::SharedPtr<arl::TextureObject>(textures->m_white)));
	frag.m_params->SetColor(param.GetColor());

	gutil::debug::Mouse *mouse = gutil::debug::Mouse::GetInstance();
	Ray ray = m_camera->GetRay(math::v2(
		(float)mouse->m_pointers[0].m_x * 2.0f / (float)mouse->m_screen_width - 1.0f,
		1.0f - (float)mouse->m_pointers[0].m_y * 2.0f / (float)mouse->m_screen_height));

	frag.m_params->SetHitPoint(HitPanel(param, ray));
	if (top) {
		DrawMesh(m_meshes.m_face, gl);
		return;
	}
	if (!param.m_flag0 && !param.m_flag1) {
		DrawMesh(m_meshes.m_borders[0], gl);
		DrawMesh(m_meshes.m_borders[1], gl);
		DrawMesh(m_meshes.m_borders[2], gl);
		DrawMesh(m_meshes.m_borders[3], gl);
	}
	if (!param.m_flag1) {
		DrawMesh(m_meshes.m_tops[0], gl);
		DrawMesh(m_meshes.m_tops[2], gl);
	}
	if (!param.m_flag0) {
		DrawMesh(m_meshes.m_tops[1], gl);
		DrawMesh(m_meshes.m_tops[3], gl);
	}
}

void PanelRenderer::EndSolid3x3Panel(const int32_t &kind, const bool &top)
{
	arl::ArlInstance *gl = arl::Gl();
	PanelVertShaderPair vert = m_shaders->GetPanelVert(kind, top);
	PanelFragShaderPair frag = m_shaders->GetPanelFrag(kind);

	frag.m_object->end();
	vert.m_object->end();
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_FILL, arl::FRONT_AND_BACK);
	gl->PopMarker();
	if (m_screen != NULL)
		m_screen->OnFinish();
}

void PanelRenderer::DrawRevolutionPanel(const uint32_t &material)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_screen != NULL) {
		m_screen->GetSurface()->ActivatePage(ui::Screen::s_root_surface->GetPageCounter());
		m_screen->OnTarget();
	}
	gl->PushMarker("draw_revolution_panel");
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_LINE, arl::FRONT_AND_BACK);

	BubbleVertShaderParams &vert = m_shaders->m_bubble_opaque_vert.m_interface;
	const ShadowParamSet::Values &shadow = m_shadow.m_set.m_values;

	vert.SetMVP(m_camera->GetViewProjection());
	vert.SetEyePos(m_camera->GetEyePos());
	vert.SetMaterialPack(GetMaterial(material)->m_values.GetPack());
	vert.SetDiffuseColor(math::v4(GetLight(material)->m_values.m_diffuse_color.m_value, 1.0f));
	vert.SetLightPos(GetLight(material)->m_values.GetPosition());
	vert.SetView(m_camera->m_transform.m_inverse);
	vert.SetBackColorZRange(math::v2(shadow.m_back_color_begin.m_value,
					 1.0f / (shadow.m_back_color_end.m_value -
						 shadow.m_back_color_begin.m_value)));
	m_shaders->m_bubble_opaque_vert.m_object.begin();
}

BubbleFragShaderPair PanelShaders::GetBubbleFrag(int32_t kind)
{
	switch (kind) {
	case PanelKind_Bubble:
		return BubbleFragShaderPair(&m_bubble_opaque_frag.m_interface,
					    &m_bubble_opaque_frag.m_object);
	case PanelKind_BubbleFolder:
		return BubbleFragShaderPair(&m_bubble_opaque_folder_frag.m_interface,
					    &m_bubble_opaque_folder_frag.m_object);
	default:
		return BubbleFragShaderPair(NULL, NULL);
	}
}

void PanelRenderer::BeginBubbleFrag(const int32_t &kind, const uint32_t &material,
				    const bool &blend)
{
	BubbleFragShaderPair frag = m_shaders->GetBubbleFrag(kind);
	arl::ArlInstance *gl = arl::Gl();
	math::v4 specular(Vector4(GetMaterial(material)->m_values.m_specular_coeff.m_value));

	SetRenderStates(kind, blend);
	frag.m_params->SetEnvMapDetail(m_background->GetReflectionTexture());
	{
		arl::DefaultTextures *textures = gl;
		common::SharedPtr<arl::TextureObject> texture = textures->m_white;
		arl::TextureSource source;

		source = texture;
		frag.m_params->SetColorMap(source);
	}
	frag.m_params->SetSpecularFactor(specular);
	frag.m_object->begin();
}

BubbleVertShaderPair PanelShaders::GetBubbleVert(int32_t kind)
{
	switch (kind) {
	case PanelKind_Bubble:
		return BubbleVertShaderPair(&m_bubble_opaque_vert.m_interface,
					    &m_bubble_opaque_vert.m_object);
	case PanelKind_BubbleFolder:
		return BubbleVertShaderPair(&m_bubble_opaque_vert.m_interface,
					    &m_bubble_opaque_vert.m_object);
	default:
		return BubbleVertShaderPair(NULL, NULL);
	}
}

void PanelRenderer::DrawBubble(const int32_t &kind, const BubbleParam &param)
{
	BubbleVertShaderPair vert = m_shaders->GetBubbleVert(kind);
	BubbleFragShaderPair frag = m_shaders->GetBubbleFrag(kind);
	arl::ArlInstance *gl = arl::Gl();
	math::matrix world(math::quaternion(sce::Vectormath::Simd::Aos::Quat(param.m_rot)));

	world.set_w(math::v4(Vector4(param.m_pos)));

	Vector4 half = Vector4(param.m_scale) * 0.5f;

	vert.m_params->SetTransform(world * math::matrix(sce::Vectormath::Simd::Aos::Matrix4::scale(
					       half.getXYZ())));
	vert.m_params->SetTransformInverseTranspose(
		(math::matrix(sce::Vectormath::Simd::Aos::Matrix4::scale(
			 recipPerElem(half).getXYZ())) *
		 world.inverse1())
			.transpose());

	ProfileMesh &profile = m_profiles.m_meshes[param.m_profile];

	vert.m_params->SetHeight(math::v2(profile.m_bottom,
					  1.0f / (profile.m_top - profile.m_bottom)));
	if (param.m_refract) {
		const MaterialParamSet::Values &material = GetMaterial(0)->m_values;

		vert.m_params->SetMaterialPack(math::matrix(
			material.m_color.m_value,
			math::v4(material.m_diffuse_coeff.m_value, material.m_specular_coeff.m_value,
				 material.m_specular_power.m_value, 0.0f),
			math::v4(material.m_fresnel_front.m_value, material.m_fresnel_control.m_value,
				 material.m_fresnel_coeff.m_value, 0.0f),
			math::v4(param.m_refract_scale, 0.0f, param.m_refract_offset,
				 material.m_second_reflection.m_value)));
	} else {
		vert.m_params->SetMaterialPack(GetMaterial(0)->m_values.GetPack());
	}
	if (param.m_fixed_light)
		vert.m_params->SetLightPos(param.GetFixedLightPos());
	else
		vert.m_params->SetLightPos(m_bubble_light.m_set.m_values.GetPosition());
	frag.m_params->SetColorMap(param.m_texture);

	float alpha = vgetq_lane_f32(param.m_color, 3);

	frag.m_params->SetAlpha(alpha);
	frag.m_params->SetBackColor(math::v4(Vector4(Vector4(param.m_back_color).getXYZ(), 1.0f)));

	float extent = param.m_extent;

	if (extent < 0.0f)
		extent = 0.0f;
	if (!(extent < 1.0f))
		extent = 1.0f;

	uint32_t rows = (int32_t)((float)(profile.m_mesh.m_height - 1) * extent);

	if (m_wireframe)
		profile.m_mesh.DrawLines(gl);
	else
		profile.m_mesh.Draw(gl, rows);
}

void PanelRenderer::EndBubbleFrag(const int32_t &kind)
{
	BubbleFragShaderPair frag = m_shaders->GetBubbleFrag(kind);
	arl::ArlInstance *gl = arl::Gl();

	frag.m_object->end();
}

void PanelRenderer::EndRevolutionPanel()
{
	arl::VertShaderObject *object = &m_shaders->m_bubble_opaque_vert.m_object;
	arl::ArlInstance *gl = arl::Gl();

	object->end();
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_FILL, arl::FRONT_AND_BACK);
	gl->PopMarker();
	if (m_screen != NULL)
		m_screen->OnFinish();
}

void PanelRenderer::Unused1()
{
}

PanelRenderer::PanelRenderer()
	: m_shaders(new PanelShaders), m_profiles(&m_profile, 1, 1), m_frame(-1), m_screen(NULL),
	  m_manager("PanelRenderer"), m_color_chip_manager("PanelRendererColorChip"),
	  m_bubble_material(m_manager, "bubble_opaque"), m_bubble_light(m_manager, "bubble"),
	  m_panel_material(m_manager, "panel"), m_panel_opaque_material(m_manager, "panel_opaque"),
	  m_panel_light(m_manager, "panel"), m_touch_wave(m_manager, "panel"),
	  m_shadow(m_manager, "bubble"), m_color_chip_material(m_color_chip_manager, "color_chip"),
	  m_color_chip_light(m_color_chip_manager, "color_chip"), m_touch_time(0.0f),
	  m_unread_flag(false), m_wireframe(false), m_rs_solid_3x3_panel(NULL),
	  m_rs_bubble_glass(NULL), m_rs_bubble_opaque(NULL), m_rs_panel_debug(NULL)
{
	CreateScreen();
}

PanelRenderer::~PanelRenderer()
{
	delete m_shaders;
	m_shaders = NULL;
	delete m_screen;
	m_screen = NULL;
}

void PanelRenderer::Init(common::SharedPtr<Camera> camera, common::SharedPtr<Camera> camera_2d,
			 common::SharedPtr<Background> background, common::SharedPtr<DrawEnv> draw_env)
{
	m_camera = camera;
	m_camera_2d = camera_2d;
	m_background = background;
	m_draw_env = draw_env;
}

/* the parameter sets moved on by the frame's time; a touch starts the time again at the place
   touched */
void PanelRenderer::Update()
{
	arl::ArlInstance *gl = arl::Gl();

	m_manager.Update(gl->m_frame_time);
	m_color_chip_manager.Update(gl->m_frame_time);
	m_frame = common::FrameCount();
	if (gutil::debug::Mouse::GetInstance()->m_buttons[0].m_pressed) {
		m_touch_time = 0.0f;
		gutil::debug::Mouse *mouse = gutil::debug::Mouse::GetInstance();
		m_touch_pos = math::v2(mouse->m_pointers[0].m_x * 2.0f / mouse->m_screen_width - 1.0f,
				       1.0f - mouse->m_pointers[0].m_y * 2.0f / mouse->m_screen_height);
	} else {
		m_touch_time += gl->m_frame_time;
	}
}

void PanelRenderer::AddSolid3x3Panel(const PanelParam &param, int32_t kind, uint32_t material)
{
	bool blend = vgetq_lane_f32(param.m_color, 3) < 0.99607843f;
	int32_t flags = 0;

	if (blend)
		flags = DRAW_BLEND;

	DrawKey key;

	key.m_word = 0;
	key.m_type = 1;
	key.m_order0 = blend;
	key.m_order2 = kind;
	key.m_material = material;

	DrawLayer *layer = m_rs_solid_3x3_panel;
	PanelRenderer *self = this;

	if (layer->AddKey(DrawLayer::Key(key.m_word, 0))) {
		key.m_order1 = 0;
		key.m_order3 = 0;
		layer->Push(key.m_word, 0, &PanelRenderer::DrawSolid3x3Panel, self, kind, material,
			    flags);
		key.m_order3 = 2;
		layer->Push(key.m_word, 0, &PanelRenderer::EndSolid3x3Panel, self, kind, false);
		key.m_order1 = 1;
		key.m_order3 = 0;
		layer->Push(key.m_word, 0, &PanelRenderer::DrawSolid3x3Panel, self, kind, material,
			    flags | DRAW_TOP);
		key.m_order3 = 2;
		layer->Push(key.m_word, 0, &PanelRenderer::EndSolid3x3Panel, self, kind, true);
	}

	bool top = false;

	key.m_order1 = 0;
	key.m_order3 = 1;
	layer->Push(key.m_word, 0, &PanelRenderer::DrawPanel, self, kind, param, top);
	top = true;
	key.m_order1 = 1;
	layer->Push(key.m_word, 0, &PanelRenderer::DrawPanel, self, kind, param, top);
}

void PanelRenderer::AddBubble(const BubbleParam &param, int32_t kind, uint32_t material,
			      DrawLayer *const &layer)
{
	bool blend = vgetq_lane_f32(param.m_color, 3) < 0.99607843f;
	PanelRenderer *self = this;
	DrawKey key;

	key.m_word = 0;
	key.m_type = 1;
	key.m_order0 = 1;
	key.m_order1 = blend;
	key.m_order3 = PanelKind_Bubble;
	key.m_material = material;
	if (layer->AddKey(DrawLayer::Key(key.m_word, 0))) {
		layer->Push(key.m_word, 0, &PanelRenderer::DrawRevolutionPanel, self, material);
		key.m_order2 = 7;
		layer->Push(key.m_word, 0, &PanelRenderer::EndRevolutionPanel, self);
	}

	int32_t pass = 1;

	if (kind == PanelKind_BubbleFolder)
		pass = 4;

	key.m_word = 0;
	key.m_type = 1;
	key.m_order0 = 1;
	key.m_order1 = blend;
	key.m_order2 = pass;
	key.m_order3 = kind;
	key.m_material = material;
	if (layer->AddKey(DrawLayer::Key(key.m_word, 0))) {
		layer->Push(key.m_word, 0, &PanelRenderer::BeginBubbleFrag, self, kind, material,
			    blend);
		key.m_order2 = pass + 2;
		layer->Push(key.m_word, 0, &PanelRenderer::EndBubbleFrag, self, kind);
	}
	key.m_order2 = pass + 1;
	layer->Push(key.m_word, 0, &PanelRenderer::DrawBubble, self, kind, param);
}

void PanelRenderer::AddBubbleObj(BubbleParam *param, int32_t type, const common::SharedPtr<WaveRenderer> &wave_renderer,
				 DrawLayer *layer)
{
	if (type == 0 || type == 1)
		param->m_profile = 0;
	if (type == 0) {
		DrawLayer *target = layer != NULL ? layer : m_rs_bubble_opaque;

		AddBubble(*param, PanelKind_Bubble, 0, target);
	} else if (type == 1) {
		DrawLayer *target = layer != NULL ? layer : m_rs_bubble_opaque;

		AddBubble(*param, PanelKind_BubbleFolder, 0, target);
	}
}

math::matrix PanelRenderer::GetPointLightPack1()
{
	return m_bubble_light.m_set.m_values.MakePack1();
}

math::matrix PanelRenderer::GetPointLightPack2()
{
	return m_bubble_light.m_set.m_values.GetPack2();
}

void PanelRenderer::DrawDebug()
{
}

/* a point of a profile, which may be left out of it (names ours) */
struct ProfilePoint {
	ProfilePoint()
	{
	}

	ProfilePoint(float x, float y) : m_point(x, y), m_skipped(false)
	{
	}

	math::v2 m_point;
	bool m_skipped;
};

/* the points kept moved down, then scaled to the profile's width from its lowest corner,
   and centred on the middle of its height if asked (names ours) */
static void NormalizeProfile(vector<ProfilePoint> *points, bool keep_skipped, bool center)
{
	uint32_t kept = 0;
	uint32_t i;

	for (i = 0; i < points->size(); i++) {
		if (keep_skipped || !(*points)[i].m_skipped)
			(*points)[kept++] = (*points)[i];
	}
	points->resize(kept);

	Vector2 low = (*points)[0].m_point.getImpl();
	Vector2 high = low;
	uint32_t count = points->size();

	for (i = 0; i < count; i++) {
		Vector2 point = (*points)[i].m_point.getImpl();

		low = minPerElem(point, low);
		high = maxPerElem(point, high);
	}
	for (i = 0; i < count; i++) {
		math::v2 &point = (*points)[i].m_point;

		point = point - math::v2(low);
		point = point / (high - low).getX();
	}
	if (center) {
		float offset = math::v2(high - low).extract_y() * -0.5f;

		for (i = 0; i < count; i++)
			(*points)[i].m_point = (*points)[i].m_point + math::v2(0.0f, offset);
	}
}

/* the bubble's profile: half an ellipse, a point every ten degrees (names ours) */
static void MakeBubbleProfile(vector<ProfilePoint> *points)
{
	points->clear();
	points->push_back(ProfilePoint(-4.37114e-08f, 0.3f));
	points->push_back(ProfilePoint(0.173648f, 0.295442f));
	points->push_back(ProfilePoint(0.34202f, 0.281908f));
	points->push_back(ProfilePoint(0.5f, 0.259808f));
	points->push_back(ProfilePoint(0.642787f, 0.229813f));
	points->push_back(ProfilePoint(0.766044f, 0.192836f));
	points->push_back(ProfilePoint(0.866025f, 0.15f));
	points->push_back(ProfilePoint(0.939693f, 0.102606f));
	points->push_back(ProfilePoint(0.984808f, 0.0520945f));
	points->push_back(ProfilePoint(1.0f, 0.0f));
	points->push_back(ProfilePoint(0.984808f, -0.0520944f));
	points->push_back(ProfilePoint(0.939693f, -0.102606f));
	points->push_back(ProfilePoint(0.866025f, -0.15f));
	points->push_back(ProfilePoint(0.766044f, -0.192836f));
	points->push_back(ProfilePoint(0.642788f, -0.229813f));
	points->push_back(ProfilePoint(0.5f, -0.259808f));
	points->push_back(ProfilePoint(0.34202f, -0.281908f));
	points->push_back(ProfilePoint(0.173648f, -0.295442f));
	points->push_back(ProfilePoint(-4.37114e-08f, -0.3f));
}

/* the columns the bubble's profile is swept into: nothing sets it (name ours) */
static int32_t s_profile_columns;

void PanelRenderer::Load(const string &dir)
{
	arl::ArlInstance *gl = arl::Gl();

	m_meshes.Init(9, 6, 8);
	{
		int32_t columns = s_profile_columns;
		vector<ProfilePoint> points((allocator<ProfilePoint>()));

		MakeBubbleProfile(&points);
		NormalizeProfile(&points, false, true);

		vector<math::v2> profile((allocator<math::v2>()));

		for (uint32_t i = 0; i < points.size(); i++)
			profile.push_back(points.at(i).m_point);
		m_profiles.m_meshes->Init(profile, columns, 6);
	}

	common::SharedPtr<arl::VertexArraySpec> &border_spec = m_meshes.m_borders[0].m_vertices.m_spec;
	common::SharedPtr<arl::VertexArraySpec> &face_spec = m_meshes.m_face.m_vertices.m_spec;

	m_shaders->m_glass_border_vert.m_object.init(*gl, border_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_glass_top_vert.m_object.init(*gl, face_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_glass_frag.m_object.init(*gl);
	m_shaders->m_opaque_border_vert.m_object.init(*gl, border_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_opaque_top_vert.m_object.init(*gl, face_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_opaque_frag.m_object.init(*gl);
	m_shaders->m_opaque_wave_border_vert.m_object.init(*gl, border_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_opaque_wave_top_vert.m_object.init(*gl, face_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_opaque_wave_frag.m_object.init(*gl);
	m_shaders->m_glass_border_glowing_vert.m_object.init(*gl, border_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_glass_glowing_frag.m_object.init(*gl);
	m_shaders->m_bubble_image_vert.m_object.init(*gl, m_profiles.m_meshes->m_mesh.m_vertices.m_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_bubble_image_frag.m_object.init(*gl);
	m_shaders->m_bubble_opaque_vert.m_object.init(*gl, m_profiles.m_meshes->m_mesh.m_vertices.m_spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_bubble_opaque_frag.m_object.setMultisampleMode(SCE_GXM_MULTISAMPLE_4X);
	m_shaders->m_bubble_opaque_folder_frag.m_object.setMultisampleMode(SCE_GXM_MULTISAMPLE_4X);
	m_shaders->m_bubble_opaque_frag.m_object.init(*gl);
	m_shaders->m_bubble_opaque_folder_frag.m_object.init(*gl);

	/* the layers draw to the screen's own buffer while there is no screen */
	int32_t mode = m_screen == NULL;

	m_rs_solid_3x3_panel = m_draw_env->AddLayer("m_rs_solid_3x3_panel", 232, mode, 0x4000);
	m_rs_solid_3x3_panel->m_draw_mode = 2;
	m_rs_bubble_opaque = m_draw_env->AddLayer("m_rs_bubble_opaque", 230, mode, 0x4000);
	m_rs_bubble_opaque->m_draw_mode = 2;
	m_rs_bubble_glass = m_draw_env->AddLayer("m_rs_bubble_glass", 225, mode, 0x4000);
	m_rs_bubble_glass->m_draw_mode = 2;
	m_rs_panel_debug = m_draw_env->AddLayer("m_rs_panel_debug", 900, 2, 0x4000);
	m_rs_panel_debug->Push(0x40000000, 0, &PanelRenderer::DrawDebug, this);
	m_rs_panel_debug->m_draw_mode = 2;

	m_bubble_light.m_keys.m_default.m_values.m_distance.SetRange(0.0f, 10000.0f);
	m_panel_light.m_keys.m_default.m_values.m_distance.SetRange(0.0f, 10000.0f);
	m_bubble_light.m_set.m_values.m_distance.SetRange(0.0f, 10000.0f);
	m_panel_light.m_set.m_values.m_distance.SetRange(0.0f, 10000.0f);
	m_bubble_material.m_keys.m_default.m_values.m_specular_power.SetRange(0.8f, 200.0f);
	m_bubble_material.m_set.m_values.m_specular_power.SetRange(0.8f, 200.0f);

	m_manager.SetPaths(dir, "panel/psets");
	m_color_chip_manager.SetPaths(dir, "panel/psets");

	MaterialParamSet &bubble_material = m_bubble_material.m_keys.m_default;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.517115f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.804327f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0406_ruby", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.474925f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.721903f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0407_purple_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.398339f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.893615f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0408_01_orange", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.273296f);
	bubble_material.findParamAndSet("fresnel_coeff", 1.206423f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0411_purple_02", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.416545f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.893615f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0415_A5", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.474925f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.893615f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0518_dalia", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.260046f);
	bubble_material.findParamAndSet("fresnel_coeff", 1.821123f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.43537f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.804327f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_0518_ruby", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.539229f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.512249f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_A1", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.585595f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.512249f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_black", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.474925f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.893615f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.495684f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.76228f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_blue_FLAT", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.495684f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.683152f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_bluewhite_05", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.363721f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.610351f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_casis", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.517115f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.721903f);
	bubble_material.findParamAndSet("refraction_coeff", 1.0015f);
	bubble_material.findParamAndSet("refraction_amount", -0.03f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_default_0512_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.539229f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.482351f);
	bubble_material.findParamAndSet("refraction_coeff", 1.0f);
	bubble_material.findParamAndSet("refraction_amount", -0.0f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_green_0512_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.161334f);
	bubble_material.findParamAndSet("fresnel_coeff", 1.524467f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.585595f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.804327f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.495684f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.893615f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_maintosh", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.539232f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.54353f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_marrygold", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.585595f);
	bubble_material.findParamAndSet("fresnel_coeff", 1.094277f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_peach_0512_001", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.517115f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.683152f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_purple_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.43537f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.54353f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_purplered", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.301304f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.645983f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.495684f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.610351f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.517115f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.54353f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.517115f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.804327f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.474925f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.721903f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.398339f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.893615f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.273296f);
	bubble_material.findParamAndSet("fresnel_coeff", 1.206423f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = bubble_material.m_values;

	bubble_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	bubble_material.findParamAndSet("diffuse_coeff", 0.964285f);
	bubble_material.findParamAndSet("specular_coeff", 0.029041f);
	bubble_material.findParamAndSet("specular_power", 153.75714f);
	bubble_material.findParamAndSet("fresnel_front", 0.0f);
	bubble_material.findParamAndSet("fresnel_control", 0.771703f);
	bubble_material.findParamAndSet("fresnel_coeff", 0.576211f);
	bubble_material.findParamAndSet("refraction_coeff", 1.02451f);
	bubble_material.findParamAndSet("refraction_amount", -0.053567f);
	bubble_material.findParamAndSet("second_reflection", 0.0f);
	m_bubble_material.m_keys.GetKey("iboot", true)->m_values = bubble_material.m_values;

	MaterialParamSet &panel_material = m_panel_material.m_keys.m_default;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0406_ruby", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0407_purple_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0408_01_orange", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0411_purple_02", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0415_A5", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0518_dalia", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_0518_ruby", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_A1", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_black", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_blue_FLAT", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_bluewhite_05", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_casis", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_default_0512_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_green_0512_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_maintosh", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_marrygold", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_peach_0512_001", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_purple_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_purplered", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = panel_material.m_values;

	panel_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_material.findParamAndSet("diffuse_coeff", 0.455357f);
	panel_material.findParamAndSet("specular_coeff", 0.106742f);
	panel_material.findParamAndSet("specular_power", 10.542857f);
	panel_material.findParamAndSet("fresnel_front", 0.0f);
	panel_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_material.findParamAndSet("fresnel_coeff", 0.721903f);
	panel_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_material.m_keys.GetKey("iboot", true)->m_values = panel_material.m_values;

	MaterialParamSet &panel_opaque_material = m_panel_opaque_material.m_keys.m_default;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0406_ruby", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0407_purple_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0408_01_orange", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0411_purple_02", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0415_A5", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0518_dalia", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_0518_ruby", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_A1", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_black", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_blue_FLAT", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_bluewhite_05", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_casis", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_default_0512_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_green_0512_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_maintosh", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_marrygold", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_peach_0512_001", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_purple_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_purplered", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = panel_opaque_material.m_values;

	panel_opaque_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	panel_opaque_material.findParamAndSet("diffuse_coeff", 1.0f);
	panel_opaque_material.findParamAndSet("specular_coeff", 2.735693f);
	panel_opaque_material.findParamAndSet("specular_power", 10.542857f);
	panel_opaque_material.findParamAndSet("fresnel_front", 0.0f);
	panel_opaque_material.findParamAndSet("fresnel_control", 0.749091f);
	panel_opaque_material.findParamAndSet("fresnel_coeff", 0.893615f);
	panel_opaque_material.findParamAndSet("refraction_coeff", 2.188555f);
	panel_opaque_material.findParamAndSet("refraction_amount", 0.04464f);
	panel_opaque_material.findParamAndSet("second_reflection", 0.0f);
	m_panel_opaque_material.m_keys.GetKey("iboot", true)->m_values = panel_opaque_material.m_values;

	PointLightSphereParamSet &bubble_light = m_bubble_light.m_keys.m_default;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0406_ruby", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0407_purple_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0408_01_orange", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0411_purple_02", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0415_A5", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0518_dalia", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_0518_ruby", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_A1", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_black", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_blue_FLAT", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_bluewhite_05", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_casis", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_default_0512_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_green_0512_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_maintosh", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_marrygold", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_peach_0512_001", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_purple_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_purplered", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = bubble_light.m_values;

	bubble_light.findParamAndSet("theta_phi", 1.150047f, -0.056096f);
	bubble_light.findParamAndSet("distance", 939.0627f);
	bubble_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	bubble_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	bubble_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	bubble_light.findParamAndSet("fade", 1.0f);
	m_bubble_light.m_keys.GetKey("iboot", true)->m_values = bubble_light.m_values;

	PointLightSphereParamSet &panel_light = m_panel_light.m_keys.m_default;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0406_ruby", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0407_purple_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0408_01_orange", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0411_purple_02", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0415_A5", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0518_dalia", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_0518_ruby", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_A1", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_black", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_blue_FLAT", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_bluewhite_05", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_casis", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_default_0512_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_green_0512_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_maintosh", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_marrygold", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_peach_0512_001", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_purple_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_purplered", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = panel_light.m_values;

	panel_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	panel_light.findParamAndSet("distance", 1582.0312f);
	panel_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	panel_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	panel_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	panel_light.findParamAndSet("fade", 1.0f);
	m_panel_light.m_keys.GetKey("iboot", true)->m_values = panel_light.m_values;

	TouchWaveParamSet &touch_wave = m_touch_wave.m_keys.m_default;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0406_ruby", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0407_purple_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0408_01_orange", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0411_purple_02", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0415_A5", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0518_dalia", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0518_gray_FLAT", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_0518_ruby", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_A1", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_black", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_blueTB_0513_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_blue_FLAT", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_bluewhite_05", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_casis", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_default_0512_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_green_0512_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_koiao_0512_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_limegreen_0513_02", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_maintosh", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_marrygold", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_peach_0512_001", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_purple_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_purplered", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_redTB_0513_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_turquoise_0513_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_yellow_0512_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_color_vari_2g_01", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_color_vari_2g_02", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_color_vari_2g_03", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("effects_color_vari_2g_04", true)->m_values = touch_wave.m_values;

	touch_wave.findParamAndSet("half_width", 0.35f);
	touch_wave.findParamAndSet("speed", 3.0f);
	touch_wave.findParamAndSet("fade", 2.5f);
	touch_wave.findParamAndSet("scale", 151.0f);
	m_touch_wave.m_keys.GetKey("iboot", true)->m_values = touch_wave.m_values;

	MaterialParamSet &color_chip_material = m_color_chip_material.m_keys.m_default;

	color_chip_material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	color_chip_material.findParamAndSet("diffuse_coeff", 1.0f);
	color_chip_material.findParamAndSet("specular_coeff", 2.735693f);
	color_chip_material.findParamAndSet("specular_power", 10.542857f);
	color_chip_material.findParamAndSet("fresnel_front", 0.0f);
	color_chip_material.findParamAndSet("fresnel_control", 0.749091f);
	color_chip_material.findParamAndSet("fresnel_coeff", 0.893615f);
	color_chip_material.findParamAndSet("refraction_coeff", 2.188555f);
	color_chip_material.findParamAndSet("refraction_amount", 0.04464f);
	m_color_chip_material.m_keys.GetKey("default", true)->m_values = color_chip_material.m_values;

	PointLightSphereParamSet &color_chip_light = m_color_chip_light.m_keys.m_default;

	color_chip_light.findParamAndSet("theta_phi", 1.430546f, 0.336599f);
	color_chip_light.findParamAndSet("distance", 1582.0312f);
	color_chip_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	color_chip_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	color_chip_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	color_chip_light.findParamAndSet("fade", 1.0f);
	m_color_chip_light.m_keys.GetKey("default", true)->m_values = color_chip_light.m_values;

	m_manager.AddVariant("effects_default_0512_01");
	m_manager.SetVariant("effects_default_0512_01");
	m_color_chip_manager.AddVariant("default");
	m_color_chip_manager.SetVariant("default");
}

PanelVertex PanelBorderVertex::operator()(const math::v2 &uv) const
{
	float angle0 = m_corner0 * 1.5707964f + 0.7853982f;
	Vector2 corner0 = Vector2(sce_paf_cosf(angle0), sce_paf_sinf(angle0)) * 1.4142135f;
	float angle1 = m_corner1 * 1.5707964f + 0.7853982f;
	Vector2 corner1 = Vector2(sce_paf_cosf(angle1), sce_paf_sinf(angle1)) * 1.4142135f;
	Vector2 outline = corner0 + (corner1 - corner0) * uv.extract_x();
	float turn = (m_angle0 + ((float)m_angle1 - (float)m_angle0) * uv.extract_x()) * 1.5707964f;
	float bend = uv.extract_y() * 1.5707964f;
	Vector2 across(sce_paf_cosf(bend), sce_paf_sinf(bend));
	Vector2 edge(sce_paf_cosf(turn), sce_paf_sinf(turn));

	return PanelVertex(math::v4(Vector4(uv.getImpl(), outline)), math::v4(Vector4(across, edge)));
}

PanelVertex PanelFaceVertex::operator()(const math::v2 &uv) const
{
	return PanelVertex(math::v4(uv.extract_x(), uv.extract_y(), 0.0f, 0.0f),
			   math::v4(0.0f, 0.0f, 0.0f, 0.0f));
}

ProfileVertex ProfileVertexGenerator::operator()(GridPosition position,
						 arl::SurfaceSize size) const
{
	math::v2 point = GetPoint(position.m_y);
	math::v2 next = GetPoint(position.m_y + 1);
	math::v2 previous = GetPoint(position.m_y - 1);
	Vector2 tangent = (next - previous).getImpl();
	Vector2 perpendicular(-tangent.getY(), tangent.getX());
	Vector2 normal = perpendicular / length(perpendicular);
	Vector2 last((float)(size.m_width - 1), (float)(size.m_height - 1));
	Vector2 uv = mulPerElem(Vector2((float)position.m_x, (float)position.m_y), recipPerElem(last));
	float turn = (1.0f - (float)position.m_x / last.getX()) * 6.2831855f;
	ProfileVertex vertex;

	vertex.m_position = math::v4(Vector4(point.getImpl(), normal));
	vertex.m_texcoord = math::v4(Vector4(uv, Vector2(sce_paf_cosf(turn), sce_paf_sinf(turn))));
	return vertex;
}

}
}
