/*
 * paf::graph::arl's VertexArraySpec: the predefined vertex layouts, and the front end's spec
 * of each (the file's name ours).
 */
#include <stdint.h>
#include <gxm.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/color.h>

namespace paf {
namespace graph {
namespace arl {

int32_t VertexArraySpec::s_count;

/* two, three and four float attributes interleaved; the first at the offset, the others
   after the first's floats as if it were at 0 */
static void AddFloatAttributes(VertexArraySpec &spec, int32_t slot0, uint32_t components0,
			       int32_t slot1, uint32_t components1, uint16_t offset)
{
	uint32_t stride = (components0 + components1) * sizeof(float);

	spec.AddAttribute(slot0, components0, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride, offset);
	spec.AddAttribute(slot1, components1, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride,
			  components0 * sizeof(float));
}

static void AddFloatAttributes(VertexArraySpec &spec, int32_t slot0, uint32_t components0,
			       int32_t slot1, uint32_t components1, int32_t slot2,
			       uint32_t components2, uint16_t offset)
{
	uint32_t stride = (components0 + components1 + components2) * sizeof(float);

	spec.AddAttribute(slot0, components0, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride, offset);
	spec.AddAttribute(slot1, components1, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride,
			  components0 * sizeof(float));
	spec.AddAttribute(slot2, components2, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride,
			  (components0 + components1) * sizeof(float));
}

static void AddFloatAttributes(VertexArraySpec &spec, int32_t slot0, uint32_t components0,
			       int32_t slot1, uint32_t components1, int32_t slot2,
			       uint32_t components2, int32_t slot3, uint32_t components3,
			       uint16_t offset)
{
	uint32_t stride = (components0 + components1 + components2 + components3) * sizeof(float);

	spec.AddAttribute(slot0, components0, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride, offset);
	spec.AddAttribute(slot1, components1, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride,
			  components0 * sizeof(float));
	spec.AddAttribute(slot2, components2, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride,
			  (components0 + components1) * sizeof(float));
	spec.AddAttribute(slot3, components3, SCE_GXM_ATTRIBUTE_FORMAT_F32, stride,
			  (components0 + components1 + components2) * sizeof(float));
}

void VertexArraySpec::attribInfo(Predefined predefined, unsigned short offset)
{
	switch (predefined) {
	case ATTRIB0_F2:
		AddAttribute(0, 2, SCE_GXM_ATTRIBUTE_FORMAT_F32, 8, offset);
		break;
	case ATTRIB0_F3:
		AddAttribute(0, 3, SCE_GXM_ATTRIBUTE_FORMAT_F32, 12, offset);
		break;
	case ATTRIB0_F4:
		AddAttribute(0, 4, SCE_GXM_ATTRIBUTE_FORMAT_F32, 16, offset);
		break;
	case ATTRIB0_H4:
		AddAttribute(0, 4, SCE_GXM_ATTRIBUTE_FORMAT_F16, 8, offset);
		break;
	case ATTRIB3_F2:
		AddAttribute(3, 2, SCE_GXM_ATTRIBUTE_FORMAT_F32, 8, offset);
		break;
	case ATTRIB3_F3:
		AddAttribute(3, 3, SCE_GXM_ATTRIBUTE_FORMAT_F32, 12, offset);
		break;
	case ATTRIB3_F4:
		AddAttribute(3, 4, SCE_GXM_ATTRIBUTE_FORMAT_F32, 16, offset);
		break;
	case ATTRIB4_F2:
		AddAttribute(4, 2, SCE_GXM_ATTRIBUTE_FORMAT_F32, 8, offset);
		break;
	case ATTRIB4_F3:
		AddAttribute(4, 3, SCE_GXM_ATTRIBUTE_FORMAT_F32, 12, offset);
		break;
	case ATTRIB4_F4:
		AddAttribute(4, 4, SCE_GXM_ATTRIBUTE_FORMAT_F32, 16, offset);
		break;
	case ATTRIB2_F2:
		AddAttribute(2, 2, SCE_GXM_ATTRIBUTE_FORMAT_F32, 8, offset);
		break;
	case ATTRIB2_F3:
		AddAttribute(2, 3, SCE_GXM_ATTRIBUTE_FORMAT_F32, 12, offset);
		break;
	case ATTRIB2_F4:
		AddAttribute(2, 4, SCE_GXM_ATTRIBUTE_FORMAT_F32, 16, offset);
		break;
	case ATTRIB1_F3:
		AddAttribute(1, 3, SCE_GXM_ATTRIBUTE_FORMAT_F32, 12, offset);
		break;
	case ATTRIB0_F4_ATTRIB2_F4:
		AddFloatAttributes(*this, 0, 4, 2, 4, offset);
		break;
	case ATTRIB0_F3_ATTRIB1_F3:
		AddFloatAttributes(*this, 0, 3, 1, 3, offset);
		break;
	case ATTRIB0_F4_ATTRIB1_F4:
		AddFloatAttributes(*this, 0, 4, 1, 4, offset);
		break;
	case ATTRIB0_F2_ATTRIB3_F2:
		AddFloatAttributes(*this, 0, 2, 3, 2, offset);
		break;
	case ATTRIB0_F2_ATTRIB3_F3:
		AddFloatAttributes(*this, 0, 2, 3, 3, offset);
		break;
	case ATTRIB0_F2_ATTRIB3_F4:
		AddFloatAttributes(*this, 0, 2, 3, 4, offset);
		break;
	case ATTRIB0_F3_ATTRIB3_F2:
		AddFloatAttributes(*this, 0, 3, 3, 2, offset);
		break;
	case ATTRIB0_F3_ATTRIB3_F4:
		AddFloatAttributes(*this, 0, 3, 3, 4, offset);
		break;
	case ATTRIB0_F3_ATTRIB3_F4_ATTRIB4_F2:
		AddFloatAttributes(*this, 0, 3, 3, 4, 4, 2, offset);
		break;
	case ATTRIB0_F3_ATTRIB3_F2_ATTRIB1_F3:
		AddFloatAttributes(*this, 0, 3, 3, 2, 1, 3, offset);
		break;
	case ATTRIB0_F3_ATTRIB3_F2_ATTRIB2_F4:
		AddFloatAttributes(*this, 0, 3, 3, 2, 2, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB3_F4:
		AddFloatAttributes(*this, 0, 4, 3, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB5_F4:
		AddFloatAttributes(*this, 0, 4, 5, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB3_F4_ATTRIB4_F4:
		AddFloatAttributes(*this, 0, 4, 3, 4, 4, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB4_F4:
		AddFloatAttributes(*this, 0, 4, 4, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB1_F4_ATTRIB3_F4:
		AddFloatAttributes(*this, 0, 4, 1, 4, 3, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB2_F4_ATTRIB3_F4:
		AddFloatAttributes(*this, 0, 4, 2, 4, 3, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB2_F4_ATTRIB4_F4:
		AddFloatAttributes(*this, 0, 4, 2, 4, 4, 4, offset);
		break;
	case ATTRIB0_F4_ATTRIB2_F4_ATTRIB3_F4_ATTRIB4_F4:
		AddFloatAttributes(*this, 0, 4, 2, 4, 3, 4, 4, 4, offset);
		break;
	case ATTRIB3_F4_ATTRIB4_F4:
		AddFloatAttributes(*this, 3, 4, 4, 4, offset);
		break;
	case ATTRIB3_F4_ATTRIB4_F4_ATTRIB5_F4:
		AddFloatAttributes(*this, 3, 4, 4, 4, 5, 4, offset);
		break;
	case ATTRIB3_F4_ATTRIB4_F4_ATTRIB5_F4_ATTRIB6_F4:
		AddFloatAttributes(*this, 3, 4, 4, 4, 5, 4, 6, 4, offset);
		break;
	}
}

}
}
}

using namespace paf;
using namespace paf::graph::arl;

common::SharedPtr<VertexArraySpec> ScePafGraphics_4D1E289A(ShaderState *state,
							   VertexArraySpec::Predefined predefined,
							   SceGxmIndexSource index_source)
{
	int32_t index;

	switch (index_source) {
	case SCE_GXM_INDEX_SOURCE_INDEX_16BIT:
		index = 0;
		break;
	case SCE_GXM_INDEX_SOURCE_INDEX_32BIT:
		index = 2;
		break;
	case SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT:
		index = 1;
		break;
	case SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT:
		index = 3;
		break;
	default:
		return common::SharedPtr<VertexArraySpec>();
	}

	common::SharedPtr<VertexArraySpec> *spec = &state->m_specs[predefined][index];

	if (spec->get() == NULL) {
		VertexArraySpec *made = new VertexArraySpec(index_source);

		made->attribInfo(predefined, 0);
		*spec = common::SharedPtr<VertexArraySpec>(made);
	}
	return *spec;
}
