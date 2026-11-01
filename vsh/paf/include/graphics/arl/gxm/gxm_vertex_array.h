/*
 * paf::graph::arl's vertex array specs: the attributes of one vertex stream, each named by a
 * predefined slot the shader programs map to a semantic; and the vertex arrays, a spec with
 * its vertices in video memory. Class and method names from the mangled export names
 * (VertexBuffer ours); the file's name and every member name ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_VERTEX_ARRAY_H
#define PAF_GRAPHICS_ARL_GXM_GXM_VERTEX_ARRAY_H

#include <stdint.h>
#include <gxm.h>
#include <common/shared_ptr.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_texture.h>
#include <std/string.h>

namespace paf {
namespace graph {
namespace arl {

class VertexArraySpec {
public:
	/* the attribute slots attribInfo knows, laid out when its file is converted */
	enum Predefined {
	};

	enum {
		MAX_ATTRIBUTES = 12
	};

	class Attribute {
	public:
		/* the slot, which picks the shader program's attribute register */
		int32_t m_slot;
		uint32_t m_components;
		SceGxmAttributeFormat m_format;
		uint32_t m_size;
		uint16_t m_offset;
	};

	void attribInfo(Predefined slot, unsigned short offset);

	Attribute m_attributes[MAX_ATTRIBUTES];
	uint32_t m_count;
	uint32_t m_stride;
	uint32_t m_index_source;
	/* a bit for each kind of attribute added */
	uint32_t m_kinds;
};

class ArlInstance;

/* the video memory a vertex array's vertices are in */
class VertexBuffer {
public:
	VertexBuffer(const common::SharedPtr<VramAllocator> &allocator)
		: m_memory(new SurfaceMemory(allocator))
	{
	}

	common::SharedPtr<SurfaceMemory> m_memory;
};

/* vertices laid out by a spec, copied into video memory */
class VertexArray {
public:
	VertexArray()
	{
	}

	VertexArray(common::SharedPtr<VramAllocator> allocator,
		    const common::SharedPtr<VertexArraySpec> &spec, const void *vertices,
		    uint32_t count)
		: m_spec(spec), m_buffer(new VertexBuffer(allocator))
	{
		uint32_t size = count * spec->m_stride;
		SurfaceMemory *memory = m_buffer->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, size);
			memory->m_size = size;
		}
		sce_paf_memcpy(m_buffer->m_memory->m_addr, vertices, size);
	}

	static void DrawArrays(ArlInstance &gl, Primitive primitive, int32_t first,
			       uint32_t count);

	common::SharedPtr<VertexArraySpec> m_spec;
	common::SharedPtr<VertexBuffer> m_buffer;
};

}
}
}

#endif
