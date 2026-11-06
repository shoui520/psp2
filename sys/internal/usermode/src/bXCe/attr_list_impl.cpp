/*
 * ScebXCe's AttributeListImpl: the C++ attribute list over the library's list, its own or
 * another object's.
 */
#include "bxce_internal.h"

namespace bXCe {

AttributeListImpl::AttributeListImpl() : list(NULL), isReference(false)
{
	list = (BxceAttributeList *)ScebXCe_B8DDE741(sizeof(BxceAttributeList));
}

AttributeListImpl::~AttributeListImpl()
{
	if (list != NULL && !isReference)
		ScebXCe_EC560647(list);
	isReference = false;
	list = NULL;
}

AttributeListImpl::AttributeListImpl(BxceAttributeList *list) : list(list), isReference(true)
{
}

int AttributeListImpl::initialize()
{
	if (list == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_E7C8B20D(list));
}

int AttributeListImpl::terminate()
{
	if (list == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	ScebXCe_9C2F9897(list);
	return bxceConvertResult(0);
}

const BxceAttr *AttributeListImpl::getAttribute(const char *name, SceSize length) const
{
	if (list == NULL)
		return NULL;
	return ScebXCe_2AD8DA26(list, name, length);
}

const BxceAttr *AttributeListImpl::getAttribute(SceSize index) const
{
	if (list == NULL)
		return NULL;
	return ScebXCe_01DC4E22(list, index);
}

int AttributeListImpl::getLength() const
{
	if (list == NULL)
		return 0;
	return ScebXCe_69F4B8B6(list);
}

int AttributeListImpl::addAttribute(const char *name, const char *value)
{
	if (list == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return bxceConvertResult(ScebXCe_3DD761CF(list, name, value));
}

void AttributeListImpl::clear()
{
	if (list != NULL)
		ScebXCe_B367042F(list);
}

} /* namespace bXCe */
