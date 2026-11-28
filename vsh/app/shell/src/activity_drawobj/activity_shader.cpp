/*
 * ActivityDrawObj GXM shaders and draw pipelines (0x8138D828..0x81393627).
 * Sony path: app/common/src/activity_drawobj/.
 */
#include "activity_drawobj.h"

#include <gxm.h>
#include <std/string.h>
#include <vectormath.h>
#include <framework.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>

using paf::common::SharedPtr;
using paf::graph::PlaneObjDrawParams;
using paf::graph::Shader;
using paf::graph::Surface;
using paf::graph::arl::ArlInstance;
using paf::graph::arl::FragShaderObject;
using paf::graph::arl::ProgramState;
using paf::graph::arl::ShaderInstance;
using paf::graph::arl::ShaderInterfaceBase;
using paf::graph::arl::ShaderParameter;
using paf::graph::arl::ShaderParameterBase;
using paf::graph::arl::ShaderState;
using paf::graph::arl::TextureSource;
using paf::graph::arl::VertShaderObject;
using paf::graph::arl::VertexArray;
using paf::graph::arl::VertexArraySpec;
using sce::Vectormath::Simd::Aos::Vector2;
using sce::Vectormath::Simd::Aos::Vector4;

extern "C" {
extern const uint8_t g_activityInternalVpGxp[];
extern const uint32_t g_activityInternalVpGxp_size;
extern const uint8_t g_activityInternalFpGxp[];
extern const uint32_t g_activityInternalFpGxp_size;
extern const uint8_t g_activityColorOnlyVpGxp[];
extern const uint32_t g_activityColorOnlyVpGxp_size;
extern const uint8_t g_activityColorOnlyFpGxp[];
extern const uint32_t g_activityColorOnlyFpGxp_size;
extern const uint8_t g_activityInternalPartialVpGxp[];
extern const uint32_t g_activityInternalPartialVpGxp_size;
extern const uint8_t g_activityColorOnlyPartialVpGxp[];
extern const uint32_t g_activityColorOnlyPartialVpGxp_size;
extern const uint8_t g_activityPartialMaskVpGxp[];
extern const uint32_t g_activityPartialMaskVpGxp_size;
extern const uint8_t g_activityPartialMaskFpGxp[];
extern const uint32_t g_activityPartialMaskFpGxp_size;
extern const uint8_t g_activityInternalFullVpGxp[];
extern const uint32_t g_activityInternalFullVpGxp_size;
extern const uint8_t g_activityColorOnlyFullVpGxp[];
extern const uint32_t g_activityColorOnlyFullVpGxp_size;
extern const uint8_t g_activityFullMaskVpGxp[];
extern const uint32_t g_activityFullMaskVpGxp_size;
extern const uint8_t g_activityFullMaskFpGxp[];
extern const uint32_t g_activityFullMaskFpGxp_size;
extern const uint8_t g_activityLinkIconVpGxp[];
extern const uint32_t g_activityLinkIconVpGxp_size;
extern const uint8_t g_activityLinkIconFpGxp[];
extern const uint32_t g_activityLinkIconFpGxp_size;
extern const uint8_t g_activityAvatorVpGxp[];
extern const uint32_t g_activityAvatorVpGxp_size;
extern const uint8_t g_activityAvatorFpGxp[];
extern const uint32_t g_activityAvatorFpGxp_size;
}

#define ACTIVITY_SHADER_BINARY(path, sym)			\
	virtual const char *GetName()				\
	{							\
		return path;					\
	}							\
	virtual const uint8_t *GetBinary()			\
	{							\
		return sym;					\
	}							\
	virtual uint32_t GetBinarySize()			\
	{							\
		return sym##_size;				\
	}

/* ---- Vertex shader interfaces ---- */

class ActivityInternalVertInterface : public ShaderInterfaceBase {
public:
	ActivityInternalVertInterface() : m_mvp(m_setup) {}
	virtual void Init() { m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0); }
	virtual void Apply() { m_mvp.Apply(); }
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_internal.vp",
			       g_activityInternalVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
};

class ActivityColorOnlyVertInterface : public ShaderInterfaceBase {
public:
	ActivityColorOnlyVertInterface() : m_mvp(m_setup), m_regionFrame(m_setup) {}
	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_regionFrame.init("Region0", ShaderParameterBase::UNIFORM, 0);
	}
	virtual void Apply()
	{
		m_mvp.Apply();
		m_regionFrame.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_color_only.vp",
			       g_activityColorOnlyVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v4> m_regionFrame;
};

class ActivityInternalPartialVertInterface : public ShaderInterfaceBase {
public:
	ActivityInternalPartialVertInterface() : m_mvp(m_setup) {}
	virtual void Init() { m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0); }
	virtual void Apply() { m_mvp.Apply(); }
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_internal_partial.vp",
			       g_activityInternalPartialVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
};

class ActivityColorOnlyPartialVertInterface : public ShaderInterfaceBase {
public:
	ActivityColorOnlyPartialVertInterface() : m_mvp(m_setup), m_regionFrame(m_setup) {}
	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_regionFrame.init("Region0", ShaderParameterBase::UNIFORM, 0);
	}
	virtual void Apply()
	{
		m_mvp.Apply();
		m_regionFrame.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_color_only_partial.vp",
			       g_activityColorOnlyPartialVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v4> m_regionFrame;
};

class ActivityPartialMaskVertInterface : public ShaderInterfaceBase {
public:
	ActivityPartialMaskVertInterface() : m_mvp(m_setup), m_regionFrame(m_setup) {}
	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_regionFrame.init("Region0", ShaderParameterBase::UNIFORM, 0);
	}
	virtual void Apply()
	{
		m_mvp.Apply();
		m_regionFrame.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_partial_mask.vp",
			       g_activityPartialMaskVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v4> m_regionFrame;
};

class ActivityInternalFullVertInterface : public ShaderInterfaceBase {
public:
	ActivityInternalFullVertInterface() : m_mvp(m_setup) {}
	virtual void Init() { m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0); }
	virtual void Apply() { m_mvp.Apply(); }
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_internal_full.vp",
			       g_activityInternalFullVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
};

class ActivityColorOnlyFullVertInterface : public ShaderInterfaceBase {
public:
	ActivityColorOnlyFullVertInterface() : m_mvp(m_setup), m_regionFrame(m_setup) {}
	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_regionFrame.init("Region0", ShaderParameterBase::UNIFORM, 0);
	}
	virtual void Apply()
	{
		m_mvp.Apply();
		m_regionFrame.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_color_only_full.vp",
			       g_activityColorOnlyFullVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v4> m_regionFrame;
};

class ActivityFullMaskVertInterface : public ShaderInterfaceBase {
public:
	ActivityFullMaskVertInterface()
		: m_mvp(m_setup), m_regionFrame(m_setup), m_regionImage(m_setup) {}
	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_regionFrame.init("Region0", ShaderParameterBase::UNIFORM, 0);
		m_regionImage.init("Region1", ShaderParameterBase::UNIFORM, 0);
	}
	virtual void Apply()
	{
		m_mvp.Apply();
		m_regionFrame.Apply();
		m_regionImage.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_full_mask.vp",
			       g_activityFullMaskVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v4> m_regionFrame;
	ShaderParameter<paf::math::v4> m_regionImage;
};

class ActivityLinkIconVertInterface : public ShaderInterfaceBase {
public:
	ActivityLinkIconVertInterface() : m_mvp(m_setup), m_regionIcon(m_setup) {}
	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_regionIcon.init("Region0", ShaderParameterBase::UNIFORM, 0);
	}
	virtual void Apply()
	{
		m_mvp.Apply();
		m_regionIcon.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_linkicon.vp",
			       g_activityLinkIconVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v4> m_regionIcon;
};

class ActivityAvatorVertInterface : public ShaderInterfaceBase {
public:
	ActivityAvatorVertInterface() : m_mvp(m_setup), m_regionAvatar(m_setup) {}
	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_regionAvatar.init("Region0", ShaderParameterBase::UNIFORM, 0);
	}
	virtual void Apply()
	{
		m_mvp.Apply();
		m_regionAvatar.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_avator.vp",
			       g_activityAvatorVpGxp);

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v4> m_regionAvatar;
};

/* ---- Fragment shader interfaces ---- */

class ActivityInternalFragInterface : public ShaderInterfaceBase {
public:
	ActivityInternalFragInterface() : m_color(m_setup) {}
	virtual void Init() { m_color.init("Color", ShaderParameterBase::UNIFORM, 0); }
	virtual void Apply() { m_color.Apply(); }
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_internal.fp",
			       g_activityInternalFpGxp);

	ShaderParameter<paf::math::v4> m_color;
};

class ActivityColorOnlyFragInterface : public ShaderInterfaceBase {
public:
	ActivityColorOnlyFragInterface() : m_color(m_setup), m_frameTexture(m_setup) {}
	virtual void Init()
	{
		m_color.init("Color", ShaderParameterBase::UNIFORM, 0);
		m_frameTexture.init("Texture0", 0, 0);
	}
	virtual void Apply()
	{
		m_color.Apply();
		m_frameTexture.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_color_only.fp",
			       g_activityColorOnlyFpGxp);

	ShaderParameter<paf::math::v4> m_color;
	ShaderParameter<TextureSource> m_frameTexture;
};

class ActivityPartialMaskFragInterface : public ShaderInterfaceBase {
public:
	ActivityPartialMaskFragInterface()
		: m_color(m_setup), m_frameTexture(m_setup), m_imageTexture(m_setup),
		  m_maskTexture(m_setup) {}
	virtual void Init()
	{
		m_color.init("Color", ShaderParameterBase::UNIFORM, 0);
		m_frameTexture.init("Texture0", 0, 0);
		m_imageTexture.init("Texture1", 1, 0);
		m_maskTexture.init("Texture2", 2, 0);
	}
	virtual void Apply()
	{
		m_color.Apply();
		m_frameTexture.Apply();
		m_imageTexture.Apply();
		m_maskTexture.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_partial_mask.fp",
			       g_activityPartialMaskFpGxp);

	ShaderParameter<paf::math::v4> m_color;
	ShaderParameter<TextureSource> m_frameTexture;
	ShaderParameter<TextureSource> m_imageTexture;
	ShaderParameter<TextureSource> m_maskTexture;
};

class ActivityFullMaskFragInterface : public ShaderInterfaceBase {
public:
	ActivityFullMaskFragInterface()
		: m_color(m_setup), m_frameTexture(m_setup), m_imageTexture(m_setup),
		  m_maskTexture(m_setup) {}
	virtual void Init()
	{
		m_color.init("Color", ShaderParameterBase::UNIFORM, 0);
		m_frameTexture.init("Texture0", 0, 0);
		m_imageTexture.init("Texture1", 1, 0);
		m_maskTexture.init("Texture2", 2, 0);
	}
	virtual void Apply()
	{
		m_color.Apply();
		m_frameTexture.Apply();
		m_imageTexture.Apply();
		m_maskTexture.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_full_mask.fp",
			       g_activityFullMaskFpGxp);

	ShaderParameter<paf::math::v4> m_color;
	ShaderParameter<TextureSource> m_frameTexture;
	ShaderParameter<TextureSource> m_imageTexture;
	ShaderParameter<TextureSource> m_maskTexture;
};

class ActivityLinkIconFragInterface : public ShaderInterfaceBase {
public:
	ActivityLinkIconFragInterface()
		: m_color(m_setup), m_iconTexture(m_setup), m_maskTexture(m_setup) {}
	virtual void Init()
	{
		m_color.init("Color", ShaderParameterBase::UNIFORM, 0);
		m_iconTexture.init("Texture0", 1, 0);
		m_maskTexture.init("Texture1", 2, 0);
	}
	virtual void Apply()
	{
		m_color.Apply();
		m_iconTexture.Apply();
		m_maskTexture.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_linkicon.fp",
			       g_activityLinkIconFpGxp);

	ShaderParameter<paf::math::v4> m_color;
	ShaderParameter<TextureSource> m_iconTexture;
	ShaderParameter<TextureSource> m_maskTexture;
};

class ActivityAvatorFragInterface : public ShaderInterfaceBase {
public:
	ActivityAvatorFragInterface()
		: m_color(m_setup), m_avatarTexture(m_setup), m_maskTexture(m_setup) {}
	virtual void Init()
	{
		m_color.init("Color", ShaderParameterBase::UNIFORM, 0);
		m_avatarTexture.init("Texture0", 1, 0);
		m_maskTexture.init("Texture1", 2, 0);
	}
	virtual void Apply()
	{
		m_color.Apply();
		m_avatarTexture.Apply();
		m_maskTexture.Apply();
	}
	virtual void Unused() {}
	ACTIVITY_SHADER_BINARY("app/common/src/activity_drawobj/cg/activity_avator.fp",
			       g_activityAvatorFpGxp);

	ShaderParameter<paf::math::v4> m_color;
	ShaderParameter<TextureSource> m_avatarTexture;
	ShaderParameter<TextureSource> m_maskTexture;
};

typedef ShaderInstance<ActivityInternalVertInterface, VertShaderObject> InternalVertInst;
typedef ShaderInstance<ActivityInternalFragInterface, FragShaderObject> InternalFragInst;
typedef ShaderInstance<ActivityColorOnlyVertInterface, VertShaderObject> ColorOnlyVertInst;
typedef ShaderInstance<ActivityColorOnlyFragInterface, FragShaderObject> ColorOnlyFragInst;
typedef ShaderInstance<ActivityInternalPartialVertInterface, VertShaderObject> InternalPartialVertInst;
typedef ShaderInstance<ActivityColorOnlyPartialVertInterface, VertShaderObject> ColorOnlyPartialVertInst;
typedef ShaderInstance<ActivityPartialMaskVertInterface, VertShaderObject> PartialMaskVertInst;
typedef ShaderInstance<ActivityPartialMaskFragInterface, FragShaderObject> PartialMaskFragInst;
typedef ShaderInstance<ActivityInternalFullVertInterface, VertShaderObject> InternalFullVertInst;
typedef ShaderInstance<ActivityColorOnlyFullVertInterface, VertShaderObject> ColorOnlyFullVertInst;
typedef ShaderInstance<ActivityFullMaskVertInterface, VertShaderObject> FullMaskVertInst;
typedef ShaderInstance<ActivityFullMaskFragInterface, FragShaderObject> FullMaskFragInst;
typedef ShaderInstance<ActivityLinkIconVertInterface, VertShaderObject> LinkIconVertInst;
typedef ShaderInstance<ActivityLinkIconFragInterface, FragShaderObject> LinkIconFragInst;
typedef ShaderInstance<ActivityAvatorVertInterface, VertShaderObject> AvatorVertInst;
typedef ShaderInstance<ActivityAvatorFragInterface, FragShaderObject> AvatorFragInst;

/* Global shader program instances (0x815CA890..0x815CA93C) */
static SharedPtr<InternalVertInst> s_internalVert;
static SharedPtr<InternalFragInst> s_internalFrag;
static SharedPtr<ColorOnlyVertInst> s_colorOnlyVert;
static SharedPtr<ColorOnlyFragInst> s_colorOnlyFrag;
static SharedPtr<InternalPartialVertInst> s_internalPartialVert;
static SharedPtr<ColorOnlyPartialVertInst> s_colorOnlyPartialVert;
static SharedPtr<PartialMaskVertInst> s_partialMaskVert;
static SharedPtr<PartialMaskFragInst> s_partialMaskFrag;
static SharedPtr<InternalFullVertInst> s_internalFullVert;
static SharedPtr<ColorOnlyFullVertInst> s_colorOnlyFullVert;
static SharedPtr<FullMaskVertInst> s_fullMaskVert;
static SharedPtr<FullMaskFragInst> s_fullMaskFrag;
static SharedPtr<LinkIconVertInst> s_linkIconVert;
static SharedPtr<LinkIconFragInst> s_linkIconFrag;
static SharedPtr<AvatorVertInst> s_avatorVert;
static SharedPtr<AvatorFragInst> s_avatorFrag;

static SharedPtr<Shader> s_colorOnlyShader;
static SharedPtr<Shader> s_partialMaskShader;
static SharedPtr<Shader> s_fullMaskShader;
static SharedPtr<Shader> s_linkIconShader;
static SharedPtr<Shader> s_avatorShader;

static int32_t s_shaderInitCount = 0;
static paf::intrusive_ptr<Surface> s_fallbackSurface;

static inline TextureSource makeSurfaceSource(const paf::intrusive_ptr<Surface> &surface)
{
	TextureSource source;
	source.m_surface = surface;
	return source;
}

static inline paf::math::v4 makeSurfaceRegion(const Surface *surface)
{
	return paf::math::v4(Vector4(Vector2(0.0f, 0.0f), surface->GetFactor().getImpl()));
}

static inline void bindVertexStream(ArlInstance *gl, const VertexArray *vertices)
{
	ShaderState *shaderState = gl;
	shaderState->m_dirty_vertex |= vertices->m_spec->m_kinds;
	shaderState->m_dirty_fragment |= 1U;
	sceGxmSetVertexStream(gl->GetRawContext(), 0, vertices->m_buffer->m_memory->m_addr);
}

static inline void clearVertexStreamState(ArlInstance *gl)
{
	ShaderState *shaderState = gl;
	shaderState->m_dirty_vertex = 0;
	shaderState->m_dirty_fragment = 0;
}

static inline float getModelScaleY(ArlInstance *gl)
{
	sce::Vectormath::Simd::Aos::Vector4 colY = gl->m_model.Top().m_matrix.getImpl().getCol1();
	return static_cast<float>(sce::Vectormath::Simd::Aos::length(colY));
}

static inline void resolveTextures(const PlaneObjDrawParams &params,
				   paf::intrusive_ptr<Surface> *outSurfaces, uint32_t count)
{
	for (uint32_t i = 0; i < count; ++i)
		outSurfaces[i] = s_fallbackSurface;

	if (params.m_textures == NULL)
		return;

	for (uint32_t i = 0; i < count && i < params.m_num_textures; ++i) {
		if (params.m_textures[i].get() != NULL) {
			params.m_textures[i]->Texturize();
			outSurfaces[i] = params.m_textures[i];
		}
	}
}

/* ---- Five paf::graph::Shader implementations ---- */

/* Mode 0 shader (0x8138D828..0x8138E01D) */
class ActivityColorOnlyShader : public Shader {
public:
	virtual ~ActivityColorOnlyShader() {}

	/* 0x8138D838 */
	virtual int32_t Draw(PlaneObjDrawParams &params)
	{
		ArlInstance *gl = paf::graph::arl::Gl();
		if (s_colorOnlyVert.get() == NULL || s_colorOnlyFrag.get() == NULL)
			return 0;

		paf::intrusive_ptr<Surface> surfaces[1];
		resolveTextures(params, surfaces, 1);

		s_colorOnlyVert->m_interface.m_regionFrame.Set(makeSurfaceRegion(surfaces[0].get()));
		s_colorOnlyVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_colorOnlyFrag->m_interface.m_color.Set(gl->m_color);
		s_colorOnlyFrag->m_interface.m_frameTexture.Set(makeSurfaceSource(surfaces[0]));

		bindVertexStream(gl, params.m_vertices);

		s_colorOnlyVert->m_object.begin();
		s_colorOnlyFrag->m_object.begin();
		params.m_indices->draw(*gl, paf::graph::arl::TRIANGLES, params.m_count);
		s_colorOnlyFrag->m_object.end();
		s_colorOnlyVert->m_object.end();

		s_internalVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_internalFrag->m_interface.m_color.Set(gl->m_color);

		s_internalVert->m_object.begin();
		s_internalFrag->m_object.begin();
		ActivityMeshIndexBuffer *interior =
			activityDrawObjGetIndexBuffer(ACTIVITY_MESH_COLOR_ONLY_INTERIOR);
		if (interior != NULL)
			interior->indices->draw(*gl, paf::graph::arl::TRIANGLES, interior->count);
		s_internalVert->m_object.end();
		s_internalFrag->m_object.end();

		clearVertexStreamState(gl);
		return 0;
	}
};

/* Mode 1 shader (0x8138E020..0x8138EF6B) */
class ActivityPartialMaskShader : public Shader {
public:
	virtual ~ActivityPartialMaskShader() {}

	/* 0x8138E02E */
	virtual int32_t Draw(PlaneObjDrawParams &params)
	{
		ArlInstance *gl = paf::graph::arl::Gl();
		if (s_partialMaskVert.get() == NULL || s_partialMaskFrag.get() == NULL)
			return 0;

		paf::intrusive_ptr<Surface> surfaces[3];
		resolveTextures(params, surfaces, 3);

		s_partialMaskVert->m_interface.m_regionFrame.Set(
			makeSurfaceRegion(surfaces[0].get()));
		s_partialMaskVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_partialMaskFrag->m_interface.m_color.Set(gl->m_color);
		s_partialMaskFrag->m_interface.m_frameTexture.Set(makeSurfaceSource(surfaces[0]));
		s_partialMaskFrag->m_interface.m_imageTexture.Set(makeSurfaceSource(surfaces[1]));
		s_partialMaskFrag->m_interface.m_maskTexture.Set(makeSurfaceSource(surfaces[2]));

		bindVertexStream(gl, params.m_vertices);

		float scaleY = getModelScaleY(gl);

		s_partialMaskVert->m_object.begin();
		s_partialMaskFrag->m_object.begin();
		uint32_t maskedCount = params.m_count;
		if (scaleY > 256.0f)
			maskedCount -= 12;
		params.m_indices->draw(*gl, paf::graph::arl::TRIANGLES, maskedCount);
		s_partialMaskFrag->m_object.end();
		s_partialMaskVert->m_object.end();

		s_colorOnlyPartialVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_colorOnlyPartialVert->m_interface.m_regionFrame.Set(
			makeSurfaceRegion(surfaces[0].get()));
		s_colorOnlyFrag->m_interface.m_color.Set(gl->m_color);
		s_colorOnlyFrag->m_interface.m_frameTexture.Set(makeSurfaceSource(surfaces[0]));

		s_colorOnlyPartialVert->m_object.begin();
		s_colorOnlyFrag->m_object.begin();
		ActivityMeshIndexBuffer *border =
			activityDrawObjGetIndexBuffer(ACTIVITY_MESH_PARTIAL_BORDER);
		if (border != NULL) {
			uint32_t borderCount = border->count;
			if (scaleY < 256.0f)
				borderCount -= 6;
			border->indices->draw(*gl, paf::graph::arl::TRIANGLES, borderCount);
		}
		s_colorOnlyFrag->m_object.end();
		s_colorOnlyPartialVert->m_object.end();

		s_internalPartialVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_internalFrag->m_interface.m_color.Set(gl->m_color);

		s_internalPartialVert->m_object.begin();
		s_internalFrag->m_object.begin();
		ActivityMeshIndexBuffer *interior =
			activityDrawObjGetIndexBuffer(ACTIVITY_MESH_PARTIAL_INTERIOR);
		if (interior != NULL) {
			uint32_t interiorCount = interior->count;
			if (scaleY < 256.0f)
				interiorCount -= 6;
			interior->indices->draw(*gl, paf::graph::arl::TRIANGLES, interiorCount);
		}
		s_internalFrag->m_object.end();
		s_internalPartialVert->m_object.end();

		clearVertexStreamState(gl);
		return 0;
	}
};

/* Mode 2 shader (0x8138EF6C..0x81390015) */
class ActivityFullMaskShader : public Shader {
public:
	ActivityFullMaskShader();
	virtual ~ActivityFullMaskShader();
	virtual int32_t Draw(PlaneObjDrawParams &params);

private:
	SharedPtr<FullMaskVertInst> m_vert;
	SharedPtr<FullMaskFragInst> m_frag;
};

/* 0x8138EF6C */
ActivityFullMaskShader::ActivityFullMaskShader()
	: m_vert(s_fullMaskVert), m_frag(s_fullMaskFrag)
{
}

/* 0x8138EFCE / 0x8138F08E */
ActivityFullMaskShader::~ActivityFullMaskShader()
{
}

/* 0x8138F09E */
int32_t ActivityFullMaskShader::Draw(PlaneObjDrawParams &params)
{
	ArlInstance *gl = paf::graph::arl::Gl();
	if (s_fullMaskVert.get() == NULL || s_fullMaskFrag.get() == NULL)
		return 0;

	paf::intrusive_ptr<Surface> surfaces[3];
	resolveTextures(params, surfaces, 3);

	s_fullMaskVert->m_interface.m_regionFrame.Set(makeSurfaceRegion(surfaces[0].get()));
	s_fullMaskVert->m_interface.m_regionImage.Set(makeSurfaceRegion(surfaces[1].get()));
	s_fullMaskVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
	s_fullMaskFrag->m_interface.m_color.Set(gl->m_color);
	s_fullMaskFrag->m_interface.m_frameTexture.Set(makeSurfaceSource(surfaces[0]));
	s_fullMaskFrag->m_interface.m_imageTexture.Set(makeSurfaceSource(surfaces[1]));
	s_fullMaskFrag->m_interface.m_maskTexture.Set(makeSurfaceSource(surfaces[2]));

	bindVertexStream(gl, params.m_vertices);

	float scaleY = getModelScaleY(gl);

	s_fullMaskVert->m_object.begin();
	s_fullMaskFrag->m_object.begin();
	uint32_t maskedCount = params.m_count;
	if (scaleY > 256.0f)
		maskedCount -= 84;
	params.m_indices->draw(*gl, paf::graph::arl::TRIANGLES, maskedCount);
	s_fullMaskFrag->m_object.end();
	s_fullMaskVert->m_object.end();

	if (scaleY > 256.0f) {
		s_colorOnlyFullVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_colorOnlyFullVert->m_interface.m_regionFrame.Set(
			makeSurfaceRegion(surfaces[0].get()));
		s_colorOnlyFrag->m_interface.m_color.Set(gl->m_color);
		s_colorOnlyFrag->m_interface.m_frameTexture.Set(makeSurfaceSource(surfaces[0]));

		s_colorOnlyFullVert->m_object.begin();
		s_colorOnlyFrag->m_object.begin();
		ActivityMeshIndexBuffer *lowerBorder =
			activityDrawObjGetIndexBuffer(ACTIVITY_MESH_FULL_LOWER_BORDER);
		if (lowerBorder != NULL)
			lowerBorder->indices->draw(*gl, paf::graph::arl::TRIANGLES,
						   lowerBorder->count);
		s_colorOnlyFrag->m_object.end();
		s_colorOnlyFullVert->m_object.end();

		s_internalFullVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_internalFrag->m_interface.m_color.Set(gl->m_color);

		s_internalFullVert->m_object.begin();
		s_internalFrag->m_object.begin();
		ActivityMeshIndexBuffer *lowerInterior =
			activityDrawObjGetIndexBuffer(ACTIVITY_MESH_FULL_LOWER_INTERIOR);
		if (lowerInterior != NULL)
			lowerInterior->indices->draw(*gl, paf::graph::arl::TRIANGLES,
						     lowerInterior->count);
		s_internalFrag->m_object.end();
		s_internalFullVert->m_object.end();
	}

	clearVertexStreamState(gl);
	return 0;
}

/* Link icon shader (0x81390016) */
class ActivityLinkIconShader : public Shader {
public:
	virtual ~ActivityLinkIconShader() {}

	/* 0x81390016 */
	virtual int32_t Draw(PlaneObjDrawParams &params)
	{
		ArlInstance *gl = paf::graph::arl::Gl();
		if (s_linkIconVert.get() == NULL || s_linkIconFrag.get() == NULL)
			return 0;

		paf::intrusive_ptr<Surface> surfaces[2];
		resolveTextures(params, surfaces, 2);

		s_linkIconVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_linkIconVert->m_interface.m_regionIcon.Set(makeSurfaceRegion(surfaces[0].get()));
		s_linkIconFrag->m_interface.m_color.Set(gl->m_color);
		s_linkIconFrag->m_interface.m_iconTexture.Set(makeSurfaceSource(surfaces[0]));
		s_linkIconFrag->m_interface.m_maskTexture.Set(makeSurfaceSource(surfaces[1]));

		s_linkIconVert->m_object.begin();
		s_linkIconFrag->m_object.begin();
		for (uint32_t i = 0; i < params.m_instances; ++i) {
			bindVertexStream(gl, &params.m_vertices[i]);
			if (params.m_indices != NULL)
				params.m_indices[i].draw(*gl, paf::graph::arl::TRIANGLES,
							 params.m_count);
			else
				VertexArray::DrawArrays(*gl, paf::graph::arl::TRIANGLE_STRIP, 0,
							params.m_count);
			clearVertexStreamState(gl);
		}
		s_linkIconFrag->m_object.end();
		s_linkIconVert->m_object.end();
		return 0;
	}
};

/* Avatar panel shader (0x81390724) */
class ActivityAvatorShader : public Shader {
public:
	virtual ~ActivityAvatorShader() {}

	/* 0x81390724 */
	virtual int32_t Draw(PlaneObjDrawParams &params)
	{
		ArlInstance *gl = paf::graph::arl::Gl();
		if (s_avatorVert.get() == NULL || s_avatorFrag.get() == NULL)
			return 0;

		paf::intrusive_ptr<Surface> surfaces[2];
		resolveTextures(params, surfaces, 2);

		s_avatorVert->m_interface.m_mvp.Set(gl->GetModelViewProjection());
		s_avatorVert->m_interface.m_regionAvatar.Set(makeSurfaceRegion(surfaces[0].get()));
		s_avatorFrag->m_interface.m_color.Set(gl->m_color);
		s_avatorFrag->m_interface.m_avatarTexture.Set(makeSurfaceSource(surfaces[0]));
		s_avatorFrag->m_interface.m_maskTexture.Set(makeSurfaceSource(surfaces[1]));

		s_avatorVert->m_object.begin();
		s_avatorFrag->m_object.begin();
		for (uint32_t i = 0; i < params.m_instances; ++i) {
			bindVertexStream(gl, &params.m_vertices[i]);
			if (params.m_indices != NULL)
				params.m_indices[i].draw(*gl, paf::graph::arl::TRIANGLES,
							 params.m_count);
			else
				VertexArray::DrawArrays(*gl, paf::graph::arl::TRIANGLE_STRIP, 0,
							params.m_count);
			clearVertexStreamState(gl);
		}
		s_avatorFrag->m_object.end();
		s_avatorVert->m_object.end();
		return 0;
	}
};

template<class VertInst>
static void initVertShader(SharedPtr<VertInst> &holder, ArlInstance *gl,
			   VertexArraySpec::Predefined layout)
{
	holder = SharedPtr<VertInst>(new VertInst);
	if (holder.get() != NULL) {
		ProgramState *progState = gl;
		SharedPtr<VertexArraySpec> spec = ScePafGraphics_4D1E289A(
			gl, layout, SCE_GXM_INDEX_SOURCE_INDEX_16BIT);
		holder->m_object.init(*gl, progState->m_patcher, spec,
				      SharedPtr<VertexArraySpec>(),
				      SharedPtr<VertexArraySpec>(),
				      SharedPtr<VertexArraySpec>());
	}
}

template<class FragInst>
static void initFragShader(SharedPtr<FragInst> &holder, ArlInstance *gl)
{
	holder = SharedPtr<FragInst>(new FragInst);
	if (holder.get() != NULL) {
		ProgramState *progState = gl;
		holder->m_object.init(*gl, progState->m_patcher);
	}
}

/* 0x81390E32 */
void activityShaderInit(void)
{
	if (s_shaderInitCount == 0) {
		ArlInstance *gl = paf::graph::arl::Gl();

		initVertShader(s_internalVert, gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2);
		initFragShader(s_internalFrag, gl);
		initVertShader(s_colorOnlyVert, gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2);
		initFragShader(s_colorOnlyFrag, gl);

		initVertShader(s_internalPartialVert, gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4);
		initVertShader(s_colorOnlyPartialVert, gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4);
		initVertShader(s_partialMaskVert, gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4);
		initFragShader(s_partialMaskFrag, gl);

		initVertShader(s_fullMaskVert, gl,
			       VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4_ATTRIB4_F2);
		initFragShader(s_fullMaskFrag, gl);
		initVertShader(s_internalFullVert, gl,
			       VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4_ATTRIB4_F2);
		initVertShader(s_colorOnlyFullVert, gl,
			       VertexArraySpec::ATTRIB0_F3_ATTRIB3_F4_ATTRIB4_F2);

		initVertShader(s_linkIconVert, gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2);
		initFragShader(s_linkIconFrag, gl);
		initVertShader(s_avatorVert, gl, VertexArraySpec::ATTRIB0_F3_ATTRIB3_F2);
		initFragShader(s_avatorFrag, gl);

		s_colorOnlyShader = SharedPtr<Shader>(new ActivityColorOnlyShader);
		s_partialMaskShader = SharedPtr<Shader>(new ActivityPartialMaskShader);
		s_fullMaskShader = SharedPtr<Shader>(new ActivityFullMaskShader);
		s_linkIconShader = SharedPtr<Shader>(new ActivityLinkIconShader);
		s_avatorShader = SharedPtr<Shader>(new ActivityAvatorShader);

		paf::Framework *fw = paf::Framework::Instance();
		s_fallbackSurface = paf::intrusive_ptr<Surface>(
			new Surface(fw->GetSurfacePool(), 8, 8, ImageMode_RGBA8888,
				    ImageOrder_Linear, 1, 1, false));
		if (s_fallbackSurface.get() != NULL) {
			void *pixels = s_fallbackSurface->Lock(0);
			if (pixels != NULL)
				sce_paf_memset(pixels, 0, 8 * 8 * 4);
			s_fallbackSurface->Unlock();
			s_fallbackSurface->Texturize();
		}
	}
	++s_shaderInitCount;
}

/* 0x8139251A */
void activityShaderTerm(void)
{
	if (s_shaderInitCount <= 0)
		return;
	if (--s_shaderInitCount != 0)
		return;

	s_internalVert.reset();
	s_internalFrag.reset();
	s_colorOnlyVert.reset();
	s_colorOnlyFrag.reset();
	s_internalPartialVert.reset();
	s_colorOnlyPartialVert.reset();
	s_partialMaskVert.reset();
	s_partialMaskFrag.reset();
	s_internalFullVert.reset();
	s_colorOnlyFullVert.reset();
	s_fullMaskVert.reset();
	s_fullMaskFrag.reset();
	s_linkIconVert.reset();
	s_linkIconFrag.reset();
	s_avatorVert.reset();
	s_avatorFrag.reset();

	s_colorOnlyShader.reset();
	s_partialMaskShader.reset();
	s_fullMaskShader.reset();
	s_linkIconShader.reset();
	s_avatorShader.reset();

	s_fallbackSurface.clear();
}

/* 0x8139301E */
SharedPtr<Shader> *activityDrawObjGetColorOnlyShader(void)
{
	return &s_colorOnlyShader;
}

/* 0x81393028 */
SharedPtr<Shader> *activityDrawObjGetPartialMaskShader(void)
{
	return &s_partialMaskShader;
}

/* 0x81393032 */
SharedPtr<Shader> *activityDrawObjGetFullMaskShader(void)
{
	return &s_fullMaskShader;
}

/* 0x8139303C */
SharedPtr<Shader> *activityUiGetIconShader(void)
{
	return &s_linkIconShader;
}

/* 0x81393046 */
SharedPtr<Shader> *activityUiGetPanelShader(void)
{
	return &s_avatorShader;
}
