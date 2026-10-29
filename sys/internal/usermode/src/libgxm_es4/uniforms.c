/*
 * SceGxm: uniform data written in the layout of a program's declaration.
 *
 * Floating-point vector array elements start on a 64-bit boundary, all others on a
 * 32-bit boundary: a vector of 3 takes the room of 4, 16-bit scalars and F16 vectors
 * of 2 take 32 and 64 bits, 8-bit scalars and vectors of 2 take 32 bits.
 */

#include "gxm_internal.h"

/* the next component of an array of 3-component vectors, each padded to 4 */
static inline SceUInt32 gxmUniformNextVec3Index(SceUInt32 index)
{
	index++;
	if ((index & 3) == 3)
		index++;
	return index;
}

int sceGxmSetUniformDataF(void *uniformBuffer, const SceGxmProgramParameter *parameter,
			  SceUInt32 componentOffset, SceUInt32 componentCount,
			  const float *sourceData)
{
	SceUInt32 info, vectorSize, type, maxComponentCount, index, i;

	info = parameter->info;
	vectorSize = (info & GXM_PARAMETER_COMPONENT_COUNT) >> 8;
	type = (info & GXM_PARAMETER_TYPE) >> 4;
	maxComponentCount = parameter->arraySize * vectorSize;
	if (uniformBuffer == NULL || sourceData == NULL)
		return SCE_GXM_ERROR_INVALID_POINTER;
	if ((info & GXM_PARAMETER_CATEGORY) != SCE_GXM_PARAMETER_CATEGORY_UNIFORM ||
	    componentOffset + componentCount > maxComponentCount)
		return SCE_GXM_ERROR_INVALID_VALUE;

	switch (type) {
	case SCE_GXM_PARAMETER_TYPE_F32: {
		float *data = (float *)uniformBuffer + parameter->resourceIndex;

		if (vectorSize == 3) {
			index = componentOffset + componentOffset / 3;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index = gxmUniformNextVec3Index(index);
			}
		} else {
			for (i = 0; i < componentCount; i++)
				data[componentOffset + i] = sourceData[i];
		}
		break;
	}
	case SCE_GXM_PARAMETER_TYPE_F16: {
		__fp16 *data = (__fp16 *)((SceUInt32 *)uniformBuffer + parameter->resourceIndex);

		switch (vectorSize) {
		case 3:
			index = componentOffset + componentOffset / 3;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index = gxmUniformNextVec3Index(index);
			}
			break;
		case 4:
			for (i = 0; i < componentCount; i++)
				data[componentOffset + i] = sourceData[i];
			break;
		case 2:
			for (i = 0; i < componentCount; i++) {
				index = componentOffset + i;
				data[index + (index & ~1U)] = sourceData[i];
			}
			break;
		default:
			index = componentOffset * 2;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index += 2;
			}
			break;
		}
		break;
	}
	case SCE_GXM_PARAMETER_TYPE_C10:
		return SCE_GXM_ERROR_UNSUPPORTED;
	case SCE_GXM_PARAMETER_TYPE_U32: {
		SceUInt32 *data = (SceUInt32 *)uniformBuffer + parameter->resourceIndex;

		for (i = 0; i < componentCount; i++)
			data[componentOffset + i] = sourceData[i];
		break;
	}
	case SCE_GXM_PARAMETER_TYPE_S32: {
		SceInt32 *data = (SceInt32 *)uniformBuffer + parameter->resourceIndex;

		for (i = 0; i < componentCount; i++)
			data[componentOffset + i] = sourceData[i];
		break;
	}
	case SCE_GXM_PARAMETER_TYPE_U16: {
		SceUInt16 *data = (SceUInt16 *)((SceUInt32 *)uniformBuffer + parameter->resourceIndex);

		if (vectorSize == 3) {
			index = componentOffset + componentOffset / 3;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index = gxmUniformNextVec3Index(index);
			}
		} else if (vectorSize == 4 || vectorSize == 2) {
			for (i = 0; i < componentCount; i++)
				data[componentOffset + i] = sourceData[i];
		} else {
			index = componentOffset * 2;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index += 2;
			}
		}
		break;
	}
	case SCE_GXM_PARAMETER_TYPE_S16: {
		SceInt16 *data = (SceInt16 *)((SceUInt32 *)uniformBuffer + parameter->resourceIndex);

		if (vectorSize == 3) {
			index = componentOffset + componentOffset / 3;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index = gxmUniformNextVec3Index(index);
			}
		} else if (vectorSize == 4 || vectorSize == 2) {
			for (i = 0; i < componentCount; i++)
				data[componentOffset + i] = sourceData[i];
		} else {
			index = componentOffset * 2;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index += 2;
			}
		}
		break;
	}
	case SCE_GXM_PARAMETER_TYPE_U8: {
		SceUInt8 *data = (SceUInt8 *)((SceUInt32 *)uniformBuffer + parameter->resourceIndex);

		switch (vectorSize) {
		case 3:
			index = componentOffset + componentOffset / 3;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index = gxmUniformNextVec3Index(index);
			}
			break;
		case 4:
			for (i = 0; i < componentCount; i++)
				data[componentOffset + i] = sourceData[i];
			break;
		case 2:
			for (i = 0; i < componentCount; i++) {
				index = componentOffset + i;
				data[index + (index & ~1U)] = sourceData[i];
			}
			break;
		default:
			index = componentOffset * 4;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index += 4;
			}
			break;
		}
		break;
	}
	case SCE_GXM_PARAMETER_TYPE_S8: {
		SceInt8 *data = (SceInt8 *)((SceUInt32 *)uniformBuffer + parameter->resourceIndex);

		switch (vectorSize) {
		case 3:
			index = componentOffset + componentOffset / 3;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index = gxmUniformNextVec3Index(index);
			}
			break;
		case 4:
			for (i = 0; i < componentCount; i++)
				data[componentOffset + i] = sourceData[i];
			break;
		case 2:
			for (i = 0; i < componentCount; i++) {
				index = componentOffset + i;
				data[index + (index & ~1U)] = sourceData[i];
			}
			break;
		default:
			index = componentOffset * 4;
			for (i = 0; i < componentCount; i++) {
				data[index] = sourceData[i];
				index += 4;
			}
			break;
		}
		break;
	}
	default:
		return SCE_GXM_ERROR_INVALID_VALUE;
	}
	return 0;
}
