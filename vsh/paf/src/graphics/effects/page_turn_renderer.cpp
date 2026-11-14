/*
 * The page-turn renderer: a page object's page drawn turning about a fold, its front in bands
 * in front of the fold and its back behind it, with the shaders of paf/cg/drawobj/page_turn,
 * lit by its parameter sets ("PageTurnRenderer", from page_turn/psets).
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/color.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/effects/page_turn_renderer.h>
#include <graphics/surface/surface_display.h>
#include <widget/core/environment.h>
#include <src/common/frame_count.h>
#include "../../std/math.h"

using sce::Vectormath::Simd::floatInVec;
using sce::Vectormath::Simd::Aos::Quat;
using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector3;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

/* the corners a clip leaves of a polygon (names ours) */
class PageTurnPolygon {
public:
	void Add(Vector2 point)
	{
		m_points[m_count++] = point;
	}

	Vector2 m_points[10];
	int32_t m_count;
};

/* the polygon's edges clipped to the half plane in front of the line (names ours) */
static inline void ClipPolygon(PageTurnPolygon &out, const Vector2 *in, int32_t count,
			       const Vector2 &point, const Vector2 &normal)
{
	out.m_count = 0;
	if (count != 0) {
		for (int32_t i = 0; i < count; i++) {
			Vector2 a = in[i];
			Vector2 b = in[i != count - 1 ? i + 1 : 0];
			int32_t result = ClipSegment(a, b, point, normal);

			if (result == 0) {
				out.Add(b);
			} else if (result == 1) {
				out.Add(a);
				out.Add(b);
			} else if (result == 2) {
				out.Add(b);
			}
		}
	}
}

/* the polygon put in the vertices as a fan, when it has an area (names ours) */
static inline void AddFan(vector<PageTurnStrip> &strips, Vector2 *vertices, uint32_t &index,
			  uint32_t offset, const Vector2 *points, int32_t count)
{
	if (count > 2) {
		uint32_t first = index;

		for (int32_t i = 0; i < count; i++)
			vertices[index++] = points[i];

		PageTurnStrip strip;

		strip.m_first = offset + first;
		strip.m_count = index - first;
		strips.push_back(strip);
	}
}

void PageTurnMesh::Build(const PageTurnFold &fold, math::v2 curl)
{
	arl::ArlInstance *gl = arl::Gl();

	if (m_division == 0) {
		m_division = 200;
		m_vertices = arl::VertexArray(gl->m_default_allocator,
					      ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F2,
								      SCE_GXM_INDEX_SOURCE_INDEX_32BIT),
					      m_division * 2);
	}
	if ((curl.getImpl().getX() == m_curl.getImpl().getX() &&
	     curl.getImpl().getY() == m_curl.getImpl().getY()).getAsBool())
		return;

	m_offset = m_offset == 0 ? m_division : 0;

	Vector2 *vertices = static_cast<Vector2 *>(m_vertices.m_buffer->m_memory->m_addr) + m_offset;
	uint32_t index = 0;

	m_front_strips.clear();
	m_back_strips.clear();

	Vector2 origin = fold.m_origin.getImpl();
	Vector2 normal = fold.m_normal.getImpl();
	Vector2 behind = -normal;
	Vector2 near = origin;
	Vector2 far = origin + normal * 0.01f;
	PageTurnPolygon band;
	PageTurnPolygon strip;
	float i;

	/* in front of the fold: bands a hundredth wide, wider each time, up to the corner
	   furthest in front */
	for (i = 0.0f; (floatInVec(fold.m_max.m_distance) > dot(normal, near - origin)).getAsBool();
	     far = far + normal * (i * 0.01f)) {
		ClipPolygon(band, reinterpret_cast<const Vector2 *>(fold.m_quad), 4, near, normal);
		ClipPolygon(strip, band.m_points, band.m_count, far, behind);
		AddFan(m_front_strips, vertices, index, m_offset, strip.m_points, strip.m_count);
		i += 1.0f;
		near = far;
	}

	/* behind it: sixteen bands, thinner towards the fold, down to the corner furthest
	   behind */
	Vector2 bands[16];
	float depth = __builtin_fabsf(fold.m_min.m_distance);
	uint32_t k;

	for (k = 0; k < 16; k++) {
		Vector2 t = Vector2((float)k, (float)(k + 1)) * 0.0625f;

		bands[k] = Vector2(t.getX() * t.getX() * (t.getX() * -2.0f + 3.0f),
				   t.getY() * t.getY() * (t.getY() * -2.0f + 3.0f)) * depth;
	}
	for (k = 0; k < 16; k++) {
		Vector2 start = origin - normal * bands[k].getX();
		Vector2 end = origin - normal * bands[k].getY();

		ClipPolygon(band, reinterpret_cast<const Vector2 *>(fold.m_quad), 4, start, behind);
		ClipPolygon(strip, band.m_points, band.m_count, end, normal);
		AddFan(m_back_strips, vertices, index, m_offset, strip.m_points, strip.m_count);
	}
}

void PageTurnRenderer::MakeDistanceTexture(common::SharedPtr<arl::TextureObject> &texture)
{
	texture = arl::CreateTexture<ImageMode_L8>(arl::Gl()->m_default_allocator,
						   arl::SurfaceSize(64, 64), NULL, arl::SurfaceFlags(),
						   0);

	vector<uint8_t> levels;
	int32_t height = texture->m_desc.m_height;
	int32_t width = texture->m_desc.m_width;

	for (int32_t y = 0; y != height; y++) {
		for (int32_t x = 0; x != width; x++) {
			float distance = length(Vector2((float)x / (float)(width - 1),
							(float)y / (float)(height - 1)));
			float level = distance < 1.0f ? distance : 1.0f;

			if (level < 0.0f)
				level = 0.0f;
			levels.push_back((uint8_t)((level < 1.0f ? level : 1.0f) * 255.0f));
		}
	}

	arl::Texture2D *object = static_cast<arl::Texture2D *>(texture.get());

	ScePafGraphics_309D2902(object, 0, object->m_desc.m_width, object->m_desc.m_height,
				object->m_desc.m_format, object->m_desc.m_levels, levels.ptr);
	texture->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	texture->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
}

/* Sony's PageTurnRenderer constructor calls these rather than inlining them. */
template<>
inline __attribute__((noinline))
ParamSetInstance<MaterialParamSet>::ParamSetInstance(ParamSetKeysManager &manager, const string &variant)
	: m_keys(variant), m_set(true)
{
	m_keys.Add(&m_set);
	manager.Add(&m_keys);
}

template<>
inline __attribute__((noinline))
ParamSetInstance<PointLightSphereParamSet>::ParamSetInstance(ParamSetKeysManager &manager,
							    const string &variant)
	: m_keys(variant), m_set(true)
{
	m_keys.Add(&m_set);
	manager.Add(&m_keys);
}

template<>
inline __attribute__((noinline))
ParamSetInstance<TurnPageParamSet>::ParamSetInstance(ParamSetKeysManager &manager, const string &variant)
	: m_keys(variant), m_set(true)
{
	m_keys.Add(&m_set);
	manager.Add(&m_keys);
}

template<>
inline __attribute__((noinline)) WaveMesh<WaveVertex>::WaveMesh()
	: m_index_count(0), m_width(0), m_height(0), m_strip_count(0),
	  m_primitive(arl::TRIANGLE_STRIP), m_line_count(0)
{
}

/* the light's PointLightPack2, which Sony's Draw calls out of line where every other user has it
   inline (SNC stopped inlining into Draw; name ours) */
static __attribute__((noinline)) math::matrix
GetPointLightPack2(const PointLightSphereParamSet::Values &values)
{
	return values.GetPack2();
}

PageTurnRenderer::PageTurnRenderer(const string &dir)
	: m_shaders(new PageTurnShaders), m_debug_transform(false), m_unused_flag0(false),
	  m_unused_flag1(false), m_unused_flag2(false), m_wireframe(false), m_unused_flag3(false),
	  m_manager("PageTurnRenderer"), m_frame(-1), m_material(m_manager, ""),
	  m_light(m_manager, ""), m_shadow_light(m_manager, "for_shadow"),
	  m_turn_page(m_manager, ""), m_draw_material(true),
	  m_corner(arl::TextureSource::OutOfLine()), m_distance(arl::TextureSource::OutOfLine()),
	  m_background(arl::TextureSource::OutOfLine()), m_screen(NULL)
{
	m_exp_minus.Init(128, 5.0f);

	m_light.m_keys.m_default.m_values.m_distance.SetRange(0.0f, 10000.0f);
	m_light.m_set.m_values.m_distance.SetRange(0.0f, 10000.0f);
	m_shadow_light.m_keys.m_default.m_values.m_distance.SetRange(0.0f, 10000.0f);
	m_shadow_light.m_set.m_values.m_distance.SetRange(0.0f, 10000.0f);

	m_manager.SetPaths(dir, "page_turn/psets");

	MaterialParamSet &material = m_material.m_keys.m_default;

	material.findParamAndSet("color", 1.0f, 1.0f, 1.0f, 1.0f);
	material.findParamAndSet("diffuse_coeff", 0.892857f);
	material.findParamAndSet("specular_coeff", 1.066221f);
	material.findParamAndSet("specular_power", 16.742855f);
	material.findParamAndSet("fresnel_front", 0.080356f);
	material.findParamAndSet("fresnel_control", 0.801524f);
	material.findParamAndSet("fresnel_coeff", 0.287673f);
	material.findParamAndSet("refraction_coeff", 3.048937f);
	material.findParamAndSet("refraction_amount", 0.383927f);
	material.findParamAndSet("second_reflection", 0.11439f);
	m_material.m_keys.GetKey("default", true)->m_values = material.m_values;

	PointLightSphereParamSet &light = m_light.m_keys.m_default;

	light.findParamAndSet("theta_phi", 1.318346f, 0.560998f);
	light.findParamAndSet("distance", 2234.0393f);
	light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	light.findParamAndSet("fade", 1.0f);
	m_light.m_keys.GetKey("default", true)->m_values = light.m_values;

	PointLightSphereParamSet &shadow_light = m_shadow_light.m_keys.m_default;

	shadow_light.findParamAndSet("theta_phi", 1.570796f, 0.168299f);
	shadow_light.findParamAndSet("distance", 770.0803f);
	shadow_light.findParamAndSet("attn", 1.0f, 0.0f, 0.0f);
	shadow_light.findParamAndSet("ambient_color", 0.2f, 0.2f, 0.2f);
	shadow_light.findParamAndSet("diffuse_color", 1.0f, 1.0f, 1.0f);
	shadow_light.findParamAndSet("fade", 1.0f);
	m_shadow_light.m_keys.GetKey("default", true)->m_values = shadow_light.m_values;

	TurnPageParamSet &turn_page = m_turn_page.m_keys.m_default;

	turn_page.findParamAndSet("top_scale", 0.232332f);
	turn_page.findParamAndSet("top_curl", 0.607142f);
	turn_page.findParamAndSet("back_opacity", 0.080356f);
	turn_page.findParamAndSet("bottom_scale", 0.008928f);
	turn_page.findParamAndSet("shadow", 250.0f);
	turn_page.findParamAndSet("shadow_level", 0.303571f);
	turn_page.findParamAndSet("normal_scale_z", 3.303571f);
	turn_page.findParamAndSet("squeeze_z", 1.0f);
	turn_page.findParamAndSet("empty_shadow", 16.211538f);
	turn_page.findParamAndSet("empty_shadow_level", 0.482142f);
	turn_page.findParamAndSet("empty_corner_shadow", 0.301606f);
	turn_page.findParamAndSet("empty_color", 1.0f, 1.0f, 1.0f);
	turn_page.findParamAndSet("empty_shadow_color", 0.375f, 0.401785f, 0.419642f);
	turn_page.findParamAndSet("occluder_shadow_sharpness", 0.033735f);
	turn_page.findParamAndSet("occluder_shadow_level", 0.723214f);
	turn_page.findParamAndSet("occluder_shadow_color", 1.0f, 1.0f, 1.0f, 1.0f);
	m_turn_page.m_keys.GetKey("default", true)->m_values = turn_page.m_values;

	m_manager.AddVariant("default");
	m_manager.SetVariant("default");

	arl::ArlInstance *gl = arl::Gl();
	common::SharedPtr<arl::VertexArraySpec> spec =
		ScePafGraphics_4D1E289A(gl, arl::VertexArraySpec::ATTRIB0_F2,
					SCE_GXM_INDEX_SOURCE_INDEX_32BIT);

	m_shaders->m_bottom_vert.m_object.init(*gl, spec, common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_bottom_frag.m_object.init(*gl);
	m_shaders->m_top_vert.m_object.init(*gl, spec, common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_top_frag.m_object.init(*gl);
	m_shaders->m_bottom_round_vert.m_object.init(*gl, spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_bottom_round_frag.m_object.init(*gl);
	m_shaders->m_bottom_round_empty_vert.m_object.init(*gl, spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_bottom_round_empty_frag.m_object.init(*gl);
	m_shaders->m_top_round_vert.m_object.init(*gl, spec,
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>());
	m_shaders->m_top_round_frag.m_object.init(*gl);

	m_shaders->m_quad.Init(false, false);
	m_shaders->m_quad_ndc.Init(true, true);
	m_shaders->m_strips.Init(gl->m_default_allocator, 40,
				 m_shaders->m_quad_ndc.m_vertices.m_spec, "");

	arl::ProgramState *state = gl;

	m_shaders->m_quad_vert.m_object.init(*gl, state->m_patcher,
		m_shaders->m_quad.m_vertices.m_spec, common::SharedPtr<arl::VertexArraySpec>(),
		common::SharedPtr<arl::VertexArraySpec>(), common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	m_shaders->m_pure_copy_frag.m_object.init(*gl, state->m_patcher);
	state = gl;
	m_shaders->m_shadow_frag.m_object.init(*gl, state->m_patcher);

	m_corner.Load(ScePafGraphics_4E038C05()->m_ptr->m_resource_path +
			      "/page_turn/textures/page_corner.dds",
		      false);
	MakeDistanceTexture(m_distance.m_texture);
	if (m_distance.m_texture.get() != NULL) {
		m_distance.m_texture->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
		m_distance.m_texture->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	}
	if (m_distance.m_surface.get() != NULL) {
		m_distance.m_surface->SetWrapS(CLAMP_TO_EDGE);
		m_distance.m_surface->SetWrapT(CLAMP_TO_EDGE);
		m_distance.m_surface->Texturize();
	}
	if (m_background.m_texture.get() != NULL) {
		m_background.m_texture->setWrapS(arl::TEXTURE_WRAP_REPEAT);
		m_background.m_texture->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	}
	if (m_background.m_surface.get() != NULL) {
		m_background.m_surface->SetWrapS(REPEAT);
		m_background.m_surface->SetWrapT(CLAMP);
		m_background.m_surface->Texturize();
	}
}

PageTurnRenderer::~PageTurnRenderer()
{
	delete m_shaders;
	m_shaders = NULL;
	delete m_screen;
	m_screen = NULL;
}

void PageTurnRenderer::Unused0()
{
}

PageTurnProjection PageTurnFold::Project(math::v2 point) const
{
	Vector2 origin = m_origin.getImpl();
	Vector2 normal = m_normal.getImpl();
	Vector2 tangent(-normal.getY(), normal.getX());
	PageTurnProjection projection;

	projection.m_foot = origin + tangent * dot(tangent, point.getImpl() - origin) /
					 dot(tangent, tangent);
	projection.m_point = point;
	projection.m_distance = dot(m_normal.getImpl(), point.getImpl() - m_origin.getImpl());
	if (projection.m_distance < 0.0f)
		projection.m_point = projection.m_foot + (projection.m_foot - point);
	return projection;
}

PageTurnFold::PageTurnFold(PageTurnParam param, math::v2 aspect)
{
	if (!param.m_curl_changed)
		return;

	m_aspect = aspect;
	m_quad[0] = math::v2(0.0f, 0.0f) * aspect;
	m_quad[1] = math::v2(1.0f, 0.0f) * aspect;
	m_quad[2] = math::v2(1.0f, 1.0f) * aspect;
	m_quad[3] = math::v2(0.0f, 1.0f) * aspect;
	m_corner = math::v2(1.0f, 1.0f) * aspect;

	/* the corner pulled no further than just inside the page, nor further back than across
	   it */
	Vector2 curl = maxPerElem(-aspect.getImpl() * 1.99999f,
				  minPerElem(aspect.getImpl() * 0.99999f, param.m_curl.getImpl()));
	Vector2 pull = curl - m_corner.getImpl();

	m_origin = (curl + m_corner.getImpl()) / 2.0f;
	m_normal = pull / length(pull);

	m_projections[0] = Project(m_quad[0]);
	m_projections[1] = Project(m_quad[1]);
	m_projections[2] = Project(m_quad[2]);
	m_projections[3] = Project(m_quad[3]);
	m_max = m_projections[0];
	m_min = m_projections[0];
	for (int32_t i = 0; i < 4; i++) {
		float distance = m_projections[i].m_distance;

		if (distance < 0.0f && distance < m_min.m_distance)
			m_min = m_projections[i];
		if (distance > 0.0f && distance > m_max.m_distance)
			m_max = m_projections[i];
	}

	/* the fold line, long enough to cross the page, clipped to its edges */
	m_outside = false;

	Vector2 along = Vector2(-m_normal.getImpl().getY(), m_normal.getImpl().getX()) * 1000.0f;

	m_edge[0] = math::v2(m_origin.getImpl() + along);
	m_edge[1] = math::v2(m_origin.getImpl() - along);
	if (ClipSegment(m_edge[0].getImpl(), m_edge[1].getImpl(), m_quad[0].getImpl(),
			Vector2(1.0f, 0.0f)) == 3)
		m_outside = true;
	if (ClipSegment(m_edge[0].getImpl(), m_edge[1].getImpl(), m_quad[0].getImpl(),
			Vector2(0.0f, 1.0f)) == 3)
		m_outside = true;
	if (ClipSegment(m_edge[0].getImpl(), m_edge[1].getImpl(), m_quad[2].getImpl(),
			-Vector2(1.0f, 0.0f)) == 3)
		m_outside = true;
	if (ClipSegment(m_edge[0].getImpl(), m_edge[1].getImpl(), m_quad[2].getImpl(),
			-Vector2(0.0f, 1.0f)) == 3)
		m_outside = true;
}

/* the mesh's fans drawn with the programs begun (name ours) */
static inline void DrawFans(arl::ArlInstance *gl, PageTurnMesh *mesh,
			    const vector<PageTurnStrip> &strips)
{
	arl::ShaderState *state;

	for (uint32_t i = 0; i < strips.size(); i++) {
		state = gl;
		state->m_dirty_vertex |= mesh->m_vertices.m_spec->m_kinds;
		state = gl;
		state->m_dirty_fragment |= 1;
		sceGxmSetVertexStream(gl->m_context, 0, mesh->m_vertices.m_buffer->m_memory->m_addr);
		arl::VertexArray::DrawArrays(*gl, arl::TRIANGLE_FAN, strips[i].m_first,
					     strips[i].m_count);
		state = gl;
		state->m_dirty_vertex = 0;
		state = gl;
		state->m_dirty_fragment = 0;
	}
}

void PageTurnRenderer::Draw(PageTurnParam *param, PageTurnMesh *mesh)
{
	arl::ArlInstance *gl = arl::Gl();

	if (param->m_texture.get() == NULL)
		return;

	float one = 1.0f;

	if (m_screen != NULL) {
		m_screen->GetSurface()->ActivatePage(ui::Screen::s_root_surface->GetPageCounter());
		m_screen->OnTarget();
	}

	math::v2 aspect = param->m_page_size / param->m_page_size.extract_y();

	gl->CullMode(arl::CULL_NONE);
	gl->Disable(arl::BLEND);
	gl->m_model.PushTop();
	gl->m_model.LoadIdentity();
	if (m_debug_transform) {
		math::matrix world(math::quaternion(Quat(param->m_rot.getImpl())));

		world.set_w(param->m_pos);
		GetCurlPoint(*param, param->m_curl);
	}
	gl->m_model.LoadKeepInverse(GetWorld(*param));
	gl->m_model.Scale(math::v3(Vector3(
		(param->m_scale * param->m_page_size.extract_y()).getImpl().get128())));

	PageTurnFold fold(*param, aspect);

	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_LINE, arl::FRONT_AND_BACK);
	param->m_texture->Texturize();

	math::matrix mvp = gl->GetModelViewProjection();
	MaterialParamSet::Values &material = m_draw_material.m_values;

	material = m_material.m_set.m_values;

	/* the fresnel turned down as the page is turned, and the curl faded out near the end */
	float turned = (param->m_curl.extract_x() / aspect.extract_x() + param->m_curl.extract_y()) *
		       0.5f;

	if (turned > one)
		turned = one;

	float fresnel = one - (one - turned * turned) * 0.35f;

	material.m_fresnel_control.SetValue(material.m_fresnel_control.m_value * fresnel);
	material.m_fresnel_coeff.SetValue(material.m_fresnel_coeff.m_value * fresnel);

	float fade = (one - turned) / (1.0f - 0.95f);

	if (fade > one)
		fade = one;

	const TurnPageParamSet::Values &turn_page = m_turn_page.m_set.m_values;
	float top_scale = fade * turn_page.m_top_scale.m_value;
	float top_curl = fade * turn_page.m_top_curl.m_value;
	bool blend = false;

	if (!(param->m_renderer_param == math::v2(0.0f, 0.0f))) {
		blend = true;
		gl->BlendFunc(arl::SRC_ALPHA, arl::ONE_MINUS_SRC_ALPHA, arl::ONE,
			      arl::ONE_MINUS_SRC_ALPHA);
		gl->BlendEquation(arl::FUNC_ADD, arl::FUNC_ADD);
		gl->Enable(arl::BLEND);
	} else {
		gl->Disable(arl::BLEND);
	}

	/* the passes: the front, the back and, blended, the empty page under the corner */
	PageTurnVertShaderPair vert[3];
	PageTurnFragShaderPair frag[3];
	uint32_t passes = 3;

	vert[2].m_params = NULL;
	vert[2].m_object = NULL;
	frag[2].m_params = NULL;
	frag[2].m_object = NULL;
	if (blend) {
		vert[0].m_params = &m_shaders->m_bottom_round_vert.m_interface;
		vert[0].m_object = &m_shaders->m_bottom_round_vert.m_object;
		frag[0].m_params = &m_shaders->m_bottom_round_frag.m_interface;
		frag[0].m_object = &m_shaders->m_bottom_round_frag.m_object;
		vert[1].m_params = &m_shaders->m_top_round_vert.m_interface;
		vert[1].m_object = &m_shaders->m_top_round_vert.m_object;
		frag[1].m_params = &m_shaders->m_top_round_frag.m_interface;
		frag[1].m_object = &m_shaders->m_top_round_frag.m_object;
		vert[2].m_params = &m_shaders->m_bottom_round_empty_vert.m_interface;
		vert[2].m_object = &m_shaders->m_bottom_round_empty_vert.m_object;
		frag[2].m_params = &m_shaders->m_bottom_round_empty_frag.m_interface;
		frag[2].m_object = &m_shaders->m_bottom_round_empty_frag.m_object;
	} else {
		vert[0].m_params = &m_shaders->m_bottom_vert.m_interface;
		vert[0].m_object = &m_shaders->m_bottom_vert.m_object;
		frag[0].m_params = &m_shaders->m_bottom_frag.m_interface;
		frag[0].m_object = &m_shaders->m_bottom_frag.m_object;
		vert[1].m_params = &m_shaders->m_top_vert.m_interface;
		vert[1].m_object = &m_shaders->m_top_vert.m_object;
		frag[1].m_params = &m_shaders->m_top_frag.m_interface;
		frag[1].m_object = &m_shaders->m_top_frag.m_object;
	}

	uint32_t i;

	for (i = 0; i != (blend ? passes : 2); i++) {
		PageTurnVertShaderParams *vp = vert[i].m_params;
		PageTurnFragShaderParams *fp = frag[i].m_params;

		vp->SetMVP(mvp);
		vp->SetModelMatrix(gl->m_model.Top().m_matrix);

		math::v4 plane(Vector4(fold.m_origin.getImpl(), fold.m_normal.getImpl()));
		math::v4 shape(turn_page.m_bottom_scale.m_value, top_curl, top_scale, 0.0f);
		math::v4 deepest(Vector4(Vector2(fold.m_min.m_distance, fold.m_max.m_distance),
					 fold.m_corner.getImpl()));
		Vector2 recip_aspect = recipPerElem(aspect.getImpl());

		fp->SetGrep(param->m_curl);
		fp->SetFoldPlane(plane);
		fp->SetParams(shape);
		fp->SetDeepest(deepest);
		fp->SetAspect(math::v4(Vector4(aspect.getImpl(), recip_aspect)));
		fp->SetNormalScaleZ(turn_page.m_normal_scale_z.m_value);
		fp->SetMaterialPack(material.GetPack());
		fp->SetPointLightPack1(m_light.m_set.m_values.GetPack1());
		fp->SetPointLightPack2(GetPointLightPack2(m_light.m_set.m_values));
		fp->SetEye(gl->m_view.GetInverse().w());

		vp->SetGrep(param->m_curl);
		vp->SetFoldPlane(plane);
		vp->SetParams(shape);
		vp->SetDeepest(deepest);
		vp->SetAspect(math::v4(Vector4(aspect.getImpl(), recip_aspect)));
		vp->SetNormalScaleZ(turn_page.m_normal_scale_z.m_value);
		vp->SetMaterialPack(material.GetPack());
		vp->SetPointLightPack1(m_light.m_set.m_values.GetPack1());
		vp->SetPointLightPack2(GetPointLightPack2(m_light.m_set.m_values));
		vp->SetEye(gl->m_view.GetInverse().w());

		/* the corner: the part of the page turned, and the texture refracted under it */
		if (blend) {
			Vector2 corner = mulPerElem(param->m_renderer_param.getImpl(), recip_aspect);

			fp->SetCornerSize(math::v4(Vector4(Vector2(1.0f) - corner, recipPerElem(corner))));
		} else {
			fp->SetCornerSize(math::v4(1.0f, 1.0f, 0.0f, 0.0f));
		}
		fp->SetCornerTex(m_corner);

		Vector2 size((float)param->m_texture->GetWidth(), (float)param->m_texture->GetHeight());

		fp->SetCornerRefract(math::v2(mulPerElem(recipPerElem(size), Vector2(1.5f))));
		fp->SetEmptyShadow(one - turn_page.m_empty_corner_shadow.m_value);
		fp->SetEmptyShadowColor(math::v4(Vector4(
			Vector3(s_color_white.getImpl().get128()) -
				turn_page.m_empty_shadow_color.m_value.getImpl(),
			1.0f)));
		fp->SetEmptyColor(math::v4(turn_page.m_empty_color.m_value, param->m_alpha));
		if (m_background.m_surface.get() != NULL) {
			/* worked out and not used */
			math::v2 screen(960.0f, 544.0f);

			screen /= screen.getImpl().getY();

			math::v2 background((float)m_background.m_surface->GetWidth(),
					    (float)m_background.m_surface->GetHeight());

			background /= background.getImpl().getY();
			fp->SetEmptyBackground(arl::TextureSource(m_background.m_surface));
			fp->SetEmptyBackgroundUVFactor(m_background.m_surface->GetFactor());
		} else {
			arl::DefaultTextures *textures = gl;

			fp->SetEmptyBackground(arl::TextureSource(textures->m_white));
			fp->SetEmptyBackgroundUVFactor(math::v2(1.0f, 1.0f));
		}

		vp->SetSqueezeZ(math::v4(1.0f, 1.0f, turn_page.m_squeeze_z.m_value, 1.0f));
		vp->SetExpMinusTexture(arl::TextureSource(m_exp_minus.m_texture));
		vp->SetExpMinusRange(math::v2(m_exp_minus.m_range, one / m_exp_minus.m_range));

		arl::TextureSource texture;

		texture.m_surface = param->m_texture;
		fp->SetTex(texture);

		/* the region of the texture drawn, less the margins round the page */
		Vector2 factor = param->m_texture->GetFactor().getImpl();
		Vector2 low = mulPerElem(param->m_region.getImpl().getXY(), factor);
		Vector2 high = mulPerElem(param->m_region.getImpl().getXY() +
					  Vector2(vget_high_f32(param->m_region.getImpl().get128())), factor);
		math::v4 uv(Vector4(high - low, low));

		if (!(param->m_margin == math::v2(0.0f, 0.0f))) {
			Vector2 page = param->m_page_size.getImpl();
			Vector2 scale = recipPerElem(page + mulPerElem(param->m_margin.getImpl(),
								      Vector2(2.0f)));
			Vector2 middle = mulPerElem(high + low, Vector2(0.5f));
			Vector2 half = mulPerElem(mulPerElem(high - low, mulPerElem(page, scale)),
						  Vector2(0.5f));
			Vector2 start = middle - half;

			uv = math::v4(Vector4(middle + half - start, start));
		}
		vp->SetUVTransform(uv);
		vp->SetShadow(math::v4(turn_page.m_shadow.m_value, turn_page.m_shadow_level.m_value,
				       turn_page.m_empty_shadow.m_value,
				       turn_page.m_empty_shadow_level.m_value));
		fp->SetBackSideOpacity(turn_page.m_back_opacity.m_value);
	}

	mesh->Build(fold, param->m_curl);
	if (mesh->m_front_strips.size() != 0) {
		arl::VertShaderObject *vo = vert[0].m_object;
		arl::FragShaderObject *fo = frag[0].m_object;

		vo->begin();
		fo->begin();
		DrawFans(gl, mesh, mesh->m_front_strips);
		fo->end();
		vo->end();
	}
	if (mesh->m_back_strips.size() != 0) {
		for (i = 1; i < (blend ? passes : 2); i++) {
			arl::VertShaderObject *vo = vert[i].m_object;
			arl::FragShaderObject *fo = frag[i].m_object;

			vo->begin();
			fo->begin();
			DrawFans(gl, mesh, mesh->m_back_strips);
			fo->end();
			vo->end();
		}
	}
	if (m_wireframe)
		gl->PolygonMode(arl::POLYGON_MODE_FILL, arl::FRONT_AND_BACK);
	gl->m_model.Pop();
	if (m_screen != NULL)
		m_screen->OnFinish();
}

/* a shader's interface, its setters called through its vtable (name ours) */
template<class Interface, class Object>
static inline Interface *FragParams(arl::ShaderInstance<Interface, Object> &shader)
{
	return &shader.m_interface;
}

void PageTurnRenderer::ReleaseTextures()
{
	FragParams(m_shaders->m_top_frag)->SetTex(arl::TextureSource());
	FragParams(m_shaders->m_bottom_frag)->SetTex(arl::TextureSource());
	FragParams(m_shaders->m_top_round_frag)->SetTex(arl::TextureSource());
	FragParams(m_shaders->m_bottom_round_frag)->SetTex(arl::TextureSource());
	FragParams(m_shaders->m_top_round_frag)->SetEmptyBackground(arl::TextureSource());
	FragParams(m_shaders->m_bottom_round_frag)->SetEmptyBackground(arl::TextureSource());
	FragParams(m_shaders->m_bottom_round_empty_frag)->SetEmptyBackground(arl::TextureSource());
}

void PageTurnRenderer::Update()
{
	arl::ArlInstance *gl = arl::Gl();

	m_manager.Update(gl->m_frame_time);
	m_frame = common::FrameCount();
}

void PageTurnRenderer::Unused1()
{
}

}
}
