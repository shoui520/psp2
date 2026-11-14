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
	/* the vertex layouts attribInfo makes: each attribute by its slot (ATTRIBn) and its
	   components (Fn floats, H4 four halves), interleaved in this order (names ours) */
	enum Predefined {
		ATTRIB0_F2 = 0,
		ATTRIB0_F3 = 1,
		ATTRIB0_F4 = 2,
		ATTRIB0_H4 = 3,
		ATTRIB3_F2 = 4,
		ATTRIB3_F3 = 5,
		ATTRIB3_F4 = 6,
		ATTRIB4_F2 = 7,
		ATTRIB4_F3 = 8,
		ATTRIB4_F4 = 9,
		ATTRIB2_F2 = 10,
		ATTRIB2_F3 = 11,
		ATTRIB2_F4 = 12,
		ATTRIB1_F3 = 13,
		ATTRIB0_F4_ATTRIB2_F4 = 14,
		ATTRIB0_F3_ATTRIB1_F3 = 15,
		ATTRIB0_F4_ATTRIB1_F4 = 16,
		ATTRIB0_F2_ATTRIB3_F2 = 17,
		ATTRIB0_F2_ATTRIB3_F3 = 18,
		ATTRIB0_F2_ATTRIB3_F4 = 19,
		ATTRIB0_F3_ATTRIB3_F2 = 20,
		ATTRIB0_F3_ATTRIB3_F4 = 21,
		ATTRIB0_F3_ATTRIB3_F4_ATTRIB4_F2 = 22,
		ATTRIB0_F3_ATTRIB3_F2_ATTRIB1_F3 = 23,
		ATTRIB0_F3_ATTRIB3_F2_ATTRIB2_F4 = 24,
		ATTRIB0_F4_ATTRIB3_F4 = 25,
		ATTRIB0_F4_ATTRIB5_F4 = 26,
		ATTRIB0_F4_ATTRIB3_F4_ATTRIB4_F4 = 27,
		ATTRIB0_F4_ATTRIB4_F4 = 28,
		ATTRIB0_F4_ATTRIB1_F4_ATTRIB3_F4 = 29,
		ATTRIB0_F4_ATTRIB2_F4_ATTRIB3_F4 = 30,
		ATTRIB0_F4_ATTRIB2_F4_ATTRIB4_F4 = 31,
		ATTRIB0_F4_ATTRIB2_F4_ATTRIB3_F4_ATTRIB4_F4 = 32,
		ATTRIB3_F4_ATTRIB4_F4 = 33,
		ATTRIB3_F4_ATTRIB4_F4_ATTRIB5_F4 = 34,
		ATTRIB3_F4_ATTRIB4_F4_ATTRIB5_F4_ATTRIB6_F4 = 35
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
		/* the stream's stride, which the first attribute added gives the spec */
		uint32_t m_stride;
		uint16_t m_offset;
	};

	VertexArraySpec(SceGxmIndexSource index_source)
	{
		for (int32_t i = 0; i < MAX_ATTRIBUTES; i++) {
			m_attributes[i].m_slot = -1;
			m_attributes[i].m_components = 0;
			m_attributes[i].m_format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
			m_attributes[i].m_stride = 0;
			m_attributes[i].m_offset = 0;
		}
		m_count = 0;
		m_stride = 0;
		m_index_source = index_source;
		m_kinds = 0;
		s_count++;
	}

	~VertexArraySpec()
	{
		s_count--;
	}

	void AddAttribute(int32_t slot, uint32_t components, SceGxmAttributeFormat format,
			  uint32_t stride, uint16_t offset)
	{
		Attribute &attribute = m_attributes[m_count++];

		attribute.m_slot = slot;
		attribute.m_components = components;
		attribute.m_format = format;
		attribute.m_stride = stride;
		attribute.m_offset = offset;
		if (m_stride == 0)
			m_stride = stride;
		m_kinds |= 1 << slot;
	}

	void attribInfo(Predefined predefined, unsigned short offset);

	/* the specs alive (name ours) */
	static int32_t s_count;

	Attribute m_attributes[MAX_ATTRIBUTES];
	uint32_t m_count;
	uint32_t m_stride;
	uint32_t m_index_source;
	/* a bit for each slot added */
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
		VertexBuffer *buffer = m_buffer.get();
		SurfaceMemory *memory = buffer->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, size);
			memory->m_size = size;
		}
		sce_paf_memcpy(buffer->m_memory->m_addr, vertices, size);
	}

	/* the same, out of line: Sony's TextGraphicsImpl::Draw calls it for its quad, where every
	   other user has it inline (the tag and its name ours) */
	struct OutOfLine {
	};

	__attribute__((noinline)) VertexArray(common::SharedPtr<VramAllocator> allocator,
					      const common::SharedPtr<VertexArraySpec> &spec,
					      const void *vertices, uint32_t count, OutOfLine)
		: m_spec(spec), m_buffer(new VertexBuffer(allocator))
	{
		uint32_t size = count * spec->m_stride;
		VertexBuffer *buffer = m_buffer.get();
		SurfaceMemory *memory = buffer->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, size);
			memory->m_size = size;
		}
		sce_paf_memcpy(buffer->m_memory->m_addr, vertices, size);
	}

	/* room for count vertices, put in later (the page-turn meshes') */
	VertexArray(common::SharedPtr<VramAllocator> allocator,
		    common::SharedPtr<VertexArraySpec> spec, uint32_t count)
		: m_spec(spec), m_buffer(new VertexBuffer(allocator))
	{
		uint32_t size = count * spec->m_stride;
		SurfaceMemory *memory = m_buffer->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, size);
			memory->m_size = size;
		}
	}

	/* a buffer the vertices are put in later (the wave meshes') */
	VertexArray(common::SharedPtr<VertexArraySpec> spec, common::SharedPtr<VertexBuffer> buffer)
		: m_spec(spec), m_buffer(buffer)
	{
	}

	static void DrawArrays(ArlInstance &gl, Primitive primitive, int32_t first,
			       uint32_t count);

	common::SharedPtr<VertexArraySpec> m_spec;
	common::SharedPtr<VertexBuffer> m_buffer;
};

/* the video memory an index array's indices are in (name ours) */
class IndexMemory {
public:
	IndexMemory(const common::SharedPtr<VramAllocator> &allocator)
		: m_memory(new SurfaceMemory(allocator))
	{
	}

	common::SharedPtr<SurfaceMemory> m_memory;
};

/* indices in video memory; the front end's is the 16-bit indices 0 to 65535, which
   VertexArray::DrawArrays draws a range of vertices with */
class IndexArray {
public:
	enum {
		SIZE = 0x20000
	};

	/* nothing, its format unset */
	IndexArray()
	{
	}

	IndexArray(common::SharedPtr<VramAllocator> allocator)
		: m_memory(new IndexMemory(allocator)), m_format(SCE_GXM_INDEX_FORMAT_U16)
	{
		SurfaceMemory *memory = m_memory->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, SIZE);
			memory->m_size = SIZE;
		}

		uint16_t *index = (uint16_t *)m_memory->m_memory->m_addr;

		for (uint32_t i = 0; i < 0x10000; i++)
			*index++ = i;
	}

	/* count indices copied in, when there are any (PlaneObj's meshes) */
	IndexArray(common::SharedPtr<VramAllocator> allocator, const uint16_t *indices,
		   uint32_t count)
		: m_memory(new IndexMemory(allocator)), m_format(SCE_GXM_INDEX_FORMAT_U16)
	{
		uint32_t size = count * sizeof(uint16_t);
		IndexMemory *index_memory = m_memory.get();
		SurfaceMemory *memory = index_memory->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, size);
			memory->m_size = size;
		}
		if (indices != NULL)
			sce_paf_memcpy(index_memory->m_memory->m_addr, indices, size);
	}

	/* count 32-bit indices copied in, when there are any (the wave meshes past 65536
	   vertices) */
	IndexArray(common::SharedPtr<VramAllocator> allocator, const uint32_t *indices,
		   uint32_t count)
		: m_memory(new IndexMemory(allocator)), m_format(SCE_GXM_INDEX_FORMAT_U32)
	{
		uint32_t size = count * sizeof(uint32_t);
		SurfaceMemory *memory = m_memory->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, size);
			memory->m_size = size;
		}
		if (indices != NULL)
			sce_paf_memcpy(m_memory->m_memory->m_addr, indices, size);
	}

	/* count indices of the format copied in, when there are any (the wave meshes' lines) */
	IndexArray(common::SharedPtr<VramAllocator> allocator, const void *indices, uint32_t count,
		   SceGxmIndexFormat format)
		: m_memory(new IndexMemory(allocator)), m_format(format)
	{
		uint32_t size = count * (format == SCE_GXM_INDEX_FORMAT_U16 ? 2 : 4);
		SurfaceMemory *memory = m_memory->m_memory.get();

		if (memory->m_addr == NULL) {
			memory->m_allocator->Alloc(memory, size);
			memory->m_size = size;
		}
		if (indices != NULL)
			sce_paf_memcpy(m_memory->m_memory->m_addr, indices, size);
	}

	void draw(ArlInstance &gl, Primitive primitive, uint32_t count);

	common::SharedPtr<IndexMemory> m_memory;
	SceGxmIndexFormat m_format;
};

}
}
}

/* the instanced draws, the index array repeated every wrap indices (names of the parameters
   ours) */
extern "C" {
void ScePafGraphics_72FB29E9(paf::graph::arl::ArlInstance *gl,
			     paf::graph::arl::Primitive primitive, uint32_t count, uint32_t wrap);
void ScePafGraphics_AE5BBA4B(paf::graph::arl::IndexArray *indices,
			     paf::graph::arl::ArlInstance *gl,
			     paf::graph::arl::Primitive primitive, uint32_t count, uint32_t wrap);
}

#endif
