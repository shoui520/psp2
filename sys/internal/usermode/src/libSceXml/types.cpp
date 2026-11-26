/*
 * types.cpp: SimpleData, String, MemAllocator, and SerializeParameter methods
 * (0x81003624-0x8100372B).
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

SimpleData::SimpleData()
	: buffer(NULL), size(0)
{
}

SimpleData::SimpleData(const char *data, size_t len)
	: buffer(data), size(len)
{
}

String::String()
	: SimpleData(NULL, 0)
{
}

String::String(const String &src)
	: SimpleData(src.buffer, src.size)
{
}

String &String::operator=(const String &src)
{
	buffer = src.buffer;
	size = src.size;
	return *this;
}

String::String(const char *str, size_t len)
	: SimpleData(str, len)
{
}

String::String(const char *str)
	: SimpleData(str, 0)
{
	if (str != NULL)
		size = (size_t)bxceStrnlen(str, BXCE_ATTR_STRING_MAX);
}

MemAllocator::MemAllocator()
{
}

MemAllocator::~MemAllocator()
{
}

SerializeParameter::SerializeParameter()
{
	serializeOption = 0;
	bxceMemset(reserve, 0, sizeof(reserve));
}

} /* namespace Xml */
} /* namespace sce */
