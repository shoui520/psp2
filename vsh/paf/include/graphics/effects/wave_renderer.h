/*
 * What the wave renderer draws with: its shaders' interfaces (paf/cg/drawobj/wave/*.vp and .fp,
 * their parameters Sony's names), the grid its waves and sky are drawn as, the sky dome, and a
 * wave instance: one FFT surface drawn over a grid with its own parameters. The shaders' file
 * names and parameters are Sony's; the classes', the members' and this file's names are ours.
 */
#ifndef PAF_GRAPHICS_EFFECTS_WAVE_RENDERER_H
#define PAF_GRAPHICS_EFFECTS_WAVE_RENDERER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include "../../../src/graphics/arl/gxm/gxm_framebuffer_object.h"
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/effects/fft_wave_surface.h>
#include <graphics/effects/param.h>
#include <graphics/effects/post_effects.h>
#include <graphics/effects/wave_param.h>
#include <std/vector>
#include <math/ngp/matrix.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>

namespace paf {

/* the wave meshes' index lists: Sony's meshes destroy them inline */
template<>
inline vector<uint16_t, allocator<uint16_t> >::~vector()
{
	if (ptr != NULL) {
		for (size_type i = count; i != 0; --i)
			ptr[i - 1].~uint16_t();
		alloc.deallocate(ptr, limit);
	}
	ptr = (uint16_t *)-1;
	count = -1;
	limit = -1;
}

template<>
inline vector<uint32_t, allocator<uint32_t> >::~vector()
{
	if (ptr != NULL) {
		for (size_type i = count; i != 0; --i)
			ptr[i - 1].~uint32_t();
		alloc.deallocate(ptr, limit);
	}
	ptr = (uint32_t *)-1;
	count = -1;
	limit = -1;
}

namespace graph {

/* a shader interface's name, binary and its size, as each generated interface has them */
#define WAVE_SHADER_BINARY(path, size)						\
	virtual const char *GetName()							\
	{										\
		return path;								\
	}										\
											\
	virtual const uint8_t *GetBinary()						\
	{										\
		return s_binary;							\
	}										\
											\
	virtual uint32_t GetBinarySize()						\
	{										\
		return sizeof(s_binary);						\
	}										\
											\
	/* built by psp2cgc */								\
	static const uint8_t s_binary[size]

/* the waves' vertices, moved by the heights in the position texture */
class WaveVertShader : public arl::ShaderInterfaceBase {
public:
	WaveVertShader()
		: m_vp(m_setup), m_model_matrix(m_setup), m_pos_tex(m_setup), m_half_pixel(m_setup),
		  m_world_scale_hack(m_setup)
	{
	}

	virtual void Init()
	{
		m_vp.init("VP", arl::ShaderParameterBase::UNIFORM, 0);
		m_model_matrix.init("ModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
		m_pos_tex.init("PosTex", 6, 0);
		m_half_pixel.init("HalfPixel", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_vp.Apply();
		m_model_matrix.Apply();
		m_pos_tex.Apply();
		m_half_pixel.Apply();
		m_world_scale_hack.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave.vp", 0x238);

	arl::ShaderParameter<math::matrix> m_vp;
	arl::ShaderParameter<math::matrix> m_model_matrix;
	arl::ShaderParameter<arl::TextureSource> m_pos_tex;
	arl::ShaderParameter<math::v2> m_half_pixel;
	arl::ShaderParameter<float> m_world_scale_hack;
};

/* the waves' colour, from the lit texture */
class WaveFragShader : public arl::ShaderInterfaceBase {
public:
	WaveFragShader() : m_lit_tex(m_setup)
	{
	}

	virtual void Init()
	{
		m_lit_tex.init("LitTex", 1, 0);
	}

	virtual void Apply()
	{
		m_lit_tex.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave.fp", 0x104);

	arl::ShaderParameter<arl::TextureSource> m_lit_tex;
};

/* the waves' positions drawn into a texture */
class WavePosVertShader : public arl::ShaderInterfaceBase {
public:
	WavePosVertShader() : m_mvp(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave_pos.vp", 0x134);

	arl::ShaderParameter<math::matrix> m_mvp;
};

/* the positions from the height map, extruded along the cross section */
class WavePosFragShader : public arl::ShaderInterfaceBase {
public:
	WavePosFragShader()
		: m_height_map(m_setup), m_distortion_scale(m_setup),
		  m_distortion_uv_scale_rotate(m_setup), m_cross_section(m_setup),
		  m_cross_section_scale(m_setup), m_vm(m_setup), m_persp_hack(m_setup)
	{
	}

	virtual void Init()
	{
		m_height_map.init("HeightMap", 7, 0);
		m_distortion_scale.init("DistortionScale", arl::ShaderParameterBase::UNIFORM, 0);
		m_distortion_uv_scale_rotate.init("DistortionUVScaleRotate",
						  arl::ShaderParameterBase::UNIFORM, 0);
		m_cross_section.init("extrude_args.CrossSection", arl::ShaderParameterBase::UNIFORM,
				     0);
		m_cross_section_scale.init("extrude_args.CrossSectionScale",
					   arl::ShaderParameterBase::UNIFORM, 0);
		m_vm.init("extrude_args.VM", arl::ShaderParameterBase::UNIFORM, 0);
		m_persp_hack.init("extrude_args.PerspHack", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_height_map.Apply();
		m_distortion_scale.Apply();
		m_distortion_uv_scale_rotate.Apply();
		m_cross_section.Apply();
		m_cross_section_scale.Apply();
		m_vm.Apply();
		m_persp_hack.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave_pos.fp", 0x4b0);

	arl::ShaderParameter<arl::TextureSource> m_height_map;
	arl::ShaderParameter<float> m_distortion_scale;
	arl::ShaderParameter<math::v2> m_distortion_uv_scale_rotate;
	arl::ShaderParameter<math::v4> m_cross_section;
	arl::ShaderParameter<math::v3> m_cross_section_scale;
	arl::ShaderParameter<math::matrix> m_vm;
	arl::ShaderParameter<math::v4> m_persp_hack;
};

/* the waves lit, from their position and normal textures */
class WaveLitTexFragShader : public arl::ShaderInterfaceBase {
public:
	WaveLitTexFragShader()
		: m_model_matrix(m_setup), m_pos_tex(m_setup), m_nor_tex(m_setup),
		  m_half_pixel(m_setup), m_eye_pos(m_setup), m_material_pack(m_setup),
		  m_point_light_pack2(m_setup), m_point_light_pack1(m_setup), m_edge_color(m_setup),
		  m_center_color(m_setup), m_fog_pack(m_setup), m_edge_fade(m_setup)
	{
	}

	virtual void Init()
	{
		m_model_matrix.init("ModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
		m_pos_tex.init("PosTex", 3, 0);
		m_nor_tex.init("NorTex", 4, 0);
		m_half_pixel.init("HalfPixel", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_material_pack.init("MaterialPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack2.init("PointLightPack2", arl::ShaderParameterBase::UNIFORM, 0);
		m_point_light_pack1.init("PointLightPack1", arl::ShaderParameterBase::UNIFORM, 0);
		m_edge_color.init("EdgeColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_center_color.init("CenterColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_fog_pack.init("FogPack", arl::ShaderParameterBase::UNIFORM, 0);
		m_edge_fade.init("EdgeFade", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_model_matrix.Apply();
		m_pos_tex.Apply();
		m_nor_tex.Apply();
		m_half_pixel.Apply();
		m_eye_pos.Apply();
		m_material_pack.Apply();
		m_point_light_pack2.Apply();
		m_point_light_pack1.Apply();
		m_edge_color.Apply();
		m_center_color.Apply();
		m_fog_pack.Apply();
		m_edge_fade.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/wave_lit_tex.fp", 0x58c);

	arl::ShaderParameter<math::matrix> m_model_matrix;
	arl::ShaderParameter<arl::TextureSource> m_pos_tex;
	arl::ShaderParameter<arl::TextureSource> m_nor_tex;
	arl::ShaderParameter<math::v2> m_half_pixel;
	arl::ShaderParameter<math::v3> m_eye_pos;
	arl::ShaderParameter<math::matrix> m_material_pack;
	arl::ShaderParameter<math::matrix> m_point_light_pack2;
	arl::ShaderParameter<math::matrix> m_point_light_pack1;
	arl::ShaderParameter<math::v4> m_edge_color;
	arl::ShaderParameter<math::v4> m_center_color;
	arl::ShaderParameter<math::matrix> m_fog_pack;
	arl::ShaderParameter<math::v2> m_edge_fade;
};

/* the sky dome's vertices and its colours, from the sun and the waves' normals */
class SkydomeVertShader : public arl::ShaderInterfaceBase {
public:
	SkydomeVertShader()
		: m_vp(m_setup), m_m(m_setup), m_world_scale_hack(m_setup), m_horizon_angle(m_setup),
		  m_eye_pos(m_setup), m_sun_pos(m_setup), m_wave_fog_color(m_setup),
		  m_sun_color(m_setup), m_sun_power(m_setup), m_sun_control(m_setup),
		  m_zenith_color(m_setup), m_horizon_color(m_setup), m_color_blend_range(m_setup),
		  m_wave_nor_tex(m_setup), m_distortion_scale(m_setup), m_wave_model_matrix(m_setup)
	{
	}

	virtual void Init()
	{
		m_vp.init("VP", arl::ShaderParameterBase::UNIFORM, 0);
		m_m.init("M", arl::ShaderParameterBase::UNIFORM, 0);
		m_world_scale_hack.init("WorldScaleHack", arl::ShaderParameterBase::UNIFORM, 0);
		m_horizon_angle.init("HorizonAngle", arl::ShaderParameterBase::UNIFORM, 0);
		m_eye_pos.init("EyePos", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_pos.init("SunPos", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_fog_color.init("WaveFogColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_color.init("SunColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_power.init("SunPower", arl::ShaderParameterBase::UNIFORM, 0);
		m_sun_control.init("SunControl", arl::ShaderParameterBase::UNIFORM, 0);
		m_zenith_color.init("ZenithColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_horizon_color.init("HorizonColor", arl::ShaderParameterBase::UNIFORM, 0);
		m_color_blend_range.init("ColorBlendRange", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_nor_tex.init("WaveNorTex", 15, 0);
		m_distortion_scale.init("DistortionScale", arl::ShaderParameterBase::UNIFORM, 0);
		m_wave_model_matrix.init("WaveModelMatrix", arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_vp.Apply();
		m_m.Apply();
		m_world_scale_hack.Apply();
		m_horizon_angle.Apply();
		m_eye_pos.Apply();
		m_sun_pos.Apply();
		m_wave_fog_color.Apply();
		m_sun_color.Apply();
		m_sun_power.Apply();
		m_sun_control.Apply();
		m_zenith_color.Apply();
		m_horizon_color.Apply();
		m_color_blend_range.Apply();
		m_wave_nor_tex.Apply();
		m_distortion_scale.Apply();
		m_wave_model_matrix.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/skydome.vp", 0x70c);

	arl::ShaderParameter<math::matrix> m_vp;
	arl::ShaderParameter<math::matrix> m_m;
	arl::ShaderParameter<float> m_world_scale_hack;
	arl::ShaderParameter<math::v4> m_horizon_angle;
	arl::ShaderParameter<math::v3> m_eye_pos;
	arl::ShaderParameter<math::v3> m_sun_pos;
	arl::ShaderParameter<math::v4> m_wave_fog_color;
	arl::ShaderParameter<math::v3> m_sun_color;
	arl::ShaderParameter<float> m_sun_power;
	arl::ShaderParameter<float> m_sun_control;
	arl::ShaderParameter<math::v3> m_zenith_color;
	arl::ShaderParameter<math::v3> m_horizon_color;
	arl::ShaderParameter<math::v2> m_color_blend_range;
	arl::ShaderParameter<arl::TextureSource> m_wave_nor_tex;
	arl::ShaderParameter<math::v3> m_distortion_scale;
	arl::ShaderParameter<math::matrix> m_wave_model_matrix;
};

/* the sky dome's fragments: the colour its vertices give */
class SkydomeFragShader : public arl::ShaderInterfaceBase {
public:
	virtual void Init()
	{
	}

	virtual void Apply()
	{
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/wave/skydome.fp", 0xcc);
};

/* a vertex of the waves' grid: where it is and its normal (names ours); 0x20 bytes */
class WaveVertex {
public:
	math::v4 m_position;
	math::v4 m_normal;
};

class SkydomeVertex;
class Camera;

/* what makes each vertex of a grid from where it is in the grid, 0 to 1 across (names ours) */
template<class Vertex>
class VertexGenerator {
public:
	virtual Vertex operator()(const math::v2 &uv) const = 0;

	virtual ~VertexGenerator()
	{
	}
};

/*
 * The indices of a grid of size vertices as a triangle strip, two rows of it at a time, the
 * rows joined by degenerate triangles when join is set; count counts them, and indices (when
 * not NULL) is given them. What is returned is how many indices the first two rows take
 * (names ours).
 */
template<class T>
uint32_t MakeGridStrip(vector<T> *indices, arl::SurfaceSize size, bool join, uint32_t *count);

/*
 * A grid of width by height vertices, drawn as one triangle strip with an index array of 16-bit
 * indices, or 32-bit ones past 65536 vertices (names ours); 0x40 bytes.
 */
template<class Vertex>
class WaveMesh {
public:
	WaveMesh() : m_index_count(0), m_width(0), m_height(0), m_strip_count(0),
		     m_primitive(arl::TRIANGLE_STRIP), m_line_count(0)
	{
	}

	/* the grid's vertices in a buffer of the allocator laid out by the spec (0x81202718 for
	   the waves' own) */
	__attribute__((noinline))
	WaveMesh(arl::SurfaceSize size, common::SharedPtr<arl::VertexArraySpec> spec,
		 common::SharedPtr<arl::VramAllocator> allocator);

	/* every vertex made by the generator (0x812039e0 for the waves' own) */
	__attribute__((noinline)) void Fill(const VertexGenerator<Vertex> &generator);

	/* the strip's first rows, all of it when rows is the last one (0x81203068 for the waves'
	   own) */
	__attribute__((noinline)) void Draw(arl::ArlInstance *gl, uint32_t rows);

	/* the grid's lines, their indices made the first time (0x81203404 for the waves' own) */
	__attribute__((noinline)) void DrawLines(arl::ArlInstance *gl);

	uint32_t m_index_count;
	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_strip_count;
	arl::IndexArray m_lines;
	uint32_t m_line_count;
	arl::Primitive m_primitive;
	arl::VertexArray m_vertices;
	arl::IndexArray m_indices;
};

/* a vertex at each index, and the count counted */
template<class T>
inline void AddGridIndex(vector<T> *indices, uint32_t *count, T index)
{
	if (indices != NULL)
		indices->push_back(index);
	(*count)++;
}

template<class T>
uint32_t MakeGridStrip(vector<T> *indices, arl::SurfaceSize size, bool join, uint32_t *count)
{
	uint32_t first_count = 0;
	T width = size.m_width;
	T height = size.m_height;
	T row = 0;
	T next_row = width;

	*count = 0;
	if (indices != NULL)
		indices->clear();
	for (T y = 0; y < height - 1; y++) {
		for (T x = 0; x < width; x++, row++, next_row++) {
			AddGridIndex<T>(indices, count, next_row);
			AddGridIndex<T>(indices, count, row);
		}
		if (join && y < height - 2) {
			AddGridIndex<T>(indices, count, row - 1);
			AddGridIndex<T>(indices, count, next_row);
			AddGridIndex<T>(indices, count, next_row);
			AddGridIndex<T>(indices, count, next_row);
		}
		if (first_count == 0 && indices != NULL)
			first_count = indices->size();
	}
	return first_count;
}

template<class Vertex>
WaveMesh<Vertex>::WaveMesh(arl::SurfaceSize size, common::SharedPtr<arl::VertexArraySpec> spec,
			   common::SharedPtr<arl::VramAllocator> allocator)
	: m_index_count(0), m_width(size.m_width), m_height(size.m_height), m_strip_count(0),
	  m_primitive(arl::TRIANGLE_STRIP), m_line_count(0)
{
	m_vertices = arl::VertexArray(spec, common::SharedPtr<arl::VertexBuffer>(
						    new arl::VertexBuffer(allocator)));

	arl::SurfaceMemory *memory = m_vertices.m_buffer->m_memory.get();
	uint32_t vertex_size = spec->m_stride * m_width * m_height;

	if (memory->m_addr == NULL) {
		memory->m_allocator->Alloc(memory, vertex_size);
		memory->m_size = vertex_size;
	}

	uint32_t count = 0;

	MakeGridStrip<uint32_t>(NULL, size, true, &count);
	if (size.m_width * size.m_height > 0x10000) {
		vector<uint32_t> indices;

		m_strip_count = MakeGridStrip<uint32_t>(&indices, size, true, &count);
		m_indices = arl::IndexArray(allocator, &indices[0], indices.size());
		m_index_count = indices.size();
	} else {
		vector<uint16_t> indices;

		m_strip_count = MakeGridStrip<uint16_t>(&indices, size, true, &count);
		m_indices = arl::IndexArray(allocator, &indices[0], indices.size());
		m_index_count = indices.size();
	}
}

/* the grid's rows and columns as lines, a pair of indices each (names ours) */
template<class T>
void MakeGridLines(vector<T> *lines, arl::SurfaceSize size)
{
	T width = size.m_width;
	T height = size.m_height;

	lines->clear();
	for (T y = 0; y < height; y++) {
		for (T x = 0; x < width - 1; x++) {
			lines->push_back(y * width + x);
			lines->push_back(y * width + x + 1);
		}
	}
	for (T x = 0; x < width; x++) {
		for (T y = 0; y < height - 1; y++) {
			lines->push_back(y * width + x);
			lines->push_back((y + 1) * width + x);
		}
	}
}

template<class Vertex>
void WaveMesh<Vertex>::Draw(arl::ArlInstance *gl, uint32_t rows)
{
	arl::ShaderState *state;

	if (m_index_count == 0)
		return;

	state = gl;
	state->m_dirty_vertex |= m_vertices.m_spec->m_kinds;
	state = gl;
	state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl->m_context, 0, m_vertices.m_buffer->m_memory->m_addr);
	if (m_index_count != 0) {
		if (rows == m_height - 1)
			m_indices.draw(*gl, m_primitive, m_index_count);
		else
			m_indices.draw(*gl, m_primitive, m_strip_count * rows);
	}
	if (m_index_count != 0) {
		state = gl;
		state->m_dirty_vertex = 0;
		state = gl;
		state->m_dirty_fragment = 0;
	}
}

template<class Vertex>
void WaveMesh<Vertex>::DrawLines(arl::ArlInstance *gl)
{
	arl::ShaderState *state;

	if (m_index_count == 0)
		return;

	state = gl;
	state->m_dirty_vertex |= m_vertices.m_spec->m_kinds;
	state = gl;
	state->m_dirty_fragment |= 1;
	sceGxmSetVertexStream(gl->m_context, 0, m_vertices.m_buffer->m_memory->m_addr);
	if (m_line_count == 0) {
		switch (m_indices.m_format) {
		case SCE_GXM_INDEX_FORMAT_U16: {
			vector<uint16_t> lines;

			MakeGridLines<uint16_t>(&lines, arl::SurfaceSize(m_width, m_height));
			m_lines = arl::IndexArray(gl->m_default_allocator, &lines[0], lines.size(),
						  m_indices.m_format);
			m_line_count = lines.size();
			break;
		}
		case SCE_GXM_INDEX_FORMAT_U32: {
			vector<uint32_t> lines;

			MakeGridLines<uint32_t>(&lines, arl::SurfaceSize(m_width, m_height));
			m_lines = arl::IndexArray(gl->m_default_allocator, &lines[0], lines.size(),
						  m_indices.m_format);
			m_line_count = lines.size();
			break;
		}
		}
	}
	m_lines.draw(*gl, arl::LINES, m_line_count);
	if (m_index_count != 0) {
		state = gl;
		state->m_dirty_vertex = 0;
		state = gl;
		state->m_dirty_fragment = 0;
	}
}

template<class Vertex>
void WaveMesh<Vertex>::Fill(const VertexGenerator<Vertex> &generator)
{
	if (m_index_count == 0)
		return;

	Vertex *vertex = (Vertex *)m_vertices.m_buffer->m_memory->m_addr;
	sce::Vectormath::Simd::Aos::Vector2 scale =
		recipPerElem(sce::Vectormath::Simd::Aos::Vector2((float)(int32_t)(m_width - 1),
								 (float)(int32_t)(m_height - 1)));

	for (int32_t y = 0; y < m_height; y++) {
		for (int32_t x = 0; x < m_width; x++) {
			math::v2 uv(mulPerElem(sce::Vectormath::Simd::Aos::Vector2((float)x, (float)y),
					       scale));

			*vertex++ = generator(uv);
		}
	}
}

/* the sky over the waves (names ours); 0x438 bytes */
class Skydome {
public:
	/* 0x811a6924 */
	Skydome();

	/* 0x811a6a6a: the dome's grid and shaders made (name ours) */
	void Init(int32_t width, int32_t height);
	/* 0x811a7276: the camera the sky is drawn from (name ours) */
	void SetCamera(common::SharedPtr<Camera> camera);

	/* what the waves give the dome's shaders, named by the uniforms they set */
	class DrawParam {
	public:
		float m_world_scale_hack;
		arl::TextureSource m_wave_nor_tex;
		math::matrix m_wave_model_matrix;
		math::v4 m_wave_fog_color;
		float m_distortion_scale;
	};

	/* 0x811a7306: the dome drawn with the sky's values (name ours) */
	void Draw(const DrawParam &param, const SkyParamSet::Values &values);

	WaveMesh<SkydomeVertex> m_mesh;
	arl::ShaderInstance<SkydomeVertShader, arl::VertShaderObject> m_vert;
	arl::ShaderInstance<SkydomeFragShader, arl::FragShaderObject> m_frag;
	bool m_dirty;
	common::SharedPtr<arl::Texture2D> m_texture;
	common::SharedPtr<Camera> m_camera;
	bool m_loaded;
};

class WaveRenderer;

/*
 * One surface of waves: its FFT heights, the grid they move, the textures it draws its
 * positions and normals into, and its own parameters (names ours); 0x2a8 bytes.
 */
class WaveInstance : public ParamLink {
public:
	/* the cross section's key, between the wavy and the flat one, and its curve's
	   coefficients (names ours) */
	class Profile {
	public:
		math::v4 m_key;
		math::v4 m_coefficients;
	};

	/* 0x810a0ba6: the renderer's index-th, an FFT surface of size, over a grid of resolution
	   by resolution */
	WaveInstance(int32_t index, int32_t size, int32_t resolution);
	/* 0x810a18c8 */
	~WaveInstance();

	/* the cross section's Hermite curve, from the keys of its ends and slopes (0x81203abc) */
	__attribute__((noinline)) Profile GetProfile();

	/* the instance's translation and rotation (0x81203e40, name ours) */
	__attribute__((noinline)) math::matrix GetModelMatrix();

	/* 0x810a1d38: the surface's parameters and the instance's values; the surface starts
	   again when its spectrum changes */
	void Set(const FFTWaveParam &param, const WaveInstanceParamSet::Values &values);
	/* 0x810a2068: the vertex of the grid at uv, bent along the cross section */
	WaveVertex MakeVertex(const math::v2 &uv);
	/* 0x810a08b0: the point of the cross section at t, 0 to 1 across, scaled */
	math::v2 GetProfilePoint(float t);
	/* 0x810a0948: the point and its normal */
	void GetProfilePoint(float t, math::v2 &point, math::v2 &normal);

	WaveMesh<WaveVertex> m_mesh;
	FFTWaveSurface m_surface;
	/* the first takes the renderer's spectrum (name ours) */
	int32_t m_index;
	common::SharedPtr<arl::Texture2D> m_textures[4];
	WaveInstanceParamSet::Values m_values;
	float m_last_uv_rotate;
	float m_uv_rotate_damp;
	/* how flat the cross section is, 0 to 1 */
	float m_flatness;
	/* how far the camera looks down, the renderer's m_wave_y (name ours) */
	float m_look_down;
};

inline __attribute__((noinline)) WaveInstance::Profile WaveInstance::GetProfile()
{
	Profile profile;
	sce::Vectormath::Simd::Aos::Vector4 wavy = m_values.m_cross_section.m_value.getImpl();
	sce::Vectormath::Simd::Aos::Vector4 key =
		wavy + (m_values.m_cross_section_flat.m_value.getImpl() - wavy) * m_flatness;

	profile.m_key = key;
	profile.m_coefficients = math::v4(
		dot(key, sce::Vectormath::Simd::Aos::Vector4(2.0f, -2.0f, 1.0f, 1.0f)),
		dot(key, sce::Vectormath::Simd::Aos::Vector4(-3.0f, 3.0f, -2.0f, -1.0f)),
		key.getZ(), key.getX());
	return profile;
}

inline __attribute__((noinline)) math::matrix WaveInstance::GetModelMatrix()
{
	return math::matrix(sce::Vectormath::Simd::Aos::Matrix4::translation(
		       m_values.m_translation.m_value.getImpl())) *
	       m_values.m_rotation.m_value.toMatrix(0.0001f);
}

class Background;
class DrawEnv;
class DrawLayer;
class MotionListener;

/* "viwa", a WavePreset's first word */
#define WAVE_PRESET_MAGIC 0x61776976

/*
 * The values of every set of a variant, as other modules hand them in: a header, then the
 * sets' values, vectors first, each vector in 16 bytes (names ours, the members' from the
 * parameters they set); 0x2a0 bytes.
 */
class WavePreset {
public:
	class __attribute__((aligned(16))) Material {
	public:
		math::v4 m_color;
		float m_diffuse_coeff;
		float m_specular_coeff;
		float m_specular_power;
		float m_fresnel_front;
		float m_fresnel_control;
		float m_fresnel_coeff;
		float m_refraction_coeff;
		float m_refraction_amount;
		float m_second_reflection;
	};

	class __attribute__((aligned(16))) PointLightSphere {
	public:
		math::v2 m_theta_phi;
		float m_padding[2];
		math::v3 m_attn;
		math::v3 m_ambient_color;
		math::v3 m_diffuse_color;
		float m_distance;
		float m_fade;
	};

	class __attribute__((aligned(16))) Fog {
	public:
		math::v3 m_color;
		math::v4 m_density;
		math::v4 m_light_color;
		float m_d_scale;
		float m_d_offset;
	};

	class __attribute__((aligned(16))) Sky {
	public:
		math::v2 m_theta_phi;
		float m_padding[2];
		math::v3 m_sun_color;
		math::v3 m_zenith_color;
		math::v3 m_horizon_color;
		float m_distance;
		float m_sun_power;
		float m_sun_control;
		float m_wave_fog_alpha;
		float m_radius;
		float m_xscale;
		float m_side_angle;
		float m_horizon_angle;
		float m_horizon_blend_range;
		float m_horizon_curvature;
		float m_sky_blend_start;
		float m_sky_blend_range;
		float m_pos_distortion_scale;
		float m_grad_distortion_scale;
		float m_horizon_distortion_scale;
	};

	class __attribute__((aligned(16))) FFTWave {
	public:
		float m_gravity;
		float m_a;
		float m_log_a;
		float m_patch_size;
		float m_wind_speed;
		float m_wind_dir;
		float m_time_step;
	};

	class __attribute__((aligned(16))) WaveInstance {
	public:
		math::v3 m_translation;
		math::quaternion m_rotation;
		math::v3 m_scale;
		math::v3 m_center_color;
		math::v3 m_edge_color;
		math::v4 m_cross_section;
		math::v4 m_cross_section_flat;
		float m_distortion_scale;
		float m_global_scale;
		float m_uv_scale;
		float m_uv_rotate;
		float m_center_power;
		float m_edge_power;
		float m_decay;
		float m_shadow_nor_blend;
	};

	class __attribute__((aligned(16))) WaveRenderer {
	public:
		math::v2 m_edge_fog;
		float m_padding[2];
		float m_pitch_max;
		float m_pitch_rest;
		float m_pitch_min;
		float m_pitch_debug;
		float m_roll_max;
		float m_roll_max_zone;
		float m_zoom;
		float m_camera_chase;
	};

	/* WAVE_PRESET_MAGIC */
	uint32_t m_magic;
	/* 1 */
	uint32_t m_version;
	/* the variant's index in the exporter's table */
	uint32_t m_index;
	/* 0 */
	uint32_t m_reserved;
	uint32_t m_reserved1[8];
	Material m_material;
	PointLightSphere m_point_light;
	Fog m_fog;
	Fog m_fog_lookdown;
	Sky m_sky;
	FFTWave m_fft_wave;
	WaveInstance m_wave_instance;
	WaveRenderer m_params;
};

/* the renderer of the waves (the name its constructor 0x810a54c2 gives it); 0x2090 bytes */
class WaveRenderer {
public:
	/* 0x810a54c2 */
	WaveRenderer();

	/* emitted with the renderer's code (0x811fe8e8) */
	~WaveRenderer()
	{
		while (m_instances.m_next != &m_instances)
			delete static_cast<WaveInstance *>(m_instances.m_next);
	}

	/* 0x810a2108: the texture the waves are lit into, the viewport's size scaled, the first
	   time */
	void MakeLitTexture();
	/* the lit texture, made the first time (name ours) */
	const common::SharedPtr<arl::Texture2D> &GetLitTexture()
	{
		MakeLitTexture();
		return m_lit_texture;
	}
	/* 0x810a2570: the instance's positions drawn into its first texture */
	void draw_pos(WaveInstance *instance);
	/* 0x810a30aa: every instance's positions drawn, then their normals from them (name
	   ours) */
	void DrawPositions();
	/* 0x810a31c0: every instance lit into its third texture, flipped while m_lit_feedback
	   is above 0.001 */
	void render_lighting_texture();
	/* 0x810a44ac: the instance's grid drawn lit, or its lines while m_wireframe is set (name
	   ours) */
	void draw_color(WaveInstance *instance);
	/* 0x810a500e: every instance drawn lit */
	void render_color();
	/* 0x810a5112: the motion listener's rotation put back to the resting pitch (name ours) */
	void ResetRotation();
	/* 0x810a52d8: the parameters' keys moved on a frame, then every instance's spectrum set
	   when it is the first, and its distortion damped while its rotation turns (name
	   ours) */
	void UpdateInstances();
	/* 0x810a6180: once a frame, the instances and their surfaces moved on (name ours) */
	void Update();
	/* 0x810a23ee: the fog between its own and the one looking down */
	FogParamSet::Values GetFog();
	/* 0x810a70d0: the fog's colour, opaque (name ours) */
	math::v4 GetFogColor();
	/* 0x810a5ee2 */
	void Init(common::SharedPtr<Camera> camera, common::SharedPtr<Background> background,
		  common::SharedPtr<DrawEnv> draw_env, common::SharedPtr<TextureOps> texture_ops,
		  common::SharedPtr<MotionListener> listener);
	/* 0x810a61c6: the shaders, presets and layers made, the gradient texture, count instances,
	   and the sky */
	void Load(const string &dir, uint32_t count);
	/* 0x810a8d76: every preset's values set, the files read from dir's wave/psets (name
	   ours) */
	void LoadPresets(const string &dir);
	/* 0x810a7178: m_rs_wave's draw (name ours) */
	void Draw();
	/* 0x810a54c0: m_rs_wave_debug's draw, nothing (name ours) */
	void DrawDebug();
	/* 0x810a76a0: the world of the 2D camera tilted with the waves, from the one it was reset
	   to */
	math::matrix GetTiltedWorld(const math::matrix &reset_world);
	/* 0x810a8080: the values of the variant's sets taken from preset; an error when its
	   header is not one (name ours) */
	int32_t SetPreset(const char *variant, const WavePreset &preset);

	float m_wave_scale;
	float m_wave_speed;
	/* the grids drawn as lines (name ours) */
	bool m_wireframe;
	/* the instances' surfaces stand still */
	bool m_paused;
	/* set by the constructor and read nowhere (names ours) */
	bool m_unread_flag1;
	/* handed to the surfaces' updates, which do not read it */
	bool m_update_flag;
	bool m_unread_flag2;
	bool m_unread_flag3;
	int32_t m_unread_value1;
	/* the frame cleared to the fog colour before the waves are drawn (name ours) */
	bool m_clear;
	/* the frame of the last update */
	int32_t m_frame;
	common::SharedPtr<MotionListener> m_listener;
	common::SharedPtr<TextureOps> m_texture_ops;
	/* the lit texture's size, times the viewport's */
	float m_lit_scale;
	bool m_lit_made;
	common::SharedPtr<arl::Texture2D> m_lit_texture;
	arl::FrameBufferObject m_wave_fbo;
	arl::ShaderInstance<WaveVertShader, arl::VertShaderObject> m_wave_vert;
	arl::ShaderInstance<WaveFragShader, arl::FragShaderObject> m_wave_frag;
	arl::FrameBufferObject m_pos_fbo;
	arl::ShaderInstance<WavePosVertShader, arl::VertShaderObject> m_pos_vert;
	arl::ShaderInstance<WavePosFragShader, arl::FragShaderObject> m_pos_frag;
	arl::FrameBufferObject m_lit_fbo;
	arl::ShaderInstance<WaveLitTexFragShader, arl::FragShaderObject> m_lit_frag;
	ParamList m_instances;
	Skydome m_skydome;
	ParamSetKeysManager m_param_keys;
	ParamSetInstance<MaterialParamSet> m_material;
	ParamSetInstance<PointLightSphereParamSet> m_point_light;
	ParamSetInstance<FogParamSet> m_fog;
	ParamSetInstance<FogParamSet> m_fog_lookdown;
	ParamSetInstance<SkyParamSet> m_sky;
	ParamSetInstance<FFTWaveParamSet> m_fft_wave;
	ParamSetInstance<WaveInstanceParamSet> m_wave_instance;
	ParamSetInstance<WaveRendererParamSet> m_params;
	ParamSetKeysManager m_transition_keys;
	ParamSetInstance<WaveTransitionParamSet> m_transition;
	common::SharedPtr<Background> m_background;
	common::SharedPtr<Camera> m_camera;
	common::SharedPtr<DrawEnv> m_draw_env;
	/* 128 by 64 levels, each row a power curve from x^2 to x^18 (name ours) */
	common::SharedPtr<arl::Texture2D> m_gradient;
	/* the gradient's powers, set by Load and read nowhere (names ours) */
	float m_gradient_power_min;
	float m_gradient_power_max;
	/* counted up by every update */
	int32_t m_update_count;
	/* above 0.001, each instance's lit texture is fed back through the next frame
	   (name ours) */
	float m_lit_feedback;
	DrawLayer *m_rs_wave;
	DrawLayer *m_rs_wave_debug;
	/* the 2D camera follows the waves */
	bool m_tilt;
	/* set by the constructor and read nowhere (name ours) */
	int32_t m_unread_value2;
	/* the sky dome drawn behind the waves (name ours) */
	bool m_draw_sky;
	/* what the custom renderer is given each frame */
	float m_wave_x;
	float m_wave_y;
	uint32_t m_reserved;
};

}
}

#endif
