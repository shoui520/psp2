/*
 * paf::graph::arl's shader programs and shader objects on GXM (the file's name ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/arl_util.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/color.h>
#include <std/string>
#include <std/vector>

namespace paf {

typedef graph::arl::ShaderObject<graph::arl::VertProgram>::Entry VertProgramEntry;
typedef graph::arl::ShaderObject<graph::arl::FragProgram>::Entry FragProgramEntry;

/* Sony's build calls the program caches' push_back rather than inlining it. */
template<>
inline __attribute__((noinline)) int
vector<VertProgramEntry>::push_back(const VertProgramEntry &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) VertProgramEntry(val);
		result = ++count;
	}
	return result;
}

template<>
inline __attribute__((noinline)) int
vector<FragProgramEntry>::push_back(const FragProgramEntry &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) FragProgramEntry(val);
		result = ++count;
	}
	return result;
}

namespace graph {
namespace arl {

VertShaderObject *VertShaderObject::s_current;
FragShaderObject *FragShaderObject::s_current;

void ShaderProgram::unregister_program()
{
	if (m_id != NULL) {
		sceGxmShaderPatcherUnregisterProgram(m_patcher->m_patcher, m_id);
		m_id = NULL;
		m_name = "";
		m_patcher.reset();
	}
}

ShaderProgram::~ShaderProgram()
{
	unregister_program();
}

const SceGxmProgramParameter *ShaderProgram::findParameterBySemantic(SceGxmParameterSemantic semantic,
								     uint32_t index, bool required)
{
	const SceGxmProgramParameter *param = sceGxmProgramFindParameterBySemantic(
		sceGxmShaderPatcherGetProgramFromId(m_id), semantic, index);

	if (param == NULL && !required)
		return NULL;
	return param;
}

bool ShaderProgram::register_program(const string &name, common::SharedPtr<ShaderPatcher> patcher)
{
	m_name = name;
	m_patcher = patcher;
	sceGxmShaderPatcherRegisterProgram(m_patcher->m_patcher, GetProgram(), &m_id);
	if (m_type == VERTEX) {
		for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++) {
			AttributeSemantic semantic = GetAttributeSemantic(i);
			const SceGxmProgramParameter *param =
				findParameterBySemantic(semantic.m_semantic, semantic.m_index, false);

			if (param != NULL)
				m_attribute_index[i] = sceGxmProgramParameterGetResourceIndex(param);
		}
	}
	return true;
}

bool ShaderProgram::loadBinaryFromDisk(const string &name, common::SharedPtr<ShaderPatcher> patcher)
{
	{
		Binary binary;

		load(binary.m_buffer, name, true, true);
		binary.m_data = binary.m_buffer.ptr;
		binary.m_size = binary.m_buffer.count;
		m_binary = binary;
	}
	return register_program(name, patcher);
}

bool ShaderProgram::loadBinaryEmbedded(const string &name, common::SharedPtr<ShaderPatcher> patcher,
				       const uint8_t *data, uint32_t size)
{
	m_binary = Binary(data, size);
	return register_program(name, patcher);
}

/* the category is only checked in Sony's debug builds */
const SceGxmProgramParameter *ShaderProgram::get_param(const char *name,
						       SceGxmParameterCategory category,
						       bool required)
{
	const SceGxmProgramParameter *param =
		sceGxmProgramFindParameterByName(sceGxmShaderPatcherGetProgramFromId(m_id), name);

	if (param == NULL && !required)
		return NULL;
	(void)(sceGxmProgramParameterGetCategory(param) == category);
	return param;
}

uint32_t ShaderProgram::get_param_index(const char *name, SceGxmParameterCategory category,
					bool required)
{
	const SceGxmProgramParameter *param = get_param(name, category, required);

	if (param == NULL && !required)
		return (uint32_t)-1;
	return sceGxmProgramParameterGetResourceIndex(param);
}

uint32_t ShaderProgram::getAttributeParamIndex(const char *name, bool required)
{
	return get_param_index(name, SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE, required);
}

uint32_t ShaderProgram::getUniformParamIndex(const char *name)
{
	return get_param_index(name, SCE_GXM_PARAMETER_CATEGORY_UNIFORM, false);
}

uint32_t ShaderProgram::getSamplerParamIndex(const char *name)
{
	return get_param_index(name, SCE_GXM_PARAMETER_CATEGORY_SAMPLER, false);
}

const SceGxmProgramParameter *ShaderProgram::getUniformParam(const char *name)
{
	return get_param(name, SCE_GXM_PARAMETER_CATEGORY_UNIFORM, false);
}

const SceGxmProgramParameter *ShaderProgram::getSamplerParam(const char *name)
{
	return get_param(name, SCE_GXM_PARAMETER_CATEGORY_SAMPLER, false);
}

VertCreateDependency::VertCreateDependency()
{
	m_attribute_count = 0;
	m_stream_count = 0;
}

/* a stream for each spec given, its attributes those the program has a register for */
void VertCreateDependency::setVertexFormat(common::SharedPtr<VertexArraySpec> spec0,
					   common::SharedPtr<VertexArraySpec> spec1,
					   common::SharedPtr<VertexArraySpec> spec2,
					   common::SharedPtr<VertexArraySpec> spec3)
{
	m_attribute_count = 0;
	m_stream_count = 0;
	for (int32_t i = 0; i != MAX_STREAMS; i++) {
		common::SharedPtr<VertexArraySpec> spec;

		if (i == 0)
			spec = spec0;
		if (i == 1)
			spec = spec1;
		if (i == 2)
			spec = spec2;
		if (i == 3)
			spec = spec3;
		if (spec.get() != NULL) {
			for (uint32_t j = 0; j != spec->m_count; j++) {
				SceGxmVertexAttribute &attribute = m_attributes[m_attribute_count];
				const VertexArraySpec::Attribute &source = spec->m_attributes[j];

				attribute.streamIndex = m_stream_count;
				attribute.offset = source.m_offset;
				attribute.format = source.m_format;
				attribute.componentCount = source.m_components;

				int32_t index = m_program->m_attribute_index[source.m_slot];

				attribute.regIndex = index;
				if (index != -1)
					m_attribute_count++;
			}
			m_streams[m_stream_count].stride = spec->m_stride;
			m_streams[m_stream_count].indexSource = spec->m_index_source;
			m_stream_count++;
		}
	}
}

int VertCreateDependency::createProgram(SceGxmVertexProgram *&program)
{
	return sceGxmShaderPatcherCreateVertexProgram(m_program->m_patcher->m_patcher, m_program->m_id,
						      m_attributes, m_attribute_count, m_streams,
						      m_stream_count, &program);
}

FragCreateDependency::FragCreateDependency()
{
	m_output_format = SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4;
	m_multisample_mode = SCE_GXM_MULTISAMPLE_NONE;
	m_blend_info.colorMask = SCE_GXM_COLOR_MASK_ALL;
	m_blend_info.colorFunc = SCE_GXM_BLEND_FUNC_NONE;
	m_blend_info.alphaFunc = SCE_GXM_BLEND_FUNC_NONE;
	m_blend_info.colorSrc = SCE_GXM_BLEND_FACTOR_ONE;
	m_blend_info.colorDst = SCE_GXM_BLEND_FACTOR_ZERO;
	m_blend_info.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE;
	m_blend_info.alphaDst = SCE_GXM_BLEND_FACTOR_ZERO;
}

/* blends only where the program does not use the native colour */
int FragCreateDependency::createProgram(SceGxmFragmentProgram *&program)
{
	bool native_color = sceGxmProgramIsNativeColorUsed(m_program->GetProgram());

	return sceGxmShaderPatcherCreateFragmentProgram(
		m_program->m_patcher->m_patcher, m_program->m_id, m_output_format,
		m_multisample_mode, native_color ? NULL : &m_blend_info,
		sceGxmVertexProgramGetProgram(m_vert_program->m_program), &program);
}

ShaderParameterBase::ShaderParameterBase(ShaderParameterSetupData &setup)
	: m_param(NULL), m_missing(0), m_setup(&setup), m_index(UNIFORM)
{
}

void ShaderParameterBase::init(const char *name, int32_t unit, int32_t offset)
{
	m_offset = offset;
	if (m_setup->m_program == NULL)
		return;
	if (unit == UNIFORM) {
		m_param = m_setup->m_program->getUniformParam(name);
		m_index = UNIFORM;
	} else {
		m_param = m_setup->m_program->getSamplerParam(name);
		if (m_param != NULL)
			m_index = m_setup->m_program->getSamplerParamIndex(name);
		else
			m_index = -1;
	}
}

template<class Program>
bool ShaderObject<Program>::SetupUniforms()
{
	ArlInstance *gl = m_arl;

	if ((gl->m_dirty_uniforms & Program::DIRTY_UNIFORMS) == 0)
		return false;
	m_interface->m_setup.m_uniform_buffer = NULL;
	Program::ReserveDefaultUniformBuffer(gl->m_context, &m_interface->m_setup.m_uniform_buffer);
	m_interface->Apply();
	m_arl->m_dirty_uniforms &= ~Program::DIRTY_UNIFORMS;
	return true;
}

template<class Program>
inline void ShaderObject<Program>::Setup(ArlInstance &gl, common::SharedPtr<ShaderPatcher> patcher,
				  common::SharedPtr<ShaderProgram> program)
{
	if (program->m_id == NULL)
		program->loadBinaryEmbedded(BinaryPath(string(m_interface->GetName())), patcher,
					    m_interface->GetBinary(), m_interface->GetBinarySize());
	m_arl = &gl;
	m_interface->m_setup.m_context = gl.m_context;
	m_interface->m_setup.m_program = program.get();
	m_interface->m_setup.m_uniform_buffer = NULL;
	m_interface->m_setup.m_object = NULL;
	m_interface->Init();
}

VertShaderObject::VertShaderObject(ShaderInterfaceBase *shader_interface)
	: ShaderObject<VertProgram>(shader_interface)
{
}

VertShaderObject::~VertShaderObject()
{
}

VertCreateDependency VertShaderObject::GetDependency()
{
	return m_dependency;
}

void VertShaderObject::init(ArlInstance &gl, common::SharedPtr<ShaderPatcher> patcher,
			    common::SharedPtr<VertexArraySpec> spec0,
			    common::SharedPtr<VertexArraySpec> spec1,
			    common::SharedPtr<VertexArraySpec> spec2,
			    common::SharedPtr<VertexArraySpec> spec3)
{
	m_dependency.m_program =
		common::SharedPtr<ShaderProgram>(new ShaderProgram(ShaderProgram::VERTEX));
	Setup(gl, patcher, m_dependency.m_program);
	m_dependency.setVertexFormat(spec0, spec1, spec2, spec3);
}

void VertShaderObject::init(ArlInstance &gl, common::SharedPtr<VertexArraySpec> spec0,
			    common::SharedPtr<VertexArraySpec> spec1,
			    common::SharedPtr<VertexArraySpec> spec2,
			    common::SharedPtr<VertexArraySpec> spec3)
{
	ProgramState *state = &gl;

	init(gl, state->m_patcher, spec0, spec1, spec2, spec3);
}

/* the program the patcher makes for the vertex format, kept the first time it is made */
void VertShaderObject::get_prog()
{
	SceGxmVertexProgram *program = NULL;

	m_dependency.createProgram(program);
	for (int32_t i = 0; i != m_programs.count; i++) {
		if (m_programs.ptr[i].m_program == program) {
			m_current = m_programs.ptr[i].m_object;
			sceGxmShaderPatcherReleaseVertexProgram(
				m_dependency.m_program->m_patcher->m_patcher, program);
			return;
		}
	}
	m_current = common::SharedPtr<VertProgram>(new VertProgram(m_dependency.m_program));
	m_current->m_program = program;
	m_programs.push_back(Entry(program, m_current));
}

inline void VertShaderObject::Unbind()
{
	VertProgram *program = m_current.get();
	ArlInstance *gl = m_arl;
	ProgramState *state = gl;

	state->m_vert_program = NULL;
	program->Unbind(gl->m_context);
}

inline void VertShaderObject::UnbindProgram()
{
	s_current->Unbind();
	s_current->m_current.reset();
}

bool VertShaderObject::begin()
{
	if (s_current != this) {
		if (s_current != NULL)
			UnbindProgram();
		s_current = this;
		get_prog();

		ProgramState *state = m_arl;

		state->m_vert_program = m_current.get();
		m_current->Bind(m_arl->m_context);
	}
	m_arl->m_dirty_uniforms |= ProgramState::DIRTY_VERTEX_UNIFORMS;
	m_interface->m_setup.m_uniform_buffer = NULL;
	m_interface->m_setup.m_object = this;
	return SetupUniforms();
}

void VertShaderObject::end()
{
	m_interface->m_setup.m_uniform_buffer = NULL;
	m_interface->m_setup.m_object = NULL;
}

void VertShaderObject::UnbindCurrent()
{
	if (s_current != NULL) {
		UnbindProgram();
		s_current = NULL;
	}
}

FragShaderObject::FragShaderObject(ShaderInterfaceBase *shader_interface)
	: ShaderObject<FragProgram>(shader_interface)
{
}

FragShaderObject::~FragShaderObject()
{
}

FragCreateDependency FragShaderObject::GetDependency()
{
	return m_dependency;
}

void FragShaderObject::setOutputRegisterFormat(SceGxmOutputRegisterFormat format)
{
	m_dependency.m_output_format = format;
}

void FragShaderObject::setMultisampleMode(SceGxmMultisampleMode mode)
{
	m_dependency.m_multisample_mode = mode;
}

void FragShaderObject::init(ArlInstance &gl, common::SharedPtr<ShaderPatcher> patcher)
{
	m_dependency.m_program =
		common::SharedPtr<ShaderProgram>(new ShaderProgram(ShaderProgram::FRAGMENT));
	Setup(gl, patcher, m_dependency.m_program);
}

void FragShaderObject::init(ArlInstance &gl)
{
	ProgramState *state = &gl;

	init(gl, state->m_patcher);
}

/* the program the patcher makes for the vertex program and the blending, kept the first time
   it is made */
void FragShaderObject::get_prog(VertProgram *vert_program)
{
	ArlInstance *gl = m_arl;

	m_dependency.m_blend_info = gl->m_blend_enabled ? gl->m_blend_info : gl->m_default_blend_info;
	gl->m_blend_changed = false;
	m_dependency.m_vert_program = vert_program;

	SceGxmFragmentProgram *program = NULL;

	m_dependency.createProgram(program);
	for (int32_t i = 0; i != m_programs.count; i++) {
		if (m_programs.ptr[i].m_program == program) {
			m_current = m_programs.ptr[i].m_object;
			sceGxmShaderPatcherReleaseFragmentProgram(
				m_dependency.m_program->m_patcher->m_patcher, program);
			return;
		}
	}
	m_current = common::SharedPtr<FragProgram>(new FragProgram(m_dependency.m_program));
	m_current->m_program = program;
	m_programs.push_back(Entry(program, m_current));
}

inline void FragShaderObject::Unbind()
{
	FragProgram *program = m_current.get();
	ArlInstance *gl = m_arl;
	ProgramState *state = gl;

	state->m_frag_program = NULL;
	program->Unbind(gl->m_context);
}

inline void FragShaderObject::UnbindProgram()
{
	s_current->Unbind();
	s_current->m_current.reset();
}

/* the program made again when the blending changed */
bool FragShaderObject::begin()
{
	if (s_current != this || m_arl->m_blend_changed) {
		if (s_current != NULL)
			UnbindProgram();
		s_current = this;
		get_prog(m_arl->m_vert_program);

		ProgramState *state = m_arl;

		state->m_frag_program = m_current.get();
		m_current->Bind(m_arl->m_context);
	}
	m_arl->m_dirty_uniforms |= ProgramState::DIRTY_FRAGMENT_UNIFORMS;
	m_interface->m_setup.m_uniform_buffer = NULL;
	m_interface->m_setup.m_object = this;
	return SetupUniforms();
}

void FragShaderObject::end()
{
	m_interface->m_setup.m_uniform_buffer = NULL;
	m_interface->m_setup.m_object = NULL;
}

void FragShaderObject::UnbindCurrent()
{
	if (s_current != NULL) {
		UnbindProgram();
		s_current = NULL;
	}
}

}
}
}
