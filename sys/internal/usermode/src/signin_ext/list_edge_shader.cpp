/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * ListEdgeShader — custom GXM vertex/fragment shader used by ListEdgeObj to
 * apply alpha-gradient edge fading at the top and bottom of scrollable lists.
 * Sony path: app/signup/library/signup_library/cg/drawobj/list_edge/
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <gxm.h>
#include <common/shared_ptr.h>
#include <graphics/color.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include "list_edge_shaders.h"
#include "signin_ext_internal.h"

using paf::intrusive_ptr;
using paf::common::SharedPtr;
using paf::graph::PlaneObjDrawParams;
using paf::graph::Shader;
using paf::graph::Surface;
using paf::graph::arl::ArlInstance;
using paf::graph::arl::FragShaderObject;
using paf::graph::arl::Gl;
using paf::graph::arl::ShaderInstance;
using paf::graph::arl::ShaderInterfaceBase;
using paf::graph::arl::ShaderParameter;
using paf::graph::arl::ShaderParameterBase;
using paf::graph::arl::TextureSource;
using paf::graph::arl::VertShaderObject;
using paf::graph::arl::VertexArray;
using paf::graph::arl::VertexArraySpec;

static SharedPtr<Shader> s_listEdgeShader;

/* ---- GXM shader interfaces for list_edge.vp / list_edge.fp ---- */

class ListEdgeVertInterface : public ShaderInterfaceBase {
public:
	ListEdgeVertInterface()
		: m_mvp(m_setup), m_uvOffset(m_setup), m_uvScale(m_setup), m_coefficient(m_setup)
	{
	}

	virtual ~ListEdgeVertInterface() {}

	virtual void Init()
	{
		m_mvp.init("MVP", ShaderParameterBase::UNIFORM, 0);
		m_uvOffset.init("UVOffset", ShaderParameterBase::UNIFORM, 0);
		m_uvScale.init("UVScale", ShaderParameterBase::UNIFORM, 0);
		m_coefficient.init("Coefficient", ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_uvOffset.Apply();
		m_uvScale.Apply();
		m_coefficient.Apply();
	}

	virtual void Unused() {}

	virtual const char *GetName()
	{
		return "app/signup/library/signup_library/cg/drawobj/list_edge/list_edge.vp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_listEdgeVpGxp;
	}

	virtual uint32_t GetBinarySize()
	{
		return 512;
	}

	ShaderParameter<paf::math::matrix> m_mvp;
	ShaderParameter<paf::math::v2> m_uvOffset;
	ShaderParameter<paf::math::v2> m_uvScale;
	ShaderParameter<paf::math::v2> m_coefficient;
};

class ListEdgeFragInterface : public ShaderInterfaceBase {
public:
	ListEdgeFragInterface()
		: m_color(m_setup), m_texture0(m_setup)
	{
	}

	virtual ~ListEdgeFragInterface() {}

	virtual void Init()
	{
		m_color.init("Color", ShaderParameterBase::UNIFORM, 0);
		m_texture0.init("Texture0", 1, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_texture0.Apply();
	}

	virtual void Unused() {}

	virtual const char *GetName()
	{
		return "app/signup/library/signup_library/cg/drawobj/list_edge/list_edge.fp";
	}

	virtual const uint8_t *GetBinary()
	{
		return s_listEdgeFpGxp;
	}

	virtual uint32_t GetBinarySize()
	{
		return 324;
	}

	ShaderParameter<paf::math::v4> m_color;
	ShaderParameter<TextureSource> m_texture0;
};

struct ListEdgeProgramPair {
	ShaderInstance<ListEdgeVertInterface, VertShaderObject> vert;
	ShaderInstance<ListEdgeFragInterface, FragShaderObject> frag;
};

ListEdgeShader::ListEdgeShader()
	: m_program(NULL), m_coefficient(1.0f, 0.0f)
{
}

ListEdgeShader::~ListEdgeShader()
{
	delete m_program;
}

int32_t ListEdgeShader::DrawListEdge(PlaneObjDrawParams &params)
{
	ArlInstance *gl = Gl();

	if (m_program == NULL) {
		m_program = new ListEdgeProgramPair();
		if (m_program == NULL)
			return SIGNIN_EXT_ERROR_NOMEM;

		paf::graph::arl::ProgramState *state = gl;
		SharedPtr<VertexArraySpec> spec = params.m_vertices->m_spec;
		m_program->vert.m_object.init(*gl, state->m_patcher, spec,
					      SharedPtr<VertexArraySpec>(),
					      SharedPtr<VertexArraySpec>(),
					      SharedPtr<VertexArraySpec>());
		m_program->frag.m_object.init(*gl, state->m_patcher);
	}

	Surface *surf = params.m_textures[0].get();
	paf::math::v2 uvScaleFactor = surf->GetFactor();

	m_program->vert.m_interface.m_mvp.Set(gl->GetModelViewProjection());

	paf::math::v2 regionOffset(params.m_tex_region->extract_x(),
				   params.m_tex_region->extract_y());
	paf::math::v2 regionScale(params.m_tex_region->extract_z(),
				  params.m_tex_region->extract_w());

	paf::math::v2 uvOffset(regionOffset.extract_x() * uvScaleFactor.extract_x(),
			       regionOffset.extract_y() * uvScaleFactor.extract_y());
	paf::math::v2 uvScale(regionScale.extract_x() * uvScaleFactor.extract_x(),
			      regionScale.extract_y() * uvScaleFactor.extract_y());

	m_program->vert.m_interface.m_uvOffset.Set(uvOffset);
	m_program->vert.m_interface.m_uvScale.Set(uvScale);
	m_program->vert.m_interface.m_coefficient.Set(m_coefficient);

	m_program->frag.m_interface.m_color.Set(gl->m_color);
	TextureSource texSrc;
	texSrc.m_surface = params.m_textures[0];
	m_program->frag.m_interface.m_texture0.Set(texSrc);

	m_program->vert.m_object.begin();
	m_program->frag.m_object.begin();

	paf::graph::arl::ShaderState *shaderState = gl;
	for (uint32_t i = 0; i < params.m_instances; ++i) {
		VertexArray &va = params.m_vertices[i];
		shaderState->m_dirty_vertex |= va.m_spec->m_kinds;
		shaderState->m_dirty_fragment |= 1;
		sceGxmSetVertexStream(gl->m_context, 0, va.m_buffer->m_memory->m_addr);
		if (params.m_indices != NULL) {
			params.m_indices[i].draw(*gl, paf::graph::arl::TRIANGLES,
						 params.m_count);
		} else {
			VertexArray::DrawArrays(*gl, paf::graph::arl::TRIANGLE_STRIP,
						0, params.m_count);
		}
		shaderState->m_dirty_vertex = 0;
		shaderState->m_dirty_fragment = 0;
	}

	m_program->frag.m_object.end();
	m_program->vert.m_object.end();
	m_program->frag.m_interface.m_texture0.Set(TextureSource());
	return 0;
}

int32_t ListEdgeShader::Draw(PlaneObjDrawParams &params)
{
	if (params.m_textures != NULL && params.m_textures[0].get() != NULL &&
	    params.m_tex_region != NULL)
		return DrawListEdge(params);
	return SIGNIN_EXT_ERROR_INVALID_ARG;
}

void ListEdgeShaderInit(void)
{
	SharedPtr<Shader> shader(new ListEdgeShader());
	s_listEdgeShader = shader;
}

void ListEdgeShaderTerm(void)
{
	s_listEdgeShader.reset();
}

const SharedPtr<Shader> &ListEdgeShaderGet(void)
{
	return s_listEdgeShader;
}
