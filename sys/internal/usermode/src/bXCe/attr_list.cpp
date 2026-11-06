/*
 * ScebXCe's AttributeList: the C++ interface object of an attribute list, over an
 * AttributeListImpl.
 */
#include "bxce_internal.h"

namespace bXCe {

AttributeList::AttributeList() : impl(new AttributeListImpl())
{
}

AttributeList::~AttributeList()
{
	if (impl != NULL) {
		delete impl;
		impl = NULL;
	}
}

/* an attribute list over the library's list, which it does not own */
AttributeList::AttributeList(BxceAttributeList *list) : impl(new AttributeListImpl(list))
{
}

int AttributeList::initialize()
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->initialize();
}

int AttributeList::terminate()
{
	if (impl == NULL)
		return 0;
	return impl->terminate();
}

const BxceAttr *AttributeList::getAttribute(const char *name, SceSize length) const
{
	if (impl == NULL)
		return NULL;
	return impl->getAttribute(name, length);
}

const BxceAttr *AttributeList::getAttribute(SceSize index) const
{
	if (impl == NULL)
		return NULL;
	return impl->getAttribute(index);
}

int AttributeList::getLength() const
{
	if (impl == NULL)
		return 0;
	return impl->getLength();
}

int AttributeList::addAttribute(const char *name, const char *value)
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->addAttribute(name, value);
}

void AttributeList::clear()
{
	if (impl != NULL)
		impl->clear();
}

} /* namespace bXCe */
