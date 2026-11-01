/*
 * paf::graph::arl's vertex array specs: the attributes of one vertex stream, each named by a
 * predefined slot the shader programs map to a semantic. Class and method names from the
 * mangled export names; the file's name and every member name ours.
 */
#ifndef PAF_GRAPHICS_ARL_GXM_GXM_VERTEX_ARRAY_H
#define PAF_GRAPHICS_ARL_GXM_GXM_VERTEX_ARRAY_H

#include <stdint.h>
#include <gxm.h>

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

}
}
}

#endif
