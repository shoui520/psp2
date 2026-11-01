/*
 * paf::graph::arl's shaders on GXM: a shader program is a GXM program binary registered with
 * the shader patcher; a shader object makes vertex or fragment programs from it for the
 * vertex format or blending in use, keeping each one made, and binds them; its interface
 * names the binary and sets its parameters. Class and method names from the mangled export
 * names; the file's name, the template, the state classes and every member name ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_SHADER_H
#define PAF_GRAPHICS_ARL_GXM_GXM_SHADER_H

#include <stddef.h>
#include <stdint.h>
#include <gxm.h>
#include <common/shared_ptr.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <std/map>
#include <std/string>
#include <std/vector>

namespace paf {
namespace graph {
namespace arl {

class ArlInstance;
class ShaderObjectBase;
class VertProgram;
class FragProgram;

/* the GXM shader patcher, shared by the shader programs */
class ShaderPatcher {
public:
	ShaderPatcher(SceGxmShaderPatcher *patcher) : m_patcher(patcher)
	{
	}

	SceGxmShaderPatcher *m_patcher;
};

/* the programs bound, which shaders' uniforms changed, and the patcher the shaders use */
class ProgramState {
public:
	enum {
		DIRTY_FRAGMENT_UNIFORMS = 1 << 0,
		DIRTY_VERTEX_UNIFORMS = 1 << 1
	};

	ProgramState()
	{
		m_vert_program = NULL;
		m_frag_program = NULL;
		m_dirty_uniforms = 0;
	}

	VertProgram *m_vert_program;
	FragProgram *m_frag_program;
	uint32_t m_dirty_uniforms;
	common::SharedPtr<ShaderPatcher> m_patcher;
};

/* a GXM program binary registered with the shader patcher */
class ShaderProgram {
public:
	enum Type {
		FRAGMENT,
		VERTEX
	};

	enum {
		MAX_ATTRIBUTES = VertexArraySpec::MAX_ATTRIBUTES
	};

	/* the binary: the buffer read from disk, and the bytes registered */
	class Binary {
	public:
		Binary() : m_data(NULL), m_size(0)
		{
		}

		Binary(const uint8_t *data, uint32_t size) : m_data(data), m_size(size)
		{
		}

		vector<uint8_t> m_buffer;
		const uint8_t *m_data;
		uint32_t m_size;
	};

	/* the semantic and its index a vertex attribute slot is bound to */
	class AttributeSemantic {
	public:
		AttributeSemantic() : m_semantic(SCE_GXM_PARAMETER_SEMANTIC_NONE), m_index((uint32_t)-1)
		{
		}

		AttributeSemantic(SceGxmParameterSemantic semantic, uint32_t index)
			: m_semantic(semantic), m_index(index)
		{
		}

		SceGxmParameterSemantic m_semantic;
		uint32_t m_index;
	};

	ShaderProgram(Type type) : m_type(type), m_id(NULL)
	{
		if (m_type == VERTEX) {
			for (uint32_t i = 0; i < MAX_ATTRIBUTES; i++)
				m_attribute_index[i] = -1;
		}
	}

	~ShaderProgram();

	void unregister_program();
	const SceGxmProgramParameter *findParameterBySemantic(SceGxmParameterSemantic semantic,
							      uint32_t index, bool required);
	bool register_program(const string &name, common::SharedPtr<ShaderPatcher> patcher);
	bool loadBinaryFromDisk(const string &name, common::SharedPtr<ShaderPatcher> patcher);
	bool loadBinaryEmbedded(const string &name, common::SharedPtr<ShaderPatcher> patcher,
				const uint8_t *data, uint32_t size);
	const SceGxmProgramParameter *get_param(const char *name,
						SceGxmParameterCategory category, bool required);
	uint32_t get_param_index(const char *name, SceGxmParameterCategory category,
				 bool required);
	uint32_t getAttributeParamIndex(const char *name, bool required);
	uint32_t getUniformParamIndex(const char *name);
	uint32_t getSamplerParamIndex(const char *name);
	const SceGxmProgramParameter *getUniformParam(const char *name);
	const SceGxmProgramParameter *getSamplerParam(const char *name);

	const SceGxmProgram *GetProgram() const
	{
		return m_binary.m_size != 0 ? (const SceGxmProgram *)m_binary.m_data : NULL;
	}

	/* the semantic each attribute slot of a vertex array spec is bound to; Sony's build keeps
	   it out of line */
	static __attribute__((noinline)) AttributeSemantic GetAttributeSemantic(int32_t slot)
	{
		static map<int32_t, AttributeSemantic> s_semantics;

		if (s_semantics.begin() == s_semantics.end()) {
			s_semantics[0] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_POSITION, 0);
			s_semantics[1] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_NORMAL, 0);
			s_semantics[2] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_COLOR, 0);
			s_semantics[2] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_COLOR, 0);
			s_semantics[3] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 0);
			s_semantics[4] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 1);
			s_semantics[5] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 2);
			s_semantics[6] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 3);
			s_semantics[7] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 4);
			s_semantics[8] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 5);
			s_semantics[9] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 6);
			s_semantics[10] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 7);
			s_semantics[11] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 8);
			s_semantics[12] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TEXCOORD, 9);
			s_semantics[10] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_TANGENT, 0);
			s_semantics[11] = AttributeSemantic(SCE_GXM_PARAMETER_SEMANTIC_BINORMAL, 0);
		}
		return s_semantics.find(slot)->second;
	}

	Type m_type;
	Binary m_binary;
	SceGxmShaderPatcherId m_id;
	string m_name;
	common::SharedPtr<ShaderPatcher> m_patcher;
	/* the attribute register of each slot, -1 where the program has none */
	int32_t m_attribute_index[MAX_ATTRIBUTES];
};

/* a vertex program made from a shader program, released when the last owner lets it go */
class VertProgram {
public:
	typedef SceGxmVertexProgram GxmProgram;

	enum {
		DIRTY_UNIFORMS = ProgramState::DIRTY_VERTEX_UNIFORMS
	};

	VertProgram(common::SharedPtr<ShaderProgram> shader)
		: m_bound(false), m_shader(shader), m_program(NULL)
	{
	}

	~VertProgram()
	{
		if (m_program != NULL) {
			sceGxmShaderPatcherReleaseVertexProgram(m_shader->m_patcher->m_patcher,
								m_program);
			m_program = NULL;
		}
	}

	static int ReserveDefaultUniformBuffer(SceGxmContext *context, void **buffer)
	{
		return sceGxmReserveVertexDefaultUniformBuffer(context, buffer);
	}

	void Bind(SceGxmContext *context)
	{
		m_bound = true;
		sceGxmSetVertexProgram(context, m_program);
	}

	void Unbind(SceGxmContext *context)
	{
		m_bound = false;
		sceGxmSetVertexProgram(context, NULL);
	}

	bool m_bound;
	common::SharedPtr<ShaderProgram> m_shader;
	SceGxmVertexProgram *m_program;
};

/* a fragment program made from a shader program, released when the last owner lets it go */
class FragProgram {
public:
	typedef SceGxmFragmentProgram GxmProgram;

	enum {
		DIRTY_UNIFORMS = ProgramState::DIRTY_FRAGMENT_UNIFORMS
	};

	FragProgram(common::SharedPtr<ShaderProgram> shader)
		: m_bound(false), m_shader(shader), m_program(NULL)
	{
	}

	~FragProgram()
	{
		if (m_program != NULL) {
			sceGxmShaderPatcherReleaseFragmentProgram(m_shader->m_patcher->m_patcher,
								  m_program);
			m_program = NULL;
		}
	}

	static int ReserveDefaultUniformBuffer(SceGxmContext *context, void **buffer)
	{
		return sceGxmReserveFragmentDefaultUniformBuffer(context, buffer);
	}

	void Bind(SceGxmContext *context)
	{
		m_bound = true;
		sceGxmSetFragmentProgram(context, m_program);
	}

	void Unbind(SceGxmContext *context)
	{
		m_bound = false;
		sceGxmSetFragmentProgram(context, NULL);
	}

	bool m_bound;
	common::SharedPtr<ShaderProgram> m_shader;
	SceGxmFragmentProgram *m_program;
};

/* what a vertex program is made for: the attributes and streams of the vertex format */
class VertCreateDependency {
public:
	enum {
		MAX_ATTRIBUTES = VertexArraySpec::MAX_ATTRIBUTES,
		MAX_STREAMS = 4
	};

	VertCreateDependency();

	void setVertexFormat(common::SharedPtr<VertexArraySpec> spec0,
			     common::SharedPtr<VertexArraySpec> spec1,
			     common::SharedPtr<VertexArraySpec> spec2,
			     common::SharedPtr<VertexArraySpec> spec3);
	int createProgram(SceGxmVertexProgram *&program);

	SceGxmVertexAttribute m_attributes[MAX_ATTRIBUTES];
	uint32_t m_attribute_count;
	SceGxmVertexStream m_streams[MAX_STREAMS];
	uint32_t m_stream_count;
	common::SharedPtr<ShaderProgram> m_program;
};

/* what a fragment program is made for: the vertex program, the output and the blending */
class FragCreateDependency {
public:
	FragCreateDependency();

	int createProgram(SceGxmFragmentProgram *&program);

	VertProgram *m_vert_program;
	SceGxmMultisampleMode m_multisample_mode;
	SceGxmBlendInfo m_blend_info;
	SceGxmOutputRegisterFormat m_output_format;
	common::SharedPtr<ShaderProgram> m_program;
};

/* what a shader's parameters are set up and written with */
class ShaderParameterSetupData {
public:
	SceGxmContext *m_context;
	ShaderProgram *m_program;
	/* the default uniform buffer reserved for the draw */
	void *m_uniform_buffer;
	ShaderObjectBase *m_object;
};

/* a parameter of a shader, found by name */
class ShaderParameterBase {
public:
	enum {
		/* the unit init takes for a uniform, and the index a uniform keeps */
		UNIFORM = -2
	};

	ShaderParameterBase(ShaderParameterSetupData &setup);

	void init(const char *name, int32_t unit, int32_t offset);

	const SceGxmProgramParameter *m_param;
	uint32_t m_reserved;
	ShaderParameterSetupData *m_setup;
	/* the sampler's resource index, -1 where the program has none, UNIFORM for a uniform */
	int32_t m_index;
	int32_t m_offset;
};

/* a shader's binary and its parameters, set up when the shader object is */
class ShaderInterfaceBase {
public:
	virtual ~ShaderInterfaceBase()
	{
	}

	virtual void Init() = 0;
	/* writes the parameters into the uniform buffer reserved */
	virtual void Apply() = 0;
	virtual void Unused() = 0;
	virtual const char *GetName() = 0;
	virtual const uint8_t *GetBinary() = 0;
	virtual uint32_t GetBinarySize() = 0;

	ShaderParameterSetupData m_setup;
};

class ShaderObjectBase {
public:
	virtual ~ShaderObjectBase()
	{
	}

	/* reserves the uniform buffer and writes the parameters when they changed */
	virtual bool SetupUniforms() = 0;
};

/* a vertex or fragment shader object: the programs it made, by GXM program, and the one bound */
template<class Program>
class ShaderObject : public ShaderObjectBase {
public:
	typedef typename Program::GxmProgram GxmProgram;

	class Entry {
	public:
		Entry(GxmProgram *program, const common::SharedPtr<Program> &object)
			: m_program(program), m_object(object)
		{
		}

		GxmProgram *m_program;
		common::SharedPtr<Program> m_object;
	};

	ShaderObject(ShaderInterfaceBase *shader_interface)
		: m_arl(NULL), m_interface(shader_interface)
	{
	}

	virtual ~ShaderObject()
	{
	}

	virtual bool SetupUniforms();

	/* loads the program's binary the first time, and sets up the interface's parameters */
	void Setup(ArlInstance &gl, common::SharedPtr<ShaderPatcher> patcher,
		   common::SharedPtr<ShaderProgram> program);

	/* where the binary of a shader is on the host; Sony's build keeps it out of line */
	static __attribute__((noinline)) string BinaryPath(const string &name)
	{
		string path = string("../resource.psp2/cg/") + name + string(".bin");

		return string("host0:") + path;
	}

	ArlInstance *m_arl;
	ShaderInterfaceBase *m_interface;
	vector<Entry> m_programs;
	common::SharedPtr<Program> m_current;
};

class VertShaderObject : public ShaderObject<VertProgram> {
public:
	VertShaderObject(ShaderInterfaceBase *shader_interface);
	virtual ~VertShaderObject();

	VertCreateDependency GetDependency();
	void init(ArlInstance &gl, common::SharedPtr<ShaderPatcher> patcher,
		  common::SharedPtr<VertexArraySpec> spec0, common::SharedPtr<VertexArraySpec> spec1,
		  common::SharedPtr<VertexArraySpec> spec2, common::SharedPtr<VertexArraySpec> spec3);
	void init(ArlInstance &gl, common::SharedPtr<VertexArraySpec> spec0,
		  common::SharedPtr<VertexArraySpec> spec1, common::SharedPtr<VertexArraySpec> spec2,
		  common::SharedPtr<VertexArraySpec> spec3);
	void get_prog();
	bool begin();
	void end();

	/* the program bound last unbound, when a scene ends */
	static void UnbindCurrent();

	VertCreateDependency m_dependency;

private:
	/* unbinds the program, and that of the shader object bound last */
	void Unbind();
	static void UnbindProgram();

	static VertShaderObject *s_current;
};

class FragShaderObject : public ShaderObject<FragProgram> {
public:
	FragShaderObject(ShaderInterfaceBase *shader_interface);
	virtual ~FragShaderObject();

	FragCreateDependency GetDependency();
	void setOutputRegisterFormat(SceGxmOutputRegisterFormat format);
	void setMultisampleMode(SceGxmMultisampleMode mode);
	void init(ArlInstance &gl, common::SharedPtr<ShaderPatcher> patcher);
	void init(ArlInstance &gl);
	void get_prog(VertProgram *vert_program);
	bool begin();
	void end();

	/* the program bound last unbound, when a scene ends */
	static void UnbindCurrent();

	FragCreateDependency m_dependency;

private:
	/* unbinds the program, and that of the shader object bound last */
	void Unbind();
	static void UnbindProgram();

	static FragShaderObject *s_current;
};

}
}
}

#endif
