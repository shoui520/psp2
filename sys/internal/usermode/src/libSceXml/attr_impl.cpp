/*
 * attr_impl.cpp: sce::Xml::AttrImpl.
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

AttrImpl::AttrImpl(const Initializer *init)
	: CoreWrapper<BxceAttr, AttrCoreTraits>(init)
{
}

AttrImpl::~AttrImpl()
{
}

String AttrImpl::getName() const
{
	if (core != NULL)
		return String(core->name, (size_t)core->nameLength);
	return String();
}

String AttrImpl::getValue() const
{
	String result;
	if (core != NULL) {
		result.buffer = core->value;
		result.size = (size_t)core->valueLength;
	}
	return result;
}

int AttrImpl::setName(const String *str)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceAttrSetName(core, str->buffer, (int)str->size);
	return bXResultToResultType(bxRet);
}

int AttrImpl::setValue(const String *str)
{
	if (core == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	int bxRet = bxceAttrSetValue(core, str->buffer, (int)str->size);
	return bXResultToResultType(bxRet);
}

} /* namespace Xml */
} /* namespace sce */
