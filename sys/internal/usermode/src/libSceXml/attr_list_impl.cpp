/*
 * attr_list_impl.cpp: sce::Xml::AttributeListImpl.
 */
#include "libSceXml_internal.h"

namespace sce
{
namespace Xml
{

AttributeListImpl::~AttributeListImpl()
{
	terminate();
}

int AttributeListImpl::terminate()
{
	if (list == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (!isReference) {
		ScebXCe_9C2F9897(list);
		const InitParameter *param = getInitParam();
		param->allocator->deallocate(list, param->userData);
	}
	list = NULL;
	return SCE_XML_SUCCESS;
}

int AttributeListImpl::initialize(const Initializer *init)
{
	initializer = init;
	if (list == NULL) {
		const InitParameter *param = getInitParam();
		list = (BxceAttributeList *)param->allocator->allocate(sizeof(BxceAttributeList),
								       param->userData);
	}
	int bxRet = ScebXCe_E7C8B20D(list);
	return bXResultToResultType(bxRet);
}

int AttributeListImpl::copyFrom(const AttributeListImpl *src)
{
	if (list != NULL) {
		if (!isReference) {
			ScebXCe_9C2F9897(list);
			const InitParameter *param = getInitParam();
			param->allocator->deallocate(list, param->userData);
		}
		list = NULL;
	}

	const InitParameter *param = getInitParam();
	list = (BxceAttributeList *)param->allocator->allocate(sizeof(BxceAttributeList),
							       param->userData);
	if (list == NULL)
		return SCE_XML_ERROR_NO_MEMORY;

	int bxRet = 0;
	if (src->isReference)
		list = src->list;
	else
		bxRet = bxceAttributeListCloneInit(list);
	return bXResultToResultType(bxRet);
}

int AttributeListImpl::attachReference(BxceAttributeList *refList)
{
	if (list != NULL && !isReference) {
		ScebXCe_9C2F9897(list);
		const InitParameter *param = getInitParam();
		param->allocator->deallocate(list, param->userData);
	}
	list = refList;
	isReference = true;
	return SCE_XML_SUCCESS;
}

Attr AttributeListImpl::getAttribute(const String *attName) const
{
	Attr result;
	if (list != NULL && attName != NULL && attName->buffer != NULL && attName->size != 0) {
		if (result.initialize(initializer) == SCE_XML_SUCCESS) {
			const BxceAttr *found = ScebXCe_2AD8DA26(list, attName->buffer, attName->size);
			if (found != NULL) {
				String attrName(found->name, (size_t)found->nameLength);
				result.setName(&attrName);
				String attrValue(found->value, (size_t)found->valueLength);
				result.setValue(&attrValue);
			}
		}
	}
	return result;
}

Attr AttributeListImpl::getAttribute(index_t index) const
{
	Attr result;
	if (list != NULL) {
		if (result.initialize(initializer) == SCE_XML_SUCCESS) {
			const BxceAttr *found = ScebXCe_01DC4E22(list, index);
			if (found != NULL) {
				String attrName(found->name, (size_t)found->nameLength);
				result.setName(&attrName);
				String attrValue(found->value, (size_t)found->valueLength);
				result.setValue(&attrValue);
			}
		}
	}
	return result;
}

size_t AttributeListImpl::getLength() const
{
	if (list != NULL)
		return (size_t)ScebXCe_69F4B8B6(list);
	return 0;
}

int AttributeListImpl::addAttribute(const String *name, const String *value)
{
	if (list == NULL)
		return SCE_XML_ERROR_NOT_INITIALIZED;
	if (name == NULL || name->buffer == NULL || name->size == 0 ||
	    value == NULL || value->buffer == NULL || value->size == 0)
		return SCE_XML_ERROR_INVALID_ARGUMENT;

	int bxRet = ScebXCe_3DD761CF(list, name->buffer, value->buffer);
	return bXResultToResultType(bxRet);
}

void AttributeListImpl::clear()
{
	if (list != NULL)
		ScebXCe_B367042F(list);
}

bool AttributeListImpl::isAvailable() const
{
	return list != NULL;
}

} /* namespace Xml */
} /* namespace sce */
