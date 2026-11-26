/*
 * var_alloc_buffer.cpp: sce::Xml::VarAllocBuffer (0x81002248-0x8100245B).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

VarAllocBuffer::VarAllocBuffer(const Initializer *init)
	: InitializerUser(init), buffer(NULL), usedSize(0), capacity(0)
{
}

int VarAllocBuffer::clear()
{
	if (buffer != NULL) {
		const InitParameter *param = getInitParam();
		if (param == NULL)
			return SCE_XML_ERROR_NOT_INITIALIZED;
		param->allocator->deallocate(buffer, param->userData);
		buffer = NULL;
	}
	usedSize = 0;
	capacity = 0;
	return SCE_XML_SUCCESS;
}

int VarAllocBuffer::terminate()
{
	return clear();
}

VarAllocBuffer::~VarAllocBuffer()
{
	clear();
}

int VarAllocBuffer::reserve(size_t size)
{
	clear();
	const InitParameter *param = getInitParam();
	if (param == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;

	buffer = (SceUInt8 *)param->allocator->allocate(size, param->userData);
	if (buffer == NULL)
		return SCE_XML_ERROR_NO_MEMORY;
	capacity = size;
	return SCE_XML_SUCCESS;
}

int VarAllocBuffer::copy(const SceUInt8 *src, size_t size, bool addNull)
{
	if (usedSize == 0)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (src == NULL || size + (addNull ? 1u : 0u) > capacity)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	bxceMemcpy(buffer, src, size);
	if (addNull)
		buffer[size] = '\0';
	usedSize = size;
	return SCE_XML_SUCCESS;
}

int VarAllocBuffer::copyStr(const char *str, size_t size)
{
	if (usedSize == 0)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (str == NULL || size + 1u > capacity)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	bxceMemcpy(buffer, str, size);
	buffer[size] = '\0';
	usedSize = size;
	return SCE_XML_SUCCESS;
}

int VarAllocBuffer::copyStr(const String &str)
{
	return copyStr(str.buffer, str.size);
}

} /* namespace Xml */
} /* namespace sce */
