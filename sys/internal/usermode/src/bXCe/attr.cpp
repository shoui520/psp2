/*
 * ScebXCe's Attr: the C++ interface object of an attribute, over an AttrImpl. Its own fields are
 * a BxceAttr's; it answers from them only while it has no implementation.
 */
#include "bxce_internal.h"

namespace bXCe {

Attr::Attr() : nameCapacity(0), valueCapacity(0), nameLength(0), valueLength(0), name(NULL),
	       value(NULL), next(NULL), impl(NULL)
{
	impl = new AttrImpl();
}

Attr::~Attr()
{
	if (impl != NULL) {
		delete impl;
		impl = NULL;
	}
}

int Attr::initialize()
{
	if (impl == NULL)
		return SCE_BXCE_RESULT_NOT_INITIALIZED;
	return impl->initialize();
}

int Attr::terminate()
{
	if (impl == NULL)
		return 0;
	return impl->terminate();
}

const char *Attr::getName() const
{
	if (impl == NULL)
		return name;
	return impl->getName();
}

const char *Attr::getValue() const
{
	if (impl == NULL)
		return value;
	return impl->getValue();
}

int Attr::getNameLength() const
{
	if (impl == NULL)
		return nameLength;
	return impl->getNameLength();
}

int Attr::getValueLength() const
{
	if (impl == NULL)
		return valueLength;
	return impl->getValueLength();
}

} /* namespace bXCe */
