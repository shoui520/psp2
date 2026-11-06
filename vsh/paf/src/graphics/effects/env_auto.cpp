/*
 * EnvAuto: the background made from the screen. Each frame the screen is drawn onto a sphere
 * map, blended with the map of the frame before, and the reflections sample it; the screen is
 * also copied into the shared texture the refractions sample. The class name and its
 * captureReflections and CopyScreenForRefractions are Sony's; the file's name and the other
 * names ours.
 */
#include <stdint.h>
#include <gxm.h>
#include <vectormath.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/drawobj/effect_path.h>
#include <graphics/effects/background.h>
#include <graphics/signed_angle.h>
#include <std/vector>
#include "../../std/math.h"

using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector4;

namespace paf {
namespace graph {

#define PI		3.1415927f
#define TWO_PI		6.2831855f

/* the size of the capture textures, square; Sony's build reads it each time, so it is not
   internal to this file */
int32_t s_capture_size = 64;

/* x wrapped into [lo, lo + 2): what is left over after the whole turns */
static inline float Wrap(float x)
{
	float t = (x - -1.0f) * 0.5f;
	int32_t turns = (int32_t)t;

	if (t < 0.0f)
		turns--;
	x = x - (float)turns * 2.0f;
	if (x < -1.0f || x >= 1.0f)
		x = -1.0f;
	return x;
}

void EnvAuto::UpdateSphereMap()
{
	if (m_sphere_angle == m_angle)
		return;
	m_sphere_angle = m_angle;

	vector<uint32_t> texels;
	arl::Texture2D *texture = m_sphere_map.get();
	int32_t height = texture->m_desc.m_height;
	int32_t width = texture->m_desc.m_width;

	for (int32_t y = 0; y != height; y++) {
		float v = (float)y / (float)(height - 1);

		for (int32_t x = 0; x != width; x++) {
			Vector2 pos = mulPerElem(Vector2((float)x / (float)(width - 1), v) - Vector2(0.5f),
						 Vector2(2.0f));
			float theta = length(pos) * PI;
			float phi = sce_paf_atan2f(pos.getY(), pos.getX());
			float radius = sce_paf_sinf(theta);
			float dir_x = radius * sce_paf_cosf(phi);
			float dir_y = radius * sce_paf_sinf(phi);
			math::v4 dir = math::v4(dir_x, dir_y, sce_paf_cosf(theta), 0.0f).unit();

			dir = math::v4(dir.extract_z(), dir.extract_x(), dir.extract_y(), 0.0f).unit();

			float polar;
			float azimuth;
			float up = dir.extract_z();

			if (__builtin_fabsf(up) > 0.99999f) {
				polar = up > 0.0f ? 0.0f : PI;
				azimuth = 0.0f;
			} else {
				sce::Vectormath::Simd::floatInVec cosine =
					dir.dot(math::v4(0.0f, 0.0f, 1.0f, 0.0f));

				polar = sce_paf_acosf(sce::Vectormath::Simd::fminf(
					sce::Vectormath::Simd::fmaxf(cosine,
								     sce::Vectormath::Simd::floatInVec(-1.0f)),
					sce::Vectormath::Simd::floatInVec(1.0f)).getAsFloat());
				azimuth = GetSignedAngle(math::v2(1.0f, 0.0f),
							 math::v2(dir.extract_x(), dir.extract_y()));
				if (azimuth < 0.0f)
					azimuth += TWO_PI;
			}

			float u = Wrap(azimuth / PI) * 0.5f * 0.63661975f + 0.5f;
			float s = 1.0f - polar / PI;
			float t = 1.0f - s * ((1.0f - m_angle) + s * m_angle * (3.0f - s * 2.0f));
			Vector4 color(u, t, 0.0f, 0.0f);

			color = maxPerElem(minPerElem(color, Vector4(1.0f)), Vector4(0.0f));
			color = mulPerElem(color, Vector4(255.0f));
			texels.push_back((uint32_t)vgetq_lane_f32(color.get128(), 0) |
					 (uint32_t)vgetq_lane_f32(color.get128(), 1) << 8 |
					 (uint32_t)vgetq_lane_f32(color.get128(), 2) << 16 |
					 (uint32_t)vgetq_lane_f32(color.get128(), 3) << 24);
		}
	}

	texture = m_sphere_map.get();
	ScePafGraphics_309D2902(texture, 0, texture->m_desc.m_width, texture->m_desc.m_height,
				texture->m_desc.m_format, texture->m_desc.m_levels, texels.ptr);
	m_sphere_map->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	m_sphere_map->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
}

void EnvAuto::CreateCaptureTexture(common::SharedPtr<arl::Texture2D> &texture,
				   arl::SurfaceSize size)
{
	arl::ArlInstance *gl = arl::Gl();

	texture = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	ScePafGraphics_C4BD3558(texture.get(), size.m_width, size.m_height, ImageMode_RGBA8888, 0,
				arl::SurfaceFlags()
					.Set(arl::TEXTURE)
					.Set(arl::COLOR_SURFACE)
					.Set(arl::COLOR_SURFACE_SWIZZLED));

	arl::Texture2D *object = texture.get();

	object->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	object->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	texture->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	texture->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);
}

EnvAuto::EnvAuto()
	: m_angle(1.0f), m_sphere_angle(-1.0f), m_blend(0.5f), m_disabled(false)
{
}

void EnvAuto::Load(const string &dir)
{
	arl::ArlInstance *gl = arl::Gl();

	CreateCaptureTexture(m_reflections, arl::SurfaceSize(s_capture_size, s_capture_size));
	m_sphere_map = arl::CreateTexture<ImageMode_RGBA8888>(
		gl->m_default_allocator,
		arl::SurfaceSize(m_reflections->m_desc.m_width, m_reflections->m_desc.m_height), NULL,
		arl::SurfaceFlags(), 0);
	UpdateSphereMap();
	m_refractions = m_reflections;
	m_previous = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));
	m_previous->init(m_reflections);

	arl::Texture2D *previous = m_previous.get();

	previous->setWrapS(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	previous->setWrapT(arl::TEXTURE_WRAP_CLAMP_FULL_BORDER);
	m_previous->setMagFilter(SCE_GXM_TEXTURE_FILTER_LINEAR);
	m_previous->setMinFilter(SCE_GXM_TEXTURE_FILTER_LINEAR, false);

	common::SharedPtr<arl::VertexArraySpec> spec = m_texture_ops->m_quad.m_vertices.m_spec;
	arl::ProgramState *state = gl;

	m_vert.m_object.init(*gl, state->m_patcher, spec,
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>(),
			     common::SharedPtr<arl::VertexArraySpec>());
	state = gl;
	m_frag.m_object.init(*gl, state->m_patcher);
}

void EnvAuto::Draw()
{
}

arl::TextureSource EnvAuto::GetReflectionTexture()
{
	common::SharedPtr<arl::TextureObject> texture = m_reflections;
	arl::TextureSource source;

	source = texture;
	return source;
}

arl::TextureSource EnvAuto::GetRefractionTexture()
{
	common::SharedPtr<arl::TextureObject> texture = m_refractions;
	arl::TextureSource source;

	source = texture;
	return source;
}

common::SharedPtr<arl::Texture2D> EnvAuto::GetSharedTexture()
{
	SharedTexture *shared = m_shared_texture.get();
	arl::DefaultTextures *textures = arl::Gl();

	if (shared->get() != NULL)
		return *shared;
	return textures->m_white;
}

void EnvAuto::Init(common::SharedPtr<Camera> camera, common::SharedPtr<SharedTexture> texture)
{
	m_camera = camera;
	m_shared_texture = texture;
}

void EnvAuto::SetTextureOps(common::SharedPtr<TextureOps> texture_ops)
{
	m_texture_ops = texture_ops;
}

math::matrix EnvAuto::GetUVTransform()
{
	return math::matrix::Identity();
}

void EnvAuto::SetUVTransform(const math::matrix &transform)
{
}

void EnvAuto::captureReflections()
{
	if (m_disabled)
		return;

	{
		common::SharedPtr<arl::Texture2D> current = m_reflections;

		m_reflections = m_previous;
		m_previous = current;
	}
	if (m_sphere_angle != m_angle)
		UpdateSphereMap();

	arl::ArlInstance *gl = arl::Gl();
	common::SharedPtr<arl::Texture2D> screen = *arl::Gl()->GetCopyTexture(0);

	screen->setWrapS(arl::TEXTURE_WRAP_REPEAT);

	common::SharedPtr<arl::Texture2D> dest = m_reflections;

	m_fbo.Bind(common::SharedPtr<arl::TextureObject>(dest), 0, 0);
	ScePafGraphics_199BD5F2(gl, 0);
	gl->PushMarker("EnvAuto::captureReflections");
	gl->Disable(arl::BLEND);
	gl->Disable(arl::DEPTH_TEST);
	gl->DepthMask(false, arl::FRONT_AND_BACK);
	gl->DepthFunc(arl::DEPTH_ALWAYS, arl::FRONT_AND_BACK);
	{
		common::SharedPtr<arl::TextureObject> texture = screen;

		m_frag.m_interface.m_source.Set(common::SharedPtr<arl::TextureObject>(texture));
	}
	{
		common::SharedPtr<arl::TextureObject> texture = m_sphere_map;

		m_frag.m_interface.m_rect2sphere_map.Set(common::SharedPtr<arl::TextureObject>(texture));
	}
	{
		common::SharedPtr<arl::TextureObject> texture = m_previous;

		m_frag.m_interface.m_previous.Set(common::SharedPtr<arl::TextureObject>(texture));
	}

	arl::Texture2D *previous = m_previous.get();
	Vector2 size((float)previous->m_desc.m_width, (float)previous->m_desc.m_height);

	m_frag.m_interface.m_previous_offset.Set(math::v2(mulPerElem(recipPerElem(size), Vector2(1.0f))));
	m_frag.m_interface.m_previous_blend.Set(math::v4(Vector4(m_blend)));
	m_vert.m_object.begin();
	m_frag.m_object.begin();
	m_texture_ops->DrawQuad(TextureOps::IsFlipped(dest, screen));
	m_frag.m_object.end();
	m_vert.m_object.end();
	gl->PopMarker();
	ScePafGraphics_29F5AA8B(gl, NULL, false);
	arl::FrameBufferObject::Unbind();
}

void EnvAuto::CopyScreenForRefractions()
{
	SharedTexture *shared = m_shared_texture.get();
	arl::ArlInstance *gl = arl::Gl();

	if (shared->get() == NULL) {
		*shared = common::SharedPtr<arl::Texture2D>(new arl::Texture2D(gl->m_default_allocator));

		arl::Texture2D *texture = shared->get();
		arl::SurfaceSize size = ScePafGraphics_AC339F46(gl);

		ScePafGraphics_C4BD3558(texture, size.m_width, size.m_height,
					ScePafGraphics_96173730(gl), 0, ScePafGraphics_BB119805(gl));
	}
	gl->CopyTexImage2D(*shared, 0, 0, 0, (*shared)->m_desc.m_width,
			   (*shared)->m_desc.m_height);
}

}
}
